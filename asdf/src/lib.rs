use anyhow::Context as _;
use anyhow::anyhow as error;
use anyhow::bail;
use crate::protocol::Ban;
use futures_util::StreamExt as _;
use ipnet::IpNet;
use serde::Deserialize;
use serde::Serialize;
use std::collections::HashMap;
use std::collections::hash_map;
use std::convert::Infallible;
use std::future::Future;
use std::mem;
use std::net::SocketAddr;
use std::net::TcpListener;
use std::pin::Pin;
use std::sync::Arc;
use tokio::io::AsyncRead;
use tokio::io::AsyncWrite;
use tokio::net::TcpListener as AsyncTcpListener;
use tokio::net::TcpStream as AsyncTcpStream;
use tokio::sync::mpsc;
use tokio::sync::watch;
use tokio::sync::Mutex as AsyncMutex;
use tokio::task::JoinError;

// Reexports.
use self::pool::TaskPool;
use self::pool::TaskPoolHandle;
use self::time::Timestamp;

mod io;
mod pool;
mod protocol;
mod time;

// TODO: expiry
#[derive(Default)]
pub struct Bans {
    bans: HashMap<IpNet, BanData>,
    sorted: Arc<Vec<Ban>>,
}

impl Bans {
    fn on_replace_bans(&mut self, mut message: protocol::ReplaceBansMessage) -> anyhow::Result<bool> {
        if !message.bans.is_sorted() {
            Arc::make_mut(&mut message.bans).sort();
        }
        if self.sorted == message.bans {
            return Ok(false);
        }

        let old = mem::replace(&mut self.sorted, message.bans);
        self.bans.clear();
        for &Ban { net, expiry, ref reason } in self.sorted.iter() {
            if self.bans.insert(net, BanData { expiry, reason: reason.clone() }).is_some() {
                // Roll back to previous known-good state.
                self.on_replace_bans(protocol::ReplaceBansMessage { bans: old }).unwrap();
                bail!("duplicate bans in replace bans message");
            }
        }
        Ok(true)
    }
    fn on_ban_message(&mut self, message: protocol::BanMessage) -> bool {
        use protocol::BanMessage::*;
        match message {
            AddBan(protocol::AddBanMessage {
                expiry,
                reason,
                net,
            }) => {
                match self.bans.entry(net) {
                    hash_map::Entry::Vacant(v) => {
                        v.insert(BanData {
                            expiry,
                            reason: reason.clone(),
                        });
                        {
                            let index = self.sorted.binary_search_by_key(&net, |b| b.net).unwrap_err();
                            Arc::make_mut(&mut self.sorted).insert(index, Ban {
                                net,
                                expiry,
                                reason,
                            });
                        }
                        true
                    }
                    hash_map::Entry::Occupied(mut o) => {
                        let ban = o.get_mut();
                        let replace = ban.expiry < expiry;
                        if replace {
                            *ban = BanData {
                                expiry,
                                reason: reason.clone(),
                            };
                            {
                                let index = self.sorted.binary_search_by_key(&net, |b| b.net).unwrap();
                                let element = &mut Arc::make_mut(&mut self.sorted)[index];
                                element.expiry = expiry;
                                element.reason = reason;
                            }
                        }
                        replace
                    }
                }
            }
            RemoveBan(protocol::RemoveBanMessage { net }) => {
                let changed = self.bans.remove(&net).is_some();
                if changed {
                    let index = self.sorted.binary_search_by_key(&net, |b| b.net).unwrap();
                    Arc::make_mut(&mut self.sorted).remove(index);
                }
                changed
            }
        }
    }
    pub fn current_ban_list(&self) -> Arc<Vec<Ban>> {
        self.sorted.clone()
    }
}

#[derive(Deserialize, Serialize)]
struct BanData {
    expiry: Timestamp,
    reason: Arc<str>,
}

pub struct Asdf {
    state: State,
    backend: Backend,
}

impl Asdf {
    pub fn listen(bindaddr: SocketAddr) -> anyhow::Result<Asdf> {
        let state = State::default();
        let backend = Backend::Local(state.clone());

        let listener = TcpListener::bind(bindaddr).context("bind")?;
        listener.set_nonblocking(true).context("set_nonblocking")?;

        tokio::spawn(accept(listener.try_into().context("tokio")?, state.clone(), backend.clone()));

        Ok(Asdf {
            state,
            backend,
        })
    }
    pub fn remote(connect_to: Arc<str>) -> Asdf {
        let state = State::default();
        let (tx, rx) = mpsc::channel(16);

        tokio::spawn(connect(state.clone(), rx, connect_to));

        Asdf {
            state,
            backend: Backend::Remote(Remote {
                writer: tx,
            }),
        }
    }
    pub fn subscribe_bans(&self) -> watch::Receiver<Bans> {
        self.state.bans.subscribe()
    }
    pub fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        self.backend.try_on_ban_message(message)
    }
}

#[derive(Clone, Default)]
pub struct State {
    bans: Arc<watch::Sender<Bans>>,
}

#[derive(Clone)]
pub struct Remote {
    writer: mpsc::Sender<protocol::BanMessage>,
}

#[derive(Clone)]
pub enum Backend {
    Local(State),
    Remote(Remote),
}

impl Backend {
    async fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        use self::Backend::*;
        match self {
            Local(state) => state.on_ban_message(message),
            Remote(remote) => remote.on_ban_message(message).await,
        }
    }
    fn try_on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        use self::Backend::*;
        match self {
            Local(state) => state.on_ban_message(message),
            Remote(remote) => remote.try_on_ban_message(message),
        }
    }
}

impl State {
    fn on_replace_bans(&self, message: protocol::ReplaceBansMessage) -> anyhow::Result<()> {
        {
            let mut result = None;
            self.bans.send_if_modified(|bans| {
                // on_replace_bans doesn't change the bans when an error is returned.
                *result.insert(bans.on_replace_bans(message)).as_ref().unwrap_or(&false)
            });
            result.unwrap()?;
        }
        Ok(())
    }
    fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        {
            self.bans.send_if_modified(|bans| bans.on_ban_message(message));
        }
        Ok(())
    }
}

impl Remote {
    async fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        self.writer.send(message).await.expect("upstream queue should never go away");
        Ok(())
    }
    fn try_on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        self.writer.try_send(message)
            .map_err(|err| match err {
                mpsc::error::TrySendError::Full(_) => error!("too many ban messages"),
                mpsc::error::TrySendError::Closed(_) => panic!("upstream queue should never go away"),
            })
    }
}

async fn handle_ban_subscription(
    writer: Arc<AsyncMutex<io::Writer<protocol::ServerMessage>>>,
    state: State,
) -> anyhow::Result<()> {
    let mut watch = state.bans.subscribe();
    loop {
        let ban_list = watch.borrow_and_update().current_ban_list();
        writer.lock().await.write(&protocol::ReplaceBansMessage {
            bans: ban_list,
        }.into()).await?;
        if watch.changed().await.is_err() {
            // Change sender has gone away. We don't need to send updates anymore.
            return Ok(());
        }
    }
}

fn error_close(name: &str, result: &Result<anyhow::Result<Infallible>, JoinError>) -> protocol::CloseMessage {
    let message = match result {
        Ok(Ok(infallible)) => match *infallible {}
        Ok(Err(err)) => if name == "main" {
            format!("{err:#}")
        } else {
            format!("task {name} error: {err:#}")
        },
        Err(err) => format!("task {name}: {err}"),
    };
    protocol::CloseMessage {
        error: Some(message.into()),
    }
}

/// Creates a task pool with a main task and error handling.
///
/// The whole task pool quits when the main task quits.
///
/// When any task in the pool errors or panics, the `error` function is called.
///
/// # Errors
///
/// Returns an error (in addition to calling the `error` function) when any
/// tasks errors or panics.
pub async fn task_pool_with_main_and_error<M, E, MF, EF>(main: M, error: E)
    -> anyhow::Result<()>
where
    M: FnOnce(TaskPoolHandle) -> MF,
    E: FnOnce(protocol::CloseMessage) -> EF,
    MF: Future<Output = anyhow::Result<()>> + Send + 'static,
    EF: Future<Output = anyhow::Result<()>> + Send + 'static,
{
    let (mut task_pool, handle) = TaskPool::new();
    handle.clone().spawn("main", main(handle)).await;
    while let Some(task) = task_pool.next().await {
        let result: Result<anyhow::Result<Infallible>, JoinError> = match task.result {
            Ok(Ok(())) => {
                if task.name == "main" {
                    // The main task quit, let's quit as well.
                    task_pool.close().await;
                    return Ok(());
                } else {
                    // Another task quit, nothing to do.
                    continue;
                }
            }
            Ok(Err(err)) => Ok(Err(err)),
            Err(err) => Err(err),
        };
        let close_message = error_close(task.name, &result);
        let error_result = error(close_message).await;
        task_pool.close().await;
        let () = error_result
            .with_context(|| format!("sending error failed while processing another error: {}", error_close(task.name, &result).error.unwrap()))?;
        let result = result
            .with_context(|| format!("task {} panicked", task.name))?;
        let infallible = if task.name == "main" {
            result?
        } else {
            result.with_context(|| format!("task {} errored", task.name))?
        };
        match infallible {}
    }
    // main task has to quit or error before this.
    unreachable!();
}

pub async fn accept(listener: AsyncTcpListener, state: State, backend: Backend) -> anyhow::Result<()> {
    loop {
        let (stream, remote) = listener.accept().await.context("failed to accept")?;
        let (reader, writer) = stream.into_split();
        tokio::spawn({
            let state = state.clone();
            let backend = backend.clone();
            async move {
                if let Err(err) = handle_client_connection(Box::pin(reader), Box::pin(writer), state, backend).await {
                    log::error!("error for {remote}: {err:#}");
                }
            }
        });
    }
}

pub async fn handle_client_connection(
    reader: Pin<Box<dyn AsyncRead + Send + Sync>>,
    writer: Pin<Box<dyn AsyncWrite + Send + Sync>>,
    state: State,
    backend: Backend,
) -> anyhow::Result<()> {
    let (reader, writer) = io::from(reader, writer);
    let writer = Arc::new(AsyncMutex::new(writer));
    let writer2 = writer.clone();
    task_pool_with_main_and_error(
        |handle| handle_client_connection_impl(handle, reader, writer2, state, backend),
        |close_message| async move {
            {
                let mut writer = writer.lock().await;
                writer.write(&close_message.into()).await?;
                writer.close().await?;
            }
            Ok(())
        },
    ).await
}

/// Main logic to handle connections from clients.
async fn handle_client_connection_impl(
    handle: TaskPoolHandle,
    reader: io::Reader<protocol::ClientMessage>,
    writer: Arc<AsyncMutex<io::Writer<protocol::ServerMessage>>>,
    state: State,
    backend: Backend,
) -> anyhow::Result<()> {
    let mut reader = reader;

    let protocol::ClientMessage::ClientHello(client_hello) = reader.read().await? else {
        bail!("first message must be client hello");
    };
    let _ = client_hello;
    writer.lock().await.write(&protocol::ServerHelloMessage.into()).await?;

    let mut state = Some(state);

    loop {
        use protocol::ClientMessage::*;
        use protocol::*;
        let ban_message: protocol::BanMessage = match reader.read().await? {
            ClientHello(_) => bail!("second client hello received"),
            Close(CloseMessage { error: _ }) => {
                {
                    let mut writer = writer.lock().await;
                    writer.write(&CloseMessage { error: None }.into()).await?;
                    writer.close().await?;
                }
                return Ok(());
            }

            AddBan(add_ban) => add_ban.into(),
            SubscribeBans(SubscribeBansMessage) => {
                let state = state.take().ok_or_else(|| error!("can only subscribe once"))?;
                handle.spawn("ban_subscription", handle_ban_subscription(writer.clone(), state)).await;
                continue;
            },
            RemoveBan(remove_ban) => remove_ban.into(),
        };
        backend.on_ban_message(ban_message).await?;
    }
}

async fn connect(
    state: State,
    ban_messages: mpsc::Receiver<protocol::BanMessage>,
    remote: Arc<str>,
) -> anyhow::Result<()> {
    // TODO: retry on error
    let res = connect_impl(state, Arc::new(AsyncMutex::new(ban_messages)), &remote).await;
    if let Err(err) = &res {
        log::error!("{remote}: {err:#}");
    }
    res
}

async fn connect_impl(
    state: State,
    ban_messages: Arc<AsyncMutex<mpsc::Receiver<protocol::BanMessage>>>,
    remote: &str,
) -> anyhow::Result<()> {
    let stream = AsyncTcpStream::connect(remote).await.context("failed to connect")?;
    let (reader, writer) = stream.into_split();
    handle_server_connection(Box::pin(reader), Box::pin(writer), state, ban_messages).await
}

pub async fn handle_server_connection(
    reader: Pin<Box<dyn AsyncRead + Send + Sync>>,
    writer: Pin<Box<dyn AsyncWrite + Send + Sync>>,
    state: State,
    ban_messages: Arc<AsyncMutex<mpsc::Receiver<protocol::BanMessage>>>,
) -> anyhow::Result<()> {
    let (reader, writer) = io::from(reader, writer);
    let writer = Arc::new(AsyncMutex::new(writer));
    let writer2 = writer.clone();
    task_pool_with_main_and_error(
        |handle| handle_server_connection_impl(handle, reader, writer2, state, ban_messages),
        |close_message| async move {
            {
                let mut writer = writer.lock().await;
                writer.write(&close_message.into()).await?;
                writer.close().await?;
            }
            Ok(())
        },
    ).await
}

/// Main logic to handle connection to servers.
async fn handle_server_connection_impl(
    handle: TaskPoolHandle,
    reader: io::Reader<protocol::ServerMessage>,
    writer: Arc<AsyncMutex<io::Writer<protocol::ClientMessage>>>,
    state: State,
    ban_messages: Arc<AsyncMutex<mpsc::Receiver<protocol::BanMessage>>>,
) -> anyhow::Result<()> {
    let mut reader = reader;

    {
        let mut writer = writer.lock().await;
        writer.write(&protocol::ClientHelloMessage::default().into()).await?;
        let server_hello = reader.read().await?;
        let protocol::ServerMessage::ServerHello(protocol::ServerHelloMessage) = server_hello else {
            bail!("expected server hello, got {server_hello:?}");
        };

        writer.write(&protocol::SubscribeBansMessage.into()).await?;
    }

    handle.spawn("ban_send", {
        let writer = writer.clone();
        async move {
            let mut ban_messages = ban_messages.lock().await;
            while let Some(ban_message) = ban_messages.recv().await {
                writer.lock().await.write(&ban_message.into()).await?;
            }
            Ok(())
        }
    }).await;

    loop {
        use protocol::ServerMessage::*;
        use protocol::*;
        match reader.read().await? {
            ServerHello(_) => bail!("second server hello received"),
            Close(CloseMessage { error: _ }) => bail!("unexpected server close"),

            ReplaceBans(replace_bans) => state.on_replace_bans(replace_bans)?,
        }
    }
}
