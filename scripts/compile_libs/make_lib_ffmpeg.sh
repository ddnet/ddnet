#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

export TARGET_PLATFORM="${1}"

# ffmpeg has no CMake build, so it is configured with its own script. DDNet only uses
# it to write the mp4 demo recordings, so everything except the H.264 and AAC encoders
# and the mp4 and mov muxers is disabled.

function make_ffmpeg() {
	local build_folder="${1}"
	local x264_folder
	x264_folder="$(realpath "../x264/${build_folder}")"
	if [ ! -f "${x264_folder}/libx264.a" ]; then
		log_error "ERROR: Build x264 first, expected ${x264_folder}/libx264.a"
		exit 1
	fi

	local configure_arguments=()
	configure_arguments+=("--disable-all")
	configure_arguments+=("--enable-avcodec")
	configure_arguments+=("--enable-avformat")
	configure_arguments+=("--enable-swresample")
	configure_arguments+=("--enable-swscale")
	configure_arguments+=("--enable-encoder=libx264,aac")
	configure_arguments+=("--enable-muxer=mp4,mov")
	configure_arguments+=("--enable-protocol=file")
	configure_arguments+=("--enable-libx264")
	configure_arguments+=("--enable-gpl")
	# None of these are used, and each one would add a build dependency
	configure_arguments+=("--disable-alsa")
	configure_arguments+=("--disable-iconv")
	configure_arguments+=("--disable-libxcb")
	configure_arguments+=("--disable-libxcb-shape")
	configure_arguments+=("--disable-libxcb-xfixes")
	configure_arguments+=("--disable-sdl2")
	configure_arguments+=("--disable-xlib")
	configure_arguments+=("--disable-zlib")
	configure_arguments+=("--pkg-config-flags=--static")
	# configure derives the pkg-config to call from --cross-prefix, and no
	# cross-prefixed one exists, so x264 would not be found on the cross lanes
	configure_arguments+=("--pkg-config=pkg-config")

	local build_cflags
	build_cflags="$(desktop_cflags_for "${build_folder}") -I${x264_folder} -I$(realpath ../x264)"
	local build_ldflags="-L${x264_folder}"
	local triple=""

	case "${TARGET_PLATFORM}" in
	linux)
		# Linux links the ffmpeg libraries statically into DDNet
		configure_arguments+=("--enable-static")
		configure_arguments+=("--disable-shared")
		configure_arguments+=("--disable-vdpau")
		configure_arguments+=("--disable-vaapi")
		configure_arguments+=("--disable-libdrm")
		build_ldflags="${build_ldflags} -ldl"
		configure_arguments+=("--extra-libs=-lpthread -lm")
		if [[ "${build_folder}" == "${LINUX_X86_BUILD_FOLDER}" ]]; then
			configure_arguments+=("--cpu=i686")
			# configure links its probe binaries without the compiler flags, so the
			# architecture has to be repeated here or every one of them fails
			build_ldflags="${build_ldflags} -m32"
		fi
		;;
	windows)
		triple="$(desktop_triple_for "${build_folder}")"
		configure_arguments+=("--disable-static")
		configure_arguments+=("--enable-shared")
		configure_arguments+=("--target_os=mingw32")
		configure_arguments+=("--cross-prefix=${triple}-")
		configure_arguments+=("--arch=$(ffmpeg_arch_for "${build_folder}")")
		configure_arguments+=("--extra-libs=-lpthread -lm")
		;;
	mac)
		triple="$(desktop_triple_for "${build_folder}")"
		configure_arguments+=("--disable-static")
		configure_arguments+=("--enable-shared")
		configure_arguments+=("--target_os=darwin")
		configure_arguments+=("--cross-prefix=${triple}-")
		configure_arguments+=("--arch=$(ffmpeg_arch_for "${build_folder}")")
		configure_arguments+=("--cc=${triple}-clang")
		configure_arguments+=("--cxx=${triple}-clang++")
		# Everything the macOS SDK would otherwise be probed for
		configure_arguments+=("--disable-appkit")
		configure_arguments+=("--disable-bzlib")
		configure_arguments+=("--disable-avfoundation")
		configure_arguments+=("--disable-coreimage")
		configure_arguments+=("--disable-securetransport")
		configure_arguments+=("--disable-audiotoolbox")
		configure_arguments+=("--disable-cuda-llvm")
		configure_arguments+=("--disable-videotoolbox")
		;;
	*)
		log_error "ERROR: ${TARGET_PLATFORM} does not need ffmpeg."
		exit 1
		;;
	esac

	# Remove absolute build paths and compiler identification from binary
	build_cflags="${build_cflags} -ffile-prefix-map=$(realpath ..)= -fno-ident"

	log_info "Building to ${build_folder}..."
	mkdir -p "${build_folder}"
	(
		cd "${build_folder}"
		if [[ ! -f ffbuild/config.mak ]]; then
			PKG_CONFIG_PATH="${x264_folder}" ../configure \
				"${configure_arguments[@]}" \
				--extra-cflags="${build_cflags}" \
				--extra-cxxflags="${build_cflags}" \
				--extra-ldflags="${build_ldflags}"
		fi
		# shellcheck disable=SC2086
		make ${BUILD_FLAGS}
	)
}

function make_all_ffmpeg() {
	if [[ "${TARGET_PLATFORM}" == "linux" ]]; then
		make_ffmpeg "${LINUX_X64_BUILD_FOLDER}"
		make_ffmpeg "${LINUX_X86_BUILD_FOLDER}"
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		make_ffmpeg "${WINDOWS_X64_BUILD_FOLDER}"
		make_ffmpeg "${WINDOWS_X86_BUILD_FOLDER}"
		make_ffmpeg "${WINDOWS_ARM64_BUILD_FOLDER}"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		make_ffmpeg "${MAC_X64_BUILD_FOLDER}"
		make_ffmpeg "${MAC_ARM64_BUILD_FOLDER}"
	else
		log_error "ERROR: Unsupported target platform: ${TARGET_PLATFORM}"
		exit 1
	fi
}

make_all_ffmpeg
