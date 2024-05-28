#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

export CXXFLAGS="$3"
export CFLAGS="$3"
export CPPFLAGS="$4"
LINKER_FLAGS="$4"

export ANDROID_NDK_ROOT="$ANDROID_NDK"
PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$PATH"
_LD_LIBRARY_PATH=".:$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$LD_LIBRARY_PATH"

function make_sqlite3() {
	(
		mkdir -p "$1"
		cd "$1" || exit 1

		TMP_COMPILER=""
		TMP_AR=""
		if [[ "${5}" == "android" ]]; then
			TMP_COMPILER="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/$3$4-clang"
			TMP_AR="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
		elif [[ "${5}" == "webasm" ]]; then
			TMP_COMPILER="emcc"
			TMP_AR="emar"
		fi

		LDFLAGS="${LINKER_FLAGS} -L./" \
			LD_LIBRARY_PATH="$_LD_LIBRARY_PATH" \
			${TMP_COMPILER} \
			-c \
			-fPIC \
			-DSQLITE_ENABLE_ATOMIC_WRITE=1 \
			-DSQLITE_ENABLE_BATCH_ATOMIC_WRITE=1 \
			-DSQLITE_ENABLE_MULTITHREADED_CHECKS=1 \
			-DSQLITE_THREADSAFE=1 \
			../sqlite3.c \
			-o sqlite3.o

		LDFLAGS="${LINKER_FLAGS} -L./" \
			LD_LIBRARY_PATH="$_LD_LIBRARY_PATH" \
			${TMP_AR} \
			rvs \
			sqlite3.a \
			sqlite3.o
	)
}

function compile_all_sqlite3() {
	if [[ "${2}" == "android" ]]; then
		make_sqlite3 build_"$2"_arm build_"$2"_arm armv7a-linux-androideabi "$1" "$2"
		make_sqlite3 build_"$2"_arm64 build_"$2"_arm64 aarch64-linux-android "$1" "$2"
		make_sqlite3 build_"$2"_x86 build_"$2"_x86 i686-linux-android "$1" "$2"
		make_sqlite3 build_"$2"_x86_64 build_"$2"_x86_64 x86_64-linux-android "$1" "$2"
	elif [[ "${2}" == "webasm" ]]; then
		make_sqlite3 build_"$2"_wasm build_"$2"_wasm "" "$1" "$2"
	fi
}

compile_all_sqlite3 "$1" "$2"
