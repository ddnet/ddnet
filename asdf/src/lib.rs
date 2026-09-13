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
use std::pin::Pin;
use std::sync::Arc;
use std::sync::Mutex;
use tokio::io::AsyncRead;
use tokio::io::AsyncWrite;
use tokio::net::TcpListener;
use tokio::net::TcpStream;
use tokio::sync::mpsc;
use tokio::sync::watch;
use tokio::sync::Mutex as AsyncMutex;
use tokio::task::JoinError;

// Reexports.
use self::pool::TaskPool;
use self::pool::TaskPoolHandle;
use self::pool::task_pool_with_main_and_error;
use self::time::Timestamp;

mod io;
mod pool;
mod protocol;
mod time;

// TODO: expiry
#[derive(Default)]
struct Bans {
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
    fn current_ban_list(&self) -> Arc<Vec<Ban>> {
        self.sorted.clone()
    }
}

#[derive(Deserialize, Serialize)]
struct BanData {
    expiry: Timestamp,
    reason: Arc<str>,
}

#[derive(Clone, Default)]
pub struct State {
    bans: Arc<Mutex<Bans>>,
    watch: watch::Sender<()>,
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
}

impl State {
    fn on_replace_bans(&self, message: protocol::ReplaceBansMessage) -> anyhow::Result<()> {
        {
            let mut bans = self.bans.lock().unwrap();
            let changed = bans.on_replace_bans(message)?;
            if changed {
                // notify everyone
                self.watch.send_replace(());
            }
        }
        Ok(())
    }
    fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        {
            let mut bans = self.bans.lock().unwrap();
            let changed = bans.on_ban_message(message);
            if changed {
                // notify everyone
                self.watch.send_replace(());
            }
        }
        Ok(())
    }
}

impl Remote {
    async fn on_ban_message(&self, message: protocol::BanMessage) -> anyhow::Result<()> {
        self.writer.send(message).await.expect("upstream queue should never go away");
        Ok(())
    }
}

async fn handle_ban_subscription(
    writer: Arc<AsyncMutex<io::Writer<protocol::ServerMessage>>>,
    state: State,
) -> anyhow::Result<()> {
    let (bans, mut watch) = {
        let State { bans, watch } = state;
        (bans, watch.subscribe())
    };
    loop {
        let ban_list = {
            let bans = bans.lock().unwrap();
            // clear notification
            watch.mark_unchanged();
            bans.current_ban_list()
        };
        writer.lock().await.write(&protocol::ReplaceBansMessage {
            bans: ban_list,
        }.into()).await?;
        if watch.changed().await.is_err() {
            // Change sender has gone away. We don't need to send updates anymore.
            return Ok(())
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
            Close(CloseMessage { error }) => {
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

pub async fn handle_server_connection(
    reader: Pin<Box<dyn AsyncRead + Send + Sync>>,
    writer: Pin<Box<dyn AsyncWrite + Send + Sync>>,
    state: State,
) -> anyhow::Result<()> {
    let (reader, writer) = io::from(reader, writer);
    let writer = Arc::new(AsyncMutex::new(writer));
    let writer2 = writer.clone();
    task_pool_with_main_and_error(
        |_| handle_server_connection_impl(reader, writer2, state),
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

async fn handle_server_connection_impl(
    reader: io::Reader<protocol::ServerMessage>,
    writer: Arc<AsyncMutex<io::Writer<protocol::ClientMessage>>>,
    state: State,
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
    loop {
        use protocol::ServerMessage::*;
        use protocol::*;
        match reader.read().await? {
            ServerHello(_) => bail!("second server hello received"),
            Close(CloseMessage { error }) => bail!("unexpected server close"),

            ReplaceBans(replace_bans) => state.on_replace_bans(replace_bans)?,
        }
    }
}

pub async fn bind() -> anyhow::Result<()> {
    let state = State::default();
    let backend = Backend::Local(state.clone());

    let listener = TcpListener::bind("127.0.0.1:8300").await.context("failed to bind")?;
    loop {
        let (stream, _) = listener.accept().await.context("failed to accept")?;
        let (reader, writer) = stream.into_split();
        tokio::spawn({
            let state = state.clone();
            let backend = backend.clone();
            async move {
                handle_client_connection(Box::pin(reader), Box::pin(writer), state, backend).await.unwrap()
            }
        });
    }
}

pub struct Connect {
    bans: watch::Receiver<Arc<Vec<Ban>>>,
    asdf: Asdf,
}

pub fn connect_and_subscribe(address: &str) -> anyhow::Result<ConnectSubscribe> {
    let (tx, rx) = watch::channel();
    Connect {
        bans: rx,
        asdf: Asdf,
    }
}

async fn connect_loop(address: &str, bans: watch::Sender<Arc<Vec<Ban>>>) {
    // TODO: retry on disconnect
    connect_impl(address, bans).await
}

async fn connect_impl(address: &str, bans: watch::Sender<Arc<Vec<Ban>>>)
    -> anyhow::Result<!>
{
    let stream = TcpStream::connect(address).await.context("failed to connect")?;
    let (reader, writer) = stream.into_split();
    handle_connect(reader, writer, bans).await
}

async fn handle_connect(
    reader: Pin<Box<dyn AsyncRead + Send + Sync>>,
    writer: Pin<Box<dyn AsyncWrite + Send + Sync>>,
    bans: watch::Sender<Arc<Vec<Ban>>>,
) {
    let (reader, writer) = io::from(reader, writer);
    let writer = Arc::new(AsyncMutex::new(writer));
    let writer2 = writer.clone();
    task_pool_with_main_and_error(
        |handle| handle_connect_impl(handle, reader, writer2, state, backend),
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

async fn handle_connect_impl(
    reader: io::Reader<protocol::ServerMessage>,
    writer: Arc<AsyncMutex<io::Writer<protocol::ClientMessage>>>,
    tx: watch::Sender<Arc<Vec<Ban>>>,
) -> anyhow::Result<!> {
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
    loop {
        use protocol::ServerMessage::*;
        use protocol::*;
        match reader.read().await? {
            ServerHello(_) => bail!("second server hello received"),
            Close(CloseMessage { error }) => bail!("unexpected server close"),

            // TODO: exit cleanly if no one listens?
            ReplaceBans(ReplaceBansMessage { bans }) => tx.send(bans).unwrap(),
        }
    }
}

pub struct Asdf(());

impl Asdf {
    pub async fn send_ban_message(&self, message: protocol::BanMessage) {
    }
}

pub async fn connect(address: &str) -> anyhow::Result<()> {
    let stream = TcpStream::connect(address).await.context("failed to connect")?;
    let (reader, writer) = stream.into_split();
    let (mut reader, mut writer) = io::from::<protocol::ServerMessage, protocol::ClientMessage>(Box::pin(reader), Box::pin(writer));
    writer.write(&protocol::ClientMessage::ClientHello(protocol::ClientHelloMessage::default())).await?;
    writer.write(&protocol::ClientMessage::AddBan(protocol::AddBanMessage {
        expiry: "2100-01-01T12:34:56Z".parse().unwrap(),
        reason: "foobar".into(),
        net: "127.0.0.1/32".parse().unwrap(),
    })).await?;
    writer.write(&protocol::ClientMessage::SubscribeBans(protocol::SubscribeBansMessage)).await?;
    loop {
        let message = reader.read().await?;
        println!("{:?}", message);
        if let protocol::ServerMessage::Close(protocol::CloseMessage { error }) = message {
            if let Some(error) = error {
                println!("error: {error:?}");
            }
            break;
        }
    }
    Ok(())
}
