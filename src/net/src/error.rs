use std::error;
use std::fmt;
use std::result;

#[derive(Debug)]
pub struct Error(String);

impl Error {
    pub fn from_string(string: String) -> Error {
        Error(string)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}
impl error::Error for Error {}

pub type Result<T> = result::Result<T, Error>;

pub trait Context {
    type T;
    fn context(self, context: &str) -> Result<Self::T>;
}

impl<T, E: fmt::Display> Context for result::Result<T, E> {
    type T = T;
    fn context(self, context: &str) -> Result<T> {
        self.map_err(|e| Error(format!("{}: {}", context, e)))
    }
}
