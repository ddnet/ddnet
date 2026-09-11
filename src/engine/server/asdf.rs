use ddnet_base::NETADDR;
use std::mem;

#[cxx::bridge]
mod ffi {
    extern "C++" {
        type NETADDR = ddnet_base::NETADDR;
    }
    extern "Rust" {
        type CAsdfRust;

        #[Self = CAsdfRust]
        fn New() -> Box<CAsdfRust>;
        unsafe fn IsBanned<'a>(&'a self, address: &NETADDR, reason: &mut &'a str) -> bool;
        fn HasChanged(&mut self) -> bool;
    }
}

pub struct CAsdfRust {
    has_changed: bool,
}

impl CAsdfRust {
    pub fn New() -> Box<CAsdfRust> {
        Box::new(CAsdfRust {
            has_changed: true,
        })
    }
    pub fn HasChanged(&mut self) -> bool {
        mem::replace(&mut self.has_changed, false)
    }
    pub fn IsBanned<'a>(&'a self, address: &NETADDR, reason: &mut &'a str) -> bool {
        false
    }
}
