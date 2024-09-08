#!/bin/bash

# $HOME must be used instead of ~ else cargo-ndk cannot find the folder
export ANDROID_HOME=$HOME/Android/Sdk
MAKEFLAGS=-j$(nproc)
export MAKEFLAGS

ANDROID_NDK_VERSION="$(cd "$ANDROID_HOME/ndk" && find . -maxdepth 1 | sort -n | tail -1)"
ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:2}"
# ANDROID_NDK_HOME must be exported for cargo-ndk
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION"

# ANDROID_API_LEVEL must specify the _minimum_ supported SDK version, otherwise this will cause linking errors at launch
ANDROID_API_LEVEL=24
ANDROID_SUB_BUILD_DIR=build_arch

COLOR_RED="\e[1;31m"
COLOR_YELLOW="\e[1;33m"
COLOR_CYAN="\e[1;36m"
COLOR_RESET="\e[0m"

SHOW_USAGE_INFO=0

if [ -z ${1+x} ]; then
	SHOW_USAGE_INFO=1
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass Android build type"
else
	ANDROID_BUILD=$1
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Android build type: ${ANDROID_BUILD}"
fi

if [ -z ${2+x} ]; then
	SHOW_USAGE_INFO=1
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass game name"
else
	GAME_NAME=$2
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Game name: ${GAME_NAME}"
fi

if [ -z ${3+x} ]; then
	SHOW_USAGE_INFO=1
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass package name"
else
	PACKAGE_NAME=$3
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Package name: ${PACKAGE_NAME}"
fi

if [ -z ${4+x} ]; then
	SHOW_USAGE_INFO=1
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build type"
else
	BUILD_TYPE=$4
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Build type: ${BUILD_TYPE}"
fi

if [ -z ${5+x} ]; then
	SHOW_USAGE_INFO=1
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build folder"
else
	BUILD_FOLDER=$5
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Build folder: ${BUILD_FOLDER}"
fi

if [ $SHOW_USAGE_INFO == 1 ]; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Usage: ./cmake_android.sh <x86/x86_64/arm/arm64/all> <Game name> <Package name> <Debug/Release> <Build folder>"
	exit 1
fi

# These are the properties of the default Android debug key. The debug key will
# automatically be created during the Gradle build if it does not exist already,
# so you don't need to create it yourself.
DEFAULT_KEY_NAME=~/.android/debug.keystore
DEFAULT_KEY_PW=android
DEFAULT_KEY_ALIAS=androiddebugkey

if [ -z ${TW_KEY_NAME+x} ]; then
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Did not pass a key path for the APK signer, using default: ${DEFAULT_KEY_NAME}"
else
	DEFAULT_KEY_NAME=$TW_KEY_NAME
fi
if [ -z ${TW_KEY_PW+x} ]; then
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Did not pass a key password for the APK signer, using default: ${DEFAULT_KEY_PW}"
else
	DEFAULT_KEY_PW=$TW_KEY_PW
fi
if [ -z ${TW_KEY_ALIAS+x} ]; then
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Did not pass a key alias for the APK signer, using default: ${DEFAULT_KEY_ALIAS}"
else
	DEFAULT_KEY_ALIAS=$TW_KEY_ALIAS
fi

export TW_KEY_NAME="${DEFAULT_KEY_NAME}"
export TW_KEY_PW=$DEFAULT_KEY_PW
export TW_KEY_ALIAS=$DEFAULT_KEY_ALIAS

ANDROID_VERSION_CODE=1
if [ -z ${TW_VERSION_CODE+x} ]; then
	ANDROID_VERSION_CODE=$(grep '#define DDNET_VERSION_NUMBER' src/game/version.h | awk '{print $3}')
	if [ -z ${ANDROID_VERSION_CODE+x} ]; then
		ANDROID_VERSION_CODE=1
	fi
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Did not pass a version code, using default: ${ANDROID_VERSION_CODE}"
else
	ANDROID_VERSION_CODE=$TW_VERSION_CODE
fi

export TW_VERSION_CODE=$ANDROID_VERSION_CODE

ANDROID_VERSION_NAME="1.0"
if [ -z ${TW_VERSION_NAME+x} ]; then
	ANDROID_VERSION_NAME="$(grep '#define GAME_RELEASE_VERSION' src/game/version.h | awk '{print $3}' | tr -d '"')"
	if [ -z ${ANDROID_VERSION_NAME+x} ]; then
		ANDROID_VERSION_NAME="1.0"
	fi
	printf "${COLOR_YELLOW}%s${COLOR_RESET}\n" "Did not pass a version name, using default: ${ANDROID_VERSION_NAME}"
else
	ANDROID_VERSION_NAME=$TW_VERSION_NAME
fi

export TW_VERSION_NAME=$ANDROID_VERSION_NAME

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Building cmake..."

function build_for_type() {
	cmake \
		-H. \
		-G "Ninja" \
		-DPREFER_BUNDLED_LIBS=ON \
		-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		-DANDROID_PLATFORM="android-${ANDROID_API_LEVEL}" \
		-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
		-DANDROID_NDK="$ANDROID_NDK_HOME" \
		-DANDROID_ABI="${2}" \
		-DANDROID_ARM_NEON=TRUE \
		-DCMAKE_ANDROID_NDK="$ANDROID_NDK_HOME" \
		-DCMAKE_SYSTEM_NAME=Android \
		-DCMAKE_SYSTEM_VERSION="$ANDROID_API_LEVEL" \
		-DCMAKE_ANDROID_ARCH_ABI="${2}" \
		-DCARGO_NDK_TARGET="${3}" \
		-DCARGO_NDK_API="$ANDROID_API_LEVEL" \
		-B"${BUILD_FOLDER}/$ANDROID_SUB_BUILD_DIR/$1" \
		-DSERVER=OFF \
		-DTOOLS=OFF \
		-DDEV=TRUE \
		-DCMAKE_CROSSCOMPILING=ON \
		-DVULKAN=ON \
		-DVIDEORECORDER=OFF
	(
		cd "${BUILD_FOLDER}/$ANDROID_SUB_BUILD_DIR/$1" || exit 1
		cmake --build . --target game-client
	)
}

mkdir -p "${BUILD_FOLDER}"

if [[ "${ANDROID_BUILD}" == "arm" || "${ANDROID_BUILD}" == "all" ]]; then
	build_for_type arm armeabi-v7a armv7-linux-androideabi &
	PID_BUILD_ARM=$!
fi

if [[ "${ANDROID_BUILD}" == "arm64" || "${ANDROID_BUILD}" == "all" ]]; then
	build_for_type arm64 arm64-v8a aarch64-linux-android &
	PID_BUILD_ARM64=$!
fi

if [[ "${ANDROID_BUILD}" == "x86" || "${ANDROID_BUILD}" == "all" ]]; then
	build_for_type x86 x86 i686-linux-android &
	PID_BUILD_X86=$!
fi

if [[ "${ANDROID_BUILD}" == "x86_64" || "${ANDROID_BUILD}" == "x64" || "${ANDROID_BUILD}" == "all" ]]; then
	build_for_type x86_64 x86_64 x86_64-linux-android &
	PID_BUILD_X86_64=$!
fi

if [ -n "$PID_BUILD_ARM" ] && ! wait "$PID_BUILD_ARM"; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Building for arm failed"
	exit 1
fi
if [ -n "$PID_BUILD_ARM64" ] && ! wait "$PID_BUILD_ARM64"; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Building for arm64 failed"
	exit 1
fi
if [ -n "$PID_BUILD_X86" ] && ! wait "$PID_BUILD_X86"; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Building for x86 failed"
	exit 1
fi
if [ -n "$PID_BUILD_X86_64" ] && ! wait "$PID_BUILD_X86_64"; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Building for x86_64 failed"
	exit 1
fi

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Copying project files..."

cd "${BUILD_FOLDER}" || exit 1

mkdir -p src/main
mkdir -p src/main/res/values
mkdir -p src/main/res/xml
mkdir -p src/main/res/mipmap

function copy_dummy_files() {
	rm -f ./"$2"
	cp ../"$1" "$2"
}

copy_dummy_files scripts/android/files/build.sh build.sh
copy_dummy_files scripts/android/files/gradle-wrapper.jar gradle-wrapper.jar
copy_dummy_files scripts/android/files/build.gradle build.gradle
copy_dummy_files scripts/android/files/gradle-wrapper.properties gradle-wrapper.properties
copy_dummy_files scripts/android/files/gradle.properties gradle.properties
copy_dummy_files scripts/android/files/proguard-rules.pro proguard-rules.pro
copy_dummy_files scripts/android/files/settings.gradle settings.gradle
copy_dummy_files scripts/android/files/AndroidManifest.xml src/main/AndroidManifest.xml
copy_dummy_files scripts/android/files/res/values/strings.xml src/main/res/values/strings.xml
copy_dummy_files scripts/android/files/res/xml/shortcuts.xml src/main/res/xml/shortcuts.xml
copy_dummy_files other/icons/DDNet_256x256x32.png src/main/res/mipmap/ic_launcher.png
copy_dummy_files other/icons/DDNet_256x256x32.png src/main/res/mipmap/ic_launcher_round.png

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Copying libraries..."

function copy_libs() {
	mkdir -p "lib/$2"
	cp "$ANDROID_SUB_BUILD_DIR/$1/libDDNet.so" "lib/$2" || exit 1
}

if [[ "${ANDROID_BUILD}" == "arm" || "${ANDROID_BUILD}" == "all" ]]; then
	copy_libs arm armeabi-v7a
fi

if [[ "${ANDROID_BUILD}" == "arm64" || "${ANDROID_BUILD}" == "all" ]]; then
	copy_libs arm64 arm64-v8a
fi

if [[ "${ANDROID_BUILD}" == "x86" || "${ANDROID_BUILD}" == "all" ]]; then
	copy_libs x86 x86
fi

if [[ "${ANDROID_BUILD}" == "x86_64" || "${ANDROID_BUILD}" == "x64" || "${ANDROID_BUILD}" == "all" ]]; then
	copy_libs x86_64 x86_64
fi

ANDROID_BUILD_DUMMY=$ANDROID_BUILD
if [[ "${ANDROID_BUILD}" == "all" ]]; then
	ANDROID_BUILD_DUMMY=arm
fi

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Copying data folder..."
mkdir -p assets/asset_integrity_files
cp -R "$ANDROID_SUB_BUILD_DIR/$ANDROID_BUILD_DUMMY/data" ./assets/asset_integrity_files

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Downloading certificate..."
curl -s -S --remote-name --time-cond cacert.pem https://curl.se/ca/cacert.pem
cp ./cacert.pem ./assets/asset_integrity_files/data/cacert.pem || exit 1

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Creating integrity index file..."
(
	cd assets/asset_integrity_files || exit 1

	tmpfile="$(mktemp /tmp/hash_strings.XXX)"

	find data -iname "*" -type f -print0 | while IFS= read -r -d $'\0' file; do
		sha_hash=$(sha256sum "$file" | cut -d' ' -f 1)
		echo "$file $sha_hash" >> "$tmpfile"
	done

	full_hash="$(sha256sum "$tmpfile" | cut -d' ' -f 1)"

	rm -f "integrity.txt"
	{
		echo "$full_hash"
		cat "$tmpfile"
	} > "integrity.txt"
)

printf "${COLOR_CYAN}%s${COLOR_RESET}\n" "Preparing gradle build..."

rm -R -f src/main/java/org
mkdir -p src/main/java
cp -R ../scripts/android/files/java/org src/main/java/
cp -R ../ddnet-libs/sdl/java/org src/main/java/

# shellcheck disable=SC1091
source ./build.sh "$GAME_NAME" "$PACKAGE_NAME" "$BUILD_TYPE"

cd ..
