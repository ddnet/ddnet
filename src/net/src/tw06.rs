use arrayvec::ArrayString;
use arrayvec::ArrayVec;
use crate::CallbackData;
use crate::ConnectionEvent;
use crate::Context as _;
use crate::Error;
use crate::Event;
use crate::PeerIndex;
use crate::PrivateIdentity;
use crate::Tw06Addr as Addr;
use crate::Result;
use crate::Something;
use getrandom::getrandom;
use mio::net::UdpSocket;
use std::collections::VecDeque;
use std::net::SocketAddr;
use std::str;
use std::time::Duration;
use std::time::Instant;

// TODO: use ddnet token impl from libtw2::net

pub struct Protocol;

impl Protocol {
    pub fn new(_: &PrivateIdentity) -> Result<Protocol> {
        Ok(Protocol)
    }
    pub fn remove_peer(&mut self, idx: PeerIndex, conn: Connection) {
        let _ = idx;
        let _ = conn;
        // Nothing to do.
    }
    pub fn on_recv(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        packet_len: usize,
        from: &SocketAddr,
    ) -> Result<Option<Something<Connection>>> {
        if !cb.accept_connections ||
            !libtw2_net::protocol::Packet::is_connect(&packet_buf[..packet_len])
        {
            return Ok(None);
        }
        let conn = Connection::new(libtw2_net::Connection::new(), Instant::now(), false, *from);
        Ok(Some(Something::NewConnection(cb.next_peer_index, conn)))
    }
    pub fn connect(
        &mut self,
        cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
        addr: Addr,
        _idx: PeerIndex,
    ) -> Result<Connection> {
        let Addr(addr) = addr;
        let epoch = Instant::now();
        let mut conn = libtw2_net::Connection::new();
        let cb = &mut Callback { socket: &cb.socket, addr: &addr, epoch };
        conn.connect(cb).context("libtw2_net::Conn::connect")?;
        Ok(Connection::new(conn, epoch, true, addr))
    }
}

enum State {
    NeedConnectEvent,
    Normal,
    Disconnected,
}

pub struct Connection {
    inner: libtw2_net::Connection,
    epoch: Instant,
    state: State,
    addr: SocketAddr,
    buffered_events: VecDeque<BufferedEvent>,
}

#[derive(Debug)]
enum BufferedEvent {
    Connect,
    /// `Chunk(data, unreliable)`
    Chunk(ArrayVec<[u8; 2048]>, bool),
    /// `Disconnect(reason, remote)`
    Disconnect(ArrayString<[u8; 2048]>, bool),
    Delete,
}

impl Connection {
    fn new(inner: libtw2_net::Connection, epoch: Instant, client: bool, addr: SocketAddr) -> Connection {
        Connection {
            inner,
            epoch,
            state: if !client { State::NeedConnectEvent } else { State::Normal },
            addr,
            buffered_events: VecDeque::with_capacity(4),
        }
    }
    pub fn on_recv(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        packet_len: usize,
        from: &SocketAddr,
    ) -> Result<()> {
        if let State::Disconnected = self.state {
            return Ok(())
        }
        assert!(*from == self.addr);
        let packet_buf_len = packet_buf.len();
        let (packet_buf, buf) = packet_buf.split_at_mut(packet_buf_len - 2048);
        let cb = &mut Callback { socket: &cb.socket, addr: &self.addr, epoch: self.epoch };
        let (events, result) = self.inner.feed(cb, &mut warn::Ignore, &packet_buf[..packet_len], buf);
        // TODO: don't allow infinite backlog
        use libtw2_net::connection::ReceiveChunk;
        use self::BufferedEvent::*;
        for event in events {
            let event = match event {
                ReceiveChunk::Connless(_) => continue,
                ReceiveChunk::Connected(chunk, reliable) => Chunk(chunk.iter().copied().collect(), !reliable),
                ReceiveChunk::Ready => Connect,
                ReceiveChunk::Disconnect(reason) => {
                    let reason = str::from_utf8(reason).ok().unwrap_or("(invalid utf-8)");
                    Disconnect(ArrayString::from(reason).unwrap(), true)
                }
            };
            if let State::NeedConnectEvent = self.state {
                self.state = State::Normal;
                self.buffered_events.push_back(Connect);
            }
            let is_disconnect = matches!(event, Disconnect(..));
            self.buffered_events.push_back(event);
            if is_disconnect {
                self.buffered_events.push_back(Delete);
                self.state = State::Disconnected;
            }
        }
        result?;
        Ok(())
    }
    pub fn recv(
        &mut self,
        _cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
        buf: &mut [u8],
    ) -> Result<Option<ConnectionEvent>> {
        use self::BufferedEvent::*;
        Ok(self.buffered_events.pop_front().map(|ev| match ev {
            Connect => Event::Connect(None).into(),
            Chunk(chunk, unreliable) => {
                buf[..chunk.len()].copy_from_slice(&chunk);
                Event::Chunk(chunk.len(), unreliable).into()
            }
            Disconnect(reason, remote) => {
                buf[..reason.len()].copy_from_slice(reason.as_bytes());
                Event::Disconnect(reason.len(), remote).into()
            }
            Delete => ConnectionEvent::Delete,
        }))
    }
    pub fn send_chunk(
        &mut self,
        cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
        frame: &[u8],
        unreliable: bool,
    ) -> Result<()> {
        if let State::Disconnected = self.state {
            return Ok(())
        }
        let cb = &mut Callback { socket: &cb.socket, addr: &self.addr, epoch: self.epoch };
        self.inner.send(cb, frame, !unreliable)
            .map_err(|e| e.unwrap_callback())
            .context("libtw2_net::Conn::send")?;
        Ok(())
    }
    pub fn close(
        &mut self,
        cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
        reason: Option<&str>,
    ) -> Result<()> {
        if let State::Disconnected = self.state {
            return Ok(())
        }
        let cb = &mut Callback { socket: &cb.socket, addr: &self.addr, epoch: self.epoch };
        let reason = reason.unwrap_or("");
        self.inner.disconnect(cb, reason.as_bytes()).context("libtw2_net::Conn::disconnect")?;
        self.buffered_events.clear();
        self.buffered_events.push_back(BufferedEvent::Disconnect(ArrayString::from(reason).unwrap(), false));
        self.buffered_events.push_back(BufferedEvent::Delete);
        self.state = State::Disconnected;
        Ok(())
    }
    pub fn timeout(&self) -> Option<Instant> {
        self.inner.needs_tick()
            .to_opt()
            .map(|ts| self.epoch + Duration::from_micros(ts.as_usecs_since_epoch()))
    }
    pub fn on_timeout(
        &mut self,
        cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
    ) -> Result<bool> {
        if let State::Disconnected = self.state {
            return Ok(false);
        }
        let cb = &mut Callback { socket: &cb.socket, addr: &self.addr, epoch: self.epoch };
        self.inner.tick(cb).context("libtw2_net::Conn::tick")?;
        Ok(false)
    }
    pub fn flush(
        &mut self,
        cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
    ) -> Result<()> {
        if let State::Disconnected = self.state {
            return Ok(())
        }
        let cb = &mut Callback { socket: &cb.socket, addr: &self.addr, epoch: self.epoch };
        self.inner.flush(cb).context("libtw2_net::Conn::flush")?;
        Ok(())
    }
}

struct Callback<'a> {
    socket: &'a UdpSocket,
    addr: &'a SocketAddr,
    epoch: Instant,
}

impl<'a> libtw2_net::connection::Callback for Callback<'a> {
    type Error = Error;
    fn secure_random(&mut self, buffer: &mut [u8]) {
        getrandom(buffer).unwrap()
        //let _ = buffer;
        //unreachable!()
    }
    fn send(&mut self, buffer: &[u8]) -> Result<()> {
        self.socket.send_to(buffer, *self.addr).context("UdpSocket::send_to")?;
        Ok(())
    }
    fn time(&mut self) -> libtw2_net::Timestamp {
        libtw2_net::Timestamp::from_usecs_since_epoch(self.epoch.elapsed().as_micros().try_into().unwrap())
    }
}
