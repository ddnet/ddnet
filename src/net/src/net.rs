use crate::normalize;
use crate::quic;
use crate::tw06;
use crate::Challenger;
use crate::Context as _;
use crate::Error;
use crate::Identity;
use crate::NoBlock as _;
use crate::PrivateIdentity;
use crate::Result;
use arrayvec::ArrayString;
use mio::net::UdpSocket;
use mio::Events;
use mio::Poll;
use std::collections::HashMap;
use std::collections::VecDeque;
use std::env;
use std::fmt;
use std::fmt::Write as _;
use std::fs;
use std::fs::File;
use std::io;
use std::io::Write as _;
use std::net::Ipv6Addr;
use std::net::SocketAddr;
use std::path::Path;
use std::str;
use std::str::FromStr;
use std::sync::Arc;
use std::time::Duration;
use std::time::Instant;
use url::Url;

// TODO: coalesce ACKs with other packets
// TODO: coalesce dgrams with stream packets
// TODO: remove double waits in the server (9999ms, then 0ms)

// Originally `NET_MAX_PAYLOAD`.
pub const MAX_FRAME_SIZE: u64 = 1394;

pub struct CallbackData {
    accept_connections: bool,
    sslkeylogfile: Option<ArcFile>,
    challenger: Challenger,
    local_addr: SocketAddr,
    socket: UdpSocket,
    next_peer_index: PeerIndex,
}

pub struct Net {
    cb: CallbackData,
    packet_buf: [u8; 65536],

    events: Events,
    poll: Poll,

    proto_quic: quic::Protocol,
    proto_tw06: tw06::Protocol,

    peer_addrs: HashMap<SocketAddr, PeerIndex>,
    peers: HashMap<PeerIndex, Connection>,
    connect_errors: VecDeque<(PeerIndex, Error)>,

    socket_readable: bool,
    readable_peers: VecDeque<PeerIndex>,
}

pub struct NetBuilder {
    bindaddr: Option<SocketAddr>,
    identity: Option<PrivateIdentity>,
    accept_connections: bool,
}

#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct PeerIndex(pub u64);

impl fmt::Display for PeerIndex {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

impl fmt::Debug for PeerIndex {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl PeerIndex {
    // TODO: get rid of pub(crate)
    pub(crate) fn get_and_increment(&mut self) -> PeerIndex {
        let result = *self;
        self.0 += 1;
        result
    }
}

#[derive(Clone, Copy)]
pub enum Event {
    Connect(Identity),
    /// `Chunk(size, unreliable)`
    Chunk(usize, bool),
    // TODO: distinguish disconnect from error?
    /// `Disconnect(reason_size, remote)`
    Disconnect(usize, bool),
}

pub enum Addr {
    Quic(QuicAddr),
    Tw06(Tw06Addr),
}

impl Addr {
    fn socket_addr(&self) -> &SocketAddr {
        use self::Addr::*;
        match self {
            Quic(QuicAddr(socket_addr, _)) => socket_addr,
            Tw06(Tw06Addr(socket_addr)) => socket_addr,
        }
    }
}

pub struct Tw06Addr(pub SocketAddr);
pub struct QuicAddr(pub SocketAddr, pub Identity);

impl FromStr for Addr {
    type Err = Error;
    fn from_str(addr: &str) -> Result<Addr> {
        let addr = Url::parse(addr).context("connect: URL")?;
        let mut ip_port: ArrayString<[u8; 64]> = ArrayString::new();
        write!(
            &mut ip_port,
            "{}:{}",
            addr.host_str().ok_or_else(|| Error::from_string(
                "connect: URL missing host".to_owned()
            ))?,
            addr.port().ok_or_else(|| Error::from_string(
                "connect: URL missing port".to_owned()
            ))?,
        )
        .unwrap();
        let sock_addr: SocketAddr =
            ip_port.parse().context("connect: IP addr")?;
        Ok(match addr.scheme() {
            "ddnet-15+quic" => {
                let fragment = addr.fragment().unwrap_or("");
                // Take at most 64 characters.
                let end = fragment
                    .char_indices()
                    .nth(64)
                    .map(|(idx, _)| idx)
                    .unwrap_or(fragment.len());
                let identity: Identity =
                    fragment[..end].parse().context("connect: identity")?;
                Addr::Quic(QuicAddr(sock_addr, identity))
            }
            "tw-0.6+udp" => Addr::Tw06(Tw06Addr(sock_addr)),
            scheme => bail!("unsupported scheme {}", scheme),
        })
    }
}

#[derive(Clone)]
pub struct ArcFile(Arc<File>);

impl io::Write for ArcFile {
    #[inline]
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        (&*self.0).write(buf)
    }
    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        (&*self.0).flush()
    }
    #[inline]
    fn write_vectored(
        &mut self,
        bufs: &[io::IoSlice<'_>],
    ) -> io::Result<usize> {
        (&*self.0).write_vectored(bufs)
    }
    // TODO(rust-lang/rust#69941): implement `is_write_vectored`
}

pub enum Something<C> {
    NewConnection(PeerIndex, C),
    ExistingConnection(PeerIndex),
}

impl<C> Something<C> {
    pub fn into<T: From<C>>(self) -> Something<T> {
        use self::Something::*;
        match self {
            NewConnection(idx, conn) => NewConnection(idx, conn.into()),
            ExistingConnection(idx) => ExistingConnection(idx),
        }
    }
}

impl NetBuilder {
    pub fn bindaddr(&mut self, bindaddr: SocketAddr) {
        self.bindaddr = Some(bindaddr);
    }
    pub fn identity(&mut self, identity: PrivateIdentity) {
        self.identity = Some(identity);
    }
    pub fn accept_connections(&mut self, accept: bool) {
        self.accept_connections = accept;
    }
    pub fn open(self) -> Result<Net> {
        let sslkeylogfile =
            if let Some(sslkeylogfile) = env::var_os("SSLKEYLOGFILE") {
                fs::OpenOptions::new()
                    .create(true)
                    .append(true)
                    .open(&sslkeylogfile)
                    .map(|file| ArcFile(Arc::new(file)))
                    .map_err(|err| {
                        error!(
                            "error opening SSLKEYLOGFILE {}: {}",
                            Path::new(&sslkeylogfile).display(),
                            err
                        )
                    })
                    .ok()
            } else {
                None
            };

        let bindaddr = self.bindaddr.unwrap_or(SocketAddr::new(
            Ipv6Addr::new(0, 0, 0, 0, 0, 0, 0, 0).into(),
            0,
        ));
        let identity = self.identity.unwrap_or_else(PrivateIdentity::random);

        let mut socket = UdpSocket::bind(bindaddr).context("bind")?;
        let local_addr = socket.local_addr().context("local_addr")?;

        let poll = Poll::new().context("mio::Poll::new")?;
        let events = Events::with_capacity(1);
        poll.registry()
            .register(&mut socket, mio::Token(0), mio::Interest::READABLE)
            .context("mio::Poll::register")?;

        info!("identity {}", identity.public());
        info!("listening on {}", bindaddr);

        Ok(Net {
            cb: CallbackData {
                accept_connections: self.accept_connections,
                sslkeylogfile,
                challenger: Challenger::new(),
                local_addr,
                socket,
                next_peer_index: PeerIndex(0),
            },
            // TODO: uninitialized
            packet_buf: [0; 65536],

            events,
            poll,

            proto_quic: quic::Protocol::new(&identity)?,
            proto_tw06: tw06::Protocol::new(&identity)?,

            peer_addrs: HashMap::new(),
            peers: HashMap::new(),
            connect_errors: VecDeque::with_capacity(1),

            socket_readable: false,
            readable_peers: VecDeque::with_capacity(4),
        })
    }
}

impl Net {
    pub fn builder() -> NetBuilder {
        NetBuilder {
            bindaddr: None,
            identity: None,
            accept_connections: false,
        }
    }
    fn socket_read(&mut self) -> Result<Option<PeerIndex>> {
        loop {
            let Some((read, from)) = self.cb.socket.recv_from(&mut self.packet_buf).no_block().context("UdpSocket::recv_from")? else { break; };
            let from = normalize(from);
            let idx = if let Some(&idx) = self.peer_addrs.get(&from) {
                idx
            } else {
                // A packet whose type we do not know. Determine it by the
                // first two bytes.
                //
                // 00000000:          stun request
                // 00000001:          stun response
                // 00000100:          teeworlds 0.7 connect/token
                // 00010000:          teeworlds 0.6 connect
                // 00100001:          teeworlds 0.7 connless
                // 01111000 01100101: ddnet 0.6 connless extended
                // 01xxxxxx xxxxxxxx: quic connected
                // 1100xxxx:          quic initial
                // 11111111:          source connless packets
                // 11111111:          teeworlds 0.6 connless

                let packet = &self.packet_buf[..read];
                let something = match (packet.get(0), packet.get(1)) {
                    (Some(&p0), Some(&p1))
                        if p0 & 0b11000000 == 0b01000000
                            && p1 != 0b01100101 =>
                    {
                        self.proto_quic
                            .on_recv(
                                &mut self.packet_buf,
                                read,
                                &from,
                                &self.cb.challenger,
                                if self.cb.accept_connections {
                                    Some(&mut self.cb.next_peer_index)
                                } else {
                                    None
                                },
                                &self.cb.local_addr,
                                self.cb.sslkeylogfile.as_ref(),
                                &self.cb.socket,
                            )
                            .map(|something| something.map(Something::into))?
                    }
                    (Some(&p0), _) if p0 & 0b11110000 == 0b11000000 => {
                        self.proto_quic
                            .on_recv(
                                &mut self.packet_buf,
                                read,
                                &from,
                                &self.cb.challenger,
                                if self.cb.accept_connections {
                                    Some(&mut self.cb.next_peer_index)
                                } else {
                                    None
                                },
                                &self.cb.local_addr,
                                self.cb.sslkeylogfile.as_ref(),
                                &self.cb.socket,
                            )
                            .map(|something| something.map(Something::into))?
                    }
                    _ => continue,
                };
                match something {
                    Some(Something::NewConnection(idx, conn)) => {
                        assert!(self.peers.insert(idx, conn).is_none());
                        assert!(self.peer_addrs.insert(from, idx).is_none());
                        idx
                    }
                    Some(Something::ExistingConnection(idx)) => {
                        assert!(self.peer_addrs.insert(from, idx).is_none());
                        idx
                    }
                    None => continue,
                }
            };
            let conn = self.peers.get_mut(&idx).unwrap();
            let _ = conn.on_recv(
                &mut self.packet_buf[..read],
                &from,
                &self.cb.local_addr,
            );
            // TODO: figure out why error packets aren't getting sent
            conn.flush(&self.cb.local_addr, &self.cb.socket, &mut self.packet_buf)
                .unwrap();
            return Ok(Some(idx));
        }
        Ok(None)
    }
    fn wait_impl(&mut self, timeout: Option<Instant>) {
        if !self.connect_errors.is_empty()
            || !self.readable_peers.is_empty()
            || self.socket_readable
        {
            return;
        }
        let user_timeout = timeout;
        let timeout;
        let mut is_user_timeout;
        loop {
            is_user_timeout = false;
            let mut timeout_instant =
                self.peers.values().filter_map(|conn| conn.timeout()).min();
            if let Some(user) = user_timeout {
                if timeout_instant.map(|to| to > user).unwrap_or(true) {
                    is_user_timeout = true;
                    timeout_instant = Some(user);
                }
            }
            timeout = timeout_instant
                .map(|t| t.saturating_duration_since(Instant::now()));
            if let Some(timeout) = timeout {
                trace!("waiting up to {:?}", timeout);
            } else {
                trace!("waiting forever");
            }
            match self.poll.poll(&mut self.events, timeout) {
                // Allow the caller to handle consequences of the interrupt.
                Err(e) if e.kind() == io::ErrorKind::Interrupted => return,
                r => r.context("mio::Poll::poll").unwrap(),
            }
            break;
        }
        if timeout == Some(Duration::ZERO) || self.events.is_empty() {
            trace!("timeout");
            if !is_user_timeout {
                for conn in self.peers.values_mut() {
                    conn.on_timeout();
                    conn.flush(
                        &self.cb.local_addr,
                        &self.cb.socket,
                        &mut self.packet_buf,
                    )
                    .unwrap();
                }
            }
        }
        if !self.events.is_empty() {
            self.socket_readable = true;
        }
    }
    pub fn wait(&mut self) {
        self.wait_impl(None)
    }
    pub fn wait_timeout(&mut self, timeout: Instant) {
        self.wait_impl(Some(timeout))
    }
    // TODO: remove errors?
    // TODO: actually remove connections once they're gone
    pub fn recv(
        &mut self,
        buf: &mut [u8],
    ) -> Result<Option<(PeerIndex, Event)>> {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        if let Some((idx, error)) = self.connect_errors.pop_front() {
            let mut remaining = &mut buf[..];
            let _ = write!(remaining, "{}", error);
            let remaining_len = remaining.len();
            return Ok(Some((
                idx,
                Event::Disconnect(buf.len() - remaining_len, true),
            )));
        }
        let mut did_nothing = true;
        loop {
            while let Some(&idx) = self.readable_peers.front() {
                // TODO: handle error. by dropping the peer?
                if let Some(ev) =
                    self.peers.get_mut(&idx).unwrap().recv(buf).unwrap()
                {
                    return Ok(Some((idx, ev)));
                }
                self.peers
                    .get_mut(&idx)
                    .unwrap()
                    .flush(&self.cb.local_addr, &self.cb.socket, &mut self.packet_buf)
                    .unwrap();
                assert!(self.readable_peers.pop_front().is_some());
                did_nothing = false;
            }
            if self.socket_readable {
                if let Some(readable_peer) = self.socket_read()? {
                    self.readable_peers.push_back(readable_peer);
                    continue;
                } else {
                    self.socket_readable = false;
                }
                did_nothing = false;
            }
            // If we're called after returning `None` in a previous iteration
            // and without something to do, check whether there's new stuff we
            // can do.
            if did_nothing {
                did_nothing = false;
                self.wait_timeout(Instant::now());
                continue;
            }
            return Ok(None);
        }
    }
    pub fn send_chunk(
        &mut self,
        idx: PeerIndex,
        frame: &[u8],
        unreliable: bool,
    ) -> Result<()> {
        assert!(frame.len() <= MAX_FRAME_SIZE as usize);
        self.peers
            .get_mut(&idx)
            .unwrap()
            .send_chunk(frame, unreliable)
            .unwrap();
        Ok(())
    }
    pub fn flush(&mut self, idx: PeerIndex) -> Result<()> {
        self.peers.get_mut(&idx).unwrap().flush(
            &self.cb.local_addr,
            &self.cb.socket,
            &mut self.packet_buf,
        )
    }
    pub fn connect(&mut self, addr: &str) -> Result<PeerIndex> {
        let idx = self.cb.next_peer_index.get_and_increment();
        let addr: Addr = match addr.parse() {
            Err(error) => {
                self.connect_errors.push_back((idx, error));
                return Ok(idx);
            }
            Ok(addr) => addr,
        };
        let socket_addr = *addr.socket_addr();
        if self.peer_addrs.contains_key(&socket_addr) {
            self.connect_errors.push_back((idx, Error::from_string(format!("already connected to {}", socket_addr))));
            return Ok(idx);
        }
        use self::Addr::*;
        let mut conn: Connection = match addr {
            Quic(addr) => self
                .proto_quic
                .connect(
                    addr,
                    idx,
                    &self.cb.local_addr,
                    self.cb.sslkeylogfile.as_ref(),
                )?
                .into(),
            Tw06(addr) => self
                .proto_tw06
                .connect(
                    addr,
                    idx,
                    &self.cb.local_addr,
                    self.cb.sslkeylogfile.as_ref(),
                )?
                .into(),
        };
        conn.flush(&self.cb.local_addr, &self.cb.socket, &mut self.packet_buf)
            .unwrap();
        assert!(self.peers.insert(idx, conn).is_none());
        assert!(self.peer_addrs.insert(socket_addr, idx).is_none());
        Ok(idx)
    }
    pub fn close(
        &mut self,
        idx: PeerIndex,
        reason: Option<&str>,
    ) -> Result<()> {
        let conn = self.peers.get_mut(&idx).unwrap();
        conn.close(reason);
        conn.flush(&self.cb.local_addr, &self.cb.socket, &mut self.packet_buf)
            .unwrap();
        self.readable_peers.push_back(idx);
        Ok(())
    }
}

enum Connection {
    Quic(quic::Connection),
    Tw06(tw06::Connection),
}

impl Connection {
    pub fn on_recv(
        &mut self,
        buf: &mut [u8],
        from: &SocketAddr,
        local_addr: &SocketAddr,
    ) -> Result<()> {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.on_recv(buf, from, local_addr),
            Tw06(inner) => inner.on_recv(buf, from, local_addr),
        }
    }
    pub fn recv(&mut self, buf: &mut [u8]) -> Result<Option<Event>> {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.recv(buf),
            Tw06(inner) => inner.recv(buf),
        }
    }
    pub fn send_chunk(&mut self, frame: &[u8], unreliable: bool) -> Result<()> {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.send_chunk(frame, unreliable),
            Tw06(inner) => inner.send_chunk(frame, unreliable),
        }
    }
    pub fn close(&mut self, reason: Option<&str>) {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.close(reason),
            Tw06(inner) => inner.close(reason),
        }
    }
    pub fn timeout(&self) -> Option<Instant> {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.timeout(),
            Tw06(inner) => inner.timeout(),
        }
    }
    pub fn on_timeout(&mut self) {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.on_timeout(),
            Tw06(inner) => inner.on_timeout(),
        }
    }
    pub fn flush(
        &mut self,
        local: &SocketAddr,
        socket: &UdpSocket,
        buf: &mut [u8],
    ) -> Result<()> {
        use self::Connection::*;
        match self {
            Quic(inner) => inner.flush(local, socket, buf),
            Tw06(inner) => inner.flush(local, socket, buf),
        }
    }
}

impl From<quic::Connection> for Connection {
    fn from(conn: quic::Connection) -> Connection {
        Connection::Quic(conn)
    }
}

impl From<tw06::Connection> for Connection {
    fn from(conn: tw06::Connection) -> Connection {
        Connection::Tw06(conn)
    }
}
