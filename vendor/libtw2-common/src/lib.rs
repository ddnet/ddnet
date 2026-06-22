pub use self::map_iter::MapIterator;
pub use self::slice::relative_size_of;
pub use self::slice::relative_size_of_mult;
pub use self::takeable::Takeable;

#[macro_use]
mod macros;

pub mod bytes;
pub mod digest;
pub mod io;
pub mod map_iter;
pub mod num;
pub mod pretty;
pub mod slice;
pub mod str;
pub mod takeable;
pub mod vec;
