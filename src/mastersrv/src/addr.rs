use arrayvec::ArrayString;
use std::fmt;
use std::fmt::Write;
use std::net::IpAddr;
use std::net::SocketAddr;
use std::str::FromStr;
use url::Url;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Protocol {
    V5,
    V6,
    V7,
    Ddper6,
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Addr {
    // `ip`, `port` come before `protocol` so that the order groups addresses
    // with the same IP addresses together.
    pub ip: IpAddr,
    pub port: u16,
    pub protocol: Protocol,
}

/// A register address, serialized like
/// tw-0.6+udp://connecting-address.invalid:8303.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct RegisterAddr {
    pub port: u16,
    pub protocol: Protocol,
}

impl fmt::Display for Protocol {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.as_str().fmt(f)
    }
}

#[derive(Clone, Copy, Debug)]
pub struct UnknownProtocol;

impl fmt::Display for UnknownProtocol {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        "protocol must be one of tw-0.5+udp, tw-0.6+udp, tw-0.7+udp or ddper-0.6+udp".fmt(f)
    }
}

impl FromStr for Protocol {
    type Err = UnknownProtocol;
    fn from_str(s: &str) -> Result<Protocol, UnknownProtocol> {
        use self::Protocol::*;
        Ok(match s {
            "tw-0.5+udp" => V5,
            "tw-0.6+udp" => V6,
            "tw-0.7+udp" => V7,
            "ddper-0.6+udp" => Ddper6,
            _ => return Err(UnknownProtocol),
        })
    }
}

impl serde::Serialize for Protocol {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(self.as_str())
    }
}

struct ProtocolVisitor;

impl<'de> serde::de::Visitor<'de> for ProtocolVisitor {
    type Value = Protocol;

    fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("one of \"tw-0.5+udp\", \"tw-0.6+udp\", \"tw-0.7+udp\" and \"ddper-0.6+udp\"")
    }
    fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<Protocol, E> {
        let invalid_value = || E::invalid_value(serde::de::Unexpected::Str(v), &self);
        Ok(Protocol::from_str(v).map_err(|_| invalid_value())?)
    }
}

impl<'de> serde::Deserialize<'de> for Protocol {
    fn deserialize<D>(deserializer: D) -> Result<Protocol, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_str(ProtocolVisitor)
    }
}

impl Protocol {
    fn as_str(self) -> &'static str {
        use self::Protocol::*;
        match self {
            V5 => "tw-0.5+udp",
            V6 => "tw-0.6+udp",
            V7 => "tw-0.7+udp",
            Ddper6 => "ddper-0.6+udp",
        }
    }
}

impl Addr {
    pub fn to_socket_addr(self) -> SocketAddr {
        SocketAddr::new(self.ip, self.port)
    }
}

impl fmt::Display for Addr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf: ArrayString<[u8; 128]> = ArrayString::new();
        write!(&mut buf, "{}://{}", self.protocol, self.to_socket_addr()).unwrap();
        buf.fmt(f)
    }
}

#[derive(Clone, Copy, Debug)]
pub struct InvalidAddr;

impl FromStr for Addr {
    type Err = InvalidAddr;
    fn from_str(s: &str) -> Result<Addr, InvalidAddr> {
        let url = Url::parse(s).map_err(|_| InvalidAddr)?;
        let protocol: Protocol = url.scheme().parse().map_err(|_| InvalidAddr)?;
        let mut ip_port: ArrayString<[u8; 64]> = ArrayString::new();
        write!(
            &mut ip_port,
            "{}:{}",
            url.host_str().ok_or(InvalidAddr)?,
            url.port().ok_or(InvalidAddr)?
        )
        .unwrap();
        let sock_addr: SocketAddr = ip_port.parse().map_err(|_| InvalidAddr)?;
        Ok(Addr {
            ip: sock_addr.ip(),
            port: sock_addr.port(),
            protocol,
        })
    }
}

impl serde::Serialize for Addr {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let mut buf: ArrayString<[u8; 128]> = ArrayString::new();
        write!(&mut buf, "{}", self).unwrap();
        serializer.serialize_str(&buf)
    }
}

struct AddrVisitor;

impl<'de> serde::de::Visitor<'de> for AddrVisitor {
    type Value = Addr;

    fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("a URL like tw-0.6+udp://127.0.0.1:8303")
    }
    fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<Addr, E> {
        let invalid_value = || E::invalid_value(serde::de::Unexpected::Str(v), &self);
        Ok(Addr::from_str(v).map_err(|_| invalid_value())?)
    }
}

impl<'de> serde::Deserialize<'de> for Addr {
    fn deserialize<D>(deserializer: D) -> Result<Addr, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_str(AddrVisitor)
    }
}

impl RegisterAddr {
    pub fn with_ip(self, ip: IpAddr) -> Addr {
        Addr {
            ip,
            port: self.port,
            protocol: self.protocol,
        }
    }
}

impl fmt::Display for RegisterAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buf: ArrayString<[u8; 128]> = ArrayString::new();
        write!(
            &mut buf,
            "{}://connecting-address.invalid:{}",
            self.protocol, self.port,
        )
        .unwrap();
        buf.fmt(f)
    }
}

#[derive(Clone, Copy, Debug)]
pub enum ParseRegisterAddrError {
    Url(url::ParseError),
    Protocol(UnknownProtocol),
    HostNotConnectingAddressInvalid,
    PortNotPresent,
}

impl fmt::Display for ParseRegisterAddrError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::ParseRegisterAddrError::*;
        match *self {
            Url(e) => write!(f, "URL parse error: {}", e),
            Protocol(e) => write!(f, "protocol parse error: {}", e),
            HostNotConnectingAddressInvalid => write!(
                f,
                "register address must have domain connecting-address.invalid"
            ),
            PortNotPresent => write!(f, "register address must specify port"),
        }
    }
}

impl FromStr for RegisterAddr {
    type Err = ParseRegisterAddrError;
    fn from_str(s: &str) -> Result<RegisterAddr, ParseRegisterAddrError> {
        use self::ParseRegisterAddrError as Error;
        let url = Url::parse(s).map_err(Error::Url)?;
        let protocol: Protocol = url.scheme().parse().map_err(Error::Protocol)?;
        if url.host_str() != Some("connecting-address.invalid") {
            return Err(Error::HostNotConnectingAddressInvalid);
        }
        let port = url.port().ok_or(Error::PortNotPresent)?;
        Ok(RegisterAddr { port, protocol })
    }
}

impl serde::Serialize for RegisterAddr {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let mut buf: ArrayString<[u8; 128]> = ArrayString::new();
        write!(&mut buf, "{}", self).unwrap();
        serializer.serialize_str(&buf)
    }
}

struct RegisterAddrVisitor;

impl<'de> serde::de::Visitor<'de> for RegisterAddrVisitor {
    type Value = RegisterAddr;

    fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("a URL like tw-0.6+udp://connecting-address.invalid:8303")
    }
    fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<RegisterAddr, E> {
        let invalid_value = || E::invalid_value(serde::de::Unexpected::Str(v), &self);
        Ok(RegisterAddr::from_str(v).map_err(|_| invalid_value())?)
    }
}

impl<'de> serde::Deserialize<'de> for RegisterAddr {
    fn deserialize<D>(deserializer: D) -> Result<RegisterAddr, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        deserializer.deserialize_str(RegisterAddrVisitor)
    }
}

#[cfg(test)]
mod test {
    use super::Addr;
    use super::Protocol;
    use super::RegisterAddr;
    use std::net::IpAddr;
    use std::str::FromStr;

    #[test]
    fn addr_from_str() {
        assert_eq!(
            Addr::from_str("tw-0.6+udp://127.0.0.1:8303").unwrap(),
            Addr {
                ip: IpAddr::from_str("127.0.0.1").unwrap(),
                port: 8303,
                protocol: Protocol::V6,
            }
        );
        assert_eq!(
            Addr::from_str("tw-0.6+udp://[::1]:8303").unwrap(),
            Addr {
                ip: IpAddr::from_str("::1").unwrap(),
                port: 8303,
                protocol: Protocol::V6,
            }
        );
    }

    #[test]
    fn register_addr_from_str() {
        assert_eq!(
            RegisterAddr::from_str("tw-0.6+udp://connecting-address.invalid:8303").unwrap(),
            RegisterAddr {
                port: 8303,
                protocol: Protocol::V6,
            }
        );
    }
}
