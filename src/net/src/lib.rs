#[macro_use]
extern crate log;

use self::challenger::Challenger;
use self::net::ArcFile;
use self::net::Tw06Addr;
use self::net::QuicAddr;
use self::net::Something;
use self::net::MAX_FRAME_SIZE;
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
    let mut builder = Net::builder();
    builder.identity(identity);
    let mut net = builder.open().context("Net::open")?;
    let mut buf = [0; 65536];
    let conn_idx = net.connect(&remote).context("Net::connect")?;
    loop {
        while let Some((idx, event)) =
            net.recv(&mut buf).context("Net::recv")?
        {
            assert_eq!(conn_idx, idx);
            use self::Event::*;
            match event {
                Connect(id) => {
                    println!("{}: connect {}", idx, id);
                    println!("{}: send nonvital \"blab\"", idx);
                    println!("{}: send \"blub\"", idx);
                    println!("{}: send \"blob\"", idx);
                    net.send_chunk(idx, b"blab", true).unwrap();
                    net.send_chunk(idx, b"blub", false).unwrap();
                    net.send_chunk(idx, b"blob", false).unwrap();
                }
                Chunk(size, nonvital) => {
                    println!(
                        "{}: {}{:?}",
                        idx,
                        if nonvital { "nonvital " } else { "" },
                        BStr::new(&buf[..size])
                    );
                }
                Disconnect(size, code) => {
                    println!(
                        "{}: disconnect ({}, {:?})",
                        idx,
                        code,
                        BStr::new(&buf[..size])
                    );
                    return Ok(());
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
        while let Some((idx, event)) =
            net.recv(&mut buf).context("Net::recv")?
        {
            use self::Event::*;
            match event {
                Connect(id) => println!("{}: connect {}", idx, id),
                Chunk(size, nonvital) => {
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
                Disconnect(size, code) => println!(
                    "{}: disconnect ({}, {:?})",
                    idx,
                    code,
                    BStr::new(&buf[..size])
                ),
            }
        }
        net.wait();
    }
}
