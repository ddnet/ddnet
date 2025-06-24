use crate::Addr;
use crate::Locations;
use ipnet::IpNet;
use serde::Deserialize;
use std::error;
use std::fmt;
use std::fs;
use std::net::IpAddr;
use std::net::SocketAddr;
use std::path::Path;
use std::str::FromStr;

pub enum ConfigLocation {
    None,
    File(Box<str>),
    LocationsFileParameter(Box<str>),
}

#[derive(Default)]
pub struct Config {
    default_ban_message: Box<str>,
    bans: Box<[Ban]>,
    port_forward_check_exceptions: Box<[ConfigAddr]>,
    pub locations: Locations,
}

#[derive(Default, Deserialize)]
#[serde(deny_unknown_fields)]
struct ParsedConfig {
    #[serde(default)]
    default_ban_message: Option<Box<str>>,
    #[serde(default)]
    bans: Box<[Ban]>,
    #[serde(default)]
    port_forward_check_exceptions: Box<[ConfigAddr]>,
    #[serde(default)]
    locations: Option<Box<str>>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Ban {
    address: ConfigAddr,
    reason: Option<Box<str>>,
}

enum ConfigAddr {
    WithPort(SocketAddr),
    OnlyIp(IpAddr),
    Range(IpNet),
}

#[derive(Debug)]
pub struct Error(String);

impl ConfigLocation {
    pub fn read(&self) -> Result<Config, Error> {
        use self::ConfigLocation::*;
        match self {
            None => Ok(Config::default()),
            File(filename) => Config::read(Path::new(&**filename)),
            LocationsFileParameter(locations_file) => {
                Config::from_locations_file_parameter(locations_file)
            }
        }
    }
}

impl Config {
    pub fn from_locations_file_parameter(locations_file: &str) -> Result<Config, Error> {
        ParsedConfig::from_locations_file_parameter(locations_file).to_config()
    }
    pub fn read(filename: &Path) -> Result<Config, Error> {
        ParsedConfig::read(filename)?.to_config()
    }
    pub fn is_banned(&self, addr: Addr) -> Option<&str> {
        for ban in &self.bans {
            if ban.address.matches(addr) {
                return Some(ban.reason.as_deref().unwrap_or(&self.default_ban_message));
            }
        }
        None
    }
    pub fn is_exempt_from_port_forward_check(&self, addr: Addr) -> bool {
        self.port_forward_check_exceptions
            .iter()
            .any(|e| e.matches(addr))
    }
}

impl ParsedConfig {
    fn from_locations_file_parameter(locations_file: &str) -> ParsedConfig {
        ParsedConfig {
            locations: Some(locations_file.into()),
            ..Default::default()
        }
    }
    fn read(filename: &Path) -> Result<ParsedConfig, Error> {
        let contents = fs::read_to_string(filename)
            .map_err(|e| Error(format!("error opening {:?}: {}", filename, e)))?;
        toml::from_str(&contents).map_err(|e| Error(format!("TOML error in {:?}: {}", filename, e)))
    }
    fn to_config(self) -> Result<Config, Error> {
        let ParsedConfig {
            default_ban_message,
            bans,
            port_forward_check_exceptions,
            locations,
        } = self;
        Ok(Config {
            default_ban_message: default_ban_message.unwrap_or_else(|| "banned".into()),
            bans,
            port_forward_check_exceptions,
            locations: locations
                .as_deref()
                .map(|l| {
                    Locations::read(Path::new(l))
                        .map_err(|e| Error(format!("couldn't open locations file {:?}: {}", l, e)))
                })
                .transpose()?
                .unwrap_or_default(),
        })
    }
}

impl ConfigAddr {
    fn matches(&self, addr: Addr) -> bool {
        use self::ConfigAddr::*;
        match *self {
            WithPort(i) => addr.ip == i.ip() && addr.port == i.port(),
            OnlyIp(i) => addr.ip == i,
            Range(i) => i.contains(&addr.ip),
        }
    }
}

impl fmt::Debug for ConfigAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::ConfigAddr::*;
        match self {
            WithPort(i) => i.fmt(f),
            OnlyIp(i) => i.fmt(f),
            Range(i) => i.fmt(f),
        }
    }
}

impl fmt::Display for ConfigAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::ConfigAddr::*;
        match self {
            WithPort(i) => i.fmt(f),
            OnlyIp(i) => i.fmt(f),
            Range(i) => i.fmt(f),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct InvalidAddr;

impl FromStr for ConfigAddr {
    type Err = InvalidAddr;
    fn from_str(s: &str) -> Result<ConfigAddr, InvalidAddr> {
        if let Ok(socket_addr) = SocketAddr::from_str(s) {
            return Ok(ConfigAddr::WithPort(socket_addr));
        }
        if let Ok(ip_addr) = IpAddr::from_str(s) {
            return Ok(ConfigAddr::OnlyIp(ip_addr));
        }
        if let Ok(ip_net) = IpNet::from_str(s) {
            return Ok(ConfigAddr::Range(ip_net));
        }
        Err(InvalidAddr)
    }
}

struct ConfigAddrVisitor;

impl<'de> serde::de::Visitor<'de> for ConfigAddrVisitor {
    type Value = ConfigAddr;

    fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str("either an IP address+port combination, an IP address or an IP address range")
    }
    fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<ConfigAddr, E> {
        let invalid_value = || E::invalid_value(serde::de::Unexpected::Str(v), &self);
        Ok(ConfigAddr::from_str(v).map_err(|_| invalid_value())?)
    }
}

impl<'de> serde::Deserialize<'de> for ConfigAddr {
    fn deserialize<D: serde::de::Deserializer<'de>>(
        deserializer: D,
    ) -> Result<ConfigAddr, D::Error> {
        deserializer.deserialize_str(ConfigAddrVisitor)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl error::Error for Error {}

#[cfg(test)]
mod test {
    use super::ConfigAddr;
    use ipnet::IpNet;
    use std::net::IpAddr;
    use std::net::SocketAddr;
    use std::str::FromStr;

    #[test]
    fn addr_from_str() {
        assert!(matches!(
            ConfigAddr::from_str("[::]:0"),
            Ok(ConfigAddr::WithPort(SocketAddr::V6(_))),
        ));
        assert!(matches!(
            ConfigAddr::from_str("::"),
            Ok(ConfigAddr::OnlyIp(IpAddr::V6(_))),
        ));
        assert!(matches!(
            ConfigAddr::from_str("::/0"),
            Ok(ConfigAddr::Range(IpNet::V6(_))),
        ));

        assert!(matches!(
            ConfigAddr::from_str("0.0.0.0:0"),
            Ok(ConfigAddr::WithPort(SocketAddr::V4(_))),
        ));
        assert!(matches!(
            ConfigAddr::from_str("0.0.0.0"),
            Ok(ConfigAddr::OnlyIp(IpAddr::V4(_))),
        ));
        assert!(matches!(
            ConfigAddr::from_str("0.0.0.0/0"),
            Ok(ConfigAddr::Range(IpNet::V4(_))),
        ));
    }
}
