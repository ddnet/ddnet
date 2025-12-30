#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

if [ -z ${1+x} ]; then
	log_error "ERROR: Specify the destination path where to run this script, please choose a path other than in the source directory"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <android/webasm>"
	exit 1
fi
BUILD_FOLDER="$1"

if [ -z ${2+x} ]; then
	log_error "ERROR: Specify the target platform: android, webasm"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <android/webasm>"
	exit 1
fi
TARGET_PLATFORM="$2"
if [[ "${TARGET_PLATFORM}" != "android" && "${TARGET_PLATFORM}" != "webasm" ]]; then
	log_error "ERROR: Specify the target platform: android, webasm"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <android/webasm>"
	exit 1
fi
if [[ "${TARGET_PLATFORM}" == "android" ]]; then
	assert_android_ndk_found
elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
	assert_emscripten_sdk_found
fi

mkdir -p "${BUILD_FOLDER}"
cd "${BUILD_FOLDER}"

function build_cmake_lib() {
	local library_dir="$1"
	local git_url="$2"
	# "branch" or "commit"
	local clone_target_type="$3"
	# name of the branch, or commit hash
	local clone_target_reference="$4"
	if [ ! -d "${library_dir}" ]; then
		if [[ "${clone_target_type}" == "branch" ]]; then
			git clone --single-branch --branch "${clone_target_reference}" "${git_url}" "${library_dir}"
		elif [[ "${clone_target_type}" == "commit" ]]; then
			git clone --no-checkout "${git_url}" "${library_dir}"
			(
				cd "${library_dir}"
				git fetch origin "${clone_target_reference}"
				git checkout "${clone_target_reference}"
			)
		fi
	fi
	(
		cd "${library_dir}"
		"${SCRIPT_DIR}"/cmake_lib_compile.sh "${library_dir}" "$TARGET_PLATFORM"
	)
}

function build_opusfile() {
	local was_there_opusfile=1
	if [ ! -d "opusfile" ]; then
		git clone --single-branch --branch "v0.12" https://github.com/xiph/opusfile opusfile
		was_there_opusfile=0
	fi
	(
		cd opusfile
		if [[ "$was_there_opusfile" == 0 ]]; then
			./autogen.sh
		fi
		"${SCRIPT_DIR}"/make_lib_opusfile.sh "$TARGET_PLATFORM"
	)
}

function build_sqlite3() {
	if [ ! -d "sqlite3" ]; then
		local sqlite_filename="sqlite-amalgamation-3460000"
		local sqlite_archive_filename="${sqlite_filename}.zip"
		wget --no-verbose "https://www.sqlite.org/2024/${sqlite_archive_filename}"
		unzip -q "${sqlite_archive_filename}"
		rm "${sqlite_archive_filename}"
		mv "${sqlite_filename}" "sqlite3"
	fi
	(
		cd sqlite3
		"${SCRIPT_DIR}"/make_lib_sqlite3.sh "$TARGET_PLATFORM"
	)
}

mkdir -p compile_libs
cd compile_libs

# BoringSSL
if [[ "$TARGET_PLATFORM" == "android" ]]; then
	log_info_header "Building BoringSSL..."
	build_cmake_lib boringssl https://boringssl.googlesource.com/boringssl "commit" "a1b6110c3aae7654cd88128186ea826cc27535be"
fi

# zlib (required to build libpng, curl and freetype for webasm)
if [[ "$TARGET_PLATFORM" == "webasm" ]]; then
	log_info_header "Building zlib..."
	build_cmake_lib zlib https://github.com/madler/zlib "branch" "v1.3.1.2"
fi

# libpng (also required to build freetype)
log_info_header "Building libpng..."
build_cmake_lib png https://github.com/glennrp/libpng "branch" "v1.6.43"

# curl
log_info_header "Building curl..."
build_cmake_lib curl https://github.com/curl/curl "branch" "curl-8_8_0"

# freetype
log_info_header "Building freetype..."
build_cmake_lib freetype https://gitlab.freedesktop.org/freetype/freetype "branch" "VER-2-13-2"

# SDL
log_info_header "Building SDL..."
build_cmake_lib sdl https://github.com/libsdl-org/SDL "branch" "release-2.32.10"

# ogg, opus, opusfile
log_info_header "Building ogg..."
build_cmake_lib ogg https://github.com/xiph/ogg "branch" "v1.3.5"
log_info_header "Building opus..."
build_cmake_lib opus https://github.com/xiph/opus "branch" "v1.5.2"
log_info_header "Building opusfile..."
build_opusfile

# sqlite3
log_info_header "Building sqlite3..."
build_sqlite3

# Copy files into ddnet-libs structure
log_info_header "Copying files into ddnet-libs structure..."
cd ..
mkdir -p ddnet-libs

function copy_libs_for_arches() {
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		${1} "${ANDROID_ARM_BUILD_FOLDER}" libarm
		${1} "${ANDROID_ARM64_BUILD_FOLDER}" libarm64
		${1} "${ANDROID_X86_BUILD_FOLDER}" lib32
		${1} "${ANDROID_X64_BUILD_FOLDER}" lib64
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		${1} "${EMSCRIPTEN_WASM_BUILD_FOLDER}" libwasm
	fi
}

if [[ "$TARGET_PLATFORM" == "android" ]]; then
	function _copy_boringssl() {
		local target_libs_folder="ddnet-libs/boringssl/$TARGET_PLATFORM/$2"
		local target_include_folder="ddnet-libs/boringssl/include/$TARGET_PLATFORM"
		mkdir -p "$target_libs_folder"
		mkdir -p "$target_include_folder"
		cp compile_libs/boringssl/"$1"/libcrypto.a "$target_libs_folder"/libcrypto.a
		cp compile_libs/boringssl/"$1"/libssl.a "$target_libs_folder"/libssl.a
		cp -R compile_libs/boringssl/include/openssl "$target_include_folder"
	}
	copy_libs_for_arches _copy_boringssl
fi

if [[ "$TARGET_PLATFORM" == "webasm" ]]; then
	function _copy_zlib() {
		local target_libs_folder="ddnet-libs/zlib/$TARGET_PLATFORM/$2"
		local target_include_folder="ddnet-libs/zlib/include/$TARGET_PLATFORM"
		mkdir -p "$target_libs_folder"
		mkdir -p "$target_include_folder"
		cp compile_libs/zlib/"$1"/libz.a "$target_libs_folder"/libz.a
		cp -R compile_libs/zlib/*.h "$target_include_folder"
		cp -R compile_libs/zlib/"$1"/*.h "$target_include_folder"
	}
	copy_libs_for_arches _copy_zlib
fi

function _copy_png() {
	local target_libs_folder="ddnet-libs/png/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/png/"$1"/libpng16.a "$target_libs_folder"/libpng16.a
}
copy_libs_for_arches _copy_png

function _copy_curl() {
	local target_libs_folder="ddnet-libs/curl/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/curl/"$1"/lib/libcurl.a "$target_libs_folder"/libcurl.a
}
copy_libs_for_arches _copy_curl

function _copy_freetype() {
	local target_libs_folder="ddnet-libs/freetype/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/freetype/"$1"/libfreetype.a "$target_libs_folder"/libfreetype.a
}
copy_libs_for_arches _copy_freetype

function _copy_sdl() {
	local target_libs_folder="ddnet-libs/sdl/$TARGET_PLATFORM/$2"
	local target_include_folder="ddnet-libs/sdl/include/$TARGET_PLATFORM"
	mkdir -p "$target_libs_folder"
	mkdir -p "$target_include_folder"
	cp compile_libs/sdl/"$1"/libSDL2.a "$target_libs_folder"/libSDL2.a
	cp -R compile_libs/sdl/include/* "$target_include_folder"
}
copy_libs_for_arches _copy_sdl

# Copy Java code from SDL2 Android project template
if [[ "$TARGET_PLATFORM" == "android" ]]; then
	target_java_folder="ddnet-libs/sdl/java"
	rm -rf "$target_java_folder"
	mkdir -p "$target_java_folder"
	cp -R compile_libs/sdl/android-project/app/src/main/java/org "$target_java_folder"/
fi

function _copy_opus() {
	local target_libs_folder="ddnet-libs/opus/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/ogg/"$1"/libogg.a "$target_libs_folder"/libogg.a
	cp compile_libs/opus/"$1"/libopus.a "$target_libs_folder"/libopus.a
	cp compile_libs/opusfile/"$1"/libopusfile.a "$target_libs_folder"/libopusfile.a
}
copy_libs_for_arches _copy_opus

function _copy_sqlite3() {
	local target_libs_folder="ddnet-libs/sqlite3/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/sqlite3/"$1"/sqlite3.a "$target_libs_folder"/libsqlite3.a
}
copy_libs_for_arches _copy_sqlite3

log_info "Done."
