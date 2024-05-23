#!/bin/bash

ANDROID_HOME=~/Android/Sdk
ANDROID_NDK="$(find "$ANDROID_HOME/ndk" -maxdepth 1 | sort -n | tail -1)"

export MAKEFLAGS=-j32

export CXXFLAGS="$3"
export CFLAGS="$3"
export CPPFLAGS="$4"
export LDFLAGS="$4"

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
			if [[ "${4}" == "webasm" ]]; then
				emconfigure ../Configure "$2" -no-tests -no-asm -static -no-afalgeng -DOPENSSL_SYS_NETWARE -DSIG_DFL=0 -DSIG_IGN=0 -DHAVE_FORK=0 -DOPENSSL_NO_AFALGENG=1 --with-rand-seed=getrandom

				sed -i 's|^CROSS_COMPILE.*$|CROSS_COMPILE=|g' Makefile
			else
				../Configure "$2" no-asm no-shared
			fi
		fi
		${5} make $MAKEFLAGS build_generated
		${5} make $MAKEFLAGS libcrypto.a
		${5} make $MAKEFLAGS libssl.a
		cd ..
	)
}

if [[ "${2}" == "android" ]]; then
	buid_openssl build_"$2"_arm android-arm "$1" "$2" ""
	buid_openssl build_"$2"_arm64 android-arm64 "$1" "$2" ""
	buid_openssl build_"$2"_x86 android-x86 "$1" "$2" ""
	buid_openssl build_"$2"_x86_64 android-x86_64 "$1" "$2" ""
elif [[ "${2}" == "webasm" ]]; then
	buid_openssl build_"$2"_wasm linux-generic64 "$1" "$2" emmake
fi
