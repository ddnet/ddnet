#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

export TARGET_PLATFORM="${1}"

function make_opusfile() {
	local build_folder="${1}"
	local build_android_triple="${2}"
	local build_extra_cflags="${3}"

	local library_path
	library_path=$(realpath "..")
	local ogg_include_path="${library_path}/ogg/include"
	if [ ! -d "${ogg_include_path}" ]; then
		log_error "ERROR: Download ogg first, include folder expected at ${ogg_include_path}"
		exit 1
	fi
	local opus_include_path="${library_path}/opus/include"
	if [ ! -d "${opus_include_path}" ]; then
		log_error "ERROR: Download opus first, include folder expected at ${opus_include_path}"
		exit 1
	fi
	local ogg_include_path_build="${library_path}/ogg/${build_folder}/include"
	if [ ! -d "${ogg_include_path_build}" ]; then
		log_error "ERROR: Compile ogg for ${build_android_triple} first, include folder expected at ${ogg_include_path_build}"
		exit 1
	fi

	# Remove absolute build paths and compiler identification from binary
	build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=$(${PATH_WRAPPER} "$(realpath "..")")="
	build_extra_cflags="${build_extra_cflags} -fno-ident"
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${ANDROID_TOOLCHAIN_ROOT}=ANDROID_TOOLCHAIN_ROOT"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		build_extra_cflags="${build_extra_cflags} -ffile-prefix-map=${EMSDK}=EMSDK"
	fi

	log_info "Building to ${build_folder}..."
	mkdir -p "${build_folder}"
	(
		cd "${build_folder}"

		local cc=""
		local ar=""
		if [[ "${TARGET_PLATFORM}" == "android" ]]; then
			cc="${ANDROID_TOOLCHAIN_ROOT}/bin/${build_android_triple}${ANDROID_API}-clang"
			ar="${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar"
		elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
			cc="emcc"
			ar="emar"
		fi

		if [[ ! -f Makefile ]]; then
			cat > Makefile << EOF
CC=${cc}
AR=${ar}
CFLAGS=${build_extra_cflags}
INCLUDES=-I${PWD}/../include \
	-I${ogg_include_path} \
	-I${opus_include_path} \
	-I${ogg_include_path_build}

libopusfile.a: opusfile.o info.o stream.o internal.o
	\$(AR) rvs libopusfile.a opusfile.o info.o stream.o internal.o

opusfile.o info.o stream.o internal.o: %.o: ../src/%.c
	\$(CC) -c \$(CFLAGS) \$(INCLUDES) \$< -o \$@
EOF
		fi

		make "${BUILD_FLAGS}"
	)
}

function make_all_opusfile() {
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		make_opusfile "${ANDROID_ARM_BUILD_FOLDER}" "${ANDROID_ARM_TRIPLE}" "${ANDROID_ARM_CFLAGS} ${ANDROID_EXTRA_RELEASE_CFLAGS}"
		make_opusfile "${ANDROID_ARM64_BUILD_FOLDER}" "${ANDROID_ARM64_TRIPLE}" "${ANDROID_ARM64_CFLAGS} ${ANDROID_EXTRA_RELEASE_CFLAGS}"
		make_opusfile "${ANDROID_X86_BUILD_FOLDER}" "${ANDROID_X86_TRIPLE}" "${ANDROID_X86_CFLAGS} ${ANDROID_EXTRA_RELEASE_CFLAGS}"
		make_opusfile "${ANDROID_X64_BUILD_FOLDER}" "${ANDROID_X64_TRIPLE}" "${ANDROID_X64_CFLAGS} ${ANDROID_EXTRA_RELEASE_CFLAGS}"
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		make_opusfile "${EMSCRIPTEN_WASM_BUILD_FOLDER}" "" "${EMSCRIPTEN_WASM_CFLAGS} ${EMSCRIPTEN_EXTRA_RELEASE_CFLAGS}"
	else
		log_error "ERROR: Unsupported target platform: ${TARGET_PLATFORM}"
		exit 1
	fi
}

make_all_opusfile
