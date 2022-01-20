#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"
echo "$ANDROID_NDK"

export MAKEFLAGS=-j32

function compile_source() {
	cmake \
		-H. \
		-G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DANDROID_NATIVE_API_LEVEL="android-$1" \
		-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
		-DANDROID_ABI="${3}" \
		-DANDROID_ARM_NEON=TRUE \
		-B"$2" \
		-DBUILD_SHARED_LIBS=OFF \
		-DHIDAPI_SKIP_LIBUSB=TRUE \
		-DCURL_USE_OPENSSL=ON \
		-DSDL_HIDAPI=OFF \
		-DOP_DISABLE_HTTP=ON \
		-DOP_DISABLE_EXAMPLES=ON \
		-DOP_DISABLE_DOCS=ON \
		-DOPENSSL_ROOT_DIR="$PWD"/../openssl/"$2" \
		-DOPENSSL_CRYPTO_LIBRARY="$PWD"/../openssl/"$2"/libcrypto.a \
		-DOPENSSL_SSL_LIBRARY="$PWD"/../openssl/"$2"/libssl.a \
		-DOPENSSL_INCLUDE_DIR="${PWD}/../openssl/include;${PWD}/../openssl/${2}/include"
	(
		cd "$2" || exit 1
		cmake --build .
	)
}

compile_source "$1" build_android_arm armeabi-v7a &
compile_source "$1" build_android_arm64 arm64-v8a &
compile_source "$1" build_android_x86 x86 &
compile_source "$1" build_android_x86_64 x86_64 &

wait
