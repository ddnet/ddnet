use ddnet_base::NETADDR;
use std::mem;

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("base/types.h");

        type NETADDR = ddnet_base::NETADDR;
    }
    extern "Rust" {
        type CAsdfImpl;

        #[Self = CAsdfImpl]
        fn New() -> Box<CAsdfImpl>;
        unsafe fn IsBanned<'a>(&'a self, address: &NETADDR, reason: &mut &'a str) -> bool;
        fn HasChanged(&mut self) -> bool;
    }
}

pub struct CAsdfImpl {
    has_changed: bool,
}

#[expect(nonstandard_style, reason = "exposed to C++")]
impl CAsdfImpl {
    pub fn New() -> Box<CAsdfImpl> {
        Box::new(CAsdfImpl {
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
