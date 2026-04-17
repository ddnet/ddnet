use crate::CSnapshot;
use crate::CSnapshotBuffer;
use libtw2_packer::IntUnpacker;
use libtw2_snapshot::format as snap_format;
use libtw2_snapshot::snap;
use libtw2_snapshot::snap::RawSnap;
use libtw2_warn as warn;
use std::mem;
use std::pin::Pin;

#[allow(unused_must_use)] // in generated code for CSnapshotDelta_New
#[cxx::bridge]
mod ffi {
    extern "C++" {
        include!("engine/shared/snapshot.h");

        type CSnapshot = super::CSnapshot;
        type CSnapshotBuffer = super::CSnapshotBuffer;
    }
    extern "Rust" {
        type CSnapshotDelta;

        // TODO(cxx 1.0.164): #[Self = "CSnapshotDelta"]
        pub fn CSnapshotDelta_DiffItem(past: &[i32], current: &[i32], out: &mut [i32]);
        /// Create a new snapshot delta.
        ///
        /// # Example
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// use ddnet_engine_shared::CSnapshotDelta_New;
        ///
        /// let delta = CSnapshotDelta_New();
        /// ```
        // TODO(cxx 1.0.164): #[Self = "CSnapshotDelta"]
        pub fn CSnapshotDelta_New() -> Box<CSnapshotDelta>;
        pub fn Clone(&mut self) -> Box<CSnapshotDelta>;
        pub fn GetDataRate(&self, type_: i32) -> u64;
        pub fn GetDataUpdates(&self, type_: i32) -> u64;
        pub fn SetStaticsize(&mut self, type_: i32, size: usize);
        pub fn EmptyDelta(&self) -> &[i32];
        pub fn CreateDelta(&mut self, from: &CSnapshot, to: &CSnapshot, delta: &mut [i32]) -> i32;
        pub fn UnpackDelta(
            &mut self,
            from: &CSnapshot,
            to: Pin<&mut CSnapshotBuffer>,
            delta: &[i32],
        ) -> i32;
    }
}

/// An object allowing you to operate on snapshot deltas.
///
/// It can diff two snapshotts to create a snapshot delta
/// ([`CSnapshotDelta::CreateDelta`]), use a snapshot and a delta to
/// reconstruct a newer snapshot ([`CSnapshotDelta::UnpackDelta`]).
///
/// This requires that both sender and receiver have set the same static item
/// item sizes ([`CSnapshotDelta::SetStaticsize`]).
#[derive(Clone, Default)]
pub struct CSnapshotDelta {
    static_sizes: Vec<u16>,
    buf: Buffer,
}

#[derive(Clone, Default)]
struct Buffer {
    snap_from: RawSnap,
    snap_to: RawSnap,
    delta: snap::Delta,
    write: Vec<i32>,
}

/// Diffs two snapshot items of the same size using the Teeworlds snapshot
/// delta algorithm.
///
/// # Panics
///
/// Panics if any of the passed slices differ in length.
///
/// # Example
///
/// ```
/// # extern crate ddnet_test;
/// use ddnet_engine_shared::CSnapshotDelta_DiffItem;
///
/// let past = [123];
/// let current = [456];
/// let mut result = [0];
///
/// CSnapshotDelta_DiffItem(&past, &current, &mut result);
/// ```
pub fn CSnapshotDelta_DiffItem(past: &[i32], current: &[i32], out: &mut [i32]) {
    snap_format::create_item_delta(Some(past), current, out)
        .expect("can't have `past` and `out` of different sizes")
}

/// Creates a new [`CSnapshotDelta`].
///
/// It still needs to be fed with static item sizes
/// ([`CSnapshotDelta::SetStaticsize`]).
///
/// # Example
///
/// ```
/// # extern crate ddnet_test;
/// use ddnet_engine_shared::CSnapshotDelta_New;
///
/// let delta = CSnapshotDelta_New();
/// ```
pub fn CSnapshotDelta_New() -> Box<CSnapshotDelta> {
    Box::new(CSnapshotDelta::default())
}

impl CSnapshotDelta {
    /// Creates a new [`CSnapshotDelta`] from an existing one.
    pub fn Clone(&self) -> Box<CSnapshotDelta> {
        Box::new(self.clone())
    }

    #[allow(missing_docs)] // not implemented
    pub fn GetDataRate(&self, type_: i32) -> u64 {
        let _ = type_;
        // TODO
        0
    }

    #[allow(missing_docs)] // not implemented
    pub fn GetDataUpdates(&self, type_: i32) -> u64 {
        let _ = type_;
        // TODO
        0
    }

    /// Tells the snapshot delta algorithm that it can assume a specific item
    /// size for a type.
    ///
    /// This function must be called with increasing `type_`, starting with 0.
    /// The `size` must be the size in bytes of the item, **not** a count of
    /// `i32`s. Pass `0` to indicate that a given item type doesn't have a
    /// known static size.
    ///
    /// Both sender and receiver must register the same static item sizes for
    /// the created snapshot deltas to be intelligible to each other.
    ///
    /// # Panics
    ///
    /// If the types aren't registered in order (see above) or if the item size
    /// is not divisible by 4.
    ///
    /// # Example
    ///
    /// ```
    /// # extern crate ddnet_test;
    /// use ddnet_engine_shared::CSnapshotDelta_New;
    ///
    /// let mut delta = CSnapshotDelta_New();
    /// delta.SetStaticsize(0, 0); // no known size
    /// delta.SetStaticsize(1, 40); // NETOBJTYPE_PLAYERINPUT, a snapshot object, for whatever reason
    /// ```
    pub fn SetStaticsize(&mut self, type_: i32, size: usize) {
        let type_: usize = type_.try_into().unwrap();
        assert!(type_ <= self.static_sizes.len());
        assert!(size % 4 == 0);
        let i32_len = u16::try_from(size).unwrap() / 4;
        if type_ < self.static_sizes.len() {
            assert!(self.static_sizes[type_] == i32_len);
        } else {
            self.static_sizes.push(i32_len);
        }
    }

    /// Returns the representation of an empty delta.
    ///
    /// I.e. the delta between a snapshot and itself.
    ///
    /// # Example
    ///
    /// ```
    /// # extern crate ddnet_test;
    /// use ddnet_engine_shared::CSnapshotDelta_New;
    ///
    /// let delta = CSnapshotDelta_New();
    /// assert_eq!(delta.EmptyDelta(), &[0, 0, 0]);
    /// ```
    pub fn EmptyDelta(&self) -> &[i32] {
        &[0; 3]
    }

    /// Diffs two snapshots to create a delta.
    ///
    /// Returns the number of bytes written to the `delta` slice, or `-1` if
    /// delta creation failed.
    pub fn CreateDelta(&mut self, from: &CSnapshot, to: &CSnapshot, delta: &mut [i32]) -> i32 {
        self.buf
            .snap_from
            .read_from_ints(&mut warn::Panic, from.AsSlice())
            .expect("incoming snap must be valid");
        self.buf
            .snap_to
            .read_from_ints(&mut warn::Panic, to.AsSlice())
            .expect("incoming snap must be valid");
        self.buf
            .delta
            .create_raw(&self.buf.snap_from, &self.buf.snap_to);
        match self
            .buf
            .delta
            .write_to_ints(|type_| obj_size(&self.static_sizes, type_), delta)
        {
            Ok(written) => (written.len() * mem::size_of::<i32>()).try_into().unwrap(),
            Err(_) => -1,
        }
    }

    /// Recreates a snapshot from a base snapshot and a delta.
    ///
    /// Returns the number of bytes written to `to`, or `-1` on error.
    pub fn UnpackDelta(
        &mut self,
        from: &CSnapshot,
        to: Pin<&mut CSnapshotBuffer>,
        delta: &[i32],
    ) -> i32 {
        let mut delta = IntUnpacker::new(delta);
        if self
            .buf
            .delta
            .read_from_ints(
                &mut warn::Ignore,
                |type_| obj_size(&self.static_sizes, type_),
                &mut delta,
            )
            .is_err()
        {
            return -1;
        }
        self.buf
            .snap_from
            .read_from_ints(&mut warn::Panic, from.AsSlice())
            .expect("incoming snap must be valid");
        if self
            .buf
            .snap_to
            .read_with_delta(&mut warn::Ignore, &self.buf.snap_from, &self.buf.delta)
            .is_err()
        {
            return -1;
        }
        match self
            .buf
            .snap_to
            .write_to_ints(&mut self.buf.write, to.AsMutSlice())
        {
            Ok(written) => (written.len() * mem::size_of::<i32>()).try_into().unwrap(),
            Err(_) => -1,
        }
    }
}

fn obj_size(static_sizes: &[u16], type_: u16) -> Option<u32> {
    let type_: usize = type_.into();
    if type_ > static_sizes.len() {
        return None;
    }
    if static_sizes[type_] == 0 {
        return None;
    }
    Some(static_sizes[type_].into())
}
