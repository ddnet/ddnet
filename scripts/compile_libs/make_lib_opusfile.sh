#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

export CXXFLAGS="$3"
export CFLAGS="$3"
export CPPFLAGS="$4"
export LDFLAGS="$4"

export ANDROID_NDK_ROOT="$ANDROID_NDK"

function make_opusfile() {
	_EXISTS_PROJECT=0
	if [ -d "$1" ]; then
		_EXISTS_PROJECT=1
	else
		mkdir "$1"
	fi
	(
		cd "$1" || exit 1
		if [[ "$_EXISTS_PROJECT" == 0 ]]; then
			#not nice but doesn't matter
			cp -R ../../ogg/include .
			cp -R ../../opus/include .
			cp -R ../../ogg/"$2"/include/ogg/* include/ogg/
			cp ../../ogg/"$2"/libogg.a libogg.a
			cp ../../opus/"$2"/libopus.a libopus.a
		fi

		TMP_COMPILER=""
		TMP_AR=""
		if [[ "${5}" == "android" ]]; then
			TMP_COMPILER="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/$3$4-clang"
			TMP_AR="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
		elif [[ "${5}" == "webasm" ]]; then
			TMP_COMPILER="emcc"
			TMP_AR="emar"
		fi

		${TMP_COMPILER} \
			-c \
			-fPIC \
			-I"${PWD}"/../include \
			-I"${PWD}"/include \
			../src/opusfile.c \
			../src/info.c \
			../src/internal.c
		${TMP_COMPILER} \
			-c \
			-fPIC \
			-I"${PWD}"/../include \
			-I"${PWD}"/include \
			-include stdio.h \
			../src/stream.c
		${TMP_AR} \
			rvs \
			libopusfile.a \
			opusfile.o \
			info.o \
			stream.o \
			internal.o
	)
}

function compile_all_opusfile() {
	if [[ "${2}" == "android" ]]; then
		make_opusfile build_"$2"_arm build_"$2"_arm armv7a-linux-androideabi "$1" "$2"
		make_opusfile build_"$2"_arm64 build_"$2"_arm64 aarch64-linux-android "$1" "$2"
		make_opusfile build_"$2"_x86 build_"$2"_x86 i686-linux-android "$1" "$2"
		make_opusfile build_"$2"_x86_64 build_"$2"_x86_64 x86_64-linux-android "$1" "$2"
	elif [[ "${2}" == "webasm" ]]; then
		make_opusfile build_"$2"_wasm build_"$2"_wasm "" "$1" "$2"
	fi
}

compile_all_opusfile "$1" "$2"
