use anyhow::bail;
use anyhow::Context;
use clap::value_t_or_exit;
use clap::App;
use clap::Arg;
use std::io;
use std::net::SocketAddr;
use std::net::ToSocketAddrs;
use std::net::UdpSocket;
use std::str::FromStr;
use std::time::Duration;
use std::time::Instant;
use uuid::Uuid;

use self::IpVersion::*;

#[derive(Clone, Copy)]
enum IpVersion {
    Ipv4,
    Ipv6,
}

struct InvalidIpVersion;

impl FromStr for IpVersion {
    type Err = InvalidIpVersion;
    fn from_str(s: &str) -> Result<IpVersion, InvalidIpVersion> {
        Ok(match s {
            "ipv4" => Ipv4,
            "ipv6" => Ipv6,
            _ => return Err(InvalidIpVersion),
        })
    }
}

struct IpvResolver(IpVersion);

impl ureq::Resolver for IpvResolver {
    fn resolve(&self, netloc: &str) -> io::Result<Vec<SocketAddr>> {
        ToSocketAddrs::to_socket_addrs(netloc).map(|iter| {
            iter.filter(|s| match self.0 {
                Ipv4 => s.is_ipv4(),
                Ipv6 => s.is_ipv6(),
            })
            .collect()
        })
    }
}

fn main() -> anyhow::Result<()> {
    let matches = App::new("masterping")
        .about("Continuously checks if the masterserver is accepting registrations.")
        .arg(Arg::with_name("ipv")
            .value_name("IPV")
            .required(true)
            .help("IP version (either \"ipv6\" or \"ipv4\").")
        )
        .arg(Arg::with_name("masterurl")
            .value_name("MASTERURL")
            .required(true)
            .help("URL of the masterserver registration endpoint (something like \"https://master1.ddnet.org/ddnet/15/register\")")
        )
        .get_matches();

    let ipv = value_t_or_exit!(matches.value_of("ipv"), IpVersion);
    let url = matches.value_of("masterurl").unwrap();

    let bindaddr = match ipv {
        Ipv4 => "0.0.0.0:0",
        Ipv6 => "[::]:0",
    };
    let udp = UdpSocket::bind(bindaddr).context("UdpSocket::bind")?;
    let address = format!(
        "tw-0.6+udp://connecting-address.invalid:{}",
        udp.local_addr().context("UdpSocket::local_addr")?.port()
    );
    let challenge_secret = Uuid::new_v4().to_string();

    let agent = ureq::builder()
        .user_agent("masterping/0.0.1")
        .resolver(IpvResolver(ipv))
        .build();
    let response = agent
        .post(url)
        .set("Address", &address)
        .set("Secret", &Uuid::new_v4().to_string())
        .set("Challenge-Secret", &challenge_secret)
        .set("Info-Serial", "0")
        .send_bytes(b"")
        .map_err(|e| {
            if let ureq::Error::Status(status, response) = e {
                let status_text = response.status_text().to_owned();
                if let Ok(response) = response.into_string() {
                    eprintln!("{}", response);
                }
                ureq::Error::Status(
                    status,
                    ureq::Response::new(status, &status_text, "").unwrap(),
                )
            } else {
                e
            }
        })?
        .into_string()?;

    if response != "{\"status\":\"need_challenge\"}\n" {
        bail!("Unexpected response {:?}", response);
    }

    let expected_msg_prefix = {
        let mut expected = Vec::new();
        expected.extend_from_slice(b"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xffchal");
        expected.extend_from_slice(challenge_secret.as_bytes());
        expected.push(0);
        expected
    };

    let mut buf = [0; 65536];
    let deadline = Instant::now() + Duration::from_secs(3);
    udp.set_read_timeout(Some(Duration::from_secs(3)))
        .context("UdpSocket::set_read_timeout")?;
    loop {
        let (read, from) = udp
            .recv_from(&mut buf)
            .context("couldn't receive ping from mastersrv in time")?;
        let message = &buf[..read];
        if message.starts_with(&expected_msg_prefix) {
            println!("{}", from);
            break;
        }
        let mut duration = deadline
            .checked_duration_since(Instant::now())
            .unwrap_or_default();
        if duration == Duration::ZERO {
            duration = Duration::from_nanos(1);
        }
        udp.set_read_timeout(Some(duration))
            .context("UdpSocket::set_read_timeout")?;
    }

    Ok(())
}
