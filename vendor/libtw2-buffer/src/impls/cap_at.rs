use crate::Buffer;
use crate::BufferRef;
use crate::CapAtImpl;
use crate::ToBufferRef;
use std::marker::PhantomData;

/// The result of a `cap_at` call on a buffer.
///
/// See its documentation for more info.
pub struct CapAt<'data, T: Buffer<'data>> {
    buf: T,
    cap_at: usize,
    phantom: PhantomData<&'data ()>,
}

impl<'data, T: Buffer<'data>> CapAtImpl<'data> for T {
    fn cap_at_impl(self, len: usize) -> CapAt<'data, Self> {
        CapAt {
            buf: self,
            cap_at: len,
            phantom: PhantomData,
        }
    }
}

/// The intermediate step from a `CapAt` to a `BufferRef`.
pub struct CapAtBuffer<'data, T: ToBufferRef<'data>> {
    intermediate: T,
    cap_at: usize,
    phantom: PhantomData<&'data ()>,
}

impl<'data, T: ToBufferRef<'data>> CapAtBuffer<'data, T> {
    fn new<U: Buffer<'data, Intermediate = T>>(cap_at: CapAt<'data, U>) -> CapAtBuffer<'data, T> {
        CapAtBuffer {
            intermediate: cap_at.buf.to_to_buffer_ref(),
            cap_at: cap_at.cap_at,
            phantom: PhantomData,
        }
    }
    fn buffer<'size>(&'size mut self) -> BufferRef<'data, 'size> {
        self.intermediate.to_buffer_ref().cap_at(self.cap_at)
    }
}

impl<'data, T: Buffer<'data>> Buffer<'data> for CapAt<'data, T> {
    type Intermediate = CapAtBuffer<'data, T::Intermediate>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        CapAtBuffer::new(self)
    }
}

impl<'data, T: ToBufferRef<'data>> ToBufferRef<'data> for CapAtBuffer<'data, T> {
    fn to_buffer_ref<'size>(&'size mut self) -> BufferRef<'data, 'size> {
        self.buffer()
    }
}
