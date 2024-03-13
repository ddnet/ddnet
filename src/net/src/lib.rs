#[macro_use]
extern crate log;

use self::challenger::Challenger;
use self::net::CallbackData;
use self::net::MAX_FRAME_SIZE;
use self::net::ProtocolEvent;
use self::net::QuicAddr;
use self::net::Tw06Addr;
use self::util::normalize;
use self::util::peek_quic_varint;
use self::util::secure_hash;
use self::util::secure_random;
use self::util::write_quic_varint;
use self::util::NoBlock;
use bstr::BStr;
use error::Context;

macro_rules! bail {
    ($($arg:tt)*) => {
        return Err($crate::Error::from_string(format!($($arg)*)))
    }
}

mod challenger;
mod error;
mod ffi;
mod key;
mod net;
mod quic;
mod tw06;
mod util;

pub use self::error::Error;
pub use self::error::Result;
pub use self::key::Identity;
pub use self::key::PrivateIdentity;
pub use self::net::Addr;
pub use self::net::ConnectionEvent;
pub use self::net::Event;
pub use self::net::Net;
pub use self::net::NetBuilder;
pub use self::net::PeerIndex;

pub fn client2_main() -> Result<()> {
    env_logger::init();
    let identity =
        "6a35f116046280da62602ad979e151daee8e9109befd34337427bbb968c2bcbc"
            .parse()
            .unwrap();
    let remote = "ddnet-15+quic://127.0.0.1:4433#0026a0d653cd5f38d1002bf166167933f2f7910f26d6dd619b2b3fe769e057ee";
    //let remote = "tw-0.6+udp://127.0.0.1:4433";
    let mut builder = Net::builder();
    builder.identity(identity);
    let mut net = builder.open().context("Net::open")?;
    let mut buf = [0; 65536];
    println!("x: send connless \"bleb\" to {}", remote);
    net.send_connless_chunk(remote, b"bleb").context("Net::connect")?;
    let conn_idx = net.connect(remote).context("Net::connect")?;
    loop {
        while let Some(event) =
            net.recv(&mut buf).context("Net::recv")?
        {
            use self::Event::*;
            match event {
                Connect(idx, addr) => {
                    assert_eq!(conn_idx, idx);
                    println!("{}: connect {}", idx, addr);
                    println!("{}: send nonvital \"blab\"", idx);
                    println!("{}: send \"blub\"", idx);
                    println!("x: send connless \"bleb\" to {}", remote);
                    println!("{}: send \"blob\"", idx);
                    net.send_chunk(idx, b"blab", true).unwrap();
                    net.send_chunk(idx, b"blub", false).unwrap();
                    net.send_connless_chunk(remote, b"bleb").context("Net::connect")?;
                    net.send_chunk(idx, b"blob", false).unwrap();
                    net.flush(idx).unwrap();
                }
                Chunk(idx, size, nonvital) => {
                    assert_eq!(conn_idx, idx);
                    println!(
                        "{}: {}{:?}",
                        idx,
                        if nonvital { "nonvital " } else { "" },
                        BStr::new(&buf[..size])
                    );
                }
                Disconnect(idx, size, code) => {
                    assert_eq!(conn_idx, idx);
                    println!(
                        "{}: disconnect ({}, {:?})",
                        idx,
                        code,
                        BStr::new(&buf[..size])
                    );
                    return Ok(());
                }
                ConnlessChunk(from, size) => {
                    println!("x: connless chunk: {:?} from {}", BStr::new(&buf[..size]), from);
                }
            }
        }
        net.wait();
    }
}
pub fn server2_main() -> Result<()> {
    env_logger::init();
    let bindaddr = "0.0.0.0:4433".parse().unwrap();
    let identity =
        "5cf2a4f0ed3dc85b3f4bfa5ca97b8adeaf0d5ec165179af805a3f7751dd8ac47"
            .parse()
            .unwrap();
    let mut builder = Net::builder();
    builder.identity(identity);
    builder.bindaddr(bindaddr);
    builder.accept_connections(true);
    let mut net = builder.open().context("Net::open")?;
    let mut buf = [0; 65536];
    loop {
        while let Some(event) =
            net.recv(&mut buf).context("Net::recv")?
        {
            use self::Event::*;
            match event {
                Connect(idx, addr) => println!("{}: connect {}", idx, addr),
                Chunk(idx, size, nonvital) => {
                    println!(
                        "{}: recv {}{:?}",
                        idx,
                        if nonvital { "nonvital " } else { "" },
                        BStr::new(&buf[..size])
                    );
                    if !nonvital {
                        net.close(idx, Some("blib?")).unwrap();
                    }
                }
                Disconnect(idx, size, code) => println!(
                    "{}: disconnect ({}, {:?})",
                    idx,
                    code,
                    BStr::new(&buf[..size])
                ),
                ConnlessChunk(from, size) => {
                    println!("x: connless chunk: {:?} from {}", BStr::new(&buf[..size]), from);
                }
            }
        }
        net.wait();
    }
}
