#!/bin/bash

set -eu

DISTRO=unknown

if [[ "$(uname -s)" =~ ^MSYS_NT.* ]]; then
	printf 'TODO: support msys2.\n' 1>&2
	exit 1
elif uname -a | grep -qi ubuntu; then
	DISTRO=ubuntu
elif uname -a | grep -qi debian; then
	DISTRO=debian
else
	printf 'Your system is not supported.\n' 1>&2
	exit 1
fi

install_java() {
	if [ "$DISTRO" = ubuntu ]; then
		sudo apt-get install -y openjdk-21-jdk
	else
		sudo apt-get install -y jdk-21
	fi
}

sudo apt-get update -y
sudo apt-get install -y cmake ninja-build p7zip-full curl glslang-tools openssl
install_java
cargo install cargo-ndk
rustup target add armv7-linux-androideabi
rustup target add i686-linux-android
rustup target add aarch64-linux-android
rustup target add x86_64-linux-android
mkdir -p ~/Android/Sdk/ndk
cd ~/Android/Sdk/ndk
wget --quiet https://dl.google.com/android/repository/android-ndk-r26d-linux.zip
unzip android-ndk-r26d-linux.zip
rm android-ndk-r26d-linux.zip
cd ~/Android/Sdk
mkdir -p build-tools
cd build-tools
wget --quiet https://dl.google.com/android/repository/build-tools_r30.0.3-linux.zip
unzip build-tools_r30.0.3-linux.zip
rm build-tools_r30.0.3-linux.zip
mv android-11 30.0.3
cd ~/Android/Sdk
mkdir -p cmdline-tools
cd cmdline-tools
wget --quiet https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-11076708_latest.zip
rm commandlinetools-linux-11076708_latest.zip
mv cmdline-tools latest
yes | latest/bin/sdkmanager --licenses

echo "All dependencies installed .. OK"
