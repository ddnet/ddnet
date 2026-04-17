use crate::wildly_unsafe;
use crate::Buffer;
use crate::BufferRef;
use crate::ToBufferRef;

/// The intermediate step from a `BufferRef` to another `BufferRef`.
pub struct BufferRefBuffer<'ref_, 'data: 'ref_, 'size: 'ref_> {
    // Will only touch the length of the `BufferRef` through this reference,
    // except in `BufferRefBuffer::buffer`.
    buffer: &'ref_ mut BufferRef<'data, 'size>,
    initialized: usize,
}

impl<'r, 'd, 's> BufferRefBuffer<'r, 'd, 's> {
    fn new(buffer: &'r mut BufferRef<'d, 's>) -> BufferRefBuffer<'r, 'd, 's> {
        BufferRefBuffer {
            buffer: buffer,
            initialized: 0,
        }
    }
    fn buffer<'a>(&'a mut self) -> BufferRef<'d, 'a> {
        let len = *self.buffer.initialized_;
        unsafe {
            BufferRef::new(
                wildly_unsafe(&mut self.buffer.buffer[len..]),
                &mut self.initialized,
            )
        }
    }
}

impl<'r, 'd, 's> Drop for BufferRefBuffer<'r, 'd, 's> {
    fn drop(&mut self) {
        *self.buffer.initialized_ += self.initialized;
    }
}

impl<'r, 'd, 's> Buffer<'d> for &'r mut BufferRef<'d, 's> {
    type Intermediate = BufferRefBuffer<'r, 'd, 's>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        BufferRefBuffer::new(self)
    }
}

impl<'r, 'd, 's> ToBufferRef<'d> for BufferRefBuffer<'r, 'd, 's> {
    fn to_buffer_ref<'a>(&'a mut self) -> BufferRef<'d, 'a> {
        self.buffer()
    }
}

#[cfg(test)]
mod test {
    use crate::with_buffer;

    #[test]
    fn buffer_ref_remaining() {
        let a: &mut [u8] = &mut [0; 32];
        with_buffer(a, |mut b| {
            with_buffer(&mut b, |mut c| {
                assert!(c.remaining() == 32);
                c.write(&[0]).unwrap();
                assert!(c.remaining() == 31);
                assert!(c.initialized() == &[0]);
            });
            assert!(b.remaining() == 31);
            with_buffer(&mut b, |mut c| {
                assert!(c.remaining() == 31);
                c.write(&[1]).unwrap();
                assert!(c.remaining() == 30);
                assert!(c.initialized() == &[1]);
            });
            assert!(b.remaining() == 30);
            assert!(b.initialized() == &[0, 1]);
        })
    }
}
