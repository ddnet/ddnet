use self::NetInner::*;
use crate::Context;
use crate::Error;
use crate::Event as EventImpl;
use crate::Net as NetImpl;
use crate::NetBuilder as NetBuilderImpl;
use crate::PeerIndex;
use crate::PrivateIdentity;
use crate::Result;
use std::ffi::c_char;
use std::ffi::CStr;
use std::ffi::CString;
use std::mem;
use std::panic;
use std::ptr;
use std::result;
use std::slice;
use std::str;
use std::time::Duration;
use std::time::Instant;

pub struct DdnetNet(NetInner);

/// This is a state machine that should only ever start from `Good` or
/// `InitError`, and it may only transition from `Good` to `LaterError`.
///
/// `Temporary` is only used to abide by ownership rules when moving from
/// `Good` to `LaterError`.
enum NetInner {
    Init(NetBuilderImpl),
    Good(NetImpl),
    InitError(CString),
    LaterError(NetImpl, CString),
    Temporary,
}

#[repr(C)]
pub union DdnetNetEvent {
    inner: Option<EventImpl>,
    _padding_alignment: [u64; 8],
}

pub const DDNET_NET_EV_NONE: u64 = 0;
pub const DDNET_NET_EV_CONNECT: u64 = 1;
pub const DDNET_NET_EV_CHUNK: u64 = 2;
pub const DDNET_NET_EV_DISCONNECT: u64 = 3;

impl DdnetNet {
    /// Calls the provided function if `NetInner` is `init`, and adjusts the
    /// state machine accordingly. Otherwise, it does nothing.
    ///
    /// Returns `true` if an error has occured during this call or in an
    /// earlier method of `DdnetNet`.
    fn init<F: FnOnce(&mut NetBuilderImpl) -> Result<()>>(
        &mut self,
        f: F,
    ) -> bool {
        let impl_ = match &mut self.0 {
            Init(impl_) => impl_,
            Good(_) => {
                let impl_ = match mem::replace(&mut self.0, Temporary) {
                    Good(impl_) => impl_,
                    _ => unreachable!(),
                };
                self.0 = LaterError(
                    impl_,
                    CString::new("initialization function called after call to `ddnet_net_open`")
                        .unwrap(),
                );
                return true;
            }
            _ => return true,
        };
        if let Err(err) =
            catch_unwind(panic::AssertUnwindSafe(move || f(impl_)))
        {
            let impl_ = match mem::replace(&mut self.0, Temporary) {
                Good(impl_) => impl_,
                _ => unreachable!(),
            };
            self.0 = LaterError(impl_, err);
            true
        } else {
            false
        }
    }
    /// Calls the provided function if `NetInner` is `Good`, and adjusts the
    /// state machine accordingly. Otherwise, it does nothing.
    ///
    /// Returns `true` if an error has occured during this call or in an
    /// earlier method of `DdnetNet`.
    fn good<F: FnOnce(&mut NetImpl) -> Result<()>>(&mut self, f: F) -> bool {
        let mut impl_ = match &mut self.0 {
            Init(_) => {
                self.0 = InitError(
                    CString::new("normal function called before call to `ddnet_net_open`").unwrap(),
                );
                return true;
            }
            Good(impl_) => impl_,
            _ => return true,
        };
        if let Err(err) =
            catch_unwind(panic::AssertUnwindSafe(move || f(&mut impl_)))
        {
            let impl_ = match mem::replace(&mut self.0, Temporary) {
                Good(impl_) => impl_,
                _ => unreachable!(),
            };
            self.0 = LaterError(impl_, err);
            true
        } else {
            false
        }
    }
    fn error(&self) -> Option<(&CStr, usize)> {
        match &self.0 {
            InitError(err) => Some((err, err.as_bytes().len())),
            LaterError(_, err) => Some((err, err.as_bytes().len())),
            _ => None,
        }
    }
}

fn error_to_cstring(err: Error) -> CString {
    CString::new(format!("{}", err)).unwrap_or_else(|_| {
        let message = "error: error string contained nul byte".to_owned();
        CString::from_vec_with_nul(message.into()).unwrap()
    })
}

fn catch_unwind<T, F: FnOnce() -> Result<T> + panic::UnwindSafe>(
    f: F,
) -> result::Result<T, CString> {
    panic::catch_unwind(f)
        .unwrap_or_else(|panic| {
            let msg = match panic.downcast_ref::<&'static str>() {
                Some(s) => *s,
                None => match panic.downcast_ref::<String>() {
                    Some(s) => &s[..],
                    None => "Box<dyn Any>",
                },
            };
            Err(Error::from_string(format!("rust panic: {}", msg)))
        })
        .map_err(error_to_cstring)
}

#[no_mangle]
pub extern "C" fn ddnet_net_ev_kind(ev: &DdnetNetEvent) -> u64 {
    use self::EventImpl::*;
    match unsafe { ev.inner } {
        None => DDNET_NET_EV_NONE,
        Some(Connect(..)) => DDNET_NET_EV_CONNECT,
        Some(Chunk(..)) => DDNET_NET_EV_CHUNK,
        Some(Disconnect(..)) => DDNET_NET_EV_DISCONNECT,
    }
}
/// Can return `nullptr`.
#[no_mangle]
pub extern "C" fn ddnet_net_ev_connect_identity(
    ev: &DdnetNetEvent,
) -> Option<&[u8; 32]> {
    use self::EventImpl::*;
    match unsafe { &ev.inner } {
        Some(Connect(id)) => id.as_ref().map(|id| id.as_bytes()),
        _ => unreachable!(),
    }
}
#[no_mangle]
pub extern "C" fn ddnet_net_ev_chunk_len(ev: &DdnetNetEvent) -> usize {
    use self::EventImpl::*;
    match unsafe { ev.inner } {
        Some(Chunk(len, _)) => len,
        _ => unreachable!(),
    }
}
#[no_mangle]
pub extern "C" fn ddnet_net_ev_chunk_is_unreliable(ev: &DdnetNetEvent) -> bool {
    use self::EventImpl::*;
    match unsafe { ev.inner } {
        Some(Chunk(_, unreliable)) => unreliable,
        _ => unreachable!(),
    }
}
#[no_mangle]
pub extern "C" fn ddnet_net_ev_disconnect_reason_len(
    ev: &DdnetNetEvent,
) -> usize {
    use self::EventImpl::*;
    match unsafe { ev.inner } {
        Some(Disconnect(reason_len, _)) => reason_len,
        _ => unreachable!(),
    }
}
#[no_mangle]
pub extern "C" fn ddnet_net_ev_disconnect_is_remote(
    ev: &DdnetNetEvent,
) -> bool {
    use self::EventImpl::*;
    match unsafe { ev.inner } {
        Some(Disconnect(_, remote)) => remote,
        _ => unreachable!(),
    }
}

#[no_mangle]
pub extern "C" fn ddnet_net_new(net: *mut *mut DdnetNet) -> bool {
    let result = match catch_unwind(|| Ok(NetImpl::builder())) {
        Ok(builder) => Init(builder),
        Err(err) => InitError(err),
    };
    unsafe {
        ptr::write(net, Box::leak(Box::new(DdnetNet(result))));
        (**net).init(|_| Ok(()))
    }
}
#[no_mangle]
pub extern "C" fn ddnet_net_set_bindaddr(
    net: &mut DdnetNet,
    addr: *const c_char,
    addr_len: usize,
) -> bool {
    net.init(|builder| {
        let addr =
            unsafe { slice::from_raw_parts(addr as *const u8, addr_len) };
        let addr = str::from_utf8(addr).unwrap();
        builder.bindaddr(addr.parse().context("ddnet_net_set_bindaddr")?);
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_set_identity(
    net: &mut DdnetNet,
    private_identity: &[u8; 32],
) -> bool {
    net.init(|builder| {
        builder.identity(PrivateIdentity::from_bytes(*private_identity));
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_set_accept_connections(
    net: &mut DdnetNet,
    accept: bool,
) -> bool {
    net.init(|builder| {
        builder.accept_connections(accept);
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_open(net: &mut DdnetNet) -> bool {
    match catch_unwind(panic::AssertUnwindSafe(|| {
        let builder = match mem::replace(&mut net.0, Temporary) {
            Init(builder) => builder,
            Good(impl_) => {
                net.0 = LaterError(
                    impl_,
                    CString::new(
                        "`ddnet_net_open` called on already open instance",
                    )
                    .unwrap(),
                );
                return Ok(());
            }
            _ => return Ok(()),
        };
        net.0 = Good(builder.open()?);
        Ok(())
    })) {
        Ok(()) => {}
        Err(err) => net.0 = InitError(err),
    };
    net.good(|_| Ok(()))
}
#[no_mangle]
pub extern "C" fn ddnet_net_free(net: *mut DdnetNet) {
    unsafe {
        mem::drop(Box::from_raw(net));
    }
}
static NO_ERROR: &str = "no error\0";
#[no_mangle]
pub extern "C" fn ddnet_net_error(net: &DdnetNet) -> *const c_char {
    net.error()
        .map(|(s, _)| s)
        .unwrap_or_else(|| {
            CStr::from_bytes_with_nul(NO_ERROR.as_bytes()).unwrap()
        })
        .as_ptr()
}
#[no_mangle]
pub extern "C" fn ddnet_net_error_len(net: &DdnetNet) -> usize {
    net.error().map(|(_, l)| l).unwrap_or(NO_ERROR.len() - 1)
}

#[no_mangle]
pub extern "C" fn ddnet_net_set_userdata(net: &mut DdnetNet, peer_index: u64, userdata: *mut ()) -> bool {
    net.good(|impl_| {
        impl_.set_userdata(PeerIndex(peer_index), userdata);
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_userdata(net: &mut DdnetNet, peer_index: u64, userdata: &mut *mut ()) -> bool {
    *userdata = 0xbadc0de as *mut ();
    net.good(|impl_| {
        *userdata = impl_.userdata(PeerIndex(peer_index));
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_wait(net: &mut DdnetNet) -> bool {
    net.good(|impl_| {
        impl_.wait();
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_wait_timeout(net: &mut DdnetNet, ns: u64) -> bool {
    net.good(|impl_| {
        impl_.wait_timeout(Instant::now() + Duration::from_nanos(ns));
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_recv(
    net: &mut DdnetNet,
    buf: *mut u8,
    buf_cap: usize,
    peer_index: &mut u64,
    event: &mut DdnetNetEvent,
) -> bool {
    net.good(|impl_| {
        let buf = unsafe { slice::from_raw_parts_mut(buf, buf_cap) };
        let result = impl_.recv(buf)?;
        if let Some((cid, _)) = result {
            *peer_index = cid.0;
        }
        event.inner = result.map(|(_, ev)| ev);
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_send_chunk(
    net: &mut DdnetNet,
    peer_index: u64,
    chunk: *const u8,
    chunk_len: usize,
    unreliable: bool,
) -> bool {
    net.good(|impl_| {
        let chunk = unsafe { slice::from_raw_parts(chunk, chunk_len) };
        impl_.send_chunk(PeerIndex(peer_index), chunk, unreliable)
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_flush(net: &mut DdnetNet, peer_index: u64) -> bool {
    net.good(|impl_| impl_.flush(PeerIndex(peer_index)))
}
#[no_mangle]
pub extern "C" fn ddnet_net_connect(
    net: &mut DdnetNet,
    addr: *const c_char,
    addr_len: usize,
    peer_index: &mut u64,
) -> bool {
    net.good(|impl_| {
        let addr =
            unsafe { slice::from_raw_parts(addr as *const u8, addr_len) };
        let addr = str::from_utf8(addr).unwrap();
        *peer_index = impl_.connect(addr)?.0;
        Ok(())
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_close(
    net: &mut DdnetNet,
    peer_index: u64,
    // may be nullptr
    reason: *const c_char,
    reason_len: usize,
) -> bool {
    net.good(|impl_| {
        let reason = if !reason.is_null() {
            let reason = unsafe {
                slice::from_raw_parts(reason as *const u8, reason_len)
            };
            Some(str::from_utf8(reason).unwrap())
        } else {
            None
        };
        impl_.close(PeerIndex(peer_index), reason)
    })
}
#[no_mangle]
pub extern "C" fn ddnet_net_set_logger(
    log: extern "C" fn(
        level: i32,
        system: *const c_char,
        system_len: usize,
        message: *const c_char,
        message_len: usize,
    ),
) -> bool {
    catch_unwind(|| {
        struct Log(
            extern "C" fn(i32, *const c_char, usize, *const c_char, usize),
        );
        impl log::Log for Log {
            fn enabled(&self, _metadata: &log::Metadata) -> bool {
                true
            }
            fn log(&self, record: &log::Record) {
                use log::Level::*;
                let level = match record.level() {
                    Error => 0,
                    Warn => 1,
                    Info => 2,
                    Debug => 3,
                    Trace => 4,
                };
                let message = record.args().to_string();
                self.0(
                    level,
                    record.target().as_bytes().as_ptr() as *const c_char,
                    record.target().len(),
                    message.as_bytes().as_ptr() as *const c_char,
                    message.len(),
                );
            }
            fn flush(&self) {}
        }
        if log::set_logger(Box::leak(Box::new(Log(log)))).is_err() {
            eprintln!("ddnet_net: failed to set logger");
            return Err(From::from(Error::from_string(String::new())));
        }
        log::set_max_level(log::LevelFilter::Trace);
        info!("set logger");
        Ok(())
    })
    .is_err()
}

#[cfg(test)]
mod test {
    use super::DdnetNetEvent;
    use std::mem;

    #[test]
    fn event_padding_works() {
        let ev = DdnetNetEvent { inner: None };
        unsafe {
            assert!(
                mem::size_of_val(&ev.inner)
                    <= mem::size_of_val(&ev._padding_alignment)
            );
            assert!(
                mem::align_of_val(&ev.inner)
                    <= mem::align_of_val(&ev._padding_alignment)
            );
        }
    }
}
