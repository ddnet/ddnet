#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

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
			#not nice but doesnt matter
			cp -R ../../ogg/include .
			cp -R ../../opus/include .
			cp -R ../../ogg/"$2"/include/ogg/* include/ogg/
			cp ../../ogg/"$2"/libogg.a libogg.a
			cp ../../opus/"$2"/libopus.a libopus.a
		fi
		"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/$3$4-clang" \
			-c \
			-fPIC \
			-I"${PWD}"/../include \
			-I"${PWD}"/include \
			../src/opusfile.c \
			../src/info.c \
			../src/internal.c
		"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/$3$4-clang" \
			-c \
			-fPIC \
			-I"${PWD}"/../include \
			-I"${PWD}"/include \
			-include stdio.h \
			-Dftello=ftell \
			-Dfseek=fseek \
			../src/stream.c
		"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar" \
			rvs \
			libopusfile.a \
			opusfile.o \
			info.o \
			stream.o \
			internal.o
	)
}

function compile_all_opusfile() {
	make_opusfile build_arm build_android_arm armv7a-linux-androideabi "$1"
	make_opusfile build_arm64 build_android_arm64 aarch64-linux-android "$1"
	make_opusfile build_x86 build_android_x86 i686-linux-android "$1"
	make_opusfile build_x86_64 build_android_x86_64 x86_64-linux-android "$1"
}

compile_all_opusfile "$1"

