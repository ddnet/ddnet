use asdf::Asdf;
use asdf::Bans;
use ddnet_base::NETADDR;
use ddnet_base::StrBufExt as _;
use ddnet_engine_shared::runtime;
use std::os::raw::c_char;
use tokio::sync::watch;

#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("base/types.h");

        type NETADDR = ddnet_base::NETADDR;
    }
    extern "Rust" {
        type CAsdfImpl;

        #[Self = CAsdfImpl]
        fn New(connect_to: &str) -> Box<CAsdfImpl>;
        fn IsBanned(&self, address: &NETADDR, reason_buf: &mut [c_char]) -> bool;
        fn BansHaveChanged(&mut self) -> bool;
    }
}

pub struct CAsdfImpl {
    #[expect(unused)]
    asdf: Asdf,
    bans: watch::Receiver<Bans>,
}

#[expect(nonstandard_style, reason = "exposed to C++")]
impl CAsdfImpl {
    pub fn New(connect_to: &str) -> Box<CAsdfImpl> {
        let _runtime = runtime().enter();
        let asdf = Asdf::remote(connect_to.into());
        Box::new(CAsdfImpl {
            bans: asdf.subscribe_bans(),
            asdf,
        })
    }
    pub fn BansHaveChanged(&mut self) -> bool {
        let changed = self.bans.has_changed().expect("sender can't go away while we hold a reference");
        if changed {
            self.bans.mark_unchanged();
        }
        changed
    }
    pub fn IsBanned(&self, address: &NETADDR, reason_buf: &mut [c_char]) -> bool {
        let ip_addr = address.assert_socket_addr().ip();
        for ban in &*self.bans.borrow().current_ban_list() {
            if ban.net.contains(&ip_addr) {
                reason_buf.truncated_copy_from_and_add_nul(&ban.reason);
                return true;
            }
        }
        false
    }
}
