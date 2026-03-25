use arrayvec::ArrayVec;
use libtw2_buffer as buffer;
use libtw2_buffer::with_buffer;
use libtw2_buffer::Buffer;
use libtw2_buffer::BufferRef;
use libtw2_buffer::CapacityError;
use libtw2_common::num::Cast;
use libtw2_common::unwrap_or_return;
use libtw2_warn::Warn;
use std::mem;
use std::slice;
#[cfg(feature = "uuid")]
use uuid::Uuid;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Warning {
    OverlongIntEncoding,
    NonZeroIntPadding,
    ExcessData,
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ExcessData;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct WeirdStringTermination;

impl From<ExcessData> for Warning {
    fn from(_: ExcessData) -> Warning {
        Warning::ExcessData
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct ControlCharacters;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct IntOutOfRange;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct UnexpectedEnd;

// Format: ESDD_DDDD EDDD_DDDD EDDD_DDDD EDDD_DDDD PPPP_DDDD
// E - Extend
// S - Sign
// D - Digit (little-endian)
// P - Padding
//
// Padding must be zeroed. The extend bit specifies whether another byte
// follows.
fn read_int<W>(warn: &mut W, iter: &mut slice::Iter<u8>) -> Result<i32, UnexpectedEnd>
where
    W: Warn<Warning>,
{
    let mut result = 0;
    let mut len = 1;

    let mut src = *unwrap_or_return!(iter.next(), Err(UnexpectedEnd));
    let sign = ((src >> 6) & 1) as i32;

    result |= (src & 0b0011_1111) as i32;

    for i in 0..4 {
        if src & 0b1000_0000 == 0 {
            break;
        }
        src = *unwrap_or_return!(iter.next(), Err(UnexpectedEnd));
        len += 1;
        if i == 3 && src & 0b1111_0000 != 0 {
            warn.warn(Warning::NonZeroIntPadding);
        }
        result |= ((src & 0b0111_1111) as i32) << (6 + 7 * i);
    }

    if len > 1 && src == 0b0000_0000 {
        warn.warn(Warning::OverlongIntEncoding);
    }

    result ^= -sign;

    Ok(result)
}

/// Sets the n-th bit of a byte conditionally.
fn to_bit(b: bool, bit: u32) -> u8 {
    assert!(bit < 8);
    if b {
        1 << bit
    } else {
        0
    }
}

fn write_int<E, F: FnMut(&[u8]) -> Result<(), E>>(int: i32, f: F) -> Result<(), E> {
    let mut f = f;
    let mut buf: ArrayVec<[u8; 5]> = ArrayVec::new();
    let sign = if int < 0 { 1 } else { 0 };
    let mut int = (int ^ -sign) as u32;
    let next = (int & 0b0011_1111) as u8;
    int >>= 6;
    buf.push(to_bit(int != 0, 7) | to_bit(sign != 0, 6) | next);
    while int != 0 {
        let next = (int & 0b0111_1111) as u8;
        int >>= 7;
        buf.push(to_bit(int != 0, 7) | next);
    }
    f(&buf)
}

fn read_string<'a>(iter: &mut slice::Iter<'a, u8>) -> Result<&'a [u8], UnexpectedEnd> {
    let slice = iter.as_slice();
    // `by_ref` is needed as the iterator is silently copied otherwise.
    for (i, b) in iter.by_ref().cloned().enumerate() {
        if b == 0 {
            return Ok(&slice[..i]);
        }
    }
    Err(UnexpectedEnd)
}

fn write_string<E, F: FnMut(&[u8]) -> Result<(), E>>(string: &[u8], f: F) -> Result<(), E> {
    let mut f = f;
    assert!(string.iter().all(|&b| b != 0));
    f(string)?;
    f(&[0])?;
    Ok(())
}

pub struct Packer<'d, 's> {
    buf: BufferRef<'d, 's>,
}

impl<'r, 'd, 's> Buffer<'d> for &'r mut Packer<'d, 's> {
    type Intermediate = buffer::BufferRefBuffer<'r, 'd, 's>;
    fn to_to_buffer_ref(self) -> Self::Intermediate {
        (&mut self.buf).to_to_buffer_ref()
    }
}

impl<'d, 's> Packer<'d, 's> {
    fn new(buf: BufferRef<'d, 's>) -> Packer<'d, 's> {
        Packer { buf: buf }
    }
    pub fn write_string(&mut self, string: &[u8]) -> Result<(), CapacityError> {
        write_string(string, |b| self.buf.write(b))
    }
    pub fn write_int(&mut self, int: i32) -> Result<(), CapacityError> {
        write_int(int, |b| self.buf.write(b))
    }
    pub fn write_data(&mut self, data: &[u8]) -> Result<(), CapacityError> {
        self.write_int(data.len().try_i32().ok_or(CapacityError)?)?;
        self.buf.write(data)?;
        Ok(())
    }
    pub fn write_raw(&mut self, data: &[u8]) -> Result<(), CapacityError> {
        self.buf.write(data)
    }
    #[cfg(feature = "uuid")]
    pub fn write_uuid(&mut self, uuid: Uuid) -> Result<(), CapacityError> {
        self.write_raw(uuid.as_bytes())
    }
    pub fn write_rest(&mut self, data: &[u8]) -> Result<(), CapacityError> {
        // TODO: Fail if other stuff happens afterwards.
        self.buf.write(data)
    }
    pub fn written(self) -> &'d [u8] {
        self.buf.initialized()
    }
}

pub fn with_packer<'a, B: Buffer<'a>, F, R>(buf: B, f: F) -> R
where
    F: for<'b> FnOnce(Packer<'a, 'b>) -> R,
{
    with_buffer(buf, |b| f(Packer::new(b)))
}

pub struct Unpacker<'a> {
    original: &'a [u8],
    iter: slice::Iter<'a, u8>,
    demo: bool,
}

impl<'a> Unpacker<'a> {
    fn new_impl(data: &[u8], demo: bool) -> Unpacker<'_> {
        Unpacker {
            original: data,
            iter: data.iter(),
            demo: demo,
        }
    }
    pub fn new(data: &[u8]) -> Unpacker<'_> {
        Unpacker::new_impl(data, false)
    }
    pub fn new_from_demo(data: &[u8]) -> Unpacker<'_> {
        assert!(
            data.len() % 4 == 0,
            "demo data must be padded to a multiple of four bytes"
        );
        Unpacker::new_impl(data, true)
    }
    pub fn is_empty(&self) -> bool {
        self.iter.len() == 0
    }
    fn use_up(&mut self) {
        // Advance the iterator to the end.
        self.iter.by_ref().count();
    }
    fn error<T>(&mut self) -> Result<T, UnexpectedEnd> {
        self.use_up();
        Err(UnexpectedEnd)
    }
    pub fn read_string(&mut self) -> Result<&'a [u8], UnexpectedEnd> {
        read_string(&mut self.iter)
    }
    pub fn read_int<W: Warn<Warning>>(&mut self, warn: &mut W) -> Result<i32, UnexpectedEnd> {
        read_int(warn, &mut self.iter)
    }
    pub fn read_data<W: Warn<Warning>>(&mut self, warn: &mut W) -> Result<&'a [u8], UnexpectedEnd> {
        let len = match self.read_int(warn).map(|l| l.try_usize()) {
            Ok(Some(l)) => l,
            _ => return self.error(),
        };
        let slice = self.iter.as_slice();
        if len > slice.len() {
            return self.error();
        }
        let (data, remaining) = slice.split_at(len);
        self.iter = remaining.iter();
        Ok(data)
    }
    pub fn read_rest(&mut self) -> Result<&'a [u8], UnexpectedEnd> {
        // TODO: Fail if earlier call errored out.
        let result = Ok(self.iter.as_slice());
        self.use_up();
        result
    }
    pub fn read_raw(&mut self, len: usize) -> Result<&'a [u8], UnexpectedEnd> {
        let slice = self.iter.as_slice();
        if slice.len() < len {
            self.use_up();
            return Err(UnexpectedEnd);
        }
        let (raw, rest) = slice.split_at(len);
        self.iter = rest.iter();
        Ok(raw)
    }
    #[cfg(feature = "uuid")]
    pub fn read_uuid(&mut self) -> Result<Uuid, UnexpectedEnd> {
        Ok(Uuid::from_slice(self.read_raw(mem::size_of::<Uuid>())?).unwrap())
    }
    pub fn finish<W: Warn<ExcessData>>(&mut self, warn: &mut W) {
        if !self.demo {
            if !self.is_empty() {
                warn.warn(ExcessData);
            }
        } else {
            let rest = self.as_slice();
            if rest.len() >= 4 || rest.iter().any(|&b| b != 0) {
                warn.warn(ExcessData);
            }
        }
        self.use_up();
    }
    pub fn as_slice(&self) -> &'a [u8] {
        self.iter.as_slice()
    }
    pub fn num_bytes_read(&self) -> usize {
        self.original.len() - self.iter.len()
    }
}

pub struct IntUnpacker<'a> {
    iter: slice::Iter<'a, i32>,
}

impl<'a> IntUnpacker<'a> {
    pub fn new(slice: &[i32]) -> IntUnpacker<'_> {
        IntUnpacker { iter: slice.iter() }
    }
    pub fn is_empty(&self) -> bool {
        self.iter.len() == 0
    }
    fn use_up(&mut self) {
        // Advance the iterator to the end.
        self.iter.by_ref().count();
    }
    pub fn read_int(&mut self) -> Result<i32, UnexpectedEnd> {
        self.iter.next().copied().ok_or(UnexpectedEnd)
    }
    pub fn finish<W: Warn<ExcessData>>(&mut self, warn: &mut W) {
        if !self.is_empty() {
            warn.warn(ExcessData);
        }
        self.use_up();
    }
    pub fn as_slice(&self) -> &'a [i32] {
        self.iter.as_slice()
    }
}

pub fn in_range(v: i32, min: i32, max: i32) -> Result<i32, IntOutOfRange> {
    if min <= v && v <= max {
        Ok(v)
    } else {
        Err(IntOutOfRange)
    }
}

pub fn at_least(v: i32, min: i32) -> Result<i32, IntOutOfRange> {
    if min <= v {
        Ok(v)
    } else {
        Err(IntOutOfRange)
    }
}

pub fn to_bool(v: i32) -> Result<bool, IntOutOfRange> {
    Ok(in_range(v, 0, 1)? != 0)
}

pub fn sanitize<'a, W: Warn<Warning>>(
    warn: &mut W,
    v: &'a [u8],
) -> Result<&'a [u8], ControlCharacters> {
    if v.iter().any(|&b| b < b' ') {
        return Err(ControlCharacters);
    }
    let _ = warn;
    // TODO: Implement whitespace skipping.
    Ok(v)
}

pub fn positive(v: i32) -> Result<i32, IntOutOfRange> {
    if v >= 0 {
        Ok(v)
    } else {
        Err(IntOutOfRange)
    }
}

pub fn string_to_ints(result: &mut [i32], string: &[u8]) {
    assert!(string.iter().all(|&b| b != 0));
    // Strict less-than because of the NUL-termination.
    assert!(string.len() < result.len() * mem::size_of::<i32>());
    let mut output = result.iter_mut();
    let mut input = string.iter().cloned();
    while let Some(o) = output.next() {
        let v0 = input.next().unwrap_or(0).wrapping_add(0x80);
        let v1 = input.next().unwrap_or(0).wrapping_add(0x80);
        let v2 = input.next().unwrap_or(0).wrapping_add(0x80);
        // FIXME: Use .is_empty()
        let v3 = input
            .next()
            .unwrap_or(if output.len() != 0 { 0 } else { 0x80 })
            .wrapping_add(0x80);
        *o = (v0 as i32) << 24 | (v1 as i32) << 16 | (v2 as i32) << 8 | (v3 as i32);
    }
}

pub fn string_to_ints3(string: &[u8]) -> [i32; 3] {
    let mut result: [i32; 3] = Default::default();
    string_to_ints(&mut result, string);
    result
}
pub fn string_to_ints4(string: &[u8]) -> [i32; 4] {
    let mut result: [i32; 4] = Default::default();
    string_to_ints(&mut result, string);
    result
}
pub fn string_to_ints6(string: &[u8]) -> [i32; 6] {
    let mut result: [i32; 6] = Default::default();
    string_to_ints(&mut result, string);
    result
}

pub fn bytes_to_string<'a, W>(warn: &mut W, bytes: &'a [u8]) -> &'a [u8]
where
    W: Warn<WeirdStringTermination>,
{
    if bytes.is_empty() {
        warn.warn(WeirdStringTermination);
        return bytes;
    }
    let end = bytes
        .iter()
        .position(|&b| b == 0)
        .unwrap_or(bytes.len() - 1);
    let (string, nuls) = bytes.split_at(end);
    if !nuls.iter().all(|&b| b == 0) {
        warn.warn(WeirdStringTermination);
    }
    string
}

pub fn string_to_bytes<'a, B: Buffer<'a>>(
    buf: B,
    string: &[u8],
) -> Result<&'a [u8], CapacityError> {
    with_buffer(buf, |buf| string_to_bytes_buffer_ref(buf, string))
}

fn string_to_bytes_buffer_ref<'d, 's>(
    mut buf: BufferRef<'d, 's>,
    string: &[u8],
) -> Result<&'d [u8], CapacityError> {
    assert!(string.iter().all(|&b| b != 0));
    buf.write(string)?;
    buf.write(&[0])?;
    Ok(buf.initialized())
}

#[cfg(test)]
#[rustfmt::skip]
mod test {
    use arrayvec::ArrayVec;
    use libtw2_warn::Ignore;
    use libtw2_warn::Panic;
    use quickcheck::quickcheck;
    use std::i32;
    use super::ExcessData;
    use super::Unpacker;
    use super::Warning::*;
    use super::Warning;
    use super::with_packer;

    fn assert_int_err(bytes: &[u8]) {
        let mut unpacker = Unpacker::new(bytes);
        unpacker.read_int(&mut Panic).unwrap_err();
    }

    fn assert_int_warnings(bytes: &[u8], int: i32, warnings: &[Warning]) {
        let mut vec = vec![];
        let mut unpacker = Unpacker::new(bytes);
        assert_eq!(unpacker.read_int(&mut vec).unwrap(), int);
        assert!(unpacker.as_slice().is_empty());
        assert_eq!(vec, warnings);

        let mut buf: ArrayVec<[u8; 5]> = ArrayVec::new();
        let written = with_packer(&mut buf, |mut p| {
            p.write_int(int).unwrap();
            p.written()
        });
        if warnings.is_empty() {
            assert_eq!(written, bytes);
        } else {
            assert!(written != bytes);
        }
    }

    fn assert_int_warn(bytes: &[u8], int: i32, warning: Warning) {
        assert_int_warnings(bytes, int, &[warning]);
    }

    fn assert_int(bytes: &[u8], int: i32) {
        assert_int_warnings(bytes, int, &[]);
    }

    fn assert_str(bytes: &[u8], string: &[u8], remaining: &[u8]) {
        let mut unpacker = Unpacker::new(bytes);
        assert_eq!(unpacker.read_string().unwrap(), string);
        assert_eq!(unpacker.as_slice(), remaining);

        let mut buf = Vec::with_capacity(4096);
        let written = with_packer(&mut buf, |mut p| {
            p.write_string(string).unwrap();
            p.write_rest(remaining).unwrap();
            p.written()
        });
        assert_eq!(written, bytes);
    }

    fn assert_str_err(bytes: &[u8]) {
        let mut unpacker = Unpacker::new(bytes);
        unpacker.read_string().unwrap_err();
    }

    #[test] fn int_0() { assert_int(b"\x00", 0) }
    #[test] fn int_1() { assert_int(b"\x01", 1) }
    #[test] fn int_63() { assert_int(b"\x3f", 63) }
    #[test] fn int_m1() { assert_int(b"\x40", -1) }
    #[test] fn int_64() { assert_int(b"\x80\x01", 64) }
    #[test] fn int_m65() { assert_int(b"\xc0\x01", -65) }
    #[test] fn int_m64() { assert_int(b"\x7f", -64) }
    #[test] fn int_min() { assert_int(b"\xff\xff\xff\xff\x0f", i32::min_value()) }
    #[test] fn int_max() { assert_int(b"\xbf\xff\xff\xff\x0f", i32::max_value()) }
    #[test] fn int_quirk1() { assert_int_warn(b"\xff\xff\xff\xff\xff", 0, NonZeroIntPadding) }
    #[test] fn int_quirk2() { assert_int_warn(b"\xbf\xff\xff\xff\xff", -1, NonZeroIntPadding) }
    #[test] fn int_empty() { assert_int_err(b"") }
    #[test] fn int_extend_empty() { assert_int_err(b"\x80") }
    #[test] fn int_overlong1() { assert_int_warn(b"\x80\x00", 0, OverlongIntEncoding) }
    #[test] fn int_overlong2() { assert_int_warn(b"\xc0\x00", -1, OverlongIntEncoding) }

    #[test] fn str_empty() { assert_str(b"\0", b"", b"") }
    #[test] fn str_none() { assert_str_err(b"") }
    #[test] fn str_no_nul() { assert_str_err(b"abc") }
    #[test] fn str_rest1() { assert_str(b"abc\0def", b"abc", b"def") }
    #[test] fn str_rest2() { assert_str(b"abc\0", b"abc", b"") }
    #[test] fn str_rest3() { assert_str(b"abc\0\0", b"abc", b"\0") }
    #[test] fn str_rest4() { assert_str(b"\0\0", b"", b"\0") }

    #[test]
    fn excess_data() {
        let mut warnings = vec![];
        let mut unpacker = Unpacker::new(b"\x00");
        unpacker.finish(&mut warnings);
        assert_eq!(warnings, [ExcessData]);
    }

    quickcheck! {
        fn int_roundtrip(int: i32) -> bool {
            let mut buf: ArrayVec<[u8; 5]> = ArrayVec::new();
            let mut unpacker = Unpacker::new(with_packer(&mut buf, |mut p| {
                p.write_int(int).unwrap();
                p.written()
            }));
            let read_int = unpacker.read_int(&mut Panic).unwrap();
            int == read_int && unpacker.as_slice().is_empty()
        }

        fn int_no_panic(data: Vec<u8>) -> bool {
            let mut unpacker = Unpacker::new(&data);
            let _ = unpacker.read_int(&mut Ignore);
            true
        }

        fn string_no_panic(data: Vec<u8>) -> bool {
            let mut unpacker = Unpacker::new(&data);
            let _ = unpacker.read_string();
            true
        }
    }
}
