//! DDNet's base library, Rust part.
//!
//! DDNet's code base is separated into three major parts, `base`, `engine` and
//! `game`.
//!
//! The base library consists of operating system abstractions, and
//! game-independent data structures such as color handling and math vectors.
//! Additionally, it contains some types to support the C++-Rust-translation.

#![warn(missing_docs)]

#[cfg(test)]
extern crate ddnet_test;

mod color;
mod rust;

pub use color::*;
pub use rust::*;
