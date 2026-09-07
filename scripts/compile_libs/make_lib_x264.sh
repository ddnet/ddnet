#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

export TARGET_PLATFORM="${1}"

# x264 has no CMake build, so it is configured with its own script. It is only ever
# linked into the ffmpeg libraries, and shipped on its own for Linux where the ffmpeg
# libraries are static.

function make_x264() {
	local build_folder="${1}"
	local configure_arguments=()

	configure_arguments+=("--enable-static")
	configure_arguments+=("--enable-pic")
	configure_arguments+=("--disable-cli")
	# The encoder DDNet uses does not need any of these, and several of them would
	# pull in dependencies that are not available when cross compiling
	configure_arguments+=("--disable-gpl")
	configure_arguments+=("--disable-avs")
	configure_arguments+=("--disable-swscale")
	configure_arguments+=("--disable-lavf")
	configure_arguments+=("--disable-ffms")
	configure_arguments+=("--disable-gpac")
	configure_arguments+=("--disable-lsmash")
	configure_arguments+=("--disable-interlaced")

	local build_cflags
	build_cflags="$(desktop_cflags_for "${build_folder}") -fno-fast-math"
	local build_ldflags=""
	# configure looks for <cross-prefix>gcc, which osxcross does not ship
	desktop_toolchain_for "${build_folder}"
	# nasm only assembles x86, the arm64 lanes assemble through the C compiler
	local assembler=""
	case "$(cmake_processor_for "${build_folder}")" in
	x86_64 | x86)
		assembler="nasm"
		;;
	*)
		assembler="${DESKTOP_CC}"
		;;
	esac

	case "${TARGET_PLATFORM}" in
	linux)
		if [[ "${build_folder}" == "${LINUX_X86_BUILD_FOLDER}" ]]; then
			configure_arguments+=("--host=i686-linux")
			build_ldflags="-m32"
		fi
		;;
	windows)
		local triple
		triple="$(desktop_triple_for "${build_folder}")"
		configure_arguments+=("--host=${triple%%-w64*}-mingw32")
		configure_arguments+=("--cross-prefix=${triple}-")
		;;
	mac)
		local triple
		triple="$(desktop_triple_for "${build_folder}")"
		configure_arguments+=("--host=${triple}")
		configure_arguments+=("--cross-prefix=${triple}-")
		;;
	*)
		log_error "ERROR: ${TARGET_PLATFORM} does not need x264."
		exit 1
		;;
	esac

	# Remove absolute build paths and compiler identification from binary
	build_cflags="${build_cflags} -ffile-prefix-map=$(realpath ..)= -fno-ident"

	log_info "Building to ${build_folder}..."
	mkdir -p "${build_folder}"
	(
		cd "${build_folder}"
		if [[ ! -f config.mak ]]; then
			AS="${assembler}" CC="${DESKTOP_CC}" AR="${DESKTOP_AR}" \
				CFLAGS="${build_cflags}" LDFLAGS="${build_ldflags}" \
				../configure "${configure_arguments[@]}"
		fi
		# shellcheck disable=SC2086
		make ${BUILD_FLAGS} libx264.a
	)
}

function make_all_x264() {
	if [[ "${TARGET_PLATFORM}" == "linux" ]]; then
		make_x264 "${LINUX_X64_BUILD_FOLDER}"
		make_x264 "${LINUX_X86_BUILD_FOLDER}"
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		make_x264 "${WINDOWS_X64_BUILD_FOLDER}"
		make_x264 "${WINDOWS_X86_BUILD_FOLDER}"
		make_x264 "${WINDOWS_ARM64_BUILD_FOLDER}"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		make_x264 "${MAC_X64_BUILD_FOLDER}"
		make_x264 "${MAC_ARM64_BUILD_FOLDER}"
	else
		log_error "ERROR: Unsupported target platform: ${TARGET_PLATFORM}"
		exit 1
	fi
}

make_all_x264
