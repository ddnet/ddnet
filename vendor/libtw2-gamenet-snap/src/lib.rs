use libtw2_buffer::CapacityError;
use libtw2_common::pretty;
use libtw2_gamenet_common::error::Error;
use libtw2_packer::Packer;
use libtw2_packer::Unpacker;
use libtw2_packer::Warning;
use libtw2_warn::wrap;
use libtw2_warn::Warn;
use std::fmt;

pub const MAX_SNAPSHOT_PACKSIZE: i32 = 900;

#[derive(Clone, Copy)]
pub enum SnapMsg<'a> {
    Snap(Snap<'a>),
    SnapEmpty(SnapEmpty),
    SnapSingle(SnapSingle<'a>),
}

impl<'a> fmt::Debug for SnapMsg<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SnapMsg::Snap(ref i) => i.fmt(f),
            SnapMsg::SnapEmpty(ref i) => i.fmt(f),
            SnapMsg::SnapSingle(ref i) => i.fmt(f),
        }
    }
}

#[derive(Clone, Copy)]
pub struct Snap<'a> {
    pub tick: i32,
    pub delta_tick: i32,
    pub num_parts: i32,
    pub part: i32,
    pub crc: i32,
    pub data: &'a [u8],
}

#[derive(Clone, Copy)]
pub struct SnapEmpty {
    pub tick: i32,
    pub delta_tick: i32,
}

#[derive(Clone, Copy)]
pub struct SnapSingle<'a> {
    pub tick: i32,
    pub delta_tick: i32,
    pub crc: i32,
    pub data: &'a [u8],
}

impl<'a> Snap<'a> {
    pub fn decode<W: Warn<Warning>>(
        warn: &mut W,
        _p: &mut Unpacker<'a>,
    ) -> Result<Snap<'a>, Error> {
        let result = Ok(Snap {
            tick: _p.read_int(warn)?,
            delta_tick: _p.read_int(warn)?,
            num_parts: _p.read_int(warn)?,
            part: _p.read_int(warn)?,
            crc: _p.read_int(warn)?,
            data: _p.read_data(warn)?,
        });
        _p.finish(wrap(warn));
        result
    }
    pub fn encode<'d, 's>(&self, mut _p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError> {
        _p.write_int(self.tick)?;
        _p.write_int(self.delta_tick)?;
        _p.write_int(self.num_parts)?;
        _p.write_int(self.part)?;
        _p.write_int(self.crc)?;
        _p.write_data(self.data)?;
        Ok(_p.written())
    }
}
impl<'a> fmt::Debug for Snap<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Snap")
            .field("tick", &self.tick)
            .field("delta_tick", &self.delta_tick)
            .field("num_parts", &self.num_parts)
            .field("part", &self.part)
            .field("crc", &self.crc)
            .field("data", &pretty::Bytes::new(&self.data))
            .finish()
    }
}

impl SnapEmpty {
    pub fn decode<W: Warn<Warning>>(warn: &mut W, _p: &mut Unpacker) -> Result<SnapEmpty, Error> {
        let result = Ok(SnapEmpty {
            tick: _p.read_int(warn)?,
            delta_tick: _p.read_int(warn)?,
        });
        _p.finish(wrap(warn));
        result
    }
    pub fn encode<'d, 's>(&self, mut _p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError> {
        _p.write_int(self.tick)?;
        _p.write_int(self.delta_tick)?;
        Ok(_p.written())
    }
}
impl fmt::Debug for SnapEmpty {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("SnapEmpty")
            .field("tick", &self.tick)
            .field("delta_tick", &self.delta_tick)
            .finish()
    }
}

impl<'a> SnapSingle<'a> {
    pub fn decode<W: Warn<Warning>>(
        warn: &mut W,
        _p: &mut Unpacker<'a>,
    ) -> Result<SnapSingle<'a>, Error> {
        let result = Ok(SnapSingle {
            tick: _p.read_int(warn)?,
            delta_tick: _p.read_int(warn)?,
            crc: _p.read_int(warn)?,
            data: _p.read_data(warn)?,
        });
        _p.finish(wrap(warn));
        result
    }
    pub fn encode<'d, 's>(&self, mut _p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError> {
        _p.write_int(self.tick)?;
        _p.write_int(self.delta_tick)?;
        _p.write_int(self.crc)?;
        _p.write_data(self.data)?;
        Ok(_p.written())
    }
}
impl<'a> fmt::Debug for SnapSingle<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("SnapSingle")
            .field("tick", &self.tick)
            .field("delta_tick", &self.delta_tick)
            .field("crc", &self.crc)
            .field("data", &pretty::Bytes::new(&self.data))
            .finish()
    }
}
