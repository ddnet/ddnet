use crate::format::Warning;
use libtw2_packer::IntUnpacker;
use libtw2_packer::UnexpectedEnd;
use libtw2_packer::Unpacker;
use libtw2_warn::wrap;
use libtw2_warn::Warn;

pub trait ReadInt {
    fn is_empty(&self) -> bool;
    fn read_int<W: Warn<Warning>>(&mut self, warn: &mut W) -> Result<i32, UnexpectedEnd>;
}

impl ReadInt for IntUnpacker<'_> {
    fn is_empty(&self) -> bool {
        self.is_empty()
    }
    fn read_int<W: Warn<Warning>>(&mut self, _: &mut W) -> Result<i32, UnexpectedEnd> {
        self.read_int()
    }
}

impl ReadInt for Unpacker<'_> {
    fn is_empty(&self) -> bool {
        self.is_empty()
    }
    fn read_int<W: Warn<Warning>>(&mut self, warn: &mut W) -> Result<i32, UnexpectedEnd> {
        self.read_int(wrap(warn))
    }
}
