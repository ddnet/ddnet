pub use self::ffi::net_addr_from_str;
pub use self::ffi::net_addr_str;

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("base/net.h");

        type NETADDR = crate::NETADDR;
        unsafe fn net_addr_str(addr: *const NETADDR, string: *mut c_char, max_length: i32, add_port: bool);
        unsafe fn net_addr_from_str(addr: *mut NETADDR, string: *const c_char) -> i32;
    }
}
