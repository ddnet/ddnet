#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

export ANDROID_NDK_ROOT="$ANDROID_NDK"
PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$PATH"
_LD_LIBRARY_PATH=".:$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$LD_LIBRARY_PATH"

function make_sqlite3() {
	(
		mkdir -p "$1"
		cd "$1" || exit 1
		LDFLAGS="-L./" \
			LD_LIBRARY_PATH="$_LD_LIBRARY_PATH" \
			"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/$3$4-clang" \
				-c \
				-fPIC \
				../sqlite3.c \
				-o sqlite3.o

		LDFLAGS="-L./" \
			LD_LIBRARY_PATH="$_LD_LIBRARY_PATH" \
			"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar" \
				rvs \
				sqlite3.a \
				sqlite3.o
	)
}

function compile_all_sqlite3() {
	make_sqlite3 build_arm build_android_arm armv7a-linux-androideabi "$1"
	make_sqlite3 build_arm64 build_android_arm64 aarch64-linux-android "$1"
	make_sqlite3 build_x86 build_android_x86 i686-linux-android "$1"
	make_sqlite3 build_x86_64 build_android_x86_64 x86_64-linux-android "$1"
}

compile_all_sqlite3 "$1"

