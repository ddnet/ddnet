#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/../compile_libs/_build_common.sh"

assert_android_ndk_found

ANDROID_SUB_BUILD_DIR=build_arch

SHOW_USAGE_INFO=0

if [ -z ${1+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass Android build type"
else
	ANDROID_BUILD=$1
	if [[ "${ANDROID_BUILD}" == "x64" ]]; then
		ANDROID_BUILD="x86_64"
	fi
	log_warn "Android build type: ${ANDROID_BUILD}"
fi

if [ -z ${2+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass game name"
else
	GAME_NAME=$2
	log_warn "Game name: ${GAME_NAME}"
fi

if [ -z ${3+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass package name"
else
	PACKAGE_NAME=$3
	log_warn "Package name: ${PACKAGE_NAME}"
fi

if [ -z ${4+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass build type"
else
	BUILD_TYPE=$4
	log_warn "Build type: ${BUILD_TYPE}"
fi

if [ -z ${5+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass build folder"
else
	BUILD_FOLDER=$5
	log_warn "Build folder: ${BUILD_FOLDER}"
fi

if [ $SHOW_USAGE_INFO == 1 ]; then
	log_error "Usage: scripts/cmake_android.sh <x86/x86_64/arm/arm64/all> <Game name> <Package name> <Debug/Release> <Build folder>"
	exit 1
fi

# These are the properties of the default Android debug key. The debug key will
# automatically be created during the Gradle build if it does not exist already,
# so you don't need to create it yourself.
DEFAULT_KEY_NAME=~/.android/debug.keystore
DEFAULT_KEY_PW=android
DEFAULT_KEY_ALIAS=androiddebugkey

if [ -z ${TW_KEY_NAME+x} ]; then
	log_warn "Did not pass a key path for the APK signer, using default: ${DEFAULT_KEY_NAME}"
else
	DEFAULT_KEY_NAME=$TW_KEY_NAME
fi
if [ -z ${TW_KEY_PW+x} ]; then
	log_warn "Did not pass a key password for the APK signer, using default: ${DEFAULT_KEY_PW}"
else
	DEFAULT_KEY_PW=$TW_KEY_PW
fi
if [ -z ${TW_KEY_ALIAS+x} ]; then
	log_warn "Did not pass a key alias for the APK signer, using default: ${DEFAULT_KEY_ALIAS}"
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
	log_warn "Did not pass a version code, using default: ${ANDROID_VERSION_CODE}"
else
	ANDROID_VERSION_CODE=$TW_VERSION_CODE
fi

export TW_VERSION_CODE=$ANDROID_VERSION_CODE

ANDROID_VERSION_NAME="1.0"
if [ -z ${TW_VERSION_NAME+x} ]; then
	ANDROID_VERSION_NAME="$(grep '#define GAME_RELEASE_VERSION_INTERNAL' src/game/version.h | awk '{print $3}')"
	if [ -z ${ANDROID_VERSION_NAME+x} ]; then
		ANDROID_VERSION_NAME="1.0"
	fi
	log_warn "Did not pass a version name, using default: ${ANDROID_VERSION_NAME}"
else
	ANDROID_VERSION_NAME=$TW_VERSION_NAME
fi

export TW_VERSION_NAME=$ANDROID_VERSION_NAME

if [ -z ${SOURCE_DATE_EPOCH+x} ]; then
	if SOURCE_DATE_EPOCH="$(git log -1 --format=%ct 2> /dev/null)"; then
		export SOURCE_DATE_EPOCH
	elif [ -e source_date_epoch ]; then
		SOURCE_DATE_EPOCH="$(cat source_date_epoch)"
		export SOURCE_DATE_EPOCH
	else
		unset SOURCE_DATE_EPOCH
		if [ -z ${TW_ALLOW_NON_REPRODUCIBLE_BUILD+x} ]; then
			log_warn "Building non-reproducibly"
		else
			log_error "Cannot build reproducibly: Source directory not in git repository, not from an official source download and \`SOURCE_DATE_EPOCH\` is unset."
			log_error "Set \`TW_ALLOW_NON_REPRODUCIBLE_BUILD=1\` to build unreproducibly and ignore this check."
			exit 1
		fi
	fi
fi

function build_for_type() {
	# Remove absolute build paths from binary
	build_extra_cflags="-ffile-prefix-map=${ANDROID_TOOLCHAIN_ROOT}=ANDROID_TOOLCHAIN_ROOT"
	if [[ "${BUILD_TYPE}" == "Release" ]]; then
		build_extra_cflags="${build_extra_cflags} ${ANDROID_EXTRA_RELEASE_CFLAGS}"
	fi

	cmake \
		-H. \
		-G "Ninja" \
		-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
		-B"${BUILD_FOLDER}/$ANDROID_SUB_BUILD_DIR/$1" \
		-DCMAKE_C_FLAGS="${build_extra_cflags}" \
		-DCMAKE_CXX_FLAGS="${build_extra_cflags}" \
		-DCMAKE_ASM_FLAGS="${build_extra_cflags}" \
		-DANDROID_PLATFORM="android-${ANDROID_API}" \
		-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
		-DANDROID_NDK="$ANDROID_NDK_HOME" \
		-DANDROID_ABI="${2}" \
		-DANDROID_ARM_NEON=ON \
		-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
		-DCMAKE_ANDROID_NDK="$ANDROID_NDK_HOME" \
		-DCMAKE_SYSTEM_NAME=Android \
		-DCMAKE_SYSTEM_VERSION="$ANDROID_API" \
		-DCMAKE_ANDROID_ARCH_ABI="${2}" \
		-DCARGO_NDK_TARGET="${3}" \
		-DCARGO_NDK_API="$ANDROID_API" \
		-DANDROID_PACKAGE_NAME="${PACKAGE_NAME}" \
		-DANDROID_PACKAGE_NAME_JNI="${PACKAGE_NAME//./_}" \
		-DPREFER_BUNDLED_LIBS=ON \
		-DSERVER=ON \
		-DTOOLS=OFF \
		-DVULKAN=ON \
		-DVIDEORECORDER=OFF
	(
		cd "${BUILD_FOLDER}/$ANDROID_SUB_BUILD_DIR/$1"
		# We want word splitting
		# shellcheck disable=SC2086
		cmake --build . --target game-client game-server $BUILD_FLAGS
	)
}

mkdir -p "${BUILD_FOLDER}"

if [[ "${ANDROID_BUILD}" == "arm" || "${ANDROID_BUILD}" == "all" ]]; then
	log_info "Building cmake (arm)..."
	build_for_type arm armeabi-v7a armv7-linux-androideabi
fi

if [[ "${ANDROID_BUILD}" == "arm64" || "${ANDROID_BUILD}" == "all" ]]; then
	log_info "Building cmake (arm64)..."
	build_for_type arm64 arm64-v8a aarch64-linux-android
fi

if [[ "${ANDROID_BUILD}" == "x86" || "${ANDROID_BUILD}" == "all" ]]; then
	log_info "Building cmake (x86)..."
	build_for_type x86 x86 i686-linux-android
fi

if [[ "${ANDROID_BUILD}" == "x86_64" || "${ANDROID_BUILD}" == "all" ]]; then
	log_info "Building cmake (x86_64)..."
	build_for_type x86_64 x86_64 x86_64-linux-android
fi

log_info "Copying project files..."

cd "${BUILD_FOLDER}"

mkdir -p src/main
mkdir -p gradle/wrapper

function copy_project_files() {
	rm -f ./"$2"
	cp ../"$1" "$2"
}

copy_project_files scripts/android/files/build.sh build.sh
copy_project_files scripts/android/files/build.gradle build.gradle
copy_project_files scripts/android/files/gradlew gradlew
copy_project_files scripts/android/files/gradlew.bat gradlew.bat
copy_project_files scripts/android/files/gradle/wrapper/gradle-wrapper.jar gradle/wrapper/gradle-wrapper.jar
copy_project_files scripts/android/files/gradle/wrapper/gradle-wrapper.properties gradle/wrapper/gradle-wrapper.properties
copy_project_files scripts/android/files/gradle.properties gradle.properties
copy_project_files scripts/android/files/proguard-rules.pro proguard-rules.pro
copy_project_files scripts/android/files/settings.gradle settings.gradle
copy_project_files scripts/android/files/AndroidManifest.xml src/main/AndroidManifest.xml

rm -rf src/main/res
cp -R ../scripts/android/files/res src/main/
mkdir -p src/main/res/mipmap
cp ../other/icons/DDNet_256x256x32.png src/main/res/mipmap/ic_launcher.png
cp ../other/icons/DDNet_256x256x32.png src/main/res/mipmap/ic_launcher_round.png
chmod +x ./gradlew

log_info "Copying libraries..."

function copy_libs() {
	mkdir -p "lib/$2"
	cp "$ANDROID_SUB_BUILD_DIR/$1/libDDNet.so" "lib/$2"
	cp "$ANDROID_SUB_BUILD_DIR/$1/libDDNet-Server.so" "lib/$2"
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

if [[ "${ANDROID_BUILD}" == "x86_64" || "${ANDROID_BUILD}" == "all" ]]; then
	copy_libs x86_64 x86_64
fi

ANDROID_BUILD_DUMMY=$ANDROID_BUILD
if [[ "${ANDROID_BUILD}" == "all" ]]; then
	ANDROID_BUILD_DUMMY=arm
fi

log_info "Copying data folder..."
rm -rf assets/asset_integrity_files/data
mkdir -p assets/asset_integrity_files
cp -R "$ANDROID_SUB_BUILD_DIR/$ANDROID_BUILD_DUMMY/data" ./assets/asset_integrity_files

log_info "Creating integrity index file..."
python3 "${SCRIPT_DIR}/generate_asset_integrity_index.py"

log_info "Preparing gradle build..."
rm -rf src/main/java/org
mkdir -p src/main/java
cp -R ../scripts/android/files/java/org src/main/java/
cp -R ../ddnet-libs/sdl/java/org src/main/java/
# shellcheck source=scripts/android/files/build.sh
source ./build.sh "$GAME_NAME" "$PACKAGE_NAME" "$BUILD_TYPE"
cd ..
