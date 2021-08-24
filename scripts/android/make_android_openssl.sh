#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

export ANDROID_NDK_ROOT=$ANDROID_NDK
PATH=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$ANDROID_NDK_ROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$PATH

function buid_openssl() {
	_EXISTS_PROJECT=0
	if [ -d "$1" ]; then
		_EXISTS_PROJECT=1
	else
		mkdir "$1"
	fi
	(
		cd "$1" || exit 1
		if [[ "$_EXISTS_PROJECT" == "0" ]]; then
			../Configure "$2" no-asm no-shared
		fi
		make $MAKEFLAGS build_generated
		make $MAKEFLAGS libcrypto.a
		make $MAKEFLAGS libssl.a
		cd ..
	)
}

buid_openssl build_android_arm android-arm "$1"
buid_openssl build_android_arm64 android-arm64 "$1"
buid_openssl build_android_x86 android-x86 "$1"
buid_openssl build_android_x86_64 android-x86_64 "$1"
