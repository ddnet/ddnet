#!/bin/bash
set -e

if [ -z ${1+x} ]; then
	DIR="$HOME"
else
	DIR="${1}"
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/../compile_libs/_build_common.sh"

mkdir -p "${DIR}/Android/Sdk"
cd "${DIR}/Android/Sdk"

mkdir ndk
(
	cd ndk
	log_info "Downloading Android NDK to ${PWD}"
	wget --quiet "https://dl.google.com/android/repository/android-ndk-r29-${HOST_OS}.zip"
	unzip -q "android-ndk-r29-${HOST_OS}.zip"
	rm "android-ndk-r29-${HOST_OS}.zip"
	mv android-ndk-r29 29.0.14206865
)

mkdir build-tools
(
	cd build-tools
	log_info "Downloading Android build tools to ${PWD}"
	wget --quiet "https://dl.google.com/android/repository/build-tools_r36.1_${HOST_OS}.zip"
	unzip -q "build-tools_r36.1_${HOST_OS}.zip"
	rm "build-tools_r36.1_${HOST_OS}.zip"
	mv android-16 36.1.0
)

mkdir cmdline-tools
(
	cd cmdline-tools
	log_info "Downloading Android command-line tools to ${PWD}"
	wget --quiet "https://dl.google.com/android/repository/commandlinetools-${HOST_OS}-13114758_latest.zip"
	unzip -q "commandlinetools-${HOST_OS}-13114758_latest.zip"
	rm "commandlinetools-${HOST_OS}-13114758_latest.zip"
	mv cmdline-tools latest
	yes 2> /dev/null | latest/bin/sdkmanager --licenses > /dev/null
)
