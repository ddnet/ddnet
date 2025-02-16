#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

TARGET_LIBRARY="${1}"
export TARGET_PLATFORM="${2}"

function make_cmake() {
	local build_folder="${1}"
	local build_extra_cflags=""
	local build_extra_ldflags=""
	local cmake_arguments=()
	local cmake_wrapper=""
	local cmake_targets=""

	# Target platform settings
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		local build_android_abi="${2}"
		cmake_arguments+=("-DANDROID_PLATFORM=android-${ANDROID_API}")
		cmake_arguments+=("-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
		cmake_arguments+=("-DANDROID_NDK=${ANDROID_NDK_HOME}")
		cmake_arguments+=("-DANDROID_ABI=${build_android_abi}")
		cmake_arguments+=("-DANDROID_ARM_NEON=ON")
		cmake_arguments+=("-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
		cmake_arguments+=("-DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME}")
		cmake_arguments+=("-DCMAKE_SYSTEM_NAME=Android")
		cmake_arguments+=("-DCMAKE_SYSTEM_VERSION=${ANDROID_API}")
		cmake_arguments+=("-DCMAKE_ANDROID_ARCH_ABI=${build_android_abi}")
		# Most required C and LD flags for Android are already specified by the toolchain file
		build_extra_cflags="${ANDROID_EXTRA_RELEASE_CFLAGS}"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		cmake_wrapper="emcmake"
		build_extra_cflags="${EMSCRIPTEN_WASM_CFLAGS} ${EMSCRIPTEN_EXTRA_RELEASE_CFLAGS}"
		build_extra_ldflags="${EMSCRIPTEN_WASM_LDFLAGS}"
	fi

	# Remove absolute build paths and compiler identification from binary
	build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=$(${PATH_WRAPPER} "$(realpath "..")")="
	build_extra_cflags="${build_extra_cflags} -fno-ident"
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${ANDROID_TOOLCHAIN_ROOT}=ANDROID_TOOLCHAIN_ROOT"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${EMSDK}=EMSDK"
	fi

	# Target library settings
	if [[ "${TARGET_LIBRARY}" == "boringssl" ]]; then
		cmake_targets="--target crypto ssl"
		if [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
			cmake_arguments+=("-DOPENSSL_NO_ASM=ON")
			# Fix BoringSSL configuration failing because -O3 is used which causes
			# a warning in the Emscripten compiler that is treated as an error.
			cmake_arguments+=("-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY")
		fi
	elif [[ "${TARGET_LIBRARY}" == "curl" ]]; then
		local ssl_path="${PWD}/../boringssl"
		cmake_targets="--target libcurl.a"
		# Disable all protocols except HTTPS and HTTP
		cmake_arguments+=("-DHTTP_ONLY=ON")
		if [[ "${TARGET_PLATFORM}" == "android" ]]; then
			# Use crypto and ssl provided by BoringSSL
			cmake_arguments+=("-DCURL_USE_OPENSSL=ON")
			cmake_arguments+=("-DOPENSSL_ROOT_DIR=${ssl_path}/${build_folder}")
			cmake_arguments+=("-DOPENSSL_CRYPTO_LIBRARY=${ssl_path}/${build_folder}/libcrypto.a")
			cmake_arguments+=("-DOPENSSL_SSL_LIBRARY=${ssl_path}/${build_folder}/libssl.a")
			cmake_arguments+=("-DOPENSSL_INCLUDE_DIR=${ssl_path}/include")
		elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
			# Compile without crypto because curl does not work with Emscripten at all,
			# but we currently need curl to compile server and client.
			cmake_arguments+=("-DCURL_USE_OPENSSL=OFF")
		fi
	elif [[ "${TARGET_LIBRARY}" == "freetype" ]]; then
		local png_path="${PWD}/../png"
		cmake_targets="--target freetype"
		cmake_arguments+=("-DFT_DISABLE_HARFBUZZ=ON")
		cmake_arguments+=("-DFT_DISABLE_BZIP2=ON")
		cmake_arguments+=("-DFT_DISABLE_BROTLI=ON")
		cmake_arguments+=("-DFT_REQUIRE_PNG=ON")
		cmake_arguments+=("-DFT_REQUIRE_ZLIB=ON")
		cmake_arguments+=("-DPNG_LIBRARY=${png_path}/${build_folder}/libpng.a")
		cmake_arguments+=("-DPNG_PNG_INCLUDE_DIR=${png_path}${PATH_SEPARATOR}${png_path}/${build_folder}")
	elif [[ "${TARGET_LIBRARY}" == "ogg" ]]; then
		cmake_targets="--target ogg"
	elif [[ "${TARGET_LIBRARY}" == "opus" ]]; then
		cmake_targets="--target opus"
	elif [[ "${TARGET_LIBRARY}" == "png" ]]; then
		cmake_targets="--target png_static"
		cmake_arguments+=("-DPNG_SHARED=OFF")
	elif [[ "${TARGET_LIBRARY}" == "sdl" ]]; then
		cmake_targets="--target SDL2-static sdl_headers_copy"
		cmake_arguments+=("-DSDL_STATIC=ON")
		if [[ "${TARGET_PLATFORM}" == "android" ]]; then
			# Compile without support for hidapi and libusb
			cmake_arguments+=("-DSDL_HIDAPI=OFF")
			cmake_arguments+=("-DHIDAPI_SKIP_LIBUSB=ON")
		elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
			cmake_arguments+=("-DSDL_PTHREADS=ON")
			cmake_arguments+=("-DSDL_THREADS=ON")
		fi
	elif [[ "${TARGET_LIBRARY}" == "zlib" ]]; then
		cmake_targets="--target zlibstatic"
		cmake_arguments+=("-DZLIB_BUILD_SHARED=OFF")
	else
		log_error "ERROR: Unsupported target library: ${TARGET_LIBRARY}"
		exit 1
	fi

	# We need to build our own zlib for webasm. Android includes it in the NDK.
	if [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		if [[ "${TARGET_LIBRARY}" == "curl" || "${TARGET_LIBRARY}" == "freetype" || "${TARGET_LIBRARY}" == "png" ]]; then
			local zlib_path="${PWD}/../zlib"
			cmake_arguments+=("-DZLIB_LIBRARY=${zlib_path}/${build_folder}/libz.a")
			cmake_arguments+=("-DZLIB_INCLUDE_DIR=${zlib_path}${PATH_SEPARATOR}${zlib_path}/${build_folder}")
		fi
	fi

	log_info "Building to ${build_folder}..."
	${cmake_wrapper} cmake \
		-H. \
		-G "Ninja" \
		-DCMAKE_BUILD_TYPE=Release \
		-B"${build_folder}" \
		"${cmake_arguments[@]}" \
		-DCMAKE_C_FLAGS="${build_extra_cflags}" \
		-DCMAKE_CXX_FLAGS="${build_extra_cflags}" \
		-DCMAKE_ASM_FLAGS="${build_extra_cflags}" \
		-DCMAKE_EXE_LINKER_FLAGS="${build_extra_ldflags}" \
		-DCMAKE_SHARED_LINKER_FLAGS="${build_extra_ldflags}" \
		-DBUILD_SHARED_LIBS=OFF

	(
		cd "${build_folder}"
		# We want word splitting
		# shellcheck disable=SC2086
		cmake --build . $cmake_targets $BUILD_FLAGS
	)
}

function make_all_cmake() {
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		make_cmake "${ANDROID_ARM_BUILD_FOLDER}" "${ANDROID_ARM_ABI}"
		make_cmake "${ANDROID_ARM64_BUILD_FOLDER}" "${ANDROID_ARM64_ABI}"
		make_cmake "${ANDROID_X86_BUILD_FOLDER}" "${ANDROID_X86_ABI}"
		make_cmake "${ANDROID_X64_BUILD_FOLDER}" "${ANDROID_X64_ABI}"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		make_cmake "${EMSCRIPTEN_WASM_BUILD_FOLDER}" ""
	else
		log_error "ERROR: Unsupported target platform: ${TARGET_PLATFORM}"
		exit 1
	fi
}

make_all_cmake
