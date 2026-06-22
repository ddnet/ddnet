use crate::CUuidManager_Global;
use crate::OFFSET_UUID;
use libtw2_snapshot::format::TypeId;

mod builder;
mod delta;

pub use self::builder::*;
pub use self::delta::*;
pub use self::ffi::CSnapshot;
pub use self::ffi::CSnapshotBuffer;
pub use self::ffi::CSnapshotBuffer_New;

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("engine/shared/snapshot.h");
        include!("generated/protocolglue.h");

        /// Variable-length type containing a single encoded snapshot.
        ///
        /// It can be viewed as a slice of `i32`.
        type CSnapshot;

        /// 64 KiB buffer to write a snapshot to.
        ///
        /// It can be viewed as a slice of `i32`.
        type CSnapshotBuffer;

        // TODO(cxx 1.0.164): #[Self = "CSnapshotBuffer"]
        /// Create a new [`CSnapshotBuffer`].
        ///
        /// See [`CSnapshotBuilder`] for an example.
        pub fn CSnapshotBuffer_New() -> UniquePtr<CSnapshotBuffer>;

        /// View the snapshot as its serialized form.
        fn AsSlice(self: &CSnapshot) -> &[i32];

        /// Get a mutable slice to write a serialized snapshot to.
        fn AsMutSlice(self: Pin<&mut CSnapshotBuffer>) -> &mut [i32];

        /// Convert a 0.6 snapshot object type ID to a 0.7 snapshot object type
        /// ID if the object type has the same layout.
        ///
        /// Returns -1 if there's no object type with the same layout.
        fn Obj_SixToSeven(type_: i32) -> i32;
    }
}

/// Maximum number of items in a snapshot.
pub const SNAPSHOT_MAX_ITEMS: usize = 1024;
/// Maximum size of a snapshot in its serialized form.
pub const SNAPSHOT_MAX_SIZE: usize = 65536; // 64 KiB

fn type_id_from_i32(sixup: bool, mut type_: i32) -> Option<TypeId> {
    if type_ < OFFSET_UUID {
        if sixup {
            if type_ >= 0 {
                type_ = ffi::Obj_SixToSeven(type_);
                if type_ < 0 {
                    return None;
                }
            } else {
                type_ = -type_;
            }
        }
        Some(TypeId::Ordinal(
            type_.try_into().expect("type id must be positive"),
        ))
    } else {
        Some(TypeId::Uuid(CUuidManager_Global().GetUuid(type_).into()))
    }
}
