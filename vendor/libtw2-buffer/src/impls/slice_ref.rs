use crate::wildly_unsafe;
use crate::Buffer;
use crate::BufferRef;
use crate::ToBufferRef;
use std::mem;

/// The intermediate step from a byte slice reference to a `BufferRef`.
pub struct SliceRefBuffer<'data> {
    slice: &'data mut &'data mut [u8],
    initialized: usize,
}

impl<'d> SliceRefBuffer<'d> {
    fn new(slice: &'d mut &'d mut [u8]) -> SliceRefBuffer<'d> {
        SliceRefBuffer {
            slice: slice,
            initialized: 0,
        }
    }
    fn buffer<'s>(&'s mut self) -> BufferRef<'d, 's> {
        unsafe {
            // This is unsafe, we now have two unique (mutable) references to
            // the same `Vec`. However, we will only access `self.vec.len`
            // through `self` and only the contents through the `BufferRef`.
            BufferRef::new(wildly_unsafe(self.slice), &mut self.initialized)
        }
    }
}

impl<'d> Drop for SliceRefBuffer<'d> {
    fn drop(&mut self) {
        let slice = mem::replace(self.slice, &mut []);
        *self.slice = &mut slice[..self.initialized];
    }
}

impl<'d> Buffer<'d> for &'d mut &'d mut [u8] {
    type Intermediate = SliceRefBuffer<'d>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        SliceRefBuffer::new(self)
    }
}

impl<'d> ToBufferRef<'d> for SliceRefBuffer<'d> {
    fn to_buffer_ref<'s>(&'s mut self) -> BufferRef<'d, 's> {
        self.buffer()
    }
}
