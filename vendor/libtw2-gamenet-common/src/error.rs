#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Error {
    ControlCharacters,
    IntOutOfRange,
    InvalidIntString,
    UnexpectedEnd,
    UnknownId,
}

pub struct InvalidIntString;

impl From<libtw2_packer::ControlCharacters> for Error {
    fn from(_: libtw2_packer::ControlCharacters) -> Error {
        Error::ControlCharacters
    }
}

impl From<libtw2_packer::IntOutOfRange> for Error {
    fn from(_: libtw2_packer::IntOutOfRange) -> Error {
        Error::IntOutOfRange
    }
}

impl From<InvalidIntString> for Error {
    fn from(_: InvalidIntString) -> Error {
        Error::InvalidIntString
    }
}

impl From<libtw2_packer::UnexpectedEnd> for Error {
    fn from(_: libtw2_packer::UnexpectedEnd) -> Error {
        Error::UnexpectedEnd
    }
}
