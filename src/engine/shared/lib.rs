//! DDNet's engine interfaces, Rust part.
//!
//! DDNet's code base is separated into three major parts, `base`, `engine` and
//! `game`.
//!
//! The engine consists of game-independent code such as display setup,
//! low-level network protocol, low-level map format, etc.
//!
//! This crate in particular corresponds to the `src/engine/shared` directory
//! that contains code shared between client, server and other components.

#![warn(missing_docs)]

#[cfg(test)]
extern crate ddnet_test;

// Provides the zlib C API for the C++ part of our code.
#[cfg(feature = "zlib-rs")]
use libz_rs_sys as _;

mod config;
mod rust_version;

pub use config::*;
pub use rust_version::*;
