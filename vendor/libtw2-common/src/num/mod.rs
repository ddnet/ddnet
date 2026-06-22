#[rustfmt::skip]
mod cast;

pub use self::cast::Cast;

pub trait CastFloat {
    fn round_to_i32(self) -> i32;
    fn trunc_to_i32(self) -> i32;
}

impl CastFloat for f32 {
    fn round_to_i32(self) -> i32 {
        // TODO: Do overflow checking?
        self.round() as i32
    }
    fn trunc_to_i32(self) -> i32 {
        // TODO: Do overflow checking?
        self.trunc() as i32
    }
}
