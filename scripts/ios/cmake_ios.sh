#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/../compile_libs/_build_common.sh"

SHOW_USAGE_INFO=0

if [ -z ${1+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass iOS build target"
else
	IOS_BUILD=$1
	log_warn "iOS build target: ${IOS_BUILD}"
fi

if [ -z ${2+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass app name"
else
	APP_NAME=$2
	log_warn "App name: ${APP_NAME}"
fi

if [ -z ${3+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass bundle identifier"
else
	BUNDLE_ID=$3
	log_warn "Bundle identifier: ${BUNDLE_ID}"
fi

if [ -z ${4+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass build type"
else
	BUILD_TYPE=$4
	log_warn "Build type: ${BUILD_TYPE}"
fi

if [ -z ${5+x} ]; then
	SHOW_USAGE_INFO=1
	log_error "ERROR: Did not pass build folder"
else
	BUILD_FOLDER=$5
	log_warn "Build folder: ${BUILD_FOLDER}"
fi

if [ $SHOW_USAGE_INFO == 1 ]; then
	log_error "Usage: scripts/ios/cmake_ios.sh <device/sim-arm64/sim-x86_64/sim> <App name> <Bundle id> <Debug/Release> <Build folder>"
	exit 1
fi

assert_ios_sdk_found

IOS_SYSROOT=""
IOS_ARCH=""
case "${IOS_BUILD}" in
device)
	IOS_SYSROOT="iphoneos"
	IOS_ARCH="arm64"
	;;
sim-arm64 | simulator-arm64)
	IOS_SYSROOT="iphonesimulator"
	IOS_ARCH="arm64"
	;;
sim-x86_64 | simulator-x86_64)
	IOS_SYSROOT="iphonesimulator"
	IOS_ARCH="x86_64"
	;;
sim | simulator)
	IOS_SYSROOT="iphonesimulator"
	IOS_ARCH="$(uname -m)"
	;;
*)
	log_error "ERROR: Unsupported iOS build target: ${IOS_BUILD}"
	exit 1
	;;
esac

if [[ "${IOS_SYSROOT}" == "iphoneos" ]]; then
	IOS_RUST_TARGET="aarch64-apple-ios"
elif [[ "${IOS_ARCH}" == "arm64" ]]; then
	IOS_RUST_TARGET="aarch64-apple-ios-sim"
else
	IOS_RUST_TARGET="x86_64-apple-ios"
fi

CODE_SIGN_ARGS=()
BUILD_TOOL_ARGS=()
if [[ "${IOS_SYSROOT}" == "iphoneos" && -n "${IOS_DEVELOPMENT_TEAM}" ]]; then
	# Set up automatic signing so device builds work without opening Xcode.
	log_warn "Development team: ${IOS_DEVELOPMENT_TEAM}"
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${IOS_DEVELOPMENT_TEAM}")
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic")
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=Apple Development")
	# Let Xcode create and download the provisioning profile if it does not exist yet.
	BUILD_TOOL_ARGS+=("-allowProvisioningUpdates")
else
	# Simulator builds never need to be signed. Device builds are built unsigned
	# without a development team, which only checks that the app compiles, as
	# Xcode refuses to build them at all when signing is left enabled.
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO")
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO")
	CODE_SIGN_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=")
fi

cmake \
	-S . \
	-B "${BUILD_FOLDER}" \
	-G Xcode \
	-DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
	-DCMAKE_SYSTEM_NAME=iOS \
	-DCMAKE_OSX_SYSROOT="${IOS_SYSROOT}" \
	-DCMAKE_OSX_ARCHITECTURES="${IOS_ARCH}" \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET}" \
	-DCMAKE_RUST_COMPILER_TARGET="${IOS_RUST_TARGET}" \
	-DCLIENT_EXECUTABLE="${APP_NAME}" \
	-DIOS_BUNDLE_IDENTIFIER="${BUNDLE_ID}" \
	-DPREFER_BUNDLED_LIBS=ON \
	-DSERVER=OFF \
	-DTOOLS=OFF \
	-DVULKAN=OFF \
	-DVIDEORECORDER=OFF \
	"${CODE_SIGN_ARGS[@]}"

cmake --build "${BUILD_FOLDER}" --config "${BUILD_TYPE}" --target game-client -- "${BUILD_TOOL_ARGS[@]}"
