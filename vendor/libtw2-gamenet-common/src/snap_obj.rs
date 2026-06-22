use serde_derive::Deserialize;
use serde_derive::Serialize;
use std::fmt;
use uuid::Uuid;

#[derive(Clone, Copy, Deserialize, Eq, Ord, Hash, PartialEq, PartialOrd, Serialize)]
#[serde(untagged)]
pub enum TypeId {
    Ordinal(u16),
    Uuid(Uuid),
}

impl fmt::Debug for TypeId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            TypeId::Ordinal(i) => i.fmt(f),
            TypeId::Uuid(u) => u.fmt(f),
        }
    }
}

impl fmt::Display for TypeId {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

impl From<u16> for TypeId {
    fn from(i: u16) -> TypeId {
        TypeId::Ordinal(i)
    }
}

impl From<Uuid> for TypeId {
    fn from(u: Uuid) -> TypeId {
        TypeId::Uuid(u)
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Tick(pub i32);
