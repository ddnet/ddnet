use crate::peek_quic_varint;
use crate::secure_random;
use crate::write_quic_varint;
use crate::CallbackData;
use crate::Challenger;
use crate::ConnectionEvent;
use crate::Context as _;
use crate::Event;
use crate::Identity;
use crate::PeerIndex;
use crate::PrivateIdentity;
use crate::QuicAddr as Addr;
use crate::Result;
use crate::Something;
use crate::MAX_FRAME_SIZE;
use arrayvec::ArrayVec;
use std::cmp;
use std::collections::hash_map;
use std::collections::HashMap;
use std::fmt;
use std::io::Write as _;
use std::net::SocketAddr;
use std::ops;
use std::result::Result as StdResult;
use std::str;
use std::sync::Arc;
use std::sync::Mutex;
use std::time::Instant;

// TODO: coalesce ACKs with other packets
// TODO: coalesce dgrams with stream packets
// TODO: implement timeout after which packets are sent even without an explicit flush

pub const QUIC_CLOSE_CODE: u64 = 0xdd40a0;
pub const RETRY_TOKEN_LEN: usize = 20 + 4;

pub struct Protocol {
    config: quiche::Config,
    callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,
    connection_ids: HashMap<ConnectionId, PeerIndex>,
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
        write!(
            f,
            "{:02x}{:02x}{:02x}{:02x}{:02x}",
            self.0[0], self.0[1], self.0[2], self.0[3], self.0[4]
        )
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

trait NotDone {
    type T;
    fn not_done(self) -> quiche::Result<Option<Self::T>>;
}

impl<T> NotDone for quiche::Result<T> {
    type T = T;
    fn not_done(self) -> quiche::Result<Option<T>> {
        match self {
            Err(quiche::Error::Done) => Ok(None),
            r => r.map(Some),
        }
    }
}

trait ChallengerExt {
    fn compute_retry_token(
        &self,
        addr: &SocketAddr,
        odcid: &[u8],
    ) -> ArrayVec<[u8; RETRY_TOKEN_LEN]>;
    fn verify_retry_token<'a>(
        &self,
        addr: &SocketAddr,
        retry_token: &'a [u8],
    ) -> StdResult<&'a [u8], ()>;
}

impl ChallengerExt for Challenger {
    fn compute_retry_token(
        &self,
        addr: &SocketAddr,
        odcid: &[u8],
    ) -> ArrayVec<[u8; RETRY_TOKEN_LEN]> {
        if odcid.len() > 20 {
            panic!("connection IDs in QUICv1 cannot be longer than 20 bytes");
        }
        let mut result = ArrayVec::new();
        result
            .try_extend_from_slice(&self.compute_token(addr))
            .unwrap();
        result.try_extend_from_slice(odcid).unwrap();
        result
    }
    fn verify_retry_token<'a>(
        &self,
        addr: &SocketAddr,
        retry_token: &'a [u8],
    ) -> StdResult<&'a [u8], ()> {
        if retry_token.len() < Challenger::TOKEN_LEN {
            return Err(());
        }
        let (token, odcid) = retry_token.split_at(Challenger::TOKEN_LEN);
        let token = token.try_into().unwrap();
        self.verify_token(addr, token).map(|()| odcid)
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

fn config(
    key: &boring::pkey::PKeyRef<boring::pkey::Private>,
    cert: &boring::x509::X509Ref,
    callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,
) -> Result<quiche::Config> {
    let mut context =
        boring::ssl::SslContext::builder(boring::ssl::SslMethod::tls())
            .context("boring::SslContext::builder")?;
    context
        .set_sigalgs_list("ed25519")
        .context("boring::SslContext::set_sigalgs_list")?;
    context
        .set_private_key(key)
        .context("boring::SslContext::set_private_key")?;
    context
        .set_certificate(cert)
        .context("boring::SslContext::set_certificate")?;
    context.set_verify_callback(
        boring::ssl::SslVerifyMode::PEER,
        move |pre, store| {
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
                    }
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
                    }
                    Invalid(_) => None,
                }
            }
            // ignore boringssl's certificate verification
            let _ = pre;
            verify(
                store,
                callback_peer_identity.lock().unwrap().as_mut().unwrap(),
            )
            .is_some()
        },
    );
    let mut config = quiche::Config::with_boring_ssl_ctx(
        quiche::PROTOCOL_VERSION,
        context.build(),
    )
    .context("quiche::Config::new")?;
    config.log_keys();
    config
        .set_application_protos(&[b"ddnet-15"])
        .context("quiche::Config::set_application_protos")?;
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

impl Protocol {
    pub fn new(identity: &PrivateIdentity) -> Result<Protocol> {
        let cert = identity.generate_certificate();
        let callback_peer_identity = Arc::new(Mutex::new(None));

        Ok(Protocol {
            config: config(
                identity.as_lib(),
                &cert,
                callback_peer_identity.clone(),
            )
            .context("config")?,
            callback_peer_identity,
            connection_ids: HashMap::new(),
        })
    }
    pub fn remove_peer(&mut self, idx: PeerIndex, conn: Connection) {
        let _ = conn;
        // TODO: efficiency
        self.connection_ids.retain(|_, &mut i| i != idx);
    }
    fn new_conn_id(&self) -> ConnectionId {
        loop {
            let result = ConnectionId::random();
            if !self.connection_ids.contains_key(&result) {
                return result;
            }
        }
    }
    pub fn on_recv(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        packet_len: usize,
        from: &SocketAddr,
    ) -> Result<Option<Something<Connection>>> {
        if let Ok(header) = quiche::Header::from_slice(
            &mut packet_buf[..packet_len],
            ConnectionId::LEN,
        ) {
            trace!("received quic packet from {}: {:?}", from, header);
            let cid = ConnectionId::from_raw(&header.dcid);
            match (
                cid.map(|cid| self.connection_ids.entry(cid)),
                cb.accept_connections,
            ) {
                (Some(hash_map::Entry::Occupied(o)), _) => {
                    return Ok(Some(Something::ExistingConnection(*o.get())))
                }
                // TODO: test version negotiation
                // token is always present in Initial packets.
                (_, true)
                    if header.ty == quiche::Type::Initial
                        && !quiche::version_is_supported(header.version) =>
                {
                    let written = quiche::negotiate_version(
                        &header.scid,
                        &header.dcid,
                        packet_buf,
                    )
                    .unwrap();
                    cb.socket
                        .send_to(&packet_buf[..written], *from)
                        .context("UdpSocket::send_to")?;
                }
                // token is always present in Initial packets.
                (_, true)
                    if header.ty == quiche::Type::Initial
                        && header.token.as_ref().unwrap().is_empty() =>
                {
                    let new_scid = self.new_conn_id();
                    let token =
                        cb.challenger.compute_retry_token(from, &header.dcid);
                    let written = quiche::retry(
                        &header.scid,
                        &header.dcid,
                        &new_scid.as_raw(),
                        &token,
                        header.version,
                        packet_buf,
                    )
                    .context("quiche::retry")?; // TODO: unwrap instead?
                    trace!("sending retry to {}", from);
                    cb.socket
                        .send_to(&packet_buf[..written], *from)
                        .context("UdpSocket::send_to")?;
                }
                (Some(hash_map::Entry::Vacant(v)), true)
                    if header.ty == quiche::Type::Initial =>
                {
                    // token is always present in Initial packets.
                    let token = header.token.unwrap();
                    let odcid =
                        match cb.challenger.verify_retry_token(from, &token) {
                            Ok(odcid) => odcid,
                            Err(()) => {
                                debug!("rejecting invalid token from {}", from);
                                return Ok(None);
                            }
                        };
                    debug!("accepting connection from {}", from);
                    let mut conn = quiche::accept(
                        &header.dcid,
                        Some(&quiche::ConnectionId::from_ref(&odcid)),
                        cb.local_addr,
                        *from,
                        self.config.server(),
                    )
                    .context("quiche::accept")?; // TODO: unwrap instead?
                    if let Some(sslkeylogfile) = &cb.sslkeylogfile {
                        conn.set_keylog(Box::new(sslkeylogfile.clone()));
                    }
                    let cpi = self.callback_peer_identity.clone();
                    let conn = Connection::new(
                        conn,
                        cpi,
                        false,
                        PeerIdentity::AcceptAny,
                    );
                    let idx = cb.next_peer_index;
                    v.insert(idx);
                    return Ok(Some(Something::NewConnection(idx, conn)));
                }
                _ => {}
            };
        } else {
            trace!("received non-quic packet from {}", from);
        }
        Ok(None)
    }
    pub fn connect(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        addr: Addr,
        idx: PeerIndex,
    ) -> Result<Connection> {
        let Addr(sock_addr, peer_identity) = addr;
        let cid = self.new_conn_id();
        let mut conn = quiche::connect(
            None,
            &cid.as_raw(),
            cb.local_addr,
            sock_addr,
            self.config.client(),
        )
        .context("quiche::connect")?;
        if let Some(sslkeylogfile) = &cb.sslkeylogfile {
            conn.set_keylog(Box::new(sslkeylogfile.clone()));
        }
        let mut conn = Connection::new(
            conn,
            self.callback_peer_identity.clone(),
            true,
            PeerIdentity::Wanted(peer_identity),
        );
        conn.flush(cb, packet_buf)?;
        assert!(self.connection_ids.insert(cid, idx).is_none());
        Ok(conn)
    }
}

#[derive(PartialEq)]
enum State {
    Connecting,
    Online,
    Disconnected,
}

pub struct Connection {
    inner: quiche::Connection,
    callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,
    client: bool,
    peer_identity: PeerIdentity,
    state: State,
    buffer: [u8; 2048],
    buffer_range: ops::Range<usize>,
}

impl Connection {
    fn new(
        inner: quiche::Connection,
        callback_peer_identity: Arc<Mutex<Option<PeerIdentity>>>,
        client: bool,
        peer_identity: PeerIdentity,
    ) -> Connection {
        Connection {
            inner,
            callback_peer_identity,
            client,
            peer_identity,
            state: State::Connecting,
            buffer: [0; 2048],
            buffer_range: 0..0,
        }
    }
    pub fn on_recv(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        packet_len: usize,
        from: &SocketAddr,
    ) -> Result<()> {
        {
            let mut cpi = self.callback_peer_identity.lock().unwrap();
            assert!(cpi.is_none());
            *cpi = Some(self.peer_identity);
        }
        let result = self
            .inner
            .recv(&mut packet_buf[..packet_len], quiche::RecvInfo {
                from: *from,
                to: cb.local_addr,
            })
            .context("quiche::Conn::recv");
        self.peer_identity =
            self.callback_peer_identity.lock().unwrap().take().unwrap();
        result?;
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
        Ok(
            if let Some((len, len_encoded_len)) =
                peek_quic_varint(&self.buffer[self.buffer_range.clone()])
            {
                if len > MAX_FRAME_SIZE {
                    bail!(
                        "frames must be shorter than {} bytes, got length field with {} bytes",
                        MAX_FRAME_SIZE,
                        len
                    );
                }
                let len: usize = len.try_into().unwrap();
                let start = self.buffer_range.start + len_encoded_len;
                Some(start..start + len)
            } else {
                None
            },
        )
    }
    fn read_chunk_from_buffer(
        &mut self,
        buf: &mut [u8],
    ) -> Result<Option<usize>> {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        let chunk_range = match self.peek_chunk_range()? {
            Some(r) => r,
            None => return Ok(None),
        };
        if self.buffer_range.end < chunk_range.end {
            return Ok(None);
        }
        buf[..chunk_range.len()]
            .copy_from_slice(&self.buffer[chunk_range.clone()]);
        self.buffer_range.start = chunk_range.end;
        Ok(Some(chunk_range.len()))
    }
    pub fn recv(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        buf: &mut [u8],
    ) -> Result<Option<ConnectionEvent>> {
        assert!(buf.len() >= MAX_FRAME_SIZE as usize);

        use self::State::*;
        match self.state {
            Connecting => {
                self.flush(cb, packet_buf)?;
                if self.client {
                    // Check if the QUIC handshake is complete.
                    // TODO: send an identifier?
                    if self.inner.is_established() {
                        self.state = Online;
                        // TODO: this doesn't actually open the stream
                        self.inner
                            .stream_send(0, b"", false)
                            .context("quiche::Conn::stream_send")?;
                    }
                } else {
                    // Check if the stream is already open.
                    if self.inner.stream_capacity(0).is_ok() {
                        self.state = Online;
                    }
                }
                if self.state == Online {
                    self.check_connection_params()?;
                    return Ok(Some(Event::Connect(Some(
                        *self.peer_identity.assert_known(),
                    )).into()));
                }
            }
            Online => {}
            Disconnected => {
                return Ok(if !self.inner.is_closed() {
                    None
                } else {
                    Some(ConnectionEvent::Delete)
                });
            },
        }

        // Check if the QUIC connection was closed.
        if self.inner.is_draining() || self.inner.local_error().is_some() {
            self.state = Disconnected;
            let (len, remote) = self.extract_error(buf);
            return Ok(Some(Event::Disconnect(len, remote).into()));
        }

        // If we're not online, don't try to receive chunks.
        if self.state != Online {
            return Ok(None);
        }

        // Check if we have any datagrams lying around.
        if let Some(dgram) = self
            .inner
            .dgram_recv_vec()
            .not_done()
            .context("quiche::Conn::dgram_recv_vec")?
        {
            if dgram.first() != Some(&0) {
                bail!(
                    "invalid datagram received, first byte should be 0, is {:?}",
                    dgram.first()
                );
            }
            let len = dgram.len() - 1;
            // Buffer was checked to be long enough above.
            buf[..len].copy_from_slice(&dgram[1..]);
            return Ok(Some(Event::Chunk(len, true).into()));
        }

        // Check if we still have a chunk remaining.
        if let Some(c) = self.read_chunk_from_buffer(buf)? {
            return Ok(Some(Event::Chunk(c, false).into()));
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
        let (read, fin) = self
            .inner
            .stream_recv(0, &mut self.buffer[self.buffer_range.end..])
            .context("quiche::Conn::stream_recv")?;
        self.buffer_range.end += read;

        // Check for a chunk again.
        if let Some(c) = self.read_chunk_from_buffer(buf)? {
            return Ok(Some(Event::Chunk(c, false).into()));
        }

        // If the stream is finished and we still have data, return an error.
        if fin && !self.buffer_range.is_empty() {
            bail!("stream data remaining that does not have a full chunk");
        }

        Ok(None)
    }
    pub fn send_chunk(
        &mut self,
        _cb: &CallbackData,
        _packet_buf: &mut [u8; 65536],
        frame: &[u8],
        unreliable: bool,
    ) -> Result<()> {
        assert!(frame.len() <= MAX_FRAME_SIZE as usize);
        if !unreliable {
            // TODO: state checks?
            let mut len_buf = [0; 8];
            let len_encoded_len =
                write_quic_varint(&mut len_buf, frame.len() as u64);
            let capacity = self.inner.stream_capacity(0).ok();
            let total_len = len_encoded_len + frame.len();
            if capacity.map(|c| total_len > c).unwrap_or(true) {
                bail!(
                    "cannot send data, capacity={:?} len={}",
                    capacity,
                    total_len
                );
            }
            self.inner
                .stream_send(0, &len_buf[..len_encoded_len], false)
                .unwrap();
            self.inner.stream_send(0, frame, false).unwrap();
            Ok(())
        } else {
            let mut buf = Vec::with_capacity(1 + frame.len());
            buf.push(0);
            buf.extend_from_slice(frame);
            // `Error::Done` means that the datagram was immediately dropped
            // without being sent.
            self.inner
                .dgram_send_vec(buf)
                .not_done()
                .context("quiche::Conn::dgram_send_vec")?
                .unwrap();
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
        let mut reason = str::from_utf8(&err.reason)
            .ok()
            .unwrap_or("(invalid utf-8)");
        if reason.bytes().any(|b| b < 32) {
            reason = "(reason containing control characters)";
        }
        let len;
        if err.error_code == QUIC_CLOSE_CODE {
            len = cmp::min(buf.len(), reason.len());
            buf[..len].copy_from_slice(&reason.as_bytes()[..len]);
        } else {
            let mut remaining = &mut buf[..];
            let kind = if err.is_app { "app, " } else { "" };
            if reason.is_empty() {
                let _ = write!(
                    remaining,
                    "QUIC error ({}0x{:x})",
                    kind, err.error_code
                );
            } else {
                let _ = write!(
                    remaining,
                    "QUIC error ({}0x{:x}): {}",
                    kind, err.error_code, reason
                );
            }
            let remaining_len = remaining.len();
            len = buf.len() - remaining_len;
        }
        (len, remote)
    }
    pub fn close(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
        reason: Option<&str>,
    ) -> Result<()> {
        self.inner
            .close(true, QUIC_CLOSE_CODE, reason.unwrap_or("").as_bytes())
            .not_done()
            .context("quiche::Conn::close")?;
        self.flush(cb, packet_buf)?;
        Ok(())
    }
    pub fn timeout(&self) -> Option<Instant> {
        // TODO: Use `Connection::timeout_instant` once quiche > 0.16.0 is
        // released.
        self.inner.timeout().map(|t| Instant::now() + t)
    }
    pub fn on_timeout(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
    ) -> Result<bool> {
        self.inner.on_timeout();
        self.flush(cb, packet_buf)?;
        Ok(false)
    }
    pub fn flush(
        &mut self,
        cb: &CallbackData,
        packet_buf: &mut [u8; 65536],
    ) -> Result<()> {
        let mut num_bytes = 0;
        let mut num_packets = 0;
        loop {
            let Some((written, info)) = self.inner.send(packet_buf).not_done().context("quiche::Conn::send")? else { break; };
            let now = Instant::now();
            let delay = info.at.saturating_duration_since(now);
            if !delay.is_zero() {
                warn!("should have delayed packet by {:?}, but haven't", delay);
            }
            assert!(cb.local_addr == info.from);
            cb.socket
                .send_to(&packet_buf[..written], info.to)
                .context("UdpSocket::send_to")?;
            num_packets += 1;
            num_bytes += written;
        }
        if num_packets != 0 {
            trace!("sent {} packet(s) with {} byte(s)", num_packets, num_bytes);
        } else {
            trace!("sent no packets");
        }
        Ok(())
    }
}
