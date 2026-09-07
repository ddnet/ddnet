#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

export TARGET_PLATFORM="${1}"

# A static archive that exports the zlib C API, implemented by zlib-rs. The shipped
# libpng links against this instead of against a zlib of its own. Only Windows and
# macOS need it, the Linux libpng is static and resolves zlib from the system.

function write_zlib_rs_crate() {
	mkdir -p zlib-rs/src
	cat > zlib-rs/Cargo.toml << EOF
[package]
name = "zlib-rs-shim"
version = "0.0.1"
edition = "2021"
publish = false

[lib]
crate-type = ["staticlib"]
path = "src/lib.rs"

[dependencies]
libz-rs-sys = { version = "=${ZLIB_RS_VERSION}", default-features = false, features = ["export-symbols", "c-allocator"] }

[profile.release]
opt-level = 3
panic = "abort"
strip = true
EOF

	# no_std, so that nothing but the zlib API and libc ends up in libpng: with
	# default features the Rust std pulls its whole windows platform layer
	# (sockets, ntdll, ...) into the DLL.
	cat > zlib-rs/src/lib.rs << 'EOF'
//! Static archive exporting the zlib C API, implemented by zlib-rs.
#![no_std]

use core::alloc::{GlobalAlloc, Layout};

use libz_rs_sys as _;

unsafe extern "C" {
    fn abort() -> !;
}

/// zlib-rs links the `alloc` crate, so a global allocator has to exist, but it
/// never allocates through it: the `c-allocator` feature routes zlib-rs's own
/// allocations to `malloc`, and libpng installs its own `zalloc`/`zfree` on
/// every stream regardless. Abort instead of pretending to allocate.
struct Unused;

unsafe impl GlobalAlloc for Unused {
    unsafe fn alloc(&self, _layout: Layout) -> *mut u8 {
        unsafe { abort() }
    }

    unsafe fn dealloc(&self, _ptr: *mut u8, _layout: Layout) {
        unsafe { abort() }
    }
}

#[global_allocator]
static ALLOCATOR: Unused = Unused;

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    unsafe { abort() }
}

/// The precompiled `core` rlib is built with `panic=unwind`, so its unwind
/// tables reference this. We abort on panic, so it is never called.
#[no_mangle]
extern "C" fn rust_eh_personality() {}
EOF
}

function make_zlib_rs() {
	local build_folder="${1}"
	local rust_target="${2}"

	log_info "Building to ${build_folder}..."
	(
		cd zlib-rs
		rustup target add "${rust_target}"
		cargo build --release --target "${rust_target}"

		mkdir -p "${build_folder}/lib" "${build_folder}/include"
		cp "target/${rust_target}/release/libzlib_rs_shim.a" "${build_folder}/lib/libz.a"
		# The zlib headers libpng compiles against come from the crate that
		# implements them, so they cannot go out of sync with the archive.
		local header_dir
		header_dir="$(find "${CARGO_HOME:-${HOME}/.cargo}/registry/src" -maxdepth 2 -type d -name "libz-rs-sys-${ZLIB_RS_VERSION}" | head -1)/include"
		if [ ! -d "${header_dir}" ]; then
			log_error "ERROR: zlib headers not found, expected them at ${header_dir}"
			exit 1
		fi
		cp "${header_dir}/zlib.h" "${header_dir}/zconf.h" "${build_folder}/include/"
	)
}

function make_all_zlib_rs() {
	write_zlib_rs_crate
	if [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		make_zlib_rs "${WINDOWS_X64_BUILD_FOLDER}" "x86_64-pc-windows-gnu"
		make_zlib_rs "${WINDOWS_X86_BUILD_FOLDER}" "i686-pc-windows-gnu"
		make_zlib_rs "${WINDOWS_ARM64_BUILD_FOLDER}" "aarch64-pc-windows-gnullvm"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		make_zlib_rs "${MAC_X64_BUILD_FOLDER}" "x86_64-apple-darwin"
		make_zlib_rs "${MAC_ARM64_BUILD_FOLDER}" "aarch64-apple-darwin"
	else
		log_error "ERROR: ${TARGET_PLATFORM} does not need zlib-rs."
		exit 1
	fi
}

make_all_zlib_rs
