//! `buffer` provides safe, write-only and generics-free byte buffers that can
//! be used without initializing them first.
//!
//! The main trait of this library is `Buffer` that represents a type that can
//! contain uninitialized bytes (such as `Vec`, `ArrayVec`, etc.) and can
//! safely be read into (e.g. using `ReadBuffer`).
//!
//! In order to keep code sizes down, such a type implementing `Buffer` is
//! quickly converted into the struct `BufferRef`, so this is the type
//! receivers of types implementing `Buffer`s should work with.
//!
//! An example for the usage of this trait can be found with `ReadBuffer` which
//! implements reading into a buffer without initializing it first.

#![warn(missing_docs)]

#[macro_use]
extern crate mac;

use std::slice;

pub use impls::arrayvec::ArrayVecBuffer;
pub use impls::buffer_ref::BufferRefBuffer;
pub use impls::cap_at::CapAt;
pub use impls::cap_at::CapAtBuffer;
pub use impls::slice::SliceBuffer;
pub use impls::slice_ref::SliceRefBuffer;
pub use impls::vec::VecBuffer;
pub use traits::read_buffer_ref;
pub use traits::ReadBuffer;
pub use traits::ReadBufferMarker;
pub use traits::ReadBufferRef;

mod impls;
mod traits;

/// An error occuring when too many bytes are being pushed into a buffer.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct CapacityError;

unsafe fn wildly_unsafe<'a, 'b>(slice: &'a mut [u8]) -> &'b mut [u8] {
    slice::from_raw_parts_mut(slice.as_mut_ptr(), slice.len())
}

/// A reference to an uninitialized or partially initialized byte buffer.
///
/// It keeps track of how many bytes (from the start of the buffer) are
/// initialized.
pub struct BufferRef<'data, 'size> {
    buffer: &'data mut [u8],
    initialized_: &'size mut usize,
}

impl<'d, 's> BufferRef<'d, 's> {
    /// Creates a buffer reference from the buffer and a pointer where the
    /// length should be written to.
    ///
    /// Important: `initialized` must initially be zero.
    pub fn new(buffer: &'d mut [u8], initialized: &'s mut usize) -> BufferRef<'d, 's> {
        debug_assert!(*initialized == 0);
        BufferRef {
            buffer: buffer,
            initialized_: initialized,
        }
    }

    /// Advances the split of initialized/uninitialized data by `num_bytes` to
    /// the right.
    pub unsafe fn advance(&mut self, num_bytes: usize) {
        assert!(*self.initialized_ + num_bytes <= self.buffer.len());
        *self.initialized_ += num_bytes;
    }

    /// Writes the bytes yielded by the `bytes` iterator into the buffer.
    ///
    /// If the iterator yields more bytes than the buffer can contain, a
    /// `CapacityError` is returned.
    pub fn extend<I>(&mut self, bytes: I) -> Result<(), CapacityError>
    where
        I: Iterator<Item = u8>,
    {
        let mut buf_iter = (&mut self.buffer[*self.initialized_..]).into_iter();
        for b in bytes {
            *unwrap_or_return!(buf_iter.next(), Err(CapacityError)) = b;
            *self.initialized_ += 1;
        }
        Ok(())
    }

    /// Writes the byte slice into the buffer.
    ///
    /// If the slice contains more bytes than the buffer can contain, a
    /// `CapacityError` is returned.
    pub fn write(&mut self, bytes: &[u8]) -> Result<(), CapacityError> {
        self.extend(bytes.iter().cloned())
    }

    /// Returns the uninitialized part of the buffer.
    pub unsafe fn uninitialized_mut(&mut self) -> &mut [u8] {
        &mut self.buffer[*self.initialized_..]
    }

    /// Consumes the (mutable) buffer reference to produce a slice of the
    /// initialized data that is independent from the `BufferRef` instance.
    pub fn initialized(self) -> &'d [u8] {
        &self.buffer[..*self.initialized_]
    }

    /// Returns the amount of uninitialized bytes that are left.
    pub fn remaining(&self) -> usize {
        self.buffer.len() - *self.initialized_
    }

    fn cap_at(self, index: usize) -> BufferRef<'d, 's> {
        assert!(*self.initialized_ == 0);
        BufferRef {
            buffer: &mut self.buffer[..index],
            initialized_: self.initialized_,
        }
    }
}

/// Convenience function that converts a `T: Buffer` into a `BufferRef`.
pub fn with_buffer<'a, T: Buffer<'a>, F, R>(buffer: T, f: F) -> R
where
    F: for<'b> FnOnce(BufferRef<'a, 'b>) -> R,
{
    let mut intermediate = buffer.to_to_buffer_ref();
    f(intermediate.to_buffer_ref())
}

/// Trait for types that can act as buffer for bytes.
///
/// It should be accepted as trait bound for functions that accept buffers, and
/// should immediately be converted to `BufferRef` using the `with_buffer`
/// function.
pub trait Buffer<'data> {
    /// Intermediate result of converting the `T: Buffer` into a `BufferRef`.
    type Intermediate: ToBufferRef<'data>;
    /// Converts the `T: Buffer` into the intermediate step to `BufferRef`.
    fn to_to_buffer_ref(self) -> Self::Intermediate;
    /// Caps the buffer at the specified byte index.
    ///
    /// This means that no more than `len` bytes will be written to the buffer.
    fn cap_at(self, len: usize) -> CapAt<'data, Self>
    where
        Self: Sized,
    {
        self.cap_at_impl(len)
    }
}

/// Internal trait for the intermediate result of converting a `T: Buffer` into
/// a `BufferRef`.
pub trait ToBufferRef<'data> {
    /// Second step to convert a `T: Buffer` to a `BufferRef`.
    fn to_buffer_ref<'size>(&'size mut self) -> BufferRef<'data, 'size>;
}

trait CapAtImpl<'data>: Buffer<'data> {
    fn cap_at_impl(self, len: usize) -> CapAt<'data, Self>
    where
        Self: Sized;
}
