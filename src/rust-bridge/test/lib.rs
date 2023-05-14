//! Helper module to link the C++ code of DDNet for Rust unit tests.
//!
//! Use the global `run_rust_tests` target from CMakeLists.txt to actually run
//! the Rust test; this sets compiles the C++ and sets `DDNET_TEST_LIBRARIES`
//! appropriately, e.g. using `cmake --build build --target run_rust_tests`.
//!
//! To call `cargo doc`, set `DDNET_TEST_NO_LINK=1` so that this crate becomes a
//! stub.

#![warn(missing_docs)]
