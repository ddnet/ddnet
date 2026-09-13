use serde_with::DeserializeFromStr;
use serde_with::SerializeDisplay;
use std::fmt;
use std::str::FromStr;

#[derive(Clone, Copy, DeserializeFromStr, Eq, Hash, Ord, PartialEq, PartialOrd, SerializeDisplay)]
pub struct Timestamp(/* seconds since unix epoch */ i64);

impl fmt::Debug for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        jiff::Timestamp::from_second(self.0).unwrap().fmt(f)
    }
}
impl fmt::Display for Timestamp {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        jiff::Timestamp::from_second(self.0).unwrap().fmt(f)
    }
}

impl FromStr for Timestamp {
    type Err = anyhow::Error;
    fn from_str(s: &str) -> anyhow::Result<Timestamp> {
        // TODO: constrain formatting accepted by this parser
        Ok(Timestamp(jiff::Timestamp::from_str(s)?.as_second()))
    }
}
