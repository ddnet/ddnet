use self::read_int::ReadInt;
use libtw2_common::num::Cast;
use std::ops;

pub mod format;
pub mod manager;
pub mod receiver;
pub mod snap;
pub mod storage;

pub use self::manager::Manager;
pub use self::receiver::DeltaReceiver;
pub use self::receiver::ReceivedDelta;
pub use self::snap::Delta;
pub use self::snap::Snap;
pub use self::storage::Storage;

mod read_int;

fn to_usize(r: ops::Range<u32>) -> ops::Range<usize> {
    r.start.usize()..r.end.usize()
}
