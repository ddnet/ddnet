#!/bin/bash
set -e

COLOR_RED="\e[1;31m"
COLOR_YELLOW="\e[1;33m"
COLOR_CYAN="\e[1;36m"
COLOR_RESET="\e[0m"

function log_info() {
	printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "$1"
}

function log_warn() {
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "$1" 1>&2
}

function log_error() {
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "$1" 1>&2
}

function log_info_header() {
	local header="$1"
	local length=$((${#header} + 4))
	local border
	border=$(printf '%*s' "$length" '' | tr ' ' '#')
	printf "\n"
	log_info "$border"
	log_info "# $header #"
	log_info "$border"
}

case "$(uname -s)" in
Linux)
	HOST_OS=linux
	;;
MINGW* | MSYS*)
	HOST_OS=windows
	;;
Darwin)
	HOST_OS=darwin
	;;
*)
	log_error "ERROR: Host OS unsupported: $(uname -s)"
	exit 1
	;;
esac
export HOST_OS

if [[ "${HOST_OS}" == "windows" ]]; then
	# Ensure that binaries from MSYS2 are preferred over Windows-native commands like find and sort, which work differently.
	PATH="/usr/bin/:$PATH"
	# Path separator is different on Windows.
	PATH_SEPARATOR=":"
	# To convert MSYS2 paths to Windows paths with forward slashes that match compiler-internal paths.
	PATH_WRAPPER="cygpath -m"
else
	PATH_SEPARATOR=";"
	PATH_WRAPPER="echo"
fi
export PATH_SEPARATOR
export PATH_WRAPPER

# $ANDROID_HOME can be user-defined, else the default location is used. Important notes:
# - The path must not contain spaces on Windows.
# - $HOME must be used instead of ~ else cargo-ndk cannot find the folder.
ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
export ANDROID_HOME

export ANDROID_NDK_FOUND=0
if [ -d "$ANDROID_HOME/ndk" ]; then
	ANDROID_NDK_ROOT="$(find "${ANDROID_HOME}/ndk" -mindepth 1 -maxdepth 1 | sort -n | tail -1)"
	if [ -n "$ANDROID_NDK_ROOT" ]; then
		export ANDROID_NDK_ROOT
		# ANDROID_NDK_HOME must be exported for cargo-ndk
		export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
		export ANDROID_TOOLCHAIN_ROOT="${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/${HOST_OS}-x86_64"
		ANDROID_NDK_FOUND=1
	fi
fi

# ANDROID_API must specify the _minimum_ supported SDK version, otherwise this will cause linking errors at launch.
# Reason for minimum API 24: the NDK does not support `_FILE_OFFSET_BITS=64` until API 24 so functions like `fseeko` are missing.
export ANDROID_API=24

export ANDROID_ARM_ABI="armeabi-v7a"
export ANDROID_ARM64_ABI="arm64-v8a"
export ANDROID_X86_ABI="x86"
export ANDROID_X64_ABI="x86_64"

export ANDROID_ARM_TRIPLE="armv7a-linux-androideabi"
export ANDROID_ARM64_TRIPLE="aarch64-linux-android"
export ANDROID_X86_TRIPLE="i686-linux-android"
export ANDROID_X64_TRIPLE="x86_64-linux-android"

export ANDROID_ARM_BUILD_FOLDER="build_android_arm"
export ANDROID_ARM64_BUILD_FOLDER="build_android_arm64"
export ANDROID_X86_BUILD_FOLDER="build_android_x86"
export ANDROID_X64_BUILD_FOLDER="build_android_x86_64"

# Refer to https://android.googlesource.com/platform/ndk/+/master/docs/BuildSystemMaintainers.md and
# build/cmake/android-legacy.toolchain.cmake in the Android NDK. These flags must be updated together
# with the NDK for libraries that cannot make use of the CMake toolchain yet.
export ANDROID_COMMON_CFLAGS="-g -DANDROID -fdata-sections -ffunction-sections -funwind-tables \
	-fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security \
	-fPIC -O3 -DNDEBUG"
export ANDROID_ARM_CFLAGS="${ANDROID_COMMON_CFLAGS} \
	-march=armv7-a \
	-mthumb"
export ANDROID_ARM64_CFLAGS="${ANDROID_COMMON_CFLAGS}"
export ANDROID_X86_CFLAGS="${ANDROID_COMMON_CFLAGS}"
export ANDROID_X64_CFLAGS="${ANDROID_COMMON_CFLAGS}"
export ANDROID_EXTRA_RELEASE_CFLAGS="-g0"

export EMSCRIPTEN_WASM_BUILD_FOLDER="build_webasm_wasm"

# Refer to https://emscripten.org/docs/tools_reference/settings_reference.html
export EMSCRIPTEN_WASM_CFLAGS="-pthread -O3 -g -s USE_PTHREADS=1"
export EMSCRIPTEN_WASM_LDFLAGS="-pthread -O3 -g -s USE_PTHREADS=1 -s ASYNCIFY=1 -s WASM=1"
export EMSCRIPTEN_EXTRA_RELEASE_CFLAGS="-g0"

BUILD_FLAGS="${BUILD_FLAGS:--j$(nproc)}"
export BUILD_FLAGS

# For reproducible builds, zero all timestamps. See https://reproducible-builds.org/docs/source-date-epoch/
export SOURCE_DATE_EPOCH=0

function assert_android_ndk_found() {
	if [ $ANDROID_NDK_FOUND == 0 ]; then
		log_error "ERROR: Android NDK was not found. Expected at this location: $ANDROID_HOME/ndk"
		exit 1
	fi
}

function assert_emscripten_sdk_found() {
	if [ -z ${EMSDK+x} ]; then
		log_error "ERROR: Emscripten SDK was not found. Expected EMSDK environment variable to be set."
		log_error "Run 'source ~/emsdk/emsdk_env.sh' with the path to the Emscripten SDK."
		exit 1
	fi
}
