extern crate arrayvec;

use self::arrayvec::Array;
use self::arrayvec::ArrayVec;
use crate::Buffer;
use crate::BufferRef;
use crate::ToBufferRef;
use std::slice;

/// The intermediate step from a `ArrayVec` to a `BufferRef`.
pub struct ArrayVecBuffer<'data, A: 'data + Array<Item = u8>> {
    // Will only touch the length of the `ArrayVec` through this reference,
    // except in `ArrayVecBuffer::buffer`.
    vec: &'data mut ArrayVec<A>,
    initialized: usize,
}

impl<'d, A: Array<Item = u8>> ArrayVecBuffer<'d, A> {
    fn new(vec: &'d mut ArrayVec<A>) -> ArrayVecBuffer<'d, A> {
        ArrayVecBuffer {
            vec: vec,
            initialized: 0,
        }
    }
    fn buffer<'s>(&'s mut self) -> BufferRef<'d, 's> {
        let len = self.vec.len();
        let remaining = self.vec.capacity() - len;
        unsafe {
            let start = self.vec.as_mut_ptr().offset(len as isize);
            // This is unsafe, we now have two unique (mutable) references
            // to the same `ArrayVec`. However, we will only access
            // `self.vec.len` through `self` and only the contents through
            // the `BufferRef`.
            BufferRef::new(
                slice::from_raw_parts_mut(start, remaining),
                &mut self.initialized,
            )
        }
    }
}

impl<'d, A: Array<Item = u8>> Drop for ArrayVecBuffer<'d, A> {
    fn drop(&mut self) {
        let len = self.vec.len();
        unsafe {
            self.vec.set_len(len + self.initialized);
        }
    }
}

impl<'d, A: Array<Item = u8>> Buffer<'d> for &'d mut ArrayVec<A> {
    type Intermediate = ArrayVecBuffer<'d, A>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        ArrayVecBuffer::new(self)
    }
}

impl<'d, A: Array<Item = u8>> ToBufferRef<'d> for ArrayVecBuffer<'d, A> {
    fn to_buffer_ref<'s>(&'s mut self) -> BufferRef<'d, 's> {
        self.buffer()
    }
}
