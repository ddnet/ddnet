use crate::Timestamp;
use derive_more::From;
use ipnet::IpNet;
use serde::Deserialize;
use serde::Serialize;
use std::sync::Arc;

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
    pub protocol_version: u32,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct ServerHelloMessage;

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

    // Make sure that `protocol_version == PROTOCOL_VERSION`.
    #[derive(Debug, Deserialize, Serialize)]
    struct ClientHelloMessage {
        protocol_version: u32,
    }

    impl<'de> Deserialize<'de> for super::ClientHelloMessage {
        fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
            let ClientHelloMessage { protocol_version } = ClientHelloMessage::deserialize(deserializer)?;
            if protocol_version != PROTOCOL_VERSION {
                return Err(D::Error::custom("invalid protocol version"));
            }
            Ok(super::ClientHelloMessage {
                protocol_version,
            })
        }
    }

    impl Serialize for super::ClientHelloMessage {
        fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
            let super::ClientHelloMessage { protocol_version } = *self;
            assert!(protocol_version == PROTOCOL_VERSION);
            ClientHelloMessage {
                protocol_version,
            }.serialize(serializer)
        }
    }
}

impl Default for ClientHelloMessage {
    fn default() -> ClientHelloMessage {
        ClientHelloMessage {
            protocol_version: PROTOCOL_VERSION,
        }
    }
}
