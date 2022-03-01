use clap::App;
use clap::Arg;
use hex::FromHex;
use hexdump::hexdump;
use std::error::Error;
use std::net::Ipv6Addr;
use std::net::SocketAddr;
use std::net::SocketAddrV6;
use std::net::UdpSocket;
use std::sync::Arc;
use std::thread;

const SPECIAL: Ipv6Addr = Ipv6Addr::new(
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
);

fn secret_from_str(s: &str) -> Result<[u8; 32], Box<dyn Error>> {
    Ok(FromHex::from_hex(s)?)
}

fn addr_from_bytes(b: &[u8; 18]) -> SocketAddrV6 {
    let ip: [u8; 16] = b[..16].try_into().unwrap();
    let port: [u8; 2] = b[16..].try_into().unwrap();
    SocketAddrV6::new(Ipv6Addr::from(ip), u16::from_be_bytes(port), 0, 0)
}

fn addr_to_bytes(addr: SocketAddrV6) -> [u8; 18] {
    let mut result = [0; 18];
    result[..16].copy_from_slice(&addr.ip().octets());
    result[16..].copy_from_slice(&addr.port().to_be_bytes());
    result
}

fn listen_proxy(proxy: Arc<UdpSocket>, upstream: Arc<UdpSocket>)
    -> Result<(), Box<dyn Error>>
{
    let mut buf = [0; 2048];
    loop {
        let (read, addr) = proxy.recv_from(&mut buf[18..])?;
        let msg = &mut buf[..18 + read];
        let addr = match addr {
            SocketAddr::V4(_) => panic!("got v4 socket address: {}", addr),
            SocketAddr::V6(a) => a,
        };
        msg[..18].copy_from_slice(&addr_to_bytes(addr));
        upstream.send(msg)?;
        println!("to upstream from {}", addr);
        hexdump(&msg[18..]);
    }
}

fn listen_upstream(proxy: Arc<UdpSocket>, upstream: Arc<UdpSocket>)
    -> Result<(), Box<dyn Error>>
{
    let mut buf = [0; 2048];
    loop {
        let read = upstream.recv(&mut buf)?;
        let msg = &buf[..read];
        if msg.len() < 18 {
            eprintln!("received short message on upstream socket, len={}",
                msg.len()
            );
            continue;
        }
        let (addr, data) = msg.split_at(18);
        let addr = addr_from_bytes(addr.try_into().unwrap());

        proxy.send_to(data, addr)?;

        println!("from upstream to {}", addr);
        hexdump(data);
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let matches = App::new("UDP reverse proxy")
        .about("Non-transparent UDP reverse proxy that includes client IP \
                address and port in the upstream request.")
        .arg(Arg::with_name("proxy")
            .help("Bind address for the proxy")
            .required(true)
            .value_name("PROXY")
        )
        .arg(Arg::with_name("server")
            .help("Upstream server to reverse-proxy")
            .required(true)
            .value_name("SERVER")
        )
        .arg(Arg::with_name("secret")
            .help("Secret to use to contact the upstream server")
            .required(true)
            .value_name("SECRET")
        )
        .get_matches();

    let proxy = matches.value_of("proxy").unwrap();
    let server = matches.value_of("server").unwrap();
    let secret = secret_from_str(matches.value_of("secret").unwrap())?;

    println!("binding proxy to {}", proxy);
    let proxy = Arc::new(UdpSocket::bind(proxy)?);
    println!("binding upstream");
    let upstream = Arc::new(UdpSocket::bind("[::]:0")?);
    upstream.connect(server)?;

    {
        let mut secret_msg = [0; 50];
        secret_msg[..18].copy_from_slice(
            &addr_to_bytes(SocketAddrV6::new(SPECIAL, 1, 0, 0)),
        );
        secret_msg[18..].copy_from_slice(&secret);
        upstream.send(&secret_msg)?;
    }

    {
        let proxy = proxy.clone();
        let upstream = upstream.clone();
        thread::spawn(|| listen_proxy(proxy, upstream).unwrap());
    }
    listen_upstream(proxy, upstream).unwrap();
    Ok(())
}
