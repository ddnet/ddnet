use arrayvec::ArrayString;
use arrayvec::ArrayVec;
use crate::Context as _;
use crate::Error;
use crate::Identity;
use crate::Result;
use crate::NoBlock as _;
use crate::NotDone as _;
use crate::PrivateIdentity;
use crate::normalize;
use crate::peek_quic_varint;
use crate::secure_hash;
use crate::secure_random;
use crate::write_quic_varint;
use mio::Events;
use mio::Poll;
use mio::net::UdpSocket;
use std::cmp;
use std::collections::HashMap;
use std::collections::VecDeque;
use std::collections::hash_map;
use std::env;
use std::fmt::Write as _;
use std::fmt;
use std::fs::File;
use std::fs;
use std::io::Write as _;
use std::io;
use std::mem;
use std::net::SocketAddr;
use std::net::Ipv6Addr;
use std::ops;
use std::path::Path;
use std::str;
use std::sync::Arc;
use std::sync::Mutex;
use std::time::Duration;
use std::time::Instant;
use url::Url;

// TODO: coalesce ACKs with other packets
// TODO: coalesce dgrams with stream packets

// Originally `NET_MAX_PAYLOAD`.
pub const MAX_FRAME_SIZE: u64 = 1394;
pub const QUIC_CLOSE_CODE: u64 = 0xdd40a0;
pub const TOKEN_LEN: usize = 4;
pub const RETRY_TOKEN_LEN: usize = 20 + 4;

pub struct Net {
    config: quiche::Config,
    accept_incoming_connections: bool,
    sslkeylogfile: Option<ArcFile>,
    challenger: Challenger,
    local_addr: SocketAddr,
    socket: UdpSocket,
    events: Events,
    poll: Poll,
    packet_buf: [u8; 65536],
    callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,

    next_peer_index: PeerIndex,
    peers: HashMap<PeerIndex, Connection>,
    connection_ids: HashMap<ConnectionId, PeerIndex>,
    connect_errors: VecDeque<(PeerIndex, Error)>,

    socket_readable: bool,
    readable_peers: VecDeque<PeerIndex>,
}

pub struct NetBuilder {
    bindaddr: Option<SocketAddr>,
    identity: Option<PrivateIdentity>,
    accept_incoming_connections: bool,
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
    fn get_and_increment(&mut self) -> PeerIndex {
        let result = *self;
        self.0 += 1;
        result
    }
}

#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
struct ConnectionId([u8; ConnectionId::LEN]);

impl fmt::Display for ConnectionId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

impl fmt::Debug for ConnectionId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:02x}{:02x}{:02x}{:02x}{:02x}", self.0[0], self.0[1], self.0[2], self.0[3], self.0[4])
    }
}

impl ConnectionId {
    // TODO: think about the proper length for a connection ID
    const LEN: usize = 5;
    fn random() -> ConnectionId {
        let mut result = ConnectionId([0; ConnectionId::LEN]);
        loop {
            // TODO: seeded userspace prng?
            result.0 = secure_random();
            // In order to distinguish DDNet 0.6 extended connless packets from
            // QUIC packets with short header, make sure that connection IDs do
            // not start with ASCII 'e'. DDNet 0.6 extended connless packets
            // start with ASCII 'xe', and QUIC packets with short header start
            // the destination connection ID in the second byte, with no header
            // encryption.
            if result.0[0] != b'e' {
                break;
            }
        }
        result
    }
    fn from_raw(cid: &quiche::ConnectionId) -> Option<ConnectionId> {
        Some(ConnectionId(cid[..].try_into().ok()?))
    }
    fn as_raw(&self) -> quiche::ConnectionId {
        quiche::ConnectionId::from_ref(&self.0)
    }
}

struct Challenger {
    seed: [u8; 16],
    prev_seed: [u8; 16],
}

impl Challenger {
    pub fn new() -> Challenger {
        Challenger {
            seed: secure_random(),
            prev_seed: secure_random(),
        }
    }
    pub fn reseed(&mut self) {
        self.prev_seed = mem::replace(&mut self.seed, secure_random());
    }
    fn token(secret: &[u8; 16], addr: &SocketAddr) -> [u8; TOKEN_LEN] {
        let mut buf: ArrayVec<[u8; 128]> = ArrayVec::new();
        buf.try_extend_from_slice(secret).unwrap();
        write!(buf, "{}", addr).unwrap();

        let mut result = [0; TOKEN_LEN];
        result.copy_from_slice(&secure_hash(&buf)[..TOKEN_LEN]);
        result
    }
    pub fn compute_retry_token(&self, addr: &SocketAddr, odcid: &[u8])
        -> ArrayVec<[u8; RETRY_TOKEN_LEN]>
    {
        if odcid.len() > 20 {
            panic!("connection IDs in QUICv1 cannot be longer than 20 bytes");
        }
        let mut result = ArrayVec::new();
        result.try_extend_from_slice(&Challenger::token(&self.seed, addr)).unwrap();
        result.try_extend_from_slice(odcid).unwrap();
        result
    }
    pub fn verify_retry_token<'a>(&self, addr: &SocketAddr, retry_token: &'a [u8])
        -> Option<&'a [u8]>
    {
        if retry_token.len() < TOKEN_LEN {
            return None;
        }
        let (token, odcid) = retry_token.split_at(TOKEN_LEN);
        if token != Challenger::token(&self.seed, addr) &&
            token != Challenger::token(&self.prev_seed, addr)
        {
            return None;
        }
        Some(odcid)
    }
}

#[derive(Clone, Copy)]
enum PeerIdentity {
    AcceptAny,
    Wanted(Identity),
    Known(Identity),
    Invalid(Identity),
}

impl PeerIdentity {
    fn assert_known(&self) -> &Identity {
        use self::PeerIdentity::*;
        match self {
            Known(id) => id,
            _ => panic!("peer identity should be known"),
        }
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

#[derive(Clone)]
struct ArcFile(Arc<File>);

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
    fn write_vectored(&mut self, bufs: &[io::IoSlice<'_>]) -> io::Result<usize> {
        (&*self.0).write_vectored(bufs)
    }
    // TODO(rust-lang/rust#69941): implement `is_write_vectored`
}

fn config(
    key: &boring::pkey::PKeyRef<boring::pkey::Private>,
    cert: &boring::x509::X509Ref,
    callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,
) -> Result<quiche::Config> {
    let mut context = boring::ssl::SslContext::builder(boring::ssl::SslMethod::tls()).context("boring::SslContext::builder")?;
    context.set_sigalgs_list("ed25519").context("boring::SslContext::set_sigalgs_list")?;
    context.set_private_key(key).context("boring::SslContext::set_private_key")?;
    context.set_certificate(cert).context("boring::SslContext::set_certificate")?;
    context.set_verify_callback(boring::ssl::SslVerifyMode::PEER, move |pre, store| {
        fn verify(
            store: &mut boring::x509::X509StoreContextRef,
            peer_identity: &mut PeerIdentity,
        ) -> Option<()> {
            use self::PeerIdentity::*;
            let public = store.chain()?.get(0)?.public_key().ok()?;
            let public = Identity::try_from_lib(&public)?;
            match *peer_identity {
                AcceptAny => {
                    *peer_identity = Known(public);
                    Some(())
                },
                Wanted(identity) | Known(identity) => {
                    // TODO: verify that this verification method works with
                    // longer certificate chains
                    // TODO: constant time?
                    if public != identity {
                        *peer_identity = Invalid(identity);
                        return None;
                    }
                    *peer_identity = Known(identity);
                    Some(())
                },
                Invalid(_) => None,
            }
        }
        // ignore boringssl's certificate verification
        let _ = pre;
        verify(store, callback_peer_identity.lock().unwrap().as_mut().unwrap()).is_some()
    });
    let mut config = quiche::Config::with_boring_ssl_ctx(quiche::PROTOCOL_VERSION, context.build()).context("quiche::Config::new")?;
    config.log_keys();
    config.set_application_protos(&[b"ddnet-15"]).context("quiche::Config::set_application_protos")?;
    /*
    if client {
        //config.verify_peer(true);
        // TODO
        //config.load_verify_locations_from_directory(&cert_dir).context("quiche::Config::load_verify_locations_from_directory")?;
        //config.load_verify_locations_from_file("ed25519.crt").context("quiche::Config::load_verify_locations_from_file")?;
        //config.load_priv_key_from_pem_file("client.key").context("client.key not found or could not be loaded")?;
        //config.load_cert_chain_from_pem_file("client.crt").context("client.crt not found or could not be loaded")?;
    } else {
        //config.load_verify_locations_from_file("ed25519.crt").context("quiche::Config::load_verify_locations_from_file")?;
        //config.load_priv_key_from_pem_file("server.key").context("server.key not found or could not be loaded")?;
        //config.load_cert_chain_from_pem_file("server.crt").context("server.crt not found or could not be loaded")?;
    }
    */
    // TODO: decide on a proper number. the current one ensures datagrams of size 1394
    config.set_max_send_udp_payload_size(1423);
    config.enable_dgram(true, 32, 32);
    Ok(config)
}

trait ConfigExt {
    fn client(&mut self) -> &mut Self;
    fn server(&mut self) -> &mut Self;
}

impl ConfigExt for quiche::Config {
    fn client(&mut self) -> &mut quiche::Config {
        self.set_initial_max_data(1024 * 1024);
        self.set_initial_max_stream_data_bidi_local(1024 * 1024);
        self.set_initial_max_stream_data_bidi_remote(0);
        self.set_initial_max_stream_data_uni(0);
        self.set_initial_max_streams_bidi(0);
        self.set_initial_max_streams_uni(0);
        self
    }
    fn server(&mut self) -> &mut quiche::Config {
        self.set_initial_max_data(1024 * 1024);
        self.set_initial_max_stream_data_bidi_local(0);
        self.set_initial_max_stream_data_bidi_remote(1024 * 1024);
        self.set_initial_max_stream_data_uni(0);
        self.set_initial_max_streams_bidi(1);
        self.set_initial_max_streams_uni(0);
        self
    }
}

impl NetBuilder {
    pub fn bindaddr(&mut self, bindaddr: SocketAddr) {
        self.bindaddr = Some(bindaddr);
    }
    pub fn identity(&mut self, identity: PrivateIdentity) {
        self.identity = Some(identity);
    }
    pub fn accept_incoming_connections(&mut self, accept: bool) {
        self.accept_incoming_connections = accept;
    }
    pub fn open(self) -> Result<Net> {
        let sslkeylogfile = if let Some(sslkeylogfile) = env::var_os("SSLKEYLOGFILE") {
            fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(&sslkeylogfile)
                .map(|file| ArcFile(Arc::new(file)))
                .map_err(|err| error!("error opening SSLKEYLOGFILE {}: {}", Path::new(&sslkeylogfile).display(), err))
                .ok()
        } else {
            None
        };

        let bindaddr = self.bindaddr.unwrap_or(SocketAddr::new(Ipv6Addr::new(0, 0, 0, 0, 0, 0, 0, 0).into(), 0));
        let identity = self.identity.unwrap_or_else(PrivateIdentity::random);

        let mut socket = UdpSocket::bind(bindaddr).context("bind")?;
        let local_addr = socket.local_addr().context("local_addr")?;

        let poll = Poll::new().context("mio::Poll::new")?;
        let events = Events::with_capacity(1);
        poll.registry().register(&mut socket, mio::Token(0), mio::Interest::READABLE).context("mio::Poll::register")?;
        let cert = identity.generate_certificate();
        let callback_peer_identity = Arc::new(Mutex::new(None));

        info!("identity {}", identity.public());
        info!("listening on {}", bindaddr);

        Ok(Net {
            config: config(identity.as_lib(), &cert, callback_peer_identity.clone()).context("config")?,
            accept_incoming_connections: self.accept_incoming_connections,
            sslkeylogfile,
            challenger: Challenger::new(),
            local_addr,
            socket,
            events,
            poll,
            // TODO: uninitialized
            packet_buf: [0; 65536],
            callback_peer_identity,

            next_peer_index: PeerIndex(0),
            peers: HashMap::new(),
            connection_ids: HashMap::new(),
            connect_errors: VecDeque::with_capacity(1),

            socket_readable: false,
            readable_peers: VecDeque::with_capacity(4),
        })
    }
}

// TODO: allow selecting whether we want to permit incoming connections or not
impl Net {
    pub fn builder() -> NetBuilder {
        NetBuilder {
            bindaddr: None,
            identity: None,
            accept_incoming_connections: false,
        }
    }
    fn new_conn_id(&self) -> ConnectionId {
        loop {
            let result = ConnectionId::random();
            if !self.connection_ids.contains_key(&result) {
                return result;
            }
        }
    }
    fn socket_read(&mut self) -> Result<Option<PeerIndex>> {
        loop {
            let Some((read, from)) = self.socket.recv_from(&mut self.packet_buf).no_block().context("UdpSocket::recv_from")? else { break; };
            let from = normalize(from);
            if let Ok(header) = quiche::Header::from_slice(&mut self.packet_buf[..read], ConnectionId::LEN) {
                trace!("received quic packet from {}: {:?}", from, header);
                let cid = ConnectionId::from_raw(&header.dcid);
                let idx = match cid.map(|cid| self.connection_ids.entry(cid)) {
                    Some(hash_map::Entry::Occupied(o)) => *o.get(),
                    // TODO: test version negotiation
                    // token is always present in Initial packets.
                    _ if self.accept_incoming_connections && header.ty == quiche::Type::Initial && !quiche::version_is_supported(header.version) => {
                        let written = quiche::negotiate_version(&header.scid, &header.dcid, &mut self.packet_buf).unwrap();
                        self.socket.send_to(&self.packet_buf[..written], from).context("UdpSocket::send_to")?;
                        continue;
                    }
                    // token is always present in Initial packets.
                    _ if self.accept_incoming_connections && header.ty == quiche::Type::Initial && header.token.as_ref().unwrap().is_empty() => {
                        let new_scid = self.new_conn_id();
                        let token = self.challenger.compute_retry_token(&from, &header.dcid);
                        let written = quiche::retry(
                            &header.scid,
                            &header.dcid,
                            &new_scid.as_raw(),
                            &token,
                            header.version,
                            &mut self.packet_buf,
                        ).context("quiche::retry")?; // TODO: unwrap instead?
                        trace!("sending retry to {}", from);
                        self.socket.send_to(&self.packet_buf[..written], from).context("UdpSocket::send_to")?;
                        continue;
                    }
                    Some(hash_map::Entry::Vacant(v)) if self.accept_incoming_connections && header.ty == quiche::Type::Initial => {
                        // token is always present in Initial packets.
                        let token = header.token.unwrap();
                        let odcid = match self.challenger.verify_retry_token(&from, &token) {
                            Some(odcid) => odcid,
                            None => {
                                debug!("rejecting invalid token from {}", from);
                                continue;
                            }
                        };
                        debug!("accepting connection from {}", from);
                        let mut conn = quiche::accept(&header.dcid, Some(&quiche::ConnectionId::from_ref(&odcid)), self.local_addr, from, self.config.server()).context("quiche::accept")?; // TODO: unwrap instead?
                        if let Some(sslkeylogfile) = &self.sslkeylogfile {
                            conn.set_keylog(Box::new(sslkeylogfile.clone()));
                        }
                        let conn = Connection::new(conn, false, PeerIdentity::AcceptAny);
                        let idx = self.next_peer_index.get_and_increment();
                        v.insert(idx);
                        assert!(self.peers.insert(idx, conn).is_none());
                        idx
                    }
                    _ => continue,
                };
                let conn = self.peers.get_mut(&idx).unwrap();
                {
                    let mut cpi = self.callback_peer_identity.lock().unwrap();
                    assert!(cpi.is_none());
                    *cpi = Some(conn.peer_identity);
                }
                let _ = conn.on_recv(&mut self.packet_buf[..read], quiche::RecvInfo { from, to: self.local_addr }); // TODO: figure out why error packets aren't getting sent
                conn.flush(&self.local_addr, &self.socket, &mut self.packet_buf).unwrap();
                conn.peer_identity = self.callback_peer_identity.lock().unwrap().take().unwrap();
                return Ok(Some(idx));
            } else {
                trace!("received non-quic packet from {}", from);
            }
        }
        Ok(None)
    }
    fn wait_impl(&mut self, timeout: Option<Instant>) {
        if !self.readable_peers.is_empty() || self.socket_readable {
            return;
        }
        let user_timeout = timeout;
        let timeout;
        let mut is_user_timeout;
        loop {
            is_user_timeout = false;
            let mut timeout_instant = self.peers.values().filter_map(|conn| conn.timeout()).min();
            if let Some(user) = user_timeout {
                if timeout_instant.map(|to| to > user).unwrap_or(true) {
                    is_user_timeout = true;
                    timeout_instant = Some(user);
                }
            }
            timeout = timeout_instant.map(|t| t.saturating_duration_since(Instant::now()));
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
                    conn.flush(&self.local_addr, &self.socket, &mut self.packet_buf).unwrap();
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
    pub fn recv(&mut self, buf: &mut [u8])
        -> Result<Option<(PeerIndex, Event)>>
    {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        if let Some((idx, error)) = self.connect_errors.pop_front() {
            let mut remaining = &mut buf[..];
            let _ = write!(remaining, "{}", error);
            let remaining_len = remaining.len();
            return Ok(Some((idx, Event::Disconnect(buf.len() - remaining_len, true))));
        }
        let mut did_nothing = true;
        loop {
            while let Some(&idx) = self.readable_peers.front() {
                // TODO: handle error. by dropping the peer?
                if let Some(ev) = self.peers.get_mut(&idx).unwrap().recv(buf).unwrap() {
                    return Ok(Some((idx, ev)));
                }
                self.peers.get_mut(&idx).unwrap().flush(&self.local_addr, &self.socket, &mut self.packet_buf).unwrap();
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
    pub fn send_chunk(&mut self, idx: PeerIndex, frame: &[u8], unreliable: bool) -> Result<()> {
        self.peers.get_mut(&idx).unwrap().send_chunk(frame, unreliable).unwrap();
        Ok(())
    }
    pub fn flush(&mut self, idx: PeerIndex) -> Result<()> {
        self.peers.get_mut(&idx).unwrap().flush(&self.local_addr, &self.socket, &mut self.packet_buf)
    }
    fn parse_connect_addr(addr: &str) -> Result<(SocketAddr, Identity)> {
        let addr = Url::parse(addr).context("connect: URL")?;
        // TODO: remove version because it can be negotiated via ALPN?
        if addr.scheme() != "ddnet-15+quic" {
            bail!("unsupported scheme {}", addr.scheme());
        }
        let mut ip_port: ArrayString<[u8; 64]> = ArrayString::new();
        write!(
            &mut ip_port,
            "{}:{}",
            addr.host_str().ok_or_else(|| Error::from_string("connect: URL missing host".to_owned()))?,
            addr.port().ok_or_else(|| Error::from_string("connect: URL missing port".to_owned()))?,
        )
        .unwrap();
        let sock_addr: SocketAddr = ip_port.parse().context("connect: IP addr")?;
        let fragment = addr.fragment().unwrap_or("");
        // Take at most 64 characters.
        let end = fragment.char_indices().nth(64).map(|(idx, _)| idx).unwrap_or(fragment.len());
        let identity: Identity = fragment[..end].parse().context("connect: identity")?;
        Ok((sock_addr, identity))
    }
    pub fn connect(&mut self, addr: &str) -> Result<PeerIndex> {
        let idx = self.next_peer_index.get_and_increment();
        let (sock_addr, peer_identity) = match Net::parse_connect_addr(addr) {
            Err(error) => {
                self.connect_errors.push_back((idx, error));
                return Ok(idx);
            },
            Ok(parsed) => parsed,
        };
        let cid = self.new_conn_id();
        let mut conn = quiche::connect(
            None,
            &cid.as_raw(),
            self.local_addr,
            sock_addr,
            self.config.client(),
        ).context("quiche::connect")?;
        if let Some(sslkeylogfile) = &self.sslkeylogfile {
            conn.set_keylog(Box::new(sslkeylogfile.clone()));
        }
        let mut conn = Connection::new(conn, true, PeerIdentity::Wanted(peer_identity));
        conn.flush(&self.local_addr, &self.socket, &mut self.packet_buf).unwrap();
        assert!(self.peers.insert(idx, conn).is_none());
        assert!(self.connection_ids.insert(cid, idx).is_none());
        Ok(idx)
    }
    pub fn close(&mut self, idx: PeerIndex, reason: Option<&str>) -> Result<()> {
        let conn = self.peers.get_mut(&idx).unwrap();
        conn.close(reason);
        conn.flush(&self.local_addr, &self.socket, &mut self.packet_buf).unwrap();
        self.readable_peers.push_back(idx);
        Ok(())
    }
}

#[derive(PartialEq)]
enum State {
    Connecting,
    Online,
    Disconnected,
}

struct Connection {
    inner: quiche::Connection,
    client: bool,
    peer_identity: PeerIdentity,
    state: State,
    buffer: [u8; 2048],
    buffer_range: ops::Range<usize>,
}

impl Connection {
    fn new(inner: quiche::Connection, client: bool, peer_identity: PeerIdentity)
        -> Connection
    {
        Connection {
            inner,
            client,
            peer_identity,
            state: State::Connecting,
            buffer: [0; 2048],
            buffer_range: 0..0,
        }
    }
    pub fn on_recv(&mut self, buf: &mut [u8], recv_info: quiche::RecvInfo)
        -> Result<()>
    {
        self.inner.recv(buf, recv_info).context("quiche::Conn::recv")?;
        Ok(())
    }
    fn check_connection_params(&self) -> Result<()> {
        if let Some(max_dgram_len) = self.inner.dgram_max_writable_len() {
            // TODO: think this through, wrt. protocol compatibility
            if max_dgram_len < MAX_FRAME_SIZE as usize {
                bail!("other peer advertised support for datagrams of at most {} bytes, need {} bytes", max_dgram_len, MAX_FRAME_SIZE);
            }
        } else {
            bail!("other peer hasn't advertised support for datagrams");
        }
        Ok(())
    }
    fn peek_chunk_range(&self) -> Result<Option<ops::Range<usize>>> {
        Ok(if let Some((len, len_encoded_len)) = peek_quic_varint(&self.buffer[self.buffer_range.clone()]) {
            if len > MAX_FRAME_SIZE {
                bail!("frames must be shorter than {} bytes, got length field with {} bytes", MAX_FRAME_SIZE, len);
            }
            let len: usize = len.try_into().unwrap();
            let start = self.buffer_range.start + len_encoded_len;
            Some(start..start + len)
        } else {
            None
        })
    }
    fn read_chunk_from_buffer(&mut self, buf: &mut [u8]) -> Result<Option<usize>> {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        let chunk_range = match self.peek_chunk_range()? {
            Some(r) => r,
            None => return Ok(None),
        };
        if self.buffer_range.end < chunk_range.end {
            return Ok(None);
        }
        buf[..chunk_range.len()].copy_from_slice(&self.buffer[chunk_range.clone()]);
        self.buffer_range.start = chunk_range.end;
        Ok(Some(chunk_range.len()))
    }
    pub fn recv(&mut self, buf: &mut [u8]) -> Result<Option<Event>> {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        use self::State::*;
        match self.state {
            Connecting => {
                if self.client {
                    // Check if the QUIC handshake is complete.
                    // TODO: send an identifier?
                    if self.inner.is_established() {
                        self.state = Online;
                        // TODO: this doesn't actually open the stream
                        self.inner.stream_send(0, b"", false).context("quiche::Conn::stream_send")?;
                    }
                } else {
                    // Check if the stream is already open.
                    if self.inner.stream_capacity(0).is_ok() {
                        self.state = Online;
                    }
                }
                if self.state == Online {
                    self.check_connection_params()?;
                    return Ok(Some(Event::Connect(*self.peer_identity.assert_known())));
                }
            },
            Online => {},
            Disconnected => return Ok(None),
        }

        // Check if the QUIC connection was closed.
        if self.inner.is_draining() || self.inner.local_error().is_some() {
            self.state = Disconnected;
            let (len, remote) = self.extract_error(buf);
            return Ok(Some(Event::Disconnect(len, remote)));
        }

        // If we're not online, don't try to receive chunks.
        if self.state != Online {
            return Ok(None);
        }

        // Check if we have any datagrams lying around.
        if let Some(dgram) = self.inner.dgram_recv_vec().not_done().context("quiche::Conn::dgram_recv_vec")? {
            if dgram.first() != Some(&0) {
                bail!("invalid datagram received, first byte should be 0, is {:?}", dgram.first());
            }
            let len = dgram.len() - 1;
            // Buffer was checked to be long enough above.
            buf[..len].copy_from_slice(&dgram[1..]);
            return Ok(Some(Event::Chunk(len, true)));
        }

        // Check if we still have a chunk remaining.
        if let Some(c) = self.read_chunk_from_buffer(buf)? {
            return Ok(Some(Event::Chunk(c, false)));
        }

        // Check if there is more data (i.e. stream is actually open,
        // outstanding data exists).
        if !self.inner.stream_readable(0) {
            return Ok(None);
        }

        // Move the remaining bytes to the front.
        self.buffer.copy_within(self.buffer_range.clone(), 0);
        self.buffer_range = 0..self.buffer_range.len();

        // Read some more data from the stream.
        let (read, fin) = self.inner.stream_recv(0, &mut self.buffer[self.buffer_range.end..]).context("quiche::Conn::stream_recv")?;
        self.buffer_range.end += read;

        // Check for a chunk again.
        if let Some(c) = self.read_chunk_from_buffer(buf)? {
            return Ok(Some(Event::Chunk(c, false)));
        }

        // If the stream is finished and we still have data, return an error.
        if fin && !self.buffer_range.is_empty() {
            bail!("stream data remaining that does not have a full chunk");
        }

        Ok(None)
    }
    pub fn send_chunk(&mut self, frame: &[u8], unreliable: bool) -> Result<()> {
        assert!(frame.len() <= MAX_FRAME_SIZE as usize);
        if !unreliable {
            // TODO: state checks?
            let mut len_buf = [0; 8];
            let len_encoded_len = write_quic_varint(&mut len_buf, frame.len() as u64);
            let capacity = self.inner.stream_capacity(0).ok();
            let total_len = len_encoded_len + frame.len();
            if capacity.map(|c| total_len > c).unwrap_or(true) {
                bail!("cannot send data, capacity={:?} len={}", capacity, total_len);
            }
            self.inner.stream_send(0, &len_buf[..len_encoded_len], false).unwrap();
            self.inner.stream_send(0, frame, false).unwrap();
            Ok(())
        } else {
            let mut buf = Vec::with_capacity(1 + frame.len());
            buf.push(0);
            buf.extend_from_slice(frame);
            // `Error::Done` means that the datagram was immediately dropped
            // without being sent.
            self.inner.dgram_send_vec(buf).not_done().context("quiche::Conn::dgram_send_vec")?.unwrap();
            Ok(())
        }
    }
    fn extract_error(&self, buf: &mut [u8]) -> (usize, bool) {
        let (remote, err) = if let Some(err) = self.inner.peer_error() {
            (true, err)
        } else if let Some(err) = self.inner.local_error() {
            (false, err)
        } else {
            unreachable!();
        };
        // TODO: sanitize error
        let reason = str::from_utf8(&err.reason).ok().unwrap_or("(invalid utf-8)");
        let len;
        if err.error_code == QUIC_CLOSE_CODE {
            len = cmp::min(buf.len(), reason.len());
            buf[..len].copy_from_slice(&reason.as_bytes()[..len]);
        } else {
            let mut remaining = &mut buf[..];
            let kind = if err.is_app { "app, " } else { "" };
            if reason.is_empty() {
                let _ = write!(remaining, "QUIC error ({}0x{:x})", kind, err.error_code);
            } else {
                let _ = write!(remaining, "QUIC error ({}0x{:x}): {}", kind, err.error_code, reason);
            }
            let remaining_len = remaining.len();
            len = buf.len() - remaining_len;
        }
        (len, remote)
    }
    pub fn close(&mut self, reason: Option<&str>) {
        self.inner.close(true, QUIC_CLOSE_CODE, reason.unwrap_or("").as_bytes()).not_done().unwrap();
    }
    pub fn timeout(&self) -> Option<Instant> {
        // TODO: Use `Connection::timeout_instant` once quiche > 0.16.0 is
        // released.
        self.inner.timeout().map(|t| Instant::now() + t)
    }
    pub fn on_timeout(&mut self) {
        self.inner.on_timeout();
    }
    pub fn flush(&mut self, local: &SocketAddr, socket: &UdpSocket, buf: &mut [u8])
        -> Result<()>
    {
        let mut num_bytes = 0;
        let mut num_packets = 0;
        loop {
            let Some((written, info)) = self.inner.send(buf).not_done().context("quiche::Conn::send")? else { break; };
            let now = Instant::now();
            let delay = info.at.saturating_duration_since(now);
            if !delay.is_zero() {
                warn!("should have delayed packet by {:?}, but haven't", delay);
            }
            assert!(*local == info.from);
            socket.send_to(&buf[..written], info.to).context("UdpSocket::send_to")?;
            num_packets += 1;
            num_bytes += written;
        }
        if num_packets != 0 {
            trace!("sent {} packet(s) with {} byte(s)", num_packets, num_bytes);
        }
        Ok(())
    }
}
