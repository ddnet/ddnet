use std::array::TryFromSliceError;
use std::convert::TryInto;
use std::mem;
use zerocopy::byteorder::big_endian;

pub trait TryFromByteSlice {
    fn try_from_byte_slice(bytes: &[u8]) -> Result<&Self, TryFromSliceError>;
}

impl<const N: usize> TryFromByteSlice for [u8; N] {
    fn try_from_byte_slice(bytes: &[u8]) -> Result<&[u8; N], TryFromSliceError> {
        bytes.try_into()
    }
}

pub trait ByteArray {
    type ByteArray: AsRef<[u8]> + TryFromByteSlice;
}

pub trait AsBytesExt: ByteArray + zerocopy::AsBytes {
    fn as_byte_array(&self) -> &Self::ByteArray {
        TryFromByteSlice::try_from_byte_slice(self.as_bytes()).unwrap()
    }
}

pub trait FromBytesExt: ByteArray + zerocopy::FromBytes {
    fn ref_and_rest_from(bytes: &[u8]) -> Option<(&Self, &[u8])>
    where
        Self: Sized,
    {
        if bytes.len() < mem::size_of::<Self>() {
            return None;
        }
        let (result, rest) = bytes.split_at(mem::size_of::<Self>());
        Some((Self::ref_from(result).unwrap(), rest))
    }
    fn ref_from_array(bytes: &Self::ByteArray) -> &Self
    where
        Self: Sized,
    {
        Self::ref_from(bytes.as_ref()).unwrap()
    }
    fn from_array(bytes: Self::ByteArray) -> Self
    where
        Self: Copy + Sized,
    {
        *Self::ref_from_array(&bytes)
    }
}

impl<T: ByteArray + zerocopy::AsBytes> AsBytesExt for T {}
impl<T: ByteArray + zerocopy::FromBytes> FromBytesExt for T {}

boilerplate_packed_internal!(big_endian::U32, 4, test_be_u32_size);
