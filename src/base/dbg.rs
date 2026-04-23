use libc::c_char;
use libc::c_int;

extern "C" {
    /// Breaks into the debugger with a message.
    ///
    /// Don't use this function directly. From C++, use the `dbg_assert` or
    /// `dbg_assert_failed` macro, from Rust, use `assert!` directly.
    pub fn dbg_assert_imp(filename: *const c_char, line: c_int, fmt: *const c_char, ...) -> !;
}
