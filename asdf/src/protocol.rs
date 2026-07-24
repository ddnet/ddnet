use crate::Timestamp;
use derive_more::From;
use ipnet::IpNet;
use serde::Deserialize;
use serde::Serialize;
use std::cmp;
use std::sync::Arc;

pub const PROTOCOL_VERSION_MIN: u32 = 1;
pub const PROTOCOL_VERSION: u32 = 1;

#[derive(Clone, Debug, Deserialize, Eq, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Ban {
    pub net: IpNet,
    pub expiry: Timestamp,
    pub reason: Arc<str>,
}

#[derive(From)]
pub enum BanMessage {
    AddBan(AddBanMessage),
    RemoveBan(RemoveBanMessage),
}

#[derive(Debug, Deserialize, From, Serialize)]
#[serde(rename_all = "snake_case", tag = "kind")]
pub enum ClientMessage {
    ClientHello(ClientHelloMessage),
    Close(CloseMessage),

    AddBan(AddBanMessage),
    SubscribeBans(SubscribeBansMessage),
    RemoveBan(RemoveBanMessage),
}

#[derive(Debug, Deserialize, From, Serialize)]
#[serde(rename_all = "snake_case", tag = "kind")]
pub enum ServerMessage {
    ServerHello(ServerHelloMessage),
    Close(CloseMessage),

    ReplaceBans(ReplaceBansMessage),
}

#[derive(Debug)]
pub struct ClientHelloMessage {
    pub protocol_version_min: u32,
    pub protocol_version_max: u32,
}

#[derive(Debug)]
pub struct ServerHelloMessage {
    pub protocol_version: u32,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct CloseMessage {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<Arc<str>>,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct AddBanMessage {
    pub net: IpNet,
    pub expiry: Timestamp,
    pub reason: Arc<str>,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct RemoveBanMessage {
    pub net: IpNet,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct SubscribeBansMessage;

#[derive(Debug, Deserialize, Serialize)]
pub struct ReplaceBansMessage {
    pub bans: Arc<Vec<Ban>>,
}

mod serialization {
    use serde::Deserialize;
    use serde::Deserializer;
    use serde::Serialize;
    use serde::Serializer;
    use serde::de::Error as _;
    use super::PROTOCOL_VERSION;
    use super::PROTOCOL_VERSION_MIN;

    // Make sure that `protocol_version_min <= protocol_version_max`.
    #[derive(Debug, Deserialize, Serialize)]
    struct ClientHelloMessage {
        protocol_version_min: u32,
        protocol_version_max: u32,
    }

    // Make sure that `PROTOCOL_VERSION_MIN <= protocol_version <= PROTOCOL_VERSION`.
    #[derive(Debug, Deserialize, Serialize)]
    struct ServerHelloMessage {
        protocol_version: u32,
    }

    impl<'de> Deserialize<'de> for super::ClientHelloMessage {
        fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
            let ClientHelloMessage { protocol_version_min, protocol_version_max } = ClientHelloMessage::deserialize(deserializer)?;
            if !(protocol_version_min <= protocol_version_max) {
                return Err(D::Error::custom("invalid protocol version range"));
            }
            Ok(super::ClientHelloMessage {
                protocol_version_min,
                protocol_version_max,
            })
        }
    }

    impl Serialize for super::ClientHelloMessage {
        fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
            let super::ClientHelloMessage { protocol_version_min, protocol_version_max } = *self;
            assert!(protocol_version_min <= protocol_version_max);
            ClientHelloMessage {
                protocol_version_min,
                protocol_version_max,
            }.serialize(serializer)
        }
    }

    impl<'de> Deserialize<'de> for super::ServerHelloMessage {
        fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
            let ServerHelloMessage { protocol_version } = ServerHelloMessage::deserialize(deserializer)?;
            if !(PROTOCOL_VERSION_MIN <= protocol_version && protocol_version <= PROTOCOL_VERSION) {
                return Err(D::Error::custom("invalid server protocol version"));
            }
            Ok(super::ServerHelloMessage {
                protocol_version,
            })
        }
    }

    impl Serialize for super::ServerHelloMessage {
        fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
            let super::ServerHelloMessage { protocol_version } = *self;
            assert!(PROTOCOL_VERSION_MIN <= protocol_version && protocol_version <= PROTOCOL_VERSION);
            ServerHelloMessage {
                protocol_version,
            }.serialize(serializer)
        }
    }
}

impl Default for ClientHelloMessage {
    fn default() -> ClientHelloMessage {
        ClientHelloMessage {
            protocol_version_min: PROTOCOL_VERSION_MIN,
            protocol_version_max: PROTOCOL_VERSION,
        }
    }
}

impl ServerHelloMessage {
    pub fn try_from(client_hello: ClientHelloMessage) -> Option<ServerHelloMessage> {
        let shared_min = cmp::max(client_hello.protocol_version_min, PROTOCOL_VERSION_MIN);
        let shared_max = cmp::min(client_hello.protocol_version_max, PROTOCOL_VERSION);
        if shared_min > shared_max {
            return None;
        }
        Some(ServerHelloMessage {
            protocol_version: shared_max,
        })
    }
}

#[cfg(test)]
mod test {
    use super::ClientHelloMessage;
    use super::ServerHelloMessage;

    #[test]
    fn server_hello_from_client_hello() {
        fn from(client_min: u32, client_max: u32) -> Option<u32> {
            let client_hello = ClientHelloMessage {
                protocol_version_min: client_min,
                protocol_version_max: client_max,
            };
            ServerHelloMessage::try_from(client_hello)
                .map(|server_hello| server_hello.protocol_version)
        }
        use super::PROTOCOL_VERSION_MIN as MIN;
        use super::PROTOCOL_VERSION as MAX;
        const LOW: u32 = 0;
        const HIGH: u32 = MAX * 10;

        assert_eq!(from(MIN, MIN), Some(MIN));
        assert_eq!(from(MAX, MAX), Some(MAX));
        assert_eq!(from(MIN, MAX), Some(MAX));

        assert_eq!(from(LOW, HIGH), Some(MAX));
        assert_eq!(from(MIN, HIGH), Some(MAX));
        assert_eq!(from(MAX, HIGH), Some(MAX));
        assert_eq!(from(HIGH, HIGH), None);

        assert_eq!(from(LOW, LOW), None);
        assert_eq!(from(LOW, MIN), Some(MIN));
    }
}
