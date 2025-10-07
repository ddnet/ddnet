use crate::community_token;
use crate::Addr;
use crate::Locations;
use arrayvec::ArrayString;
use ipnet::IpNet;
use serde::Deserialize;
use serde_json as json;
use std::collections::HashMap;
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
    pub communities: Option<Box<json::value::RawValue>>,
    community_tokens: HashMap<community_token::PlainPrefix, CommunityToken>,
    default_ban_message: Option<Box<str>>,
    bans: Box<[Ban]>,
    port_forward_check_exceptions: Box<[ConfigAddr]>,
    pub locations: Locations,
}

struct CommunityToken {
    hash: community_token::Hash,
    community: ArrayString<[u8; 32]>,
}

#[derive(Default, Deserialize)]
#[serde(deny_unknown_fields)]
struct ParsedConfig {
    #[serde(default)]
    communities: Option<Communities>,
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
struct Communities {
    json: Box<str>,
    #[serde(default)]
    tokens: HashMap<Box<str>, Box<str>>,
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
    pub fn is_banned(&self, addr: Addr) -> Option<Option<&str>> {
        for ban in &self.bans {
            if ban.address.matches(addr) {
                return Some(ban.reason.as_deref().or(self.default_ban_message.as_deref()));
            }
        }
        None
    }
    pub fn is_exempt_from_port_forward_check(&self, addr: Addr) -> bool {
        self.port_forward_check_exceptions
            .iter()
            .any(|e| e.matches(addr))
    }
    pub fn community_for_token(&self, token: &str) -> Result<Option<&str>, String> {
        let plain = community_token::get_plain_prefix_from_token(token).ok_or_else(|| {
            if community_token::Verification::from_str(token).is_ok() {
                format!(
                    "need real token, not verification token: {token} (should start with ddtc_)"
                )
            } else {
                format!("invalid community token format: {token}")
            }
        })?;
        Ok(self.community_for_token_impl(plain, token))
    }
    fn community_for_token_impl(&self, plain: &str, token: &str) -> Option<&str> {
        let community_token = self.community_tokens.get(plain)?;
        community_token
            .hash
            .verify(token)
            .ok()
            .map(|()| &community_token.community[..])
    }
}

impl CommunityToken {
    fn from_config(
        verification: &str,
        community: &str,
    ) -> Result<(community_token::PlainPrefix, CommunityToken), Error> {
        let verification: community_token::Verification = verification.parse()
            .map_err(|_| {
                Error(if community_token::get_plain_prefix_from_token(verification).is_some() {
                    format!("community token, not community _verification_ token: {verification} (should start with ddvc_)")
                } else {
                    format!("invalid community verification token {verification}")
                })
            })?;
        Ok((
            verification.plain_prefix,
            CommunityToken {
                hash: verification.hash,
                community: ArrayString::from(community)
                    .map_err(|_| Error(format!("community name {:?} too long", community)))?,
            },
        ))
    }
}

fn read_communities_json(filename: &Path) -> Result<Box<json::value::RawValue>, Error> {
    let contents = fs::read_to_string(filename).map_err(|e| {
        Error(format!(
            "error opening communities JSON {:?}: {}",
            filename, e
        ))
    })?;
    let result: json::Value = json::from_str(&contents)
        .map_err(|e| Error(format!("JSON error in {:?}: {}", filename, e)))?;
    // Normalize the JSON to strip any spaces etc.
    Ok(json::value::to_raw_value(&result).unwrap())
}

fn from_community_tokens_map(
    tokens: &HashMap<Box<str>, Box<str>>,
) -> Result<HashMap<community_token::PlainPrefix, CommunityToken>, Error> {
    let mut result = HashMap::new();
    for (hashed_token, community) in tokens {
        let (prefix, token) = CommunityToken::from_config(hashed_token, community)?;
        if result.insert(prefix, token).is_some() {
            return Err(Error(format!("two tokens with prefix {:?}", prefix)));
        }
    }
    Ok(result)
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
            communities,
            default_ban_message,
            bans,
            port_forward_check_exceptions,
            locations,
        } = self;
        Ok(Config {
            communities: communities
                .as_ref()
                .map(|c| read_communities_json(Path::new(&*c.json)))
                .transpose()?,
            community_tokens: communities
                .as_ref()
                .map(|c| from_community_tokens_map(&c.tokens))
                .transpose()?
                .unwrap_or_default(),
            default_ban_message: default_ban_message.map(Into::into),
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
