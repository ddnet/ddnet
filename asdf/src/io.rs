use anyhow::Context as _;
use anyhow::bail;
use anyhow::ensure;
use serde::de::DeserializeOwned;
use serde::Serialize;
use std::fmt;
use std::pin::Pin;
use std::marker::PhantomData;
use tokio::io::AsyncBufReadExt as _;
use tokio::io::AsyncRead;
use tokio::io::AsyncWrite;
use tokio::io::AsyncWriteExt as _;
use tokio::io::BufReader;
use tokio::io::BufWriter;

pub trait Message: fmt::Debug + DeserializeOwned + Serialize {}

impl<T: fmt::Debug + DeserializeOwned + Serialize> Message for T {}

pub struct Reader<M: Message> {
    inner: Errored<ReaderInner<M>>,
}

struct ReaderInner<M: Message> {
    bytes: BufReader<Pin<Box<dyn AsyncRead + Send + Sync>>>,
    buf: String,
    phantom: PhantomData<fn() -> M>,
}

pub struct Writer<M: Message> {
    inner: Errored<WriterInner<M>>,
}

struct WriterInner<M: Message> {
    bytes: BufWriter<Pin<Box<dyn AsyncWrite + Send + Sync>>>,
    phantom: PhantomData<fn(M)>,
}

#[derive(Default)]
struct Errored<I> {
    errored: bool,
    inner: I,
}

impl<I> From<I> for Errored<I> {
    fn from(inner: I) -> Errored<I> {
        Errored {
            errored: false,
            inner,
        }
    }
}

#[allow(unused)]
impl<I> Errored<I> {
    fn check_errored(&self) -> anyhow::Result<()> {
        ensure!(!self.errored, "stream has errored");
        Ok(())
    }
    fn on_error(&mut self) {
        self.errored = true;
    }
    fn call<T, F: FnOnce(&I) -> anyhow::Result<T>>(&mut self, f: F) -> anyhow::Result<T> {
        self.check_errored()?;
        let result = f(&self.inner);
        if result.is_err() {
            self.on_error();
        }
        result
    }
    fn call_mut<T, F: FnOnce(&mut I) -> anyhow::Result<T>>(&mut self, f: F) -> anyhow::Result<T> {
        self.check_errored()?;
        let result = f(&mut self.inner);
        if result.is_err() {
            self.on_error();
        }
        result
    }
    async fn call_async<T, F: AsyncFnOnce(&I) -> anyhow::Result<T>>(&mut self, f: F) -> anyhow::Result<T> where {
        self.check_errored()?;
        let result = f(&self.inner).await;
        if result.is_err() {
            self.on_error();
        }
        result
    }
    async fn call_async_mut<T, F: AsyncFnOnce(&mut I) -> anyhow::Result<T>>(&mut self, f: F) -> anyhow::Result<T> where {
        self.check_errored()?;
        let result = f(&mut self.inner).await;
        if result.is_err() {
            self.on_error();
        }
        result
    }
}

pub fn from<RM: Message, WM: Message>(reader: Pin<Box<dyn AsyncRead + Send + Sync>>, writer: Pin<Box<dyn AsyncWrite + Send + Sync>>) -> (Reader<RM>, Writer<WM>) {
    let reader = Reader {
        inner: Errored::from(ReaderInner::new(BufReader::new(Box::pin(reader)))),
    };
    let writer = Writer {
        inner: Errored::from(WriterInner::new(BufWriter::new(Box::pin(writer)))),
    };
    (reader, writer)
}

impl<M: Message> ReaderInner<M> {
    pub fn new(bytes: BufReader<Pin<Box<dyn AsyncRead + Send + Sync>>>) -> ReaderInner<M> {
        ReaderInner {
            bytes,
            buf: String::new(),
            phantom: PhantomData,
        }
    }
}

impl<M: Message> WriterInner<M> {
    pub fn new(bytes: BufWriter<Pin<Box<dyn AsyncWrite + Send + Sync>>>) -> WriterInner<M> {
        WriterInner {
            bytes,
            phantom: PhantomData,
        }
    }
}

impl<M: Message> Reader<M> {
    pub async fn read(&mut self) -> anyhow::Result<M> {
        self.inner.call_async_mut(async move |inner| {
            inner.buf.clear();
            // TODO: unbounded
            inner.bytes.read_line(&mut inner.buf).await?;
            match inner.buf.as_bytes().last() {
                None => bail!("read stream closed unexpectedly"),
                Some(b'\n') => {},
                Some(_) => bail!("read stream closed unexpectedly while reading message"),
            }
            // TODO: this allows alternate representations that are not objects, e.g. arrays
            Ok(serde_json::from_str(&inner.buf).context("invalid message read")?)
        }).await
    }
}

impl<M: Message> Writer<M> {
    pub async fn write(&mut self, message: &M) -> anyhow::Result<()> {
        self.inner.call_async_mut(async move |inner| {
            inner.bytes.write_all(&serde_json::to_vec(message).unwrap()).await?;
            inner.bytes.write_all(b"\n").await?;
            inner.bytes.flush().await?;
            Ok(())
        }).await
    }
    pub async fn close(&mut self) -> anyhow::Result<()> {
        self.inner.call_async_mut(async move |inner| {
            inner.bytes.shutdown().await?;
            Ok(())
        }).await
    }
}
