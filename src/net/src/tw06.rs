use crate::ArcFile;
use crate::Challenger;
use crate::Context as _;
use crate::Error;
use crate::Event;
use crate::PeerIndex;
use crate::PrivateIdentity;
use crate::Tw06Addr as Addr;
use crate::Result;
use crate::Something;
use mio::net::UdpSocket;
use std::net::SocketAddr;
use std::str;
use std::time::Instant;

pub struct Protocol;

impl Protocol {
    pub fn new(_: &PrivateIdentity) -> Result<Protocol> {
        Ok(Protocol)
    }
    pub fn on_recv(
        &mut self,
        packet_buf: &mut [u8],
        packet_len: usize,
        from: &SocketAddr,
        challenger: &Challenger,
        next_peer_index: Option<&mut PeerIndex>,
        _local_addr: &SocketAddr,
        _sslkeylogfile: Option<&ArcFile>,
        socket: &UdpSocket,
    ) -> Result<Option<Something<Connection>>> {
        if next_peer_index.is_none() {
            return Ok(None);
        }
        let _ = (packet_buf, packet_len, from, challenger, next_peer_index, socket);
        todo!();
    }
    pub fn connect(
        &mut self,
        addr: Addr,
        _idx: PeerIndex,
        _local_addr: &SocketAddr,
        _sslkeylogfile: Option<&ArcFile>,
    ) -> Result<Connection> {
        let Addr(sock_addr) = addr;
        let epoch = Instant::now();
        Ok(Connection::new(libtw2_net::Connection::new(), epoch, sock_addr))
    }
}

pub struct Connection {
    inner: libtw2_net::Connection,
    epoch: Instant,
    addr: SocketAddr,
}

impl Connection {
    fn new(inner: libtw2_net::Connection, epoch: Instant, addr: SocketAddr) -> Connection {
        Connection {
            inner,
            epoch,
            addr,
        }
    }
    pub fn on_recv(
        &mut self,
        buf: &mut [u8],
        from: &SocketAddr,
        _local_addr: &SocketAddr,
    ) -> Result<()> {
        todo!();
    }
    pub fn recv(&mut self, buf: &mut [u8]) -> Result<Option<Event>> {
        todo!();
    }
    pub fn send_chunk(&mut self, frame: &[u8], unreliable: bool) -> Result<()> {
        todo!();
    }
    pub fn close(&mut self, reason: Option<&str>) {
        todo!();
    }
    pub fn timeout(&self) -> Option<Instant> {
        todo!();
    }
    pub fn on_timeout(&mut self) {
        todo!();
    }
    pub fn flush(
        &mut self,
        _local_addr: &SocketAddr,
        socket: &UdpSocket,
        buf: &mut [u8],
    ) -> Result<()> {
        if self.inner.is_unconnected() {
            self.inner.connect(&mut Callback { socket, addr: &self.addr, epoch: self.epoch })?;
        }
        todo!();
    }
}

struct Callback<'a> {
    socket: &'a UdpSocket,
    addr: &'a SocketAddr,
    epoch: Instant,
}

impl<'a> libtw2_net::connection::Callback for Callback<'a> {
    type Error = Error;
    fn send(&mut self, buffer: &[u8]) -> Result<()> {
        self.socket.send_to(buffer, *self.addr).context("UdpSocket::send_to")?;
        Ok(())
    }
    fn time(&mut self) -> libtw2_net::Timestamp {
        libtw2_net::Timestamp::from_usecs_since_epoch(self.epoch.elapsed().as_micros().try_into().unwrap())
    }
}
