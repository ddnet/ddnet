use std::error::Error;
use std::fmt;
use std::iter;
use std::str::FromStr;

#[derive(Debug)]
pub struct InvalidSliceLength;

#[derive(Clone, Copy, Hash, Eq, Ord, PartialEq, PartialOrd)]
pub struct Sha256(pub [u8; 32]);

impl Sha256 {
    pub fn from_slice(bytes: &[u8]) -> Result<Sha256, InvalidSliceLength> {
        let mut result = [0; 32];
        if bytes.len() != result.len() {
            return Err(InvalidSliceLength);
        }
        result.copy_from_slice(bytes);
        Ok(Sha256(result))
    }
}

impl fmt::Debug for Sha256 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for &b in &self.0 {
            write!(f, "{:02x}", b)?;
        }
        Ok(())
    }
}

impl fmt::Display for Sha256 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(self, f)
    }
}

#[derive(Debug)]
pub enum Sha256FromStrError {
    InvalidLength(usize),
    NonHexChar,
}

impl fmt::Display for Sha256FromStrError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Sha256FromStrError::*;
        match self {
            InvalidLength(len) => write!(f, "invalid length {}, must be 64", len),
            NonHexChar => "non-hex character".fmt(f),
        }
    }
}

impl Error for Sha256FromStrError {}

impl FromStr for Sha256 {
    type Err = Sha256FromStrError;
    fn from_str(v: &str) -> Result<Sha256, Sha256FromStrError> {
        let len = v.chars().count();
        if len != 64 {
            return Err(Sha256FromStrError::InvalidLength(len));
        }
        let mut result = [0; 32];
        // I just want to get string slices with two characters each. :(
        // Sorry for this monstrosity.
        let starts = v
            .char_indices()
            .map(|(i, _)| i)
            .chain(iter::once(v.len()))
            .step_by(2);
        let ends = {
            let mut e = starts.clone();
            e.next();
            e
        };
        for (i, (s, e)) in starts.zip(ends).enumerate() {
            result[i] =
                u8::from_str_radix(&v[s..e], 16).map_err(|_| Sha256FromStrError::NonHexChar)?;
        }
        Ok(Sha256(result))
    }
}

#[cfg(feature = "serde")]
mod serialize {
    use std::fmt;

    use super::Sha256;

    impl serde::Serialize for Sha256 {
        fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde::Serializer,
        {
            serializer.serialize_str(&format!("{}", self))
        }
    }

    struct HexSha256Visitor;

    impl<'de> serde::de::Visitor<'de> for HexSha256Visitor {
        type Value = Sha256;

        fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
            f.write_str("64 character hex value")
        }
        fn visit_str<E: serde::de::Error>(self, v: &str) -> Result<Sha256, E> {
            use super::Sha256FromStrError::*;
            v.parse().map_err(|e| match e {
                InvalidLength(len) => E::invalid_length(len, &self),
                NonHexChar => E::invalid_value(serde::de::Unexpected::Str(v), &self),
            })
        }
    }

    impl<'de> serde::Deserialize<'de> for Sha256 {
        fn deserialize<D>(deserializer: D) -> Result<Sha256, D::Error>
        where
            D: serde::de::Deserializer<'de>,
        {
            deserializer.deserialize_str(HexSha256Visitor)
        }
    }
}
