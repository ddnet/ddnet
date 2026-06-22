use crate::format::apply_item_delta;
use crate::format::create_item_delta;
use crate::format::item_data_to_uuid;
use crate::format::key;
use crate::format::key_to_id;
use crate::format::key_to_raw_type_id;
use crate::format::uuid_to_item_data;
use crate::format::DeltaDifferingSizes;
use crate::format::DeltaHeader;
use crate::format::Item;
use crate::format::RawItem;
use crate::format::SnapHeader;
use crate::format::TypeId;
use crate::format::Warning;
use crate::format::OFFSET_EXTENDED_TYPE_ID;
use crate::format::TYPE_ID_EX;
use crate::to_usize;
use crate::ReadInt;
use libtw2_buffer::CapacityError;
use libtw2_common::num::Cast;
use libtw2_gamenet_snap as msg;
use libtw2_gamenet_snap::SnapMsg;
use libtw2_gamenet_snap::MAX_SNAPSHOT_PACKSIZE;
use libtw2_packer::IntUnpacker;
use libtw2_packer::Packer;
use libtw2_packer::UnexpectedEnd;
use libtw2_packer::Unpacker;
use libtw2_warn::wrap;
use libtw2_warn::Ignore;
use libtw2_warn::Warn;
use std::cmp;
use std::collections::btree_map;
use std::collections::BTreeMap;
use std::collections::BTreeSet;
use std::fmt;
use std::iter;
use std::mem;
use std::ops;
use uuid::Uuid;

pub const MAX_SNAPSHOT_SIZE: usize = 64 * 1024; // 64 KB
pub const MAX_SNAPSHOT_ITEMS: usize = 1024;

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Error {
    UnexpectedEnd,
    IntOutOfRange,
    DeletedItemsUnpacking,
    ItemDiffsUnpacking,
    TypeIdRange,
    IdRange,
    NegativeSize,
    TooLongDiff,
    TooLongSnap,
    TooManyItems,
    DeltaDifferingSizes,
    OffsetsUnpacking,
    InvalidOffset,
    ItemsUnpacking,
    DuplicateKey,
    DuplicateUuidType,
    InvalidUuidType,
    MissingUuidType,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BuilderError {
    DuplicateKey,
    TooLongSnap,
    TooManyItems,
}

impl From<BuilderError> for Error {
    fn from(err: BuilderError) -> Error {
        match err {
            BuilderError::DuplicateKey => Error::DuplicateKey,
            BuilderError::TooLongSnap => Error::TooLongSnap,
            BuilderError::TooManyItems => Error::TooManyItems,
        }
    }
}

impl From<DeltaDifferingSizes> for Error {
    fn from(DeltaDifferingSizes: DeltaDifferingSizes) -> Error {
        Error::DeltaDifferingSizes
    }
}

impl From<libtw2_packer::IntOutOfRange> for Error {
    fn from(_: libtw2_packer::IntOutOfRange) -> Error {
        Error::IntOutOfRange
    }
}

impl From<UnexpectedEnd> for Error {
    fn from(UnexpectedEnd: UnexpectedEnd) -> Error {
        Error::UnexpectedEnd
    }
}

#[derive(Clone, Default)]
pub struct RawSnap {
    offsets: BTreeMap<i32, ops::Range<u32>>,
    buf: Vec<i32>,
}

impl RawSnap {
    pub fn empty() -> RawSnap {
        Default::default()
    }
    fn clear(&mut self) {
        self.offsets.clear();
        self.buf.clear();
    }
    fn item_from_offset(&self, offset: ops::Range<u32>) -> &[i32] {
        &self.buf[to_usize(offset)]
    }
    pub fn item(&self, raw_type_id: u16, id: u16) -> Option<&[i32]> {
        self.offsets
            .get(&key(raw_type_id, id))
            .map(|o| &self.buf[to_usize(o.clone())])
    }
    pub fn items(&self) -> RawItems<'_> {
        RawItems {
            snap: self,
            iter: self.offsets.iter(),
        }
    }
    fn serialized_ints_size(num_items: usize, num_item_data_i32s: usize) -> usize {
        // snapshot:
        //     [ 4] data_size
        //     [ 4] num_items
        //     [*4] item_offsets
        //     [  ] items
        //
        // item:
        //     [ 2] id
        //     [ 2] type_id
        //     [  ] data
        mem::size_of::<i32>() * (2 + num_items + num_items + num_item_data_i32s)
    }
    fn prepare_item_vacant<'a>(
        num_items: usize,
        entry: btree_map::VacantEntry<'a, i32, ops::Range<u32>>,
        buf: &mut Vec<i32>,
        size: usize,
    ) -> Result<&'a mut ops::Range<u32>, BuilderError> {
        let offset = buf.len();
        if num_items + 1 > MAX_SNAPSHOT_ITEMS {
            return Err(BuilderError::TooManyItems);
        }
        if RawSnap::serialized_ints_size(num_items + 1, offset + size) > MAX_SNAPSHOT_SIZE {
            return Err(BuilderError::TooLongSnap);
        }
        let start = offset.assert_u32();
        let end = (offset + size).assert_u32();
        buf.extend(iter::repeat(0).take(size));
        Ok(entry.insert(start..end))
    }
    fn add_item_uninitialized(
        &mut self,
        raw_type_id: u16,
        id: u16,
        size: usize,
    ) -> Result<&mut [i32], BuilderError> {
        let num_items = self.offsets.len();
        let offset = match self.offsets.entry(key(raw_type_id, id)) {
            btree_map::Entry::Occupied(..) => return Err(BuilderError::DuplicateKey),
            btree_map::Entry::Vacant(v) => {
                RawSnap::prepare_item_vacant(num_items, v, &mut self.buf, size)?
            }
        }
        .clone();
        Ok(&mut self.buf[to_usize(offset)])
    }
    fn add_item(&mut self, raw_type_id: u16, id: u16, data: &[i32]) -> Result<(), BuilderError> {
        self.add_item_uninitialized(raw_type_id, id, data.len())?
            .copy_from_slice(data);
        Ok(())
    }
    fn prepare_item(
        &mut self,
        raw_type_id: u16,
        id: u16,
        size: usize,
    ) -> Result<&mut [i32], Error> {
        let num_items = self.offsets.len();
        let offset = match self.offsets.entry(key(raw_type_id, id)) {
            btree_map::Entry::Occupied(o) => o.into_mut(),
            btree_map::Entry::Vacant(v) => {
                RawSnap::prepare_item_vacant(num_items, v, &mut self.buf, size)?
            }
        }
        .clone();
        Ok(&mut self.buf[to_usize(offset)])
    }
    pub fn read<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        buf: &mut Vec<i32>,
        data: &[u8],
    ) -> Result<(), Error> {
        self.clear();
        buf.clear();

        let mut unpacker = Unpacker::new(data);
        while !unpacker.is_empty() {
            match unpacker.read_int(wrap(warn)) {
                Ok(int) => buf.push(int),
                Err(UnexpectedEnd) => {
                    warn.warn(Warning::ExcessSnapData);
                    break;
                }
            }
        }

        self.read_from_ints(warn, &buf)
    }
    pub fn read_from_ints<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        data: &[i32],
    ) -> Result<(), Error> {
        self.clear();

        let mut unpacker = IntUnpacker::new(data);
        let header = SnapHeader::decode_obj(&mut unpacker)?;
        let data = unpacker.as_slice();

        let offsets_len = header.num_items.assert_usize();
        if data.len() < offsets_len {
            return Err(Error::OffsetsUnpacking);
        }
        if header.data_size % 4 != 0 {
            return Err(Error::InvalidOffset);
        }
        let items_len = (header.data_size / 4).assert_usize();
        match (offsets_len + items_len).cmp(&data.len()) {
            cmp::Ordering::Less => warn.warn(Warning::ExcessSnapData),
            cmp::Ordering::Equal => {}
            cmp::Ordering::Greater => return Err(Error::ItemsUnpacking),
        }

        let (offsets, item_data) = data.split_at(offsets_len);
        let item_data = &item_data[..items_len];

        let mut offsets = offsets.iter();
        let mut prev_offset = None;
        loop {
            let offset = offsets.next().copied();
            if let Some(offset) = offset {
                if offset < 0 {
                    return Err(Error::InvalidOffset);
                }
                if offset % 4 != 0 {
                    return Err(Error::InvalidOffset);
                }
            }
            let finished = offset.is_none();
            let offset = offset.map(|o| o.assert_usize() / 4).unwrap_or(items_len);

            if let Some(prev_offset) = prev_offset {
                if offset <= prev_offset {
                    return Err(Error::InvalidOffset);
                }
                if offset > items_len {
                    return Err(Error::InvalidOffset);
                }
                let raw_type_id = key_to_raw_type_id(item_data[prev_offset]);
                let id = key_to_id(item_data[prev_offset]);
                self.add_item(raw_type_id, id, &item_data[prev_offset + 1..offset])?;
            } else if offset != 0 {
                // First offset must be 0.
                return Err(Error::InvalidOffset);
            }

            prev_offset = Some(offset);

            if finished {
                break;
            }
        }
        Ok(())
    }
    pub fn read_with_delta<W>(
        &mut self,
        warn: &mut W,
        from: &RawSnap,
        delta: &Delta,
    ) -> Result<(), Error>
    where
        W: Warn<Warning>,
    {
        self.clear();

        let mut num_deletions = 0;
        for item in from.items() {
            if !delta.deleted_items.contains(&item.key()) {
                let out = self.prepare_item(item.raw_type_id, item.id, item.data.len())?;
                out.copy_from_slice(item.data);
            } else {
                num_deletions += 1;
            }
        }
        if num_deletions != delta.deleted_items.len() {
            warn.warn(Warning::UnknownDelete);
        }

        for (&key, offset) in &delta.updated_items {
            let raw_type_id = key_to_raw_type_id(key);
            let id = key_to_id(key);
            let diff = &delta.buf[to_usize(offset.clone())];
            let out = self.prepare_item(raw_type_id, id, diff.len())?;
            let in_ = from.item(raw_type_id, id);

            apply_item_delta(in_, diff, out)?;
        }
        Ok(())
    }
    fn write_impl<F: FnMut(i32) -> Result<(), CapacityError>>(
        &self,
        buf: &mut Vec<i32>,
        mut write_int: F,
    ) -> Result<(), CapacityError> {
        assert!(self.offsets.len() <= MAX_SNAPSHOT_ITEMS);
        let mut written = 0;
        let mut write_int = |i| {
            written += mem::size_of::<i32>();
            write_int(i)
        };
        let keys = buf;
        keys.clear();
        keys.extend(self.offsets.keys().cloned());
        keys.sort_unstable_by_key(|&k| k as u32);
        let data_size = self
            .buf
            .len()
            .checked_add(self.offsets.len())
            .expect("snap size overflow")
            .checked_mul(mem::size_of::<i32>())
            .expect("snap size overflow")
            .assert_i32();
        write_int(data_size)?;
        let num_items = self.offsets.len().assert_i32();
        write_int(num_items)?;

        let mut offset = 0;
        for &key in &*keys {
            write_int(offset)?;
            let key_offset = self.offsets[&key].clone();
            offset = offset
                .checked_add(
                    (key_offset.end - key_offset.start + 1)
                        .usize()
                        .checked_mul(mem::size_of::<i32>())
                        .expect("item size overflow")
                        .assert_i32(),
                )
                .expect("offset overflow");
        }
        for &key in &*keys {
            write_int(key)?;
            for &i in &self.buf[to_usize(self.offsets[&key].clone())] {
                write_int(i)?;
            }
        }
        assert!(written <= MAX_SNAPSHOT_SIZE);
        Ok(())
    }
    pub fn write<'d, 's>(
        &self,
        buf: &mut Vec<i32>,
        mut p: Packer<'d, 's>,
    ) -> Result<&'d [u8], CapacityError> {
        self.write_impl(buf, |int| p.write_int(int))?;
        Ok(p.written())
    }
    pub fn write_to_ints<'a>(
        &self,
        buf: &mut Vec<i32>,
        result: &'a mut [i32],
    ) -> Result<&'a [i32], CapacityError> {
        let mut iter = result.iter_mut();
        self.write_impl(buf, |int| {
            *iter.next().ok_or(CapacityError)? = int;
            Ok(())
        })?;
        let remaining = iter.len();
        let len = result.len() - remaining;
        Ok(&result[..len])
    }
    pub fn crc(&self) -> i32 {
        self.buf.iter().fold(0, |s, &a| s.wrapping_add(a))
    }
    pub fn recycle(mut self) -> RawBuilder {
        self.clear();
        RawBuilder { snap: self }
    }
}

fn read_int_err<R: ReadInt, W: Warn<Warning>>(
    reader: &mut R,
    w: &mut W,
    e: Error,
) -> Result<i32, Error> {
    reader.read_int(w).map_err(|_| e)
}

pub struct RawItems<'a> {
    snap: &'a RawSnap,
    iter: btree_map::Iter<'a, i32, ops::Range<u32>>,
}

impl<'a> Iterator for RawItems<'a> {
    type Item = RawItem<'a>;
    fn next(&mut self) -> Option<RawItem<'a>> {
        self.iter
            .next()
            .map(|(&k, o)| RawItem::from_key(k, self.snap.item_from_offset(o.clone())))
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a> ExactSizeIterator for RawItems<'a> {
    fn len(&self) -> usize {
        self.iter.len()
    }
}

impl fmt::Debug for RawSnap {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_map()
            .entries(self.items().map(
                |RawItem {
                     raw_type_id,
                     id,
                     data,
                 }| ((raw_type_id, id), data),
            ))
            .finish()
    }
}

#[derive(Clone, Default)]
pub struct Snap {
    raw: RawSnap,
    extended_types: BTreeMap<Uuid, u16>,
}

impl Snap {
    pub fn empty() -> Snap {
        Default::default()
    }
    fn raw_type_id(&self, type_id: TypeId) -> Option<u16> {
        match type_id {
            TypeId::Ordinal(ordinal) => {
                assert!(0 < ordinal && ordinal < OFFSET_EXTENDED_TYPE_ID);
                Some(ordinal)
            }
            TypeId::Uuid(uuid) => self.extended_types.get(&uuid).copied(),
        }
    }
    fn type_id(&self, raw_type_id: u16) -> Option<TypeId> {
        if raw_type_id == TYPE_ID_EX {
            None
        } else if raw_type_id < OFFSET_EXTENDED_TYPE_ID {
            Some(TypeId::Ordinal(raw_type_id))
        } else {
            // `build_from_raw()` should have checked the UUID type IDs.
            Some(TypeId::Uuid(item_data_to_uuid(
                &mut Ignore,
                self.raw.item(TYPE_ID_EX, raw_type_id).unwrap(),
            )?))
        }
    }
    pub fn item(&self, type_id: TypeId, id: u16) -> Option<&[i32]> {
        self.raw.item(self.raw_type_id(type_id)?, id)
    }
    pub fn items(&self) -> Items<'_> {
        let raw = self.raw.items();
        let remaining = self.raw.items().len() - self.extended_types.len();
        Items {
            raw,
            snap: self,
            remaining,
        }
    }
    fn build_from_raw<W: Warn<Warning>>(&mut self, warn: &mut W) -> Result<(), Error> {
        self.extended_types.clear();
        let mut prev_checked_raw_type_id = None;
        for (&item_key, offset) in &self.raw.offsets {
            let raw_type_id = key_to_raw_type_id(item_key);
            if raw_type_id == TYPE_ID_EX {
                let item_data = self.raw.item_from_offset(offset.clone());
                let uuid = item_data_to_uuid(warn, item_data).ok_or(Error::InvalidUuidType)?;
                if self.extended_types.insert(uuid, raw_type_id).is_some() {
                    return Err(Error::DuplicateUuidType);
                }
            } else if raw_type_id >= OFFSET_EXTENDED_TYPE_ID {
                if Some(raw_type_id) == prev_checked_raw_type_id {
                    continue;
                }
                if self
                    .raw
                    .offsets
                    .get(&key(TYPE_ID_EX, raw_type_id))
                    .is_none()
                {
                    return Err(Error::MissingUuidType);
                }
                prev_checked_raw_type_id = Some(raw_type_id);
            }
        }
        Ok(())
    }
    pub fn read<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        buf: &mut Vec<i32>,
        data: &[u8],
    ) -> Result<(), Error> {
        self.raw.read(warn, buf, data)?;
        self.build_from_raw(warn)?;
        Ok(())
    }
    pub fn read_from_ints<W: Warn<Warning>>(
        &mut self,
        warn: &mut W,
        data: &[i32],
    ) -> Result<(), Error> {
        self.raw.read_from_ints(warn, data)?;
        self.build_from_raw(warn)?;
        Ok(())
    }
    pub fn read_with_delta<W>(
        &mut self,
        warn: &mut W,
        from: &Snap,
        delta: &Delta,
    ) -> Result<(), Error>
    where
        W: Warn<Warning>,
    {
        self.raw.read_with_delta(warn, &from.raw, delta)?;
        self.build_from_raw(warn)?;
        Ok(())
    }
    pub fn write<'d, 's>(
        &self,
        buf: &mut Vec<i32>,
        p: Packer<'d, 's>,
    ) -> Result<&'d [u8], CapacityError> {
        self.raw.write(buf, p)
    }
    pub fn write_to_ints<'a>(
        &self,
        buf: &mut Vec<i32>,
        result: &'a mut [i32],
    ) -> Result<&'a [i32], CapacityError> {
        self.raw.write_to_ints(buf, result)
    }
    pub fn crc(&self) -> i32 {
        self.raw.crc()
    }
    /// Recycle the snap to build another one.
    ///
    /// This remembers the extended item types inserted into the snap, to keep
    /// the snapshot delta smaller.
    pub fn recycle(mut self) -> Builder {
        let mut next_type_id = OFFSET_EXTENDED_TYPE_ID;
        for &key in self.raw.offsets.keys() {
            let raw_type_id = key_to_raw_type_id(key);
            let id = key_to_id(key);
            const _: () = assert!(TYPE_ID_EX == 0);
            if raw_type_id != TYPE_ID_EX {
                break;
            }
            // Make sure we'll have space for at least 256 additional extended types.
            if id < next_type_id + 256 {
                next_type_id = id + 1;
            }
        }
        self.raw.clear();
        for (&uuid, &raw_type_id) in &self.extended_types {
            // It fit last time, it's going to fit this time.
            self.raw
                .add_item(TYPE_ID_EX, raw_type_id, &uuid_to_item_data(uuid))
                .unwrap();
        }
        Builder {
            snap: self,
            next_type_id,
        }
    }
}

pub struct Items<'a> {
    raw: RawItems<'a>,
    snap: &'a Snap,
    remaining: usize,
}

impl<'a> Iterator for Items<'a> {
    type Item = Item<'a>;
    fn next(&mut self) -> Option<Item<'a>> {
        loop {
            match self.raw.next() {
                None => return None,
                Some(RawItem {
                    raw_type_id,
                    id,
                    data,
                }) => {
                    if let Some(type_id) = self.snap.type_id(raw_type_id) {
                        self.remaining -= 1;
                        return Some(Item { type_id, id, data });
                    } else {
                        // Skip items with ill-defined types.
                        continue;
                    }
                }
            }
        }
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.remaining, Some(self.remaining))
    }
}

impl<'a> ExactSizeIterator for Items<'a> {
    fn len(&self) -> usize {
        self.remaining
    }
}

impl fmt::Debug for Snap {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_map()
            .entries(
                self.items()
                    .map(|Item { type_id, id, data }| ((type_id, id), data)),
            )
            .finish()
    }
}

#[derive(Clone, Default)]
pub struct Delta {
    deleted_items: BTreeSet<i32>,
    updated_items: BTreeMap<i32, ops::Range<u32>>,
    buf: Vec<i32>,
}

impl Delta {
    pub fn new() -> Delta {
        Default::default()
    }
    pub fn clear(&mut self) {
        self.deleted_items.clear();
        self.updated_items.clear();
        self.buf.clear();
    }
    fn prepare_update_item(&mut self, raw_type_id: u16, id: u16, size: usize) -> &mut [i32] {
        let key = key(raw_type_id, id);

        let offset = self.buf.len();
        let start = offset.assert_u32();
        let end = (offset + size).assert_u32();
        self.buf.extend(iter::repeat(0).take(size));
        assert!(self.updated_items.insert(key, start..end).is_none());
        &mut self.buf[to_usize(start..end)]
    }
    pub fn create(&mut self, from: &Snap, to: &Snap) {
        self.create_raw(&from.raw, &to.raw)
    }
    pub fn create_raw(&mut self, from: &RawSnap, to: &RawSnap) {
        self.clear();
        for RawItem {
            raw_type_id, id, ..
        } in from.items()
        {
            if to.item(raw_type_id, id).is_none() {
                assert!(self.deleted_items.insert(key(raw_type_id, id)));
            }
        }
        for RawItem {
            raw_type_id,
            id,
            data,
        } in to.items()
        {
            let from_data = from.item(raw_type_id, id);
            let out_delta = self.prepare_update_item(raw_type_id, id, data.len());
            create_item_delta(from_data, data, out_delta)
                .expect("item sizes can't be mismatched for self-created snapshots");
            // but they can be different for snapshots received over the network…
        }
    }

    fn write_impl<O, F>(&self, mut object_size: O, mut write_int: F) -> Result<(), CapacityError>
    where
        O: FnMut(u16) -> Option<u32>,
        F: FnMut(i32) -> Result<(), CapacityError>,
    {
        {
            let header = DeltaHeader {
                num_deleted_items: self.deleted_items.len().assert_i32(),
                num_updated_items: self.updated_items.len().assert_i32(),
            };
            for int in header.encode_obj() {
                write_int(int)?;
            }
        }
        for &key in &self.deleted_items {
            write_int(key)?;
        }
        for (&key, range) in &self.updated_items {
            let data = &self.buf[to_usize(range.clone())];
            let raw_type_id = key_to_raw_type_id(key);
            let id = key_to_id(key);
            write_int(raw_type_id.i32())?;
            write_int(id.i32())?;
            match object_size(raw_type_id) {
                Some(size) => assert!(size.usize() == data.len()),
                None => write_int(data.len().assert_i32())?,
            }
            for &d in data {
                write_int(d)?;
            }
        }
        Ok(())
    }
    pub fn write<'d, 's, O>(
        &self,
        object_size: O,
        p: Packer<'d, 's>,
    ) -> Result<&'d [u8], CapacityError>
    where
        O: FnMut(u16) -> Option<u32>,
    {
        let mut p = p;
        self.write_impl(object_size, |int| p.write_int(int))?;
        Ok(p.written())
    }
    pub fn write_to_ints<'a, O>(
        &self,
        object_size: O,
        result: &'a mut [i32],
    ) -> Result<&'a [i32], CapacityError>
    where
        O: FnMut(u16) -> Option<u32>,
    {
        let mut iter = result.iter_mut();
        self.write_impl(object_size, |int| {
            *iter.next().ok_or(CapacityError)? = int;
            Ok(())
        })?;
        let remaining = iter.len();
        let len = result.len() - remaining;
        Ok(&result[..len])
    }

    fn read_impl<W, O, R>(&mut self, warn: &mut W, object_size: O, p: &mut R) -> Result<(), Error>
    where
        W: Warn<Warning>,
        O: FnMut(u16) -> Option<u32>,
        R: ReadInt,
    {
        self.clear();

        let mut object_size = object_size;

        let header = DeltaHeader::decode_impl(warn, p)?;

        for _ in 0..header.num_deleted_items {
            self.deleted_items
                .insert(read_int_err(p, warn, Error::DeletedItemsUnpacking)?);
        }
        if header.num_deleted_items.assert_usize() != self.deleted_items.len() {
            warn.warn(Warning::DuplicateDelete);
        }

        let mut num_updates = 0;

        while !p.is_empty() {
            let raw_type_id = read_int_err(p, warn, Error::ItemDiffsUnpacking)?;
            let id = read_int_err(p, warn, Error::ItemDiffsUnpacking)?;

            let raw_type_id = raw_type_id.try_u16().ok_or(Error::TypeIdRange)?;
            let id = id.try_u16().ok_or(Error::IdRange)?;

            let size = match object_size(raw_type_id) {
                Some(s) => s,
                None => {
                    let s = read_int_err(p, warn, Error::ItemDiffsUnpacking)?;
                    s.try_u32().ok_or(Error::NegativeSize)?
                }
            };
            let start = self.buf.len().try_u32().ok_or(Error::TooLongDiff)?;
            let end = start.checked_add(size).ok_or(Error::TooLongDiff)?;
            for _ in 0..size {
                self.buf
                    .push(read_int_err(p, warn, Error::ItemDiffsUnpacking)?);
            }

            // In case of conflict, take later update (as the original code does).
            if self
                .updated_items
                .insert(key(raw_type_id, id), start..end)
                .is_some()
            {
                warn.warn(Warning::DuplicateUpdate);
            }

            if self.deleted_items.contains(&key(raw_type_id, id)) {
                warn.warn(Warning::DeleteUpdate);
            }
            num_updates += 1;
        }

        if num_updates != header.num_updated_items {
            warn.warn(Warning::NumUpdatedItems);
        }

        Ok(())
    }
    pub fn read_from_ints<W, O>(
        &mut self,
        warn: &mut W,
        object_size: O,
        p: &mut IntUnpacker,
    ) -> Result<(), Error>
    where
        W: Warn<Warning>,
        O: FnMut(u16) -> Option<u32>,
    {
        self.read_impl(warn, object_size, p)
    }

    pub fn read<W, O>(
        &mut self,
        warn: &mut W,
        object_size: O,
        p: &mut Unpacker,
    ) -> Result<(), Error>
    where
        W: Warn<Warning>,
        O: FnMut(u16) -> Option<u32>,
    {
        self.read_impl(warn, object_size, p)
    }
}

#[derive(Default)]
pub struct RawBuilder {
    snap: RawSnap,
}

impl RawBuilder {
    pub fn new() -> RawBuilder {
        Default::default()
    }
    pub fn add_item(&mut self, type_id: u16, id: u16, data: &[i32]) -> Result<(), BuilderError> {
        self.snap.add_item(type_id, id, data)
    }
    pub fn finish(self) -> RawSnap {
        self.snap
    }
}

pub struct Builder {
    snap: Snap,
    next_type_id: u16,
}

impl Default for Builder {
    fn default() -> Builder {
        Builder {
            snap: Default::default(),
            next_type_id: OFFSET_EXTENDED_TYPE_ID,
        }
    }
}

impl Builder {
    pub fn new() -> Builder {
        Default::default()
    }
    pub fn add_item(&mut self, type_id: TypeId, id: u16, data: &[i32]) -> Result<(), BuilderError> {
        let raw_type_id = match type_id {
            TypeId::Ordinal(ordinal) => {
                assert!(0 < ordinal && ordinal < OFFSET_EXTENDED_TYPE_ID);
                ordinal
            }
            TypeId::Uuid(uuid) => {
                match self.snap.extended_types.entry(uuid) {
                    btree_map::Entry::Occupied(o) => *o.get(),
                    btree_map::Entry::Vacant(v) => {
                        let raw_type_id = self.next_type_id;
                        assert!(OFFSET_EXTENDED_TYPE_ID <= raw_type_id, "invalid type ID");
                        assert!(raw_type_id < 0x8000, "invalid type ID");
                        self.snap.raw.add_item(
                            TYPE_ID_EX,
                            raw_type_id,
                            &uuid_to_item_data(uuid),
                        )?;
                        // Only increment `self.next_type_id` after successful
                        // insertion.
                        self.next_type_id += 1;
                        v.insert(raw_type_id);
                        raw_type_id
                    }
                }
            }
        };
        self.snap.raw.add_item(raw_type_id, id, data)
    }
    pub fn finish(self) -> Snap {
        self.snap
    }
}

pub fn delta_chunks(tick: i32, delta_tick: i32, data: &[u8], crc: i32) -> DeltaChunks<'_> {
    DeltaChunks {
        tick: tick,
        delta_tick: tick - delta_tick,
        crc: crc,
        cur_part: if !data.is_empty() { 0 } else { -1 },
        num_parts: ((data.len() + MAX_SNAPSHOT_PACKSIZE as usize - 1)
            / MAX_SNAPSHOT_PACKSIZE as usize)
            .assert_i32(),
        data: data,
    }
}

pub struct DeltaChunks<'a> {
    tick: i32,
    delta_tick: i32,
    crc: i32,
    cur_part: i32,
    num_parts: i32,
    data: &'a [u8],
}

impl<'a> Iterator for DeltaChunks<'a> {
    type Item = SnapMsg<'a>;
    fn next(&mut self) -> Option<SnapMsg<'a>> {
        if self.cur_part == self.num_parts {
            return None;
        }
        let result = if self.num_parts == 0 {
            SnapMsg::SnapEmpty(msg::SnapEmpty {
                tick: self.tick,
                delta_tick: self.delta_tick,
            })
        } else if self.num_parts == 1 {
            SnapMsg::SnapSingle(msg::SnapSingle {
                tick: self.tick,
                delta_tick: self.delta_tick,
                crc: self.crc,
                data: self.data,
            })
        } else {
            let index = self.cur_part.assert_usize();
            let start = MAX_SNAPSHOT_PACKSIZE as usize * index;
            let end = cmp::min(
                MAX_SNAPSHOT_PACKSIZE as usize * (index + 1),
                self.data.len(),
            );
            SnapMsg::Snap(msg::Snap {
                tick: self.tick,
                delta_tick: self.delta_tick,
                num_parts: self.num_parts,
                part: self.cur_part,
                crc: self.crc,
                data: &self.data[start..end],
            })
        };
        self.cur_part += 1;
        Some(result)
    }
}

#[cfg(test)]
mod test {
    use super::Builder;
    use super::Item;
    use uuid::Uuid;

    #[test]
    fn smoke_test() {
        let uuid: Uuid = "1a3fcc94-1e53-461e-912e-21200882024b".parse().unwrap();

        let mut builder = Builder::new();
        builder
            .add_item(uuid.into(), 1337, &[0x1234, 0x567890ab])
            .unwrap();
        let snap = builder.finish();

        assert_eq!(
            snap.item(uuid.into(), 1337),
            Some(&[0x1234, 0x567890ab][..])
        );
        let item = Item {
            type_id: uuid.into(),
            id: 1337,
            data: &[0x1234, 0x567890ab],
        };
        assert_eq!(snap.items().collect::<Vec<_>>(), &[item][..]);
    }
}
