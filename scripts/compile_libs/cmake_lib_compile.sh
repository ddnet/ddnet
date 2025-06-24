#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK_HOME="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"
export ANDROID_NDK_HOME

MAKEFLAGS=-j$(nproc)
export MAKEFLAGS

if [[ "${2}" == "webasm" ]]; then
	COMPILEFLAGS="-pthread -O3 -g -s USE_PTHREADS=1"
	LINKFLAGS="-pthread -O3 -g -s USE_PTHREADS=1 -s ASYNCIFY=1"
fi

COMPILEFLAGS=$3
LINKFLAGS=$4

function compile_source_android() {
	cmake \
		-H. \
		-G "Unix Makefiles" \
		-DCMAKE_BUILD_TYPE=Release \
		-DANDROID_PLATFORM="android-$1" \
		-DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
		-DANDROID_NDK="$ANDROID_NDK_HOME" \
		-DANDROID_ABI="${3}" \
		-DANDROID_ARM_NEON=TRUE \
		-DCMAKE_ANDROID_NDK="$ANDROID_NDK_HOME" \
		-DCMAKE_SYSTEM_NAME=Android \
		-DCMAKE_SYSTEM_VERSION="$1" \
		-DCMAKE_ANDROID_ARCH_ABI="${3}" \
		-DCMAKE_C_FLAGS="$COMPILEFLAGS" -DCMAKE_CXX_FLAGS="$COMPILEFLAGS" -DCMAKE_CXX_FLAGS_RELEASE="$COMPILEFLAGS" -DCMAKE_C_FLAGS_RELEASE="$COMPILEFLAGS" \
		-DCMAKE_SHARED_LINKER_FLAGS="$LINKFLAGS" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="$LINKFLAGS" \
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

function compile_source_webasm() {
	emcmake cmake \
		-H. \
		-DCMAKE_BUILD_TYPE=Release \
		-B"$2" \
		-DSDL_STATIC=TRUE \
		-DFT_DISABLE_HARFBUZZ=ON \
		-DFT_DISABLE_BZIP2=ON \
		-DFT_DISABLE_BROTLI=ON \
		-DFT_REQUIRE_ZLIB=TRUE \
		-DCMAKE_C_FLAGS="$COMPILEFLAGS -DGLEW_STATIC" -DCMAKE_CXX_FLAGS="$COMPILEFLAGS" -DCMAKE_CXX_FLAGS_RELEASE="$COMPILEFLAGS" -DCMAKE_C_FLAGS_RELEASE="$COMPILEFLAGS" \
		-DCMAKE_SHARED_LINKER_FLAGS="$LINKFLAGS" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="$LINKFLAGS" \
		-DSDL_PTHREADS=ON -DSDL_THREADS=ON \
		-DCURL_USE_OPENSSL=ON \
		-DOPUS_HARDENING=OFF \
		-DOPUS_STACK_PROTECTOR=OFF \
		-DOPENSSL_ROOT_DIR="$PWD"/../openssl/"$2" \
		-DOPENSSL_CRYPTO_LIBRARY="$PWD"/../openssl/"$2"/libcrypto.a \
		-DOPENSSL_SSL_LIBRARY="$PWD"/../openssl/"$2"/libssl.a \
		-DOPENSSL_INCLUDE_DIR="${PWD}/../openssl/include;${PWD}/../openssl/${2}/include" \
		-DZLIB_LIBRARY="${PWD}/../zlib/${2}/libz.a" -DZLIB_INCLUDE_DIR="${PWD}/../zlib;${PWD}/../zlib/${2}"
	(
		cd "$2" || exit 1
		cmake --build .
	)
}

if [[ "${2}" == "android" ]]; then
	compile_source_android "$1" build_android_arm armeabi-v7a &
	compile_source_android "$1" build_android_arm64 arm64-v8a &
	compile_source_android "$1" build_android_x86 x86 &
	compile_source_android "$1" build_android_x86_64 x86_64 &
elif [[ "${2}" == "webasm" ]]; then
	sed -i "s/include(CheckSizes)//g" CMakeLists.txt
	compile_source_webasm "$1" build_webasm_wasm wasm &
fi

wait
