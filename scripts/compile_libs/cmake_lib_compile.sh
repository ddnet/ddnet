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
	# The mobile platforms link everything statically. The desktop platforms ship a
	# mix, so each library below sets this where it differs from the shipped layout.
	local cmake_shared="OFF"

	local ios_sdk_path=""

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
	elif [[ "${TARGET_PLATFORM}" == "ios" ]]; then
		local build_ios_sysroot="${3}"
		local build_ios_arch="${4}"
		ios_sdk_path="$(xcrun --sdk "${build_ios_sysroot}${IOS_SDK_VERSION}" --show-sdk-path)"
		cmake_arguments+=("-DCMAKE_SYSTEM_NAME=iOS")
		cmake_arguments+=("-DCMAKE_OSX_SYSROOT=${ios_sdk_path}")
		cmake_arguments+=("-DCMAKE_OSX_ARCHITECTURES=${build_ios_arch}")
		cmake_arguments+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${IOS_DEPLOYMENT_TARGET}")
		cmake_arguments+=("-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY")
		build_extra_cflags="${IOS_COMMON_CFLAGS}"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		cmake_wrapper="${EMSCRIPTEN_CMAKE_WRAPPER}"
		build_extra_cflags="${EMSCRIPTEN_WASM_CFLAGS} ${EMSCRIPTEN_EXTRA_RELEASE_CFLAGS}"
		build_extra_ldflags="${EMSCRIPTEN_WASM_LDFLAGS}"
	elif [[ "${TARGET_PLATFORM}" == "linux" ]]; then
		local build_linux_arch="${2}"
		# Set explicitly rather than left to cmake, which would take it from the
		# build host and give the 32 bit lane the host's processor
		cmake_arguments+=("-DCMAKE_SYSTEM_PROCESSOR=$(cmake_processor_for "${1}")")
		build_extra_cflags="${LINUX_COMMON_CFLAGS}"
		if [[ "${build_linux_arch}" == "x86" ]]; then
			build_extra_cflags="${build_extra_cflags} -m32"
			build_extra_ldflags="${build_extra_ldflags} -m32"
			# Both are needed on a Debian multiarch host: without the architecture
			# find_package resolves to the amd64 libraries and linking fails, and
			# without the .pc path SDL picks up an incompatible glibconfig.h
			# through the ibus dependency.
			cmake_arguments+=("-DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu")
			export PKG_CONFIG_LIBDIR="/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig"
		else
			unset PKG_CONFIG_LIBDIR
		fi
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		local build_windows_triple="${2}"
		local windows_cc="${build_windows_triple}-gcc"
		local windows_cxx="${build_windows_triple}-g++"
		# llvm-mingw, which is what the arm64 lane uses, ships clang instead of gcc
		if ! command -v "${windows_cc}" > /dev/null 2>&1; then
			windows_cc="${build_windows_triple}-clang"
			windows_cxx="${build_windows_triple}-clang++"
		fi
		cmake_arguments+=("-DCMAKE_SYSTEM_NAME=Windows")
		# Without this cmake leaves the target processor unset when cross compiling,
		# and libpng then enables its NEON code without adding the sources for it
		cmake_arguments+=("-DCMAKE_SYSTEM_PROCESSOR=$(cmake_processor_for "${1}")")
		cmake_arguments+=("-DCMAKE_C_COMPILER=${windows_cc}")
		cmake_arguments+=("-DCMAKE_CXX_COMPILER=${windows_cxx}")
		# cmake detects the assembler separately from the C compiler and would
		# otherwise assemble the .S sources with the host one
		cmake_arguments+=("-DCMAKE_ASM_COMPILER=${windows_cc}")
		cmake_arguments+=("-DCMAKE_RC_COMPILER=${build_windows_triple}-windres")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY")
		build_extra_cflags="${WINDOWS_COMMON_CFLAGS}"
		# Asked of the compiler rather than derived from its name, because llvm-mingw
		# also installs its clang under the gcc names
		if "${windows_cc}" --version 2> /dev/null | grep -qi clang; then
			# cmake passes the compile flags to the link line as well, where clang
			# warns that the compile-only ones are unused. Libraries that build with
			# -Werror, such as libwebsockets, turn that warning into an error.
			build_extra_cflags="${build_extra_cflags} -Wno-unused-command-line-argument"
		fi
		# So the shipped DLLs do not depend on the mingw runtime DLLs
		build_extra_ldflags="${build_extra_ldflags} -static-libgcc"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		local build_mac_triple="${2}"
		local build_mac_arch="${3}"
		local mac_deployment_target="${MAC_DEPLOYMENT_TARGET}"
		if [[ "${TARGET_LIBRARY}" == "sdl" ]]; then
			mac_deployment_target="${MAC_SDL_DEPLOYMENT_TARGET}"
		fi
		cmake_arguments+=("-DCMAKE_SYSTEM_NAME=Darwin")
		cmake_arguments+=("-DCMAKE_SYSTEM_PROCESSOR=${build_mac_arch}")
		cmake_arguments+=("-DCMAKE_C_COMPILER=${build_mac_triple}-clang")
		cmake_arguments+=("-DCMAKE_CXX_COMPILER=${build_mac_triple}-clang++")
		# See the Windows lane, the assembler is detected separately
		cmake_arguments+=("-DCMAKE_ASM_COMPILER=${build_mac_triple}-clang")
		# osxcross does not set these and cmake would otherwise archive with the
		# host GNU ar, which Apple's linker cannot read
		cmake_arguments+=("-DCMAKE_AR=$(command -v "${build_mac_triple}-ar")")
		cmake_arguments+=("-DCMAKE_RANLIB=$(command -v "${build_mac_triple}-ranlib")")
		cmake_arguments+=("-DCMAKE_INSTALL_NAME_TOOL=$(command -v "${build_mac_triple}-install_name_tool")")
		cmake_arguments+=("-DCMAKE_OSX_SYSROOT=${OSXCROSS_SDK}")
		cmake_arguments+=("-DCMAKE_OSX_ARCHITECTURES=${build_mac_arch}")
		cmake_arguments+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${mac_deployment_target}")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH=${OSXCROSS_SDK}")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY")
		cmake_arguments+=("-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY")
		# See the Windows lane, clang warns about the compile-only flags at link time
		build_extra_cflags="${MAC_COMMON_CFLAGS} -Wno-unused-command-line-argument -mmacosx-version-min=${mac_deployment_target}"
		build_extra_ldflags="${build_extra_ldflags} -mmacosx-version-min=${mac_deployment_target}"
		local compiler_rt_dir
		compiler_rt_dir="$(osxcross_compiler_rt_dir "${build_mac_arch}")"
		if [ ! -f "${compiler_rt_dir}/libclang_rt.osx.a" ]; then
			log_error "ERROR: osxcross compiler-rt for ${build_mac_arch} was not found at ${compiler_rt_dir}."
			log_error "Run ./build_compiler_rt.sh in the osxcross checkout."
			exit 1
		fi
		# Through the standard libraries rather than the link flags, so that it is
		# appended after the objects where ld64 can actually resolve it
		cmake_arguments+=("-DCMAKE_C_STANDARD_LIBRARIES=-L${compiler_rt_dir} -lclang_rt.osx")
		cmake_arguments+=("-DCMAKE_CXX_STANDARD_LIBRARIES=-L${compiler_rt_dir} -lclang_rt.osx")
	fi

	# Remove absolute build paths and compiler identification from binary
	build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=$(${PATH_WRAPPER} "$(realpath "..")")="
	build_extra_cflags="${build_extra_cflags} -fno-ident"
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${ANDROID_TOOLCHAIN_ROOT}=ANDROID_TOOLCHAIN_ROOT"
	elif [[ "${TARGET_PLATFORM}" == "ios" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${ios_sdk_path}=IOS_SDK_ROOT"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${EMSDK}=EMSDK"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${OSXCROSS_SDK}=MACOS_SDK_ROOT"
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
		elif [[ "${TARGET_PLATFORM}" == "ios" ]]; then
			# Use Apple's native TLS stack on iOS for now. This is removed in curl 8.15 due to its inability to support TLS 1.3
			# SecTrust is available in curl 8.17
			cmake_arguments+=("-DCURL_ENABLE_SSL=ON")
			# The option is spelled CURL_USE_SECTRANSP up to and including the pinned curl 8.8.0.
			cmake_arguments+=("-DCURL_USE_SECTRANSP=ON")
			cmake_arguments+=("-DCURL_USE_OPENSSL=OFF")
		elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
			log_error "ERROR: Compiling ${TARGET_LIBRARY} for ${TARGET_PLATFORM} is unnecessary."
			exit 1
		elif [[ "${TARGET_PLATFORM}" == "linux" ]]; then
			# Linux links a generated stub instead, see gen_libs.sh
			log_error "ERROR: Compiling ${TARGET_LIBRARY} for ${TARGET_PLATFORM} is unnecessary."
			exit 1
		elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
			cmake_arguments+=("-DCURL_USE_SCHANNEL=ON")
			cmake_arguments+=("-DCURL_USE_OPENSSL=OFF")
			# Against zlib-rs, the same archive the shipped libpng uses. Without a
			# zlib there is no Content-Encoding support, and the default of AUTO
			# would silently drop it rather than fail.
			local curl_zlib_path="${PWD}/../zlib-rs/${build_folder}"
			cmake_arguments+=("-DCURL_ZLIB=ON")
			cmake_arguments+=("-DZLIB_LIBRARY=${curl_zlib_path}/lib/libz.a")
			cmake_arguments+=("-DZLIB_INCLUDE_DIR=${curl_zlib_path}/include")
			cmake_shared="ON"
			cmake_targets="--target libcurl_shared"
		elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
			# The option is spelled CURL_USE_SECTRANSP up to and including the pinned curl 8.8.0.
			cmake_arguments+=("-DCURL_USE_SECTRANSP=ON")
			cmake_arguments+=("-DCURL_USE_OPENSSL=OFF")
			cmake_targets="--target libcurl_static"
		fi
	elif [[ "${TARGET_LIBRARY}" == "freetype" ]]; then
		local png_path="${PWD}/../png"
		cmake_targets="--target freetype"
		cmake_arguments+=("-DFT_DISABLE_HARFBUZZ=ON")
		cmake_arguments+=("-DFT_DISABLE_BZIP2=ON")
		cmake_arguments+=("-DFT_DISABLE_BROTLI=ON")
		cmake_arguments+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
		if [[ "${TARGET_PLATFORM}" == "windows" || "${TARGET_PLATFORM}" == "mac" ]]; then
			# DDNet only uses freetype to rasterize glyphs, so the desktop builds
			# drop the image and compression back ends entirely
			cmake_arguments+=("-DFT_DISABLE_PNG=ON")
			cmake_arguments+=("-DFT_DISABLE_ZLIB=ON")
			cmake_shared="ON"
		else
			cmake_arguments+=("-DFT_REQUIRE_PNG=ON")
			cmake_arguments+=("-DFT_REQUIRE_ZLIB=ON")
			cmake_arguments+=("-DPNG_LIBRARY=${png_path}/${build_folder}/libpng.a")
			cmake_arguments+=("-DPNG_PNG_INCLUDE_DIR=${png_path}${PATH_SEPARATOR}${png_path}/${build_folder}")
		fi
	elif [[ "${TARGET_LIBRARY}" == "ogg" ]]; then
		cmake_arguments+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
		cmake_targets="--target ogg"
		if [[ "${TARGET_PLATFORM}" == "windows" ]]; then
			cmake_shared="ON"
		fi
	elif [[ "${TARGET_LIBRARY}" == "opus" ]]; then
		cmake_targets="--target opus"
		cmake_arguments+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
		if [[ "${TARGET_PLATFORM}" == "windows" ]]; then
			cmake_shared="ON"
			# opus only checks that the compiler accepts -fstack-protector-strong,
			# and mingw-w64 ships no libssp to resolve __stack_chk_fail against
			cmake_arguments+=("-DOPUS_STACK_PROTECTOR=OFF")
		fi
	elif [[ "${TARGET_LIBRARY}" == "png" ]]; then
		if [[ "${TARGET_PLATFORM}" == "windows" || "${TARGET_PLATFORM}" == "mac" ]]; then
			# Against the zlib-rs static archive rather than a zlib of its own
			local zlib_rs_path="${PWD}/../zlib-rs/${build_folder}"
			cmake_targets="--target png_shared"
			cmake_arguments+=("-DPNG_SHARED=ON")
			cmake_arguments+=("-DPNG_STATIC=OFF")
			cmake_arguments+=("-DZLIB_LIBRARY=${zlib_rs_path}/lib/libz.a")
			cmake_arguments+=("-DZLIB_INCLUDE_DIR=${zlib_rs_path}/include")
			cmake_shared="ON"
		else
			cmake_targets="--target png_static"
			cmake_arguments+=("-DPNG_SHARED=OFF")
		fi
	elif [[ "${TARGET_LIBRARY}" == "sdl" ]]; then
		cmake_targets="--target SDL2-static sdl_headers_copy"
		cmake_arguments+=("-DSDL_STATIC=ON")
		if [[ "${TARGET_PLATFORM}" == "linux" || "${TARGET_PLATFORM}" == "windows" || "${TARGET_PLATFORM}" == "mac" ]]; then
			cmake_targets="--target SDL2 sdl_headers_copy"
			cmake_arguments+=("-DSDL_SHARED=ON")
			cmake_arguments+=("-DSDL_STATIC=OFF")
			cmake_arguments+=("-DSDL_TESTS=OFF")
			cmake_shared="ON"
			if [[ "${TARGET_PLATFORM}" == "linux" ]]; then
				# Wayland is loaded dynamically by the system SDL, the bundled one
				# is the X11 fallback for systems where that does not work
				cmake_arguments+=("-DSDL_WAYLAND=OFF")
				cmake_arguments+=("-DSDL_RPATH=OFF")
			elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
				cmake_arguments+=("-DSDL_FRAMEWORK=ON")
				cmake_arguments+=("-DSDL_HIDAPI_LIBUSB=OFF")
			fi
		elif [[ "${TARGET_PLATFORM}" == "android" ]]; then
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
	elif [[ "${TARGET_LIBRARY}" == "websockets" ]]; then
		cmake_arguments+=("-DLWS_IPV6=ON")
		cmake_arguments+=("-DLWS_WITHOUT_TESTAPPS=ON")
		cmake_arguments+=("-DLWS_WITH_SSL=OFF")
		cmake_arguments+=("-DLWS_UNIX_SOCK=OFF")
		cmake_arguments+=("-DLWS_WITHOUT_EXTENSIONS=ON")
		cmake_arguments+=("-DLWS_WITH_SYS_SMD=OFF")
		cmake_arguments+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
		if [[ "${TARGET_PLATFORM}" == "linux" ]]; then
			cmake_arguments+=("-DLWS_WITH_STATIC=ON")
			cmake_arguments+=("-DLWS_WITH_SHARED=OFF")
			cmake_targets="--target websockets"
		else
			cmake_arguments+=("-DLWS_WITH_STATIC=OFF")
			cmake_arguments+=("-DLWS_WITH_SHARED=ON")
			cmake_targets="--target websockets_shared"
			cmake_shared="ON"
		fi
	elif [[ "${TARGET_LIBRARY}" == "wavpack" ]]; then
		cmake_arguments+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
		cmake_arguments+=("-DBUILD_TESTING=OFF")
		cmake_arguments+=("-DWAVPACK_BUILD_PROGRAMS=OFF")
		cmake_arguments+=("-DWAVPACK_ENABLE_THREADS=OFF")
		cmake_arguments+=("-DWAVPACK_INSTALL_CMAKE_MODULE=OFF")
		cmake_arguments+=("-DWAVPACK_INSTALL_DOCS=OFF")
		cmake_arguments+=("-DWAVPACK_INSTALL_PKGCONFIG_MODULE=OFF")
		if [[ "${TARGET_PLATFORM}" == "windows" ]]; then
			cmake_shared="ON"
		fi
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
		-DBUILD_SHARED_LIBS="${cmake_shared}"

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
	elif [[ "${TARGET_PLATFORM}" == "ios" ]]; then
		make_cmake "${IOS_DEVICE_BUILD_FOLDER}" "" "iphoneos" "${IOS_DEVICE_ARCH}"
		make_cmake "${IOS_SIM_ARM64_BUILD_FOLDER}" "" "iphonesimulator" "${IOS_SIM_ARM64_ARCH}"
		make_cmake "${IOS_SIM_X64_BUILD_FOLDER}" "" "iphonesimulator" "${IOS_SIM_X64_ARCH}"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		make_cmake "${EMSCRIPTEN_WASM_BUILD_FOLDER}" ""
	elif [[ "${TARGET_PLATFORM}" == "linux" ]]; then
		make_cmake "${LINUX_X64_BUILD_FOLDER}" "x86_64"
		make_cmake "${LINUX_X86_BUILD_FOLDER}" "x86"
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		make_cmake "${WINDOWS_X64_BUILD_FOLDER}" "${WINDOWS_X64_TRIPLE}"
		make_cmake "${WINDOWS_X86_BUILD_FOLDER}" "${WINDOWS_X86_TRIPLE}"
		make_cmake "${WINDOWS_ARM64_BUILD_FOLDER}" "${WINDOWS_ARM64_TRIPLE}"
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		make_cmake "${MAC_X64_BUILD_FOLDER}" "${MAC_X64_TRIPLE}" "x86_64"
		make_cmake "${MAC_ARM64_BUILD_FOLDER}" "${MAC_ARM64_TRIPLE}" "arm64"
	else
		log_error "ERROR: Unsupported target platform: ${TARGET_PLATFORM}"
		exit 1
	fi
}

make_all_cmake
