#!/bin/bash
set -e

# shellcheck source=scripts/compile_libs/sources.sh
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)/sources.sh"

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

# iOS (device + simulator)
export IOS_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-15.0}"
export IOS_COMMON_CFLAGS="-O3 -DNDEBUG -g0"

export IOS_DEVICE_ARCH="arm64"
export IOS_SIM_ARM64_ARCH="arm64"
export IOS_SIM_X64_ARCH="x86_64"

export IOS_DEVICE_BUILD_FOLDER="build_ios_device"
export IOS_SIM_ARM64_BUILD_FOLDER="build_ios_sim_arm64"
export IOS_SIM_X64_BUILD_FOLDER="build_ios_sim_x86_64"

export EMSCRIPTEN_WASM_BUILD_FOLDER="build_webasm_wasm"

# Refer to https://emscripten.org/docs/tools_reference/settings_reference.html
export EMSCRIPTEN_WASM_CFLAGS="-pthread -O3 -g -s USE_PTHREADS=1"
export EMSCRIPTEN_WASM_LDFLAGS="-pthread -O3 -g -s USE_PTHREADS=1 -s ASYNCIFY=1 -s WASM=1"
export EMSCRIPTEN_EXTRA_RELEASE_CFLAGS="-g0"

EMSCRIPTEN_CMAKE_WRAPPER="emcmake"
EMSCRIPTEN_CC="emcc"
EMSCRIPTEN_AR="emar"
if [[ "${HOST_OS}" == "windows" ]]; then
	EMSCRIPTEN_CMAKE_WRAPPER="${EMSCRIPTEN_CMAKE_WRAPPER}.bat"
	EMSCRIPTEN_CC="${EMSCRIPTEN_CC}.bat"
	EMSCRIPTEN_AR="${EMSCRIPTEN_AR}.bat"
fi
export EMSCRIPTEN_CMAKE_WRAPPER
export EMSCRIPTEN_CC
export EMSCRIPTEN_AR

# Desktop (linux, windows, mac)
export LINUX_X64_BUILD_FOLDER="build_linux_x86_64"
export LINUX_X86_BUILD_FOLDER="build_linux_x86"
export WINDOWS_X64_BUILD_FOLDER="build_windows_x86_64"
export WINDOWS_X86_BUILD_FOLDER="build_windows_x86"
export WINDOWS_ARM64_BUILD_FOLDER="build_windows_arm64"
export MAC_X64_BUILD_FOLDER="build_mac_x86_64"
export MAC_ARM64_BUILD_FOLDER="build_mac_arm64"

export WINDOWS_X64_TRIPLE="x86_64-w64-mingw32"
export WINDOWS_X86_TRIPLE="i686-w64-mingw32"
export WINDOWS_ARM64_TRIPLE="aarch64-w64-mingw32"
export MAC_X64_TRIPLE="x86_64-apple-darwin20.1"
export MAC_ARM64_TRIPLE="aarch64-apple-darwin20.1"

export LINUX_COMMON_CFLAGS="-O3 -DNDEBUG -g0 -fPIC"
export WINDOWS_COMMON_CFLAGS="-O3 -DNDEBUG -g0"

# The deployment target the shipped binaries have always been built against.
export MAC_DEPLOYMENT_TARGET="${MAC_DEPLOYMENT_TARGET:-10.9}"
# SDL needs a newer one, its GameController code does not build against 10.9.
export MAC_SDL_DEPLOYMENT_TARGET="${MAC_SDL_DEPLOYMENT_TARGET:-10.15}"
export MAC_COMMON_CFLAGS="-O3 -DNDEBUG -g0"

# osxcross defaults to the oldest SDK it has installed, which is too old for SDL.
# Both must be set to the same SDK, the wrapper reads OSXCROSS_SDK and cmake reads OSXCROSS_SDKROOT.
export OSXCROSS_SDK_VERSION="${OSXCROSS_SDK_VERSION:-15.4}"

function assert_linux_toolchain_found() {
	if [[ "${HOST_OS}" != "linux" ]]; then
		log_error "ERROR: Linux libraries must be built on Linux."
		exit 1
	fi
	if ! command -v nasm > /dev/null 2>&1; then
		log_error "ERROR: nasm was not found, x264 cannot be built without it."
		exit 1
	fi
	# The 32 bit lane needs the multilib compiler and the i386 -dev packages.
	if ! echo 'int main(void){return 0;}' | cc -m32 -x c - -o /dev/null > /dev/null 2>&1; then
		log_error "ERROR: The compiler cannot produce 32 bit binaries."
		log_error "Install gcc-multilib and the :i386 development packages, see docs/BUILDING-desktop-libs.md."
		exit 1
	fi
}

function assert_mingw_found() {
	local triple="$1"
	if ! command -v "${triple}-gcc" > /dev/null 2>&1 && ! command -v "${triple}-clang" > /dev/null 2>&1; then
		log_error "ERROR: No mingw-w64 toolchain for ${triple} was found."
		log_error "Install mingw-w64 (x86_64 and i686) or llvm-mingw (aarch64), see docs/BUILDING-desktop-libs.md."
		exit 1
	fi
	if ! command -v nasm > /dev/null 2>&1; then
		log_error "ERROR: nasm was not found, x264 cannot be built without it."
		exit 1
	fi
}

function assert_osxcross_found() {
	if ! command -v "${MAC_X64_TRIPLE}-clang" > /dev/null 2>&1; then
		log_error "ERROR: osxcross was not found. Expected ${MAC_X64_TRIPLE}-clang on PATH."
		log_error "Add the osxcross target/bin folder to PATH, see docs/BUILDING-desktop-libs.md."
		exit 1
	fi
	if [ -z ${OSXCROSS_SDK+x} ]; then
		local osxcross_bin
		osxcross_bin="$(dirname "$(command -v "${MAC_X64_TRIPLE}-clang")")"
		OSXCROSS_SDK="$(dirname "${osxcross_bin}")/SDK/MacOSX${OSXCROSS_SDK_VERSION}.sdk"
		export OSXCROSS_SDK
	fi
	if [ ! -d "${OSXCROSS_SDK}" ]; then
		log_error "ERROR: macOS SDK ${OSXCROSS_SDK_VERSION} was not found at ${OSXCROSS_SDK}."
		log_error "Set OSXCROSS_SDK, or OSXCROSS_SDK_VERSION to an SDK that is installed."
		exit 1
	fi
	export OSXCROSS_SDKROOT="${OSXCROSS_SDK}"
}

# Sets DESKTOP_CC and DESKTOP_AR for the given build folder, for the libraries that
# have no CMake build and are compiled through a generated Makefile instead.
function desktop_toolchain_for() {
	local build_folder="$1"
	case "${build_folder}" in
	"${LINUX_X64_BUILD_FOLDER}" | "${LINUX_X86_BUILD_FOLDER}")
		DESKTOP_CC="cc"
		DESKTOP_AR="ar"
		;;
	"${WINDOWS_X64_BUILD_FOLDER}")
		DESKTOP_CC="${WINDOWS_X64_TRIPLE}-gcc"
		DESKTOP_AR="${WINDOWS_X64_TRIPLE}-ar"
		;;
	"${WINDOWS_X86_BUILD_FOLDER}")
		DESKTOP_CC="${WINDOWS_X86_TRIPLE}-gcc"
		DESKTOP_AR="${WINDOWS_X86_TRIPLE}-ar"
		;;
	"${WINDOWS_ARM64_BUILD_FOLDER}")
		# llvm-mingw ships clang rather than gcc
		DESKTOP_CC="${WINDOWS_ARM64_TRIPLE}-clang"
		DESKTOP_AR="${WINDOWS_ARM64_TRIPLE}-ar"
		;;
	"${MAC_X64_BUILD_FOLDER}")
		DESKTOP_CC="${MAC_X64_TRIPLE}-clang"
		DESKTOP_AR="${MAC_X64_TRIPLE}-ar"
		;;
	"${MAC_ARM64_BUILD_FOLDER}")
		DESKTOP_CC="${MAC_ARM64_TRIPLE}-clang"
		DESKTOP_AR="${MAC_ARM64_TRIPLE}-ar"
		;;
	*)
		log_error "ERROR: No desktop toolchain known for ${build_folder}"
		exit 1
		;;
	esac
	export DESKTOP_CC DESKTOP_AR
}

# The cross compilation triple for a desktop build folder, for the libraries that are
# configured with their own scripts rather than with CMake.
function desktop_triple_for() {
	case "$1" in
	"${WINDOWS_X64_BUILD_FOLDER}") echo "${WINDOWS_X64_TRIPLE}" ;;
	"${WINDOWS_X86_BUILD_FOLDER}") echo "${WINDOWS_X86_TRIPLE}" ;;
	"${WINDOWS_ARM64_BUILD_FOLDER}") echo "${WINDOWS_ARM64_TRIPLE}" ;;
	"${MAC_X64_BUILD_FOLDER}") echo "${MAC_X64_TRIPLE}" ;;
	"${MAC_ARM64_BUILD_FOLDER}") echo "${MAC_ARM64_TRIPLE}" ;;
	*)
		log_error "ERROR: No triple known for $1"
		exit 1
		;;
	esac
}

# The architecture name ffmpeg's configure expects for a desktop build folder.
function ffmpeg_arch_for() {
	case "$1" in
	"${LINUX_X64_BUILD_FOLDER}" | "${WINDOWS_X64_BUILD_FOLDER}" | "${MAC_X64_BUILD_FOLDER}") echo "x86_64" ;;
	"${LINUX_X86_BUILD_FOLDER}" | "${WINDOWS_X86_BUILD_FOLDER}") echo "i686" ;;
	"${WINDOWS_ARM64_BUILD_FOLDER}") echo "aarch64" ;;
	"${MAC_ARM64_BUILD_FOLDER}") echo "aarch64" ;;
	*)
		log_error "ERROR: No ffmpeg architecture known for $1"
		exit 1
		;;
	esac
}

# The processor name cmake expects for a desktop build folder. Cross compiling leaves
# CMAKE_SYSTEM_PROCESSOR unset otherwise, and libraries that select their assembly
# from it then build the wrong sources or none at all.
function cmake_processor_for() {
	case "$1" in
	"${LINUX_X64_BUILD_FOLDER}" | "${WINDOWS_X64_BUILD_FOLDER}" | "${MAC_X64_BUILD_FOLDER}") echo "x86_64" ;;
	"${LINUX_X86_BUILD_FOLDER}" | "${WINDOWS_X86_BUILD_FOLDER}") echo "x86" ;;
	"${WINDOWS_ARM64_BUILD_FOLDER}") echo "aarch64" ;;
	"${MAC_ARM64_BUILD_FOLDER}") echo "arm64" ;;
	*)
		log_error "ERROR: No cmake processor known for $1"
		exit 1
		;;
	esac
}

# Extra compiler flags the desktop platforms need on top of the common ones.
function desktop_cflags_for() {
	local build_folder="$1"
	local flags=""
	case "${build_folder}" in
	"${LINUX_X64_BUILD_FOLDER}")
		flags="${LINUX_COMMON_CFLAGS}"
		;;
	"${LINUX_X86_BUILD_FOLDER}")
		flags="${LINUX_COMMON_CFLAGS} -m32"
		;;
	"${WINDOWS_X64_BUILD_FOLDER}" | "${WINDOWS_X86_BUILD_FOLDER}" | "${WINDOWS_ARM64_BUILD_FOLDER}")
		flags="${WINDOWS_COMMON_CFLAGS}"
		;;
	"${MAC_X64_BUILD_FOLDER}" | "${MAC_ARM64_BUILD_FOLDER}")
		flags="${MAC_COMMON_CFLAGS} -mmacosx-version-min=${MAC_DEPLOYMENT_TARGET} -isysroot ${OSXCROSS_SDK}"
		;;
	esac
	echo "${flags}"
}

# Creates the import library the Windows build links against. mingw's own
# libfoo.dll.a is not usable when DDNet itself is built with MSVC, so this reads the
# export table back out of the finished DLL and writes an MSVC style .lib from it.
function make_windows_import_lib() {
	local triple="$1"
	local dll_path="$2"
	local output_lib="$3"
	local dll_name
	dll_name="$(basename "${dll_path}")"
	(
		cd "$(dirname "${dll_path}")"
		gendef "${dll_name}" > /dev/null
		"${triple}-dlltool" -d "${dll_name%.dll}.def" -D "${dll_name}" -l "${output_lib}"
		rm -f "${dll_name%.dll}.def"
	)
}

# osxcross builds compiler-rt but does not install it into the clang resource
# directory, so the builtins that @available checks lower to cannot be found on their
# own. SDL is the library that needs them.
function osxcross_compiler_rt_dir() {
	local arch="$1"
	local osxcross_bin osxcross_root
	osxcross_bin="$(dirname "$(command -v "${MAC_X64_TRIPLE}-clang")")"
	osxcross_root="$(dirname "$(dirname "${osxcross_bin}")")"
	echo "${osxcross_root}/build/compiler-rt/compiler-rt/build_${arch}/lib/darwin"
}

function sha256_of() {
	if command -v sha256sum > /dev/null 2>&1; then
		sha256sum "$1" | cut -d" " -f1
	else
		shasum -a 256 "$1" | cut -d" " -f1 # fallback for macOS
	fi
}

# Downloads $1 to $2 and aborts unless it hashes to $3. Downloading to a temporary
# name first means an interrupted run cannot leave a truncated file that a later
# run mistakes for a complete download.
function download_verified() {
	local url="$1"
	local filename="$2"
	local expected_sha256="$3"
	if [ ! -f "${filename}" ]; then
		log_info "Downloading ${filename}..."
		wget --no-verbose -O "${filename}.part" "${url}"
		mv "${filename}.part" "${filename}"
	fi
	local actual_sha256
	actual_sha256="$(sha256_of "${filename}")"
	if [[ "${actual_sha256}" != "${expected_sha256}" ]]; then
		log_error "ERROR: Checksum mismatch for ${filename} downloaded from ${url}"
		log_error "Expected: ${expected_sha256}"
		log_error "Actual:   ${actual_sha256}"
		log_error "Delete the file to download it again. If the mismatch persists, the"
		log_error "upstream archive changed and must be reviewed before it is trusted."
		exit 1
	fi
}

# libtoolize looks for gm4, gnum4 or m4 through this variable and does not fall back
# to a plain m4 on the PATH, which is what opusfile's autogen.sh trips over.
if [ -z ${M4+x} ] && command -v m4 > /dev/null 2>&1; then
	M4="$(command -v m4)"
	export M4
fi

function cpu_count() {
	if command -v nproc > /dev/null 2>&1; then
		nproc
	elif command -v sysctl > /dev/null 2>&1; then
		sysctl -n hw.ncpu # fallback for macOS
	else
		echo 4
	fi
}

BUILD_FLAGS="${BUILD_FLAGS:--j$(cpu_count)}"
export BUILD_FLAGS

# For reproducible builds, zero all timestamps. See https://reproducible-builds.org/docs/source-date-epoch/
export SOURCE_DATE_EPOCH=0
# https://blog.conan.io/2019/09/02/Deterministic-builds-with-C-C++.html
export ZERO_AR_DATE=1

function assert_android_ndk_found() {
	if [ $ANDROID_NDK_FOUND == 0 ]; then
		log_error "ERROR: Android NDK was not found. Expected at this location: $ANDROID_HOME/ndk"
		exit 1
	fi
}

function assert_ios_sdk_found() {
	if [[ "${HOST_OS}" != "darwin" ]]; then
		log_error "ERROR: iOS builds require macOS."
		exit 1
	fi
	if ! command -v xcrun > /dev/null 2>&1; then
		log_error "ERROR: Xcode command line tools were not found. Install Xcode or the CLT."
		exit 1
	fi
	if ! xcrun --sdk iphoneos --show-sdk-path > /dev/null 2>&1; then
		log_error "ERROR: iOS SDK not found. Ensure Xcode is installed and selected."
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
