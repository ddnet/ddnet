use crate::net_addr_str;
use crate::net_addr_from_str;
use std::error::Error;
use std::ffi::CStr;
use std::ffi::CString;
use std::ffi::c_char;
use std::fmt;
use std::net::IpAddr;
use std::net::Ipv4Addr;
use std::net::Ipv6Addr;
use std::net::SocketAddr;
use std::str::FromStr;

/// Network address.
///
/// # Examples
///
/// ```
/// # extern crate ddnet_test;
/// use ddnet_base::NETADDR;
/// use ddnet_base::NETTYPE_IPV4;
/// use ddnet_base::NETTYPE_IPV6;
///
/// let localhost_ipv4 = NETADDR {
///     type_: NETTYPE_IPV4,
///     ip: [127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
///     port: 8303,
/// };
///
/// let localhost_ipv6 = NETADDR {
///     type_: NETTYPE_IPV6,
///     ip: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
///     port: 8303,
/// };
///
/// assert_eq!(localhost_ipv4.to_string(), "127.0.0.1:8303");
/// assert_eq!(localhost_ipv6.to_string(), "[::1]:8303");
///
/// assert_eq!(localhost_ipv4, "127.0.0.1:8303".parse().unwrap());
/// assert_eq!(localhost_ipv6, "[::1]:8303".parse().unwrap());
/// ```
#[repr(C)]
#[derive(Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct NETADDR {
    pub type_: u32,
    pub ip: [u8; 16],
    pub port: u16,
}

unsafe impl cxx::ExternType for NETADDR {
    type Id = cxx::type_id!("NETADDR");
    type Kind = cxx::kind::Trivial;
}

pub const NETADDR_MAXSTRSIZE: usize = 1 + (8 * 4 + 7) + 1 + 1 + 5 + 1; // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX
pub const NETTYPE_IPV4: u32 = 1 << 0;
pub const NETTYPE_IPV6: u32 = 1 << 1;
pub const NETTYPE_WEBSOCKET_IPV4: u32 = 1 << 2;
pub const NETTYPE_WEBSOCKET_IPV6: u32 = 1 << 3;
pub const NETTYPE_LINK_BROADCAST: u32 = 1 << 4;
pub const NETTYPE_TW7: u32 = 1 << 4;

impl NETADDR {
    pub fn assert_socket_addr(&self) -> SocketAddr {
        let ip_addr: IpAddr = match self.type_ {
            NETTYPE_IPV4 => {
                let octets = [self.ip[0], self.ip[1], self.ip[2], self.ip[3]];
                Ipv4Addr::from(octets).into()
            },
            NETTYPE_IPV6 => Ipv6Addr::from(self.ip).into(),
            type_ => {
                panic!("cannot convert NETADDR type {type_} into `SocketAddr`");
            }
        };
        SocketAddr::new(ip_addr, self.port)
    }
}

impl From<SocketAddr> for NETADDR {
    fn from(addr: SocketAddr) -> NETADDR {
        match addr {
            SocketAddr::V4(v4) => NETADDR {
                type_: NETTYPE_IPV4,
                ip: [
                    v4.ip().octets()[0],
                    v4.ip().octets()[1],
                    v4.ip().octets()[2],
                    v4.ip().octets()[3],
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                ],
                port: addr.port(),
            },
            SocketAddr::V6(v6) => NETADDR {
                type_: NETTYPE_IPV6,
                ip: v6.ip().octets(),
                port: addr.port(),
            },
        }
    }
}

impl fmt::Display for NETADDR {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut buffer = [0; NETADDR_MAXSTRSIZE];
        unsafe {
            net_addr_str(self, buffer.as_mut_ptr() as *mut c_char, buffer.len().try_into().unwrap(), true);
        }
        let s = CStr::from_bytes_until_nul(&buffer).unwrap().to_str().unwrap();
        s.fmt(f)
    }
}

#[derive(Debug)]
pub struct NetAddrFromStrError(());
impl FromStr for NETADDR {
    type Err = NetAddrFromStrError;
    fn from_str(s: &str) -> Result<NETADDR, NetAddrFromStrError> {
        let s = CString::new(s).map_err(|_| NetAddrFromStrError(()))?;
        let mut result = NETADDR::default();
        if unsafe { net_addr_from_str(&mut result, s.as_ptr()) } != 0 {
            return Err(NetAddrFromStrError(()));
        }
        Ok(result)
    }
}

impl fmt::Display for NetAddrFromStrError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        "invalid net address format".fmt(f)
    }
}
impl Error for NetAddrFromStrError {}
