use crate::error::Error;
use crate::msg::MessageId;
use crate::msg::SystemOrGame;
use crate::snap_obj;
use libtw2_buffer::CapacityError;
use libtw2_packer::with_packer;
use libtw2_packer::ExcessData;
use libtw2_packer::IntUnpacker;
use libtw2_packer::Packer;
use libtw2_packer::Unpacker;
use libtw2_packer::Warning;
use libtw2_warn::Warn;

pub trait SnapObj: Sized {
    fn decode_obj<W: Warn<ExcessData>>(
        warn: &mut W,
        obj_type_id: snap_obj::TypeId,
        p: &mut IntUnpacker,
    ) -> Result<Self, Error>;
    fn obj_type_id(&self) -> snap_obj::TypeId;
    fn encode(&self) -> &[i32];
}

pub trait Message<'a>: Sized {
    fn decode_msg<W: Warn<Warning>>(
        warn: &mut W,
        msg_id: SystemOrGame<MessageId, MessageId>,
        p: &mut Unpacker<'a>,
    ) -> Result<Self, Error>;
    fn msg_id(&self) -> SystemOrGame<MessageId, MessageId>;
    fn encode_msg<'d, 's>(&self, p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError>;
}

pub trait MessageExt<'a>: Message<'a> {
    fn decode<W: Warn<Warning>>(warn: &mut W, p: &mut Unpacker<'a>) -> Result<Self, Error> {
        let msg_id = SystemOrGame::decode_id(warn, p)?;
        Self::decode_msg(warn, msg_id, p)
    }
    fn encode<'d, 's>(&self, mut p: Packer<'d, 's>) -> Result<&'d [u8], CapacityError> {
        with_packer(&mut p, |p| self.msg_id().encode_id(p))?;
        with_packer(&mut p, |p| self.encode_msg(p))?;
        Ok(p.written())
    }
}

impl<'a, T: Message<'a>> MessageExt<'a> for T {}

pub trait ProtocolStatic {
    type SnapObj: SnapObj + 'static;
    fn obj_size(type_id: u16) -> Option<u32>;
}

pub trait Protocol<'a>: ProtocolStatic {
    type Game: Message<'a>;
    type System: Message<'a>;
}
