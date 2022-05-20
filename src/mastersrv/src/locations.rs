use arrayvec::ArrayString;
use ipnet::Ipv4Net;
use serde::Deserialize;
use std::net::IpAddr;
use std::path::Path;

pub type Location = ArrayString<[u8; 12]>;

#[derive(Deserialize)]
struct LocationRecord {
    network: Ipv4Net,
    location: Location,
}

#[derive(Debug)]
pub struct LocationsError(String);

pub struct Locations {
    locations: Vec<LocationRecord>,
}

impl Locations {
    pub fn empty() -> Locations {
        Locations {
            locations: Vec::new(),
        }
    }
    pub fn read(filename: &Path) -> Result<Locations, LocationsError> {
        let mut reader = csv::Reader::from_path(filename)
            .map_err(|e| LocationsError(format!("error opening {:?}: {}", filename, e)))?;
        let locations: Result<Vec<_>, _> = reader.deserialize().collect();
        Ok(Locations {
            locations: locations
                .map_err(|e| LocationsError(format!("error deserializing: {}", e)))?,
        })
    }
    pub fn lookup(&self, addr: IpAddr) -> Option<Location> {
        let ipv4_addr = match addr {
            IpAddr::V4(a) => a,
            IpAddr::V6(_) => return None, // sad smiley
        };
        for LocationRecord { network, location } in &self.locations {
            if network.contains(&ipv4_addr) {
                return Some(*location);
            }
        }
        None
    }
}
