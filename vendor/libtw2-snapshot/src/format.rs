use crate::snap::Error;
use crate::ReadInt;
use libtw2_buffer::CapacityError;
use libtw2_packer::IntUnpacker;
use libtw2_packer::Packer;
use libtw2_packer::Unpacker;
use libtw2_warn::wrap;
use libtw2_warn::Warn;
use uuid::Uuid;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DeltaDifferingSizes;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Warning {
    Packer(libtw2_packer::Warning),
    NonZeroPadding,
    DuplicateDelete,
    DuplicateUpdate,
    UnknownDelete,
    DeleteUpdate,
    NumUpdatedItems,
    ExcessSnapData,
    ExcessUuidItemData,
}

impl From<libtw2_packer::Warning> for Warning {
    fn from(w: libtw2_packer::Warning) -> Warning {
        Warning::Packer(w)
    }
}

pub use libtw2_gamenet_common::snap_obj::TypeId;

pub const TYPE_ID_EX: u16 = 0;
pub const OFFSET_EXTENDED_TYPE_ID: u16 = 0x4000;

pub fn key_to_raw_type_id(key: i32) -> u16 {
    ((key as u32 >> 16) & 0xffff) as u16
}

pub fn key_to_id(key: i32) -> u16 {
    ((key as u32) & 0xffff) as u16
}

pub fn key(raw_type_id: u16, id: u16) -> i32 {
    (((raw_type_id as u32) << 16) | (id as u32)) as i32
}

pub fn uuid_to_item_data(uuid: Uuid) -> [i32; 4] {
    let mut result = [0; 4];
    for (int, four_bytes) in result.iter_mut().zip(uuid.as_bytes().chunks(4)) {
        let four_bytes: [u8; 4] = four_bytes.try_into().unwrap();
        *int = i32::from_be_bytes(four_bytes);
    }
    result
}

pub fn item_data_to_uuid<W: Warn<Warning>>(warn: &mut W, mut data: &[i32]) -> Option<Uuid> {
    if data.len() > 4 {
        data = &data[..4];
        warn.warn(Warning::ExcessUuidItemData);
    }
    let data: &[i32; 4] = data.try_into().ok()?;

    let mut result = [0; 16];
    for (four_bytes, &int) in result.chunks_mut(4).zip(data) {
        four_bytes.copy_from_slice(&int.to_be_bytes());
    }
    Some(Uuid::from_bytes(result))
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Item<'a> {
    pub type_id: TypeId,
    pub id: u16,
    pub data: &'a [i32],
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct RawItem<'a> {
    pub raw_type_id: u16,
    pub id: u16,
    pub data: &'a [i32],
}

impl<'a> RawItem<'a> {
    pub fn from_key(key: i32, data: &'a [i32]) -> RawItem<'a> {
        RawItem {
            raw_type_id: key_to_raw_type_id(key),
            id: key_to_id(key),
            data: data,
        }
    }
    pub fn key(&self) -> i32 {
        key(self.raw_type_id, self.id)
    }
}

pub struct SnapHeader {
    pub data_size: i32,
    pub num_items: i32,
}

impl SnapHeader {
    pub fn decode<W: Warn<Warning>>(warn: &mut W, p: &mut Unpacker) -> Result<SnapHeader, Error> {
        Ok(SnapHeader {
            data_size: libtw2_packer::positive(p.read_int(wrap(warn))?)?,
            num_items: libtw2_packer::positive(p.read_int(wrap(warn))?)?,
        })
    }
    pub fn decode_obj(p: &mut IntUnpacker) -> Result<SnapHeader, Error> {
        Ok(SnapHeader {
            data_size: libtw2_packer::positive(p.read_int()?)?,
            num_items: libtw2_packer::positive(p.read_int()?)?,
        })
    }
}

#[derive(Clone, Copy, Debug)]
pub struct DeltaHeader {
    pub num_deleted_items: i32,
    pub num_updated_items: i32,
}

impl DeltaHeader {
    pub(crate) fn decode_impl<W: Warn<Warning>, R: ReadInt>(
        warn: &mut W,
        reader: &mut R,
    ) -> Result<DeltaHeader, Error> {
        let result = DeltaHeader {
            num_deleted_items: libtw2_packer::positive(reader.read_int(warn)?)?,
            num_updated_items: libtw2_packer::positive(reader.read_int(warn)?)?,
        };
        if reader.read_int(warn)? != 0 {
            warn.warn(Warning::NonZeroPadding);
        }
        Ok(result)
    }
    pub fn decode<W: Warn<Warning>>(warn: &mut W, p: &mut Unpacker) -> Result<DeltaHeader, Error> {
        DeltaHeader::decode_impl(warn, p)
    }
    pub fn decode_obj<W: Warn<Warning>>(
        warn: &mut W,
        p: &mut IntUnpacker,
    ) -> Result<DeltaHeader, Error> {
        DeltaHeader::decode_impl(warn, p)
    }
    pub fn encode<'d, 's>(&self, mut p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError> {
        for int in self.encode_obj() {
            p.write_int(int)?;
        }
        Ok(p.written())
    }
    pub fn encode_obj(&self) -> [i32; 3] {
        [self.num_deleted_items, self.num_updated_items, 0]
    }
}

/// Applies an item delta, which is a format internal to a snapshot delta.
///
/// `delta` and `out` must be of equal length.
///
/// # Errors
///
/// Returns [`DeltaDifferingSizes`] if `in_`'s length is different from `out.len()`.
///
/// # Panics
///
/// Panics if `delta.len() != out.len()`.
///
/// # Examples
///
/// ```
/// use libtw2_snapshot::format::apply_item_delta;
/// let mut out = [0; 4];
///
/// apply_item_delta(Some(&[0, 1337, 42, 0x7fff_ffff]), &[0, 1, -21, 1], &mut out);
/// assert_eq!(out, [0, 1338, 21, -0x8000_0000]);
///
/// apply_item_delta(None, &[0, 1, -21, 1], &mut out);
/// assert_eq!(out, [0, 1, -21, 1]);
/// ```
pub fn apply_item_delta(
    in_: Option<&[i32]>,
    delta: &[i32],
    out: &mut [i32],
) -> Result<(), DeltaDifferingSizes> {
    assert!(delta.len() == out.len());
    match in_ {
        Some(in_) => {
            if in_.len() != out.len() {
                return Err(DeltaDifferingSizes);
            }
            for i in 0..out.len() {
                out[i] = in_[i].wrapping_add(delta[i]);
            }
        }
        None => out.copy_from_slice(delta),
    }
    Ok(())
}

/// Creates an item delta, which is a format internal to a snapshot delta.
///
/// `delta` and `out` must be of equal length.
///
/// # Errors
///
/// Returns [`DeltaDifferingSizes`] if `in_`'s length is different from `out.len()`.
///
/// # Panics
///
/// Panics if `delta.len() != out.len()`.
///
/// # Examples
///
/// ```
/// use libtw2_snapshot::format::create_item_delta;
/// let mut out = [0; 4];
///
/// create_item_delta(Some(&[0, 1337, 42, 0x7fff_ffff]), &[0, 1338, 21, -0x8000_0000], &mut out);
/// assert_eq!(out, [0, 1, -21, 1]);
///
/// create_item_delta(None, &[0, 1338, 21, -0x8000_0000], &mut out);
/// assert_eq!(out, [0, 1338, 21, -0x8000_0000]);
/// ```
pub fn create_item_delta(
    from: Option<&[i32]>,
    to: &[i32],
    out: &mut [i32],
) -> Result<(), DeltaDifferingSizes> {
    assert!(to.len() == out.len());
    match from {
        Some(from) => {
            if from.len() != to.len() {
                return Err(DeltaDifferingSizes);
            }
            for i in 0..out.len() {
                out[i] = to[i].wrapping_sub(from[i]);
            }
        }
        None => out.copy_from_slice(to),
    }
    Ok(())
}
