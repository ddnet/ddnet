use arrayvec::ArrayString;
use std::fmt;
use std::net::IpAddr;
use std::path::Path;

pub type Location = ArrayString<[u8; 12]>;

#[allow(dead_code)] // only used for `Debug` impl
#[derive(Debug)]
pub struct LocationsError(String);

#[derive(Default)]
pub struct Locations {
    inner: Option<libloc::Locations>,
}

impl fmt::Display for LocationsError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl Locations {
    pub fn read(filename: &Path) -> Result<Locations, LocationsError> {
        let inner = libloc::Locations::open(filename)
            .map_err(|e| LocationsError(format!("error opening {:?}: {}", filename, e)))?;
        Ok(Locations { inner: Some(inner) })
    }
    pub fn lookup(&self, addr: IpAddr) -> Option<Location> {
        self.inner.as_ref().and_then(|inner| {
            let country_code = inner.lookup(addr)?.country_code();
            let continent_code = inner.country(country_code)?.continent_code();
            let mut result = ArrayString::new();
            result.push_str(continent_code);
            result.push_str(":");
            result.push_str(country_code);
            result.make_ascii_lowercase();
            Some(result)
        })
    }
}
