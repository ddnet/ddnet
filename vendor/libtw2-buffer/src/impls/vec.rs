use crate::Buffer;
use crate::BufferRef;
use crate::ToBufferRef;
use std::slice;

/// The intermediate step from a `Vec` to a `BufferRef`.
pub struct VecBuffer<'data> {
    // Will only touch the length of the `Vec` through this reference, except
    // in `VecBuffer::buffer`.
    vec: &'data mut Vec<u8>,
    initialized: usize,
}

impl<'data> VecBuffer<'data> {
    fn new(vec: &'data mut Vec<u8>) -> VecBuffer<'data> {
        VecBuffer {
            vec: vec,
            initialized: 0,
        }
    }
    fn buffer<'size>(&'size mut self) -> BufferRef<'data, 'size> {
        let len = self.vec.len();
        let remaining = self.vec.capacity() - len;
        unsafe {
            let start = self.vec.as_mut_ptr().offset(len as isize);
            // This is unsafe, we now have two unique (mutable) references to
            // the same `Vec`. However, we will only access `self.vec.len`
            // through `self` and only the contents through the `BufferRef`.
            BufferRef::new(
                slice::from_raw_parts_mut(start, remaining),
                &mut self.initialized,
            )
        }
    }
}

impl<'data> Drop for VecBuffer<'data> {
    fn drop(&mut self) {
        let len = self.vec.len();
        unsafe {
            self.vec.set_len(len + self.initialized);
        }
    }
}

impl<'data> Buffer<'data> for &'data mut Vec<u8> {
    type Intermediate = VecBuffer<'data>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        VecBuffer::new(self)
    }
}

impl<'data> ToBufferRef<'data> for VecBuffer<'data> {
    fn to_buffer_ref<'size>(&'size mut self) -> BufferRef<'data, 'size> {
        self.buffer()
    }
}
