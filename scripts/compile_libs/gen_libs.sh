#!/bin/bash
set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/_build_common.sh"

if [ -z ${1+x} ]; then
	log_error "ERROR: Specify the destination path where to run this script, please choose a path other than in the source directory"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <android/ios/webasm>"
	exit 1
fi
BUILD_FOLDER="$1"

TARGET_PLATFORMS="android ios webasm linux windows mac"
if [ -z ${2+x} ]; then
	log_error "ERROR: Specify the target platform: ${TARGET_PLATFORMS}"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <${TARGET_PLATFORMS// /|}>"
	exit 1
fi
TARGET_PLATFORM="$2"
case "${TARGET_PLATFORM}" in
android)
	assert_android_ndk_found
	;;
ios)
	assert_ios_sdk_found
	;;
webasm)
	assert_emscripten_sdk_found
	;;
linux)
	assert_linux_toolchain_found
	;;
windows)
	assert_mingw_found "${WINDOWS_X64_TRIPLE}"
	assert_mingw_found "${WINDOWS_X86_TRIPLE}"
	assert_mingw_found "${WINDOWS_ARM64_TRIPLE}"
	;;
mac)
	assert_osxcross_found
	;;
*)
	log_error "ERROR: Specify the target platform: ${TARGET_PLATFORMS}"
	log_error "Usage: scripts/compile_libs/gen_libs.sh <Build folder> <${TARGET_PLATFORMS// /|}>"
	exit 1
	;;
esac

mkdir -p "${BUILD_FOLDER}"
cd "${BUILD_FOLDER}"

# Checks out exactly $3 of $2 into $1. Sources are pinned by commit rather than by
# tag or branch because both can be moved after the fact, which would silently change
# what ends up in the shipped binaries. The commit is re-checked on every run, so an
# existing folder left over from an earlier version is updated instead of being reused.
function fetch_source() {
	local library_dir="$1"
	local git_url="$2"
	local commit="$3"
	if [ ! -d "${library_dir}/.git" ]; then
		rm -rf "${library_dir}"
		mkdir -p "${library_dir}"
		(
			cd "${library_dir}"
			git init --quiet .
			git remote add origin "${git_url}"
		)
	fi
	(
		cd "${library_dir}"
		if [[ "$(git rev-parse HEAD 2> /dev/null)" != "${commit}" ]]; then
			git fetch --quiet --depth 1 origin "${commit}"
			git checkout --quiet --force "${commit}"
			git clean -qxdff
		fi
		local checked_out
		checked_out="$(git rev-parse HEAD)"
		if [[ "${checked_out}" != "${commit}" ]]; then
			log_error "ERROR: ${library_dir} is at ${checked_out}, expected ${commit}"
			exit 1
		fi
	)
}

function build_cmake_lib() {
	local library_dir="$1"
	fetch_source "$@"
	(
		cd "${library_dir}"
		"${SCRIPT_DIR}"/cmake_lib_compile.sh "${library_dir}" "$TARGET_PLATFORM"
	)
}

function build_opusfile() {
	fetch_source opusfile "${OPUSFILE_URL}" "${OPUSFILE_COMMIT}"
	(
		cd opusfile
		# git clean in fetch_source removes the generated tree on a version change
		if [ ! -f configure ]; then
			./autogen.sh
		fi
		"${SCRIPT_DIR}"/make_lib_opusfile.sh "$TARGET_PLATFORM"
	)
}

function build_sqlite3() {
	if [ ! -d "sqlite3" ]; then
		local sqlite_filename="sqlite-amalgamation-${SQLITE3_VERSION}"
		local sqlite_archive_filename="${sqlite_filename}.zip"
		download_verified "${SQLITE3_URL}" "${sqlite_archive_filename}" "${SQLITE3_SHA256}"
		unzip -q "${sqlite_archive_filename}"
		rm "${sqlite_archive_filename}"
		mv "${sqlite_filename}" "sqlite3"
	fi
	(
		cd sqlite3
		"${SCRIPT_DIR}"/make_lib_sqlite3.sh "$TARGET_PLATFORM"
	)
}

mkdir -p compile_libs
cd compile_libs

case "${TARGET_PLATFORM}" in
linux | windows | mac)
	IS_DESKTOP=1
	;;
*)
	IS_DESKTOP=0
	;;
esac

# BoringSSL
if [[ "$TARGET_PLATFORM" == "android" ]]; then
	log_info_header "Building BoringSSL..."
	build_cmake_lib boringssl "${BORINGSSL_URL}" "${BORINGSSL_COMMIT}"
fi

# zlib (required to build libpng, curl and freetype for webasm)
if [[ "$TARGET_PLATFORM" == "webasm" ]]; then
	log_info_header "Building zlib..."
	build_cmake_lib zlib "${ZLIB_URL}" "${ZLIB_COMMIT}"
fi

# zlib-rs (the zlib the shared libpng links against). The Linux libpng is static
# and resolves zlib from the system, so it does not need one of its own.
if [[ "$TARGET_PLATFORM" == "windows" || "$TARGET_PLATFORM" == "mac" ]]; then
	log_info_header "Building zlib-rs..."
	"${SCRIPT_DIR}"/make_lib_zlib_rs.sh "$TARGET_PLATFORM"
fi

# libpng (also required to build freetype)
log_info_header "Building libpng..."
build_cmake_lib png "${PNG_URL}" "${PNG_COMMIT}"

# curl. Linux links against a generated stub and loads the real libcurl from the
# system at runtime, so no curl is compiled for it.
if [[ "$TARGET_PLATFORM" == "linux" ]]; then
	log_info_header "Generating stub libcurl..."
	# The headers DDNet compiles against are still the real ones, so the source is
	# checked out even though the library beside them is generated
	fetch_source curl "${CURL_URL}" "${CURL_COMMIT}"
	mkdir -p "curl/${LINUX_X64_BUILD_FOLDER}" "curl/${LINUX_X86_BUILD_FOLDER}"
	python3 "${SCRIPT_DIR}/../generate_fake_curl.py" \
		--output "$(realpath "curl/${LINUX_X64_BUILD_FOLDER}")/libcurl.so"
	python3 "${SCRIPT_DIR}/../generate_fake_curl.py" --link-args=-m32 \
		--output "$(realpath "curl/${LINUX_X86_BUILD_FOLDER}")/libcurl.so"
	# So that nobody mistakes the stub next to it for a real libcurl
	echo "libcurl.so generated by \`python3 scripts/generate_fake_curl.py\`" \
		> "curl/${LINUX_X64_BUILD_FOLDER}/VERSION"
	echo "libcurl.so generated by \`python3 scripts/generate_fake_curl.py --link-args=-m32\`" \
		> "curl/${LINUX_X86_BUILD_FOLDER}/VERSION"
elif [[ "$TARGET_PLATFORM" != "webasm" ]]; then
	log_info_header "Building curl..."
	build_cmake_lib curl "${CURL_URL}" "${CURL_COMMIT}"
fi

# freetype. Linux links the system freetype, so none is bundled for it.
if [[ "$TARGET_PLATFORM" != "linux" ]]; then
	log_info_header "Building freetype..."
	build_cmake_lib freetype "${FREETYPE_URL}" "${FREETYPE_COMMIT}"
fi

# SDL
log_info_header "Building SDL..."
build_cmake_lib sdl "${SDL_URL}" "${SDL_COMMIT}"

# ogg, opus, opusfile
log_info_header "Building ogg..."
fetch_source ogg "${OGG_URL}" "${OGG_COMMIT}"
if [[ "$TARGET_PLATFORM" == "windows" ]]; then
	# win32/ogg.def declares the library as ogg, so the DLL and the import library
	# beside it both name it ogg.dll while CMake writes the file as libogg.dll.
	# Anything linking it would then import a file that is not shipped. The
	# replacement is idempotent.
	ogg_def="ogg/win32/ogg.def"
	sed 's/^LIBRARY ogg$/LIBRARY libogg/' "${ogg_def}" > "${ogg_def}.patched"
	mv "${ogg_def}.patched" "${ogg_def}"
fi
(
	cd ogg
	"${SCRIPT_DIR}"/cmake_lib_compile.sh ogg "$TARGET_PLATFORM"
)
log_info_header "Building opus..."
if [[ "$IS_DESKTOP" == 1 ]]; then
	build_cmake_lib opus "${OPUS_URL}" "${OPUS_DESKTOP_COMMIT}"
else
	build_cmake_lib opus "${OPUS_URL}" "${OPUS_COMMIT}"
fi
log_info_header "Building opusfile..."
build_opusfile

# sqlite3. macOS links the system sqlite3, so none is bundled for it.
if [[ "$TARGET_PLATFORM" != "mac" ]]; then
	log_info_header "Building sqlite3..."
	build_sqlite3
fi

# libwebsockets, wavpack, x264 and ffmpeg are only bundled for the desktop platforms
if [[ "$IS_DESKTOP" == 1 ]]; then
	log_info_header "Building libwebsockets..."
	fetch_source websockets "${WEBSOCKETS_URL}" "${WEBSOCKETS_COMMIT}"
	if [[ "$TARGET_PLATFORM" == "windows" ]]; then
		# The 4.3 branch guards vpt in pollfd.c more narrowly than it declares it,
		# which does not compile for Windows. Fixed on lws main, so this becomes a
		# no-op once the branch catches up. The replacement is idempotent.
		pollfd_c="websockets/lib/core-net/pollfd.c"
		sed 's/^#if !defined(LWS_WITH_EVENT_LIBS)$/#if !defined(LWS_WITH_EVENT_LIBS) \&\& !defined(WIN32) \&\& !defined(_WIN32)/' \
			"${pollfd_c}" > "${pollfd_c}.patched"
		mv "${pollfd_c}.patched" "${pollfd_c}"
	fi
	(
		cd websockets
		"${SCRIPT_DIR}"/cmake_lib_compile.sh websockets "$TARGET_PLATFORM"
	)

	log_info_header "Building wavpack..."
	build_cmake_lib wavpack "${WAVPACK_URL}" "${WAVPACK_COMMIT}"

	# x264 and ffmpeg have no CMake build, so they use their own configure scripts
	log_info_header "Building x264..."
	fetch_source x264 "${X264_URL}" "${X264_COMMIT}"
	(
		cd x264
		"${SCRIPT_DIR}"/make_lib_x264.sh "$TARGET_PLATFORM"
	)

	log_info_header "Building ffmpeg..."
	fetch_source ffmpeg "${FFMPEG_URL}" "${FFMPEG_COMMIT}"
	(
		cd ffmpeg
		"${SCRIPT_DIR}"/make_lib_ffmpeg.sh "$TARGET_PLATFORM"
	)
fi

# Copy files into ddnet-libs structure
log_info_header "Copying files into ddnet-libs structure..."
cd ..
mkdir -p ddnet-libs

# Writes the version of a library into ddnet-libs, so that what is recorded there
# cannot drift from what sources.sh actually built.
function write_version_file() {
	local library="$1"
	local version="$2"
	mkdir -p "ddnet-libs/${library}"
	echo "${version}" > "ddnet-libs/${library}/VERSION"
}

# Copies one built file into the ddnet-libs tree. Shared libraries are built under
# their versioned name with a symlink next to them, so files are dereferenced, while
# directories (the macOS frameworks) keep the symlinks they contain.
function copy_desktop_file() {
	local library="$1"
	local lib_dir="$2"
	local source_file="compile_libs/$3"
	local dest_name="${4:-}"
	if [ -z "${dest_name}" ]; then
		dest_name="$(basename "${source_file}")"
	fi
	if [ ! -e "${source_file}" ]; then
		log_error "ERROR: Expected built file ${source_file}"
		exit 1
	fi
	local target_folder="ddnet-libs/${library}/${TARGET_PLATFORM}/${lib_dir}"
	mkdir -p "${target_folder}"
	if [ -d "${source_file}" ]; then
		rm -rf "${target_folder:?}/${dest_name}"
		cp -a "${source_file}" "${target_folder}/${dest_name}"
	else
		cp -Lf "${source_file}" "${target_folder}/${dest_name}"
	fi
}

# The public headers DDNet compiles against. Most are the same for every platform and
# come straight from the sources, but ogg's config_types.h and libpng's pnglibconf.h
# are generated per build and are taken from the 64 bit one.
function copy_desktop_headers() {
	local bf="$1"

	mkdir -p ddnet-libs/opus/include/ogg ddnet-libs/opus/include/opus
	cp -Lf compile_libs/ogg/include/ogg/ogg.h compile_libs/ogg/include/ogg/os_types.h ddnet-libs/opus/include/ogg/
	cp -Lf "compile_libs/ogg/${bf}/include/ogg/config_types.h" ddnet-libs/opus/include/ogg/
	cp -Lf compile_libs/opus/include/*.h ddnet-libs/opus/include/opus/
	cp -Lf compile_libs/opusfile/include/opusfile.h ddnet-libs/opus/include/

	mkdir -p ddnet-libs/png/include
	cp -Lf compile_libs/png/*.h ddnet-libs/png/include/
	cp -Lf "compile_libs/png/${bf}/pnglibconf.h" ddnet-libs/png/include/

	mkdir -p ddnet-libs/wavpack/include
	cp -Lf compile_libs/wavpack/include/wavpack.h ddnet-libs/wavpack/include/

	# DDNet checks LIBAVCODEC_VERSION_INT to decide which encoder API to call, so
	# these have to come from the same ffmpeg that was built and not from the host
	local ffmpeg_lib
	for ffmpeg_lib in libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale; do
		mkdir -p "ddnet-libs/ffmpeg/include/${ffmpeg_lib}"
		cp -Lf "compile_libs/ffmpeg/${ffmpeg_lib}"/*.h "ddnet-libs/ffmpeg/include/${ffmpeg_lib}/"
	done
	# Generated by configure rather than shipped in the sources
	cp -Lf "compile_libs/ffmpeg/${bf}/libavutil/avconfig.h" ddnet-libs/ffmpeg/include/libavutil/

	if [ -d compile_libs/sqlite3 ]; then
		mkdir -p ddnet-libs/sqlite3/include
		cp -Lf compile_libs/sqlite3/sqlite3.h ddnet-libs/sqlite3/include/
	fi

	# Not built for Linux, which links the system curl and the system freetype
	if [ -d compile_libs/curl/include/curl ]; then
		mkdir -p ddnet-libs/curl/include/curl
		# Only the headers, the folder also holds the files that build them
		cp -Lf compile_libs/curl/include/curl/*.h ddnet-libs/curl/include/curl/
		cp -Lf compile_libs/curl/include/README.md ddnet-libs/curl/include/
	fi
	if [ -d compile_libs/freetype/include ]; then
		mkdir -p ddnet-libs/freetype/include
		cp -R compile_libs/freetype/include/. ddnet-libs/freetype/include/
	fi
}

# lws generates lws_config.h per build, and the public headers are shipped next to it
# per platform because they include it.
function copy_websockets_headers() {
	local bf="$1"
	local include_folder="ddnet-libs/websockets/include/${TARGET_PLATFORM}"
	mkdir -p "${include_folder}"
	cp -Lf "compile_libs/websockets/${bf}/lws_config.h" "${include_folder}/lws_config.h"
	cp -Lf compile_libs/websockets/include/libwebsockets.h "${include_folder}/"
	cp -Lf compile_libs/websockets/include/libwebsockets.hxx "${include_folder}/"
	rm -rf "${include_folder}/libwebsockets"
	cp -R compile_libs/websockets/include/libwebsockets "${include_folder}/"
}

function copy_desktop_libs_linux() {
	local bf="$1"
	local lib_dir="$2"
	copy_desktop_file curl "${lib_dir}" "curl/${bf}/libcurl.so"
	copy_desktop_file curl "${lib_dir}" "curl/${bf}/VERSION"
	copy_desktop_file png "${lib_dir}" "png/${bf}/libpng16.a"
	copy_desktop_file sdl "${lib_dir}" "sdl/${bf}/libSDL2-2.0.so.0"
	copy_desktop_file sqlite3 "${lib_dir}" "sqlite3/${bf}/sqlite3.a" libsqlite3.a
	copy_desktop_file opus "${lib_dir}" "ogg/${bf}/libogg.a"
	copy_desktop_file opus "${lib_dir}" "opus/${bf}/libopus.a"
	copy_desktop_file opus "${lib_dir}" "opusfile/${bf}/libopusfile.a"
	copy_desktop_file websockets "${lib_dir}" "websockets/${bf}/lib/libwebsockets.a"
	copy_desktop_file wavpack "${lib_dir}" "wavpack/${bf}/libwavpack.a"
	# The Linux ffmpeg is linked into DDNet statically, x264 along with it
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavcodec/libavcodec.a"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavformat/libavformat.a"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavutil/libavutil.a"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswresample/libswresample.a"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswscale/libswscale.a"
	copy_desktop_file ffmpeg "${lib_dir}" "x264/${bf}/libx264.a"
	copy_websockets_headers "${bf}"
	strip -s "ddnet-libs/sdl/${TARGET_PLATFORM}/${lib_dir}/libSDL2-2.0.so.0"
}

function copy_desktop_libs_windows() {
	local bf="$1"
	local lib_dir="$2"
	local triple
	triple="$(desktop_triple_for "${bf}")"

	copy_desktop_file curl "${lib_dir}" "curl/${bf}/lib/libcurl.dll"
	copy_desktop_file freetype "${lib_dir}" "freetype/${bf}/libfreetype.dll"
	# libpng's CMake build drops the soversion that the shipped file name carries
	copy_desktop_file png "${lib_dir}" "png/${bf}/libpng16.dll" libpng16-16.dll
	copy_desktop_file sdl "${lib_dir}" "sdl/${bf}/SDL2.dll"
	copy_desktop_file sqlite3 "${lib_dir}" "sqlite3/${bf}/sqlite3.dll"
	copy_desktop_file opus "${lib_dir}" "ogg/${bf}/libogg.dll"
	copy_desktop_file opus "${lib_dir}" "opus/${bf}/libopus.dll"
	copy_desktop_file opus "${lib_dir}" "opusfile/${bf}/libopusfile.dll"
	copy_desktop_file websockets "${lib_dir}" "websockets/${bf}/bin/libwebsockets.dll"
	copy_desktop_file wavpack "${lib_dir}" "wavpack/${bf}/libwavpack-1.dll"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavcodec/avcodec-61.dll"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavformat/avformat-61.dll"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavutil/avutil-59.dll"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswresample/swresample-5.dll"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswscale/swscale-8.dll"

	local dll
	for dll in ddnet-libs/*/"${TARGET_PLATFORM}/${lib_dir}"/*.dll; do
		[ -f "${dll}" ] || continue
		"${triple}-strip" -s "${dll}"
	done

	# The import libraries DDNet links against, named as the build expects them
	make_windows_import_lib "${triple}" "ddnet-libs/curl/${TARGET_PLATFORM}/${lib_dir}/libcurl.dll" curl.lib
	make_windows_import_lib "${triple}" "ddnet-libs/freetype/${TARGET_PLATFORM}/${lib_dir}/libfreetype.dll" freetype.lib
	make_windows_import_lib "${triple}" "ddnet-libs/png/${TARGET_PLATFORM}/${lib_dir}/libpng16-16.dll" libpng16-16.lib
	make_windows_import_lib "${triple}" "ddnet-libs/sdl/${TARGET_PLATFORM}/${lib_dir}/SDL2.dll" SDL2.lib
	make_windows_import_lib "${triple}" "ddnet-libs/sqlite3/${TARGET_PLATFORM}/${lib_dir}/sqlite3.dll" sqlite3.lib
	make_windows_import_lib "${triple}" "ddnet-libs/opus/${TARGET_PLATFORM}/${lib_dir}/libogg.dll" ogg.lib
	make_windows_import_lib "${triple}" "ddnet-libs/opus/${TARGET_PLATFORM}/${lib_dir}/libopus.dll" opus.lib
	make_windows_import_lib "${triple}" "ddnet-libs/opus/${TARGET_PLATFORM}/${lib_dir}/libopusfile.dll" opusfile.lib
	make_windows_import_lib "${triple}" "ddnet-libs/websockets/${TARGET_PLATFORM}/${lib_dir}/libwebsockets.dll" websockets.lib
	make_windows_import_lib "${triple}" "ddnet-libs/wavpack/${TARGET_PLATFORM}/${lib_dir}/libwavpack-1.dll" wavpack.lib
	local ffmpeg_dll
	for ffmpeg_dll in avcodec-61 avformat-61 avutil-59 swresample-5 swscale-8; do
		make_windows_import_lib "${triple}" \
			"ddnet-libs/ffmpeg/${TARGET_PLATFORM}/${lib_dir}/${ffmpeg_dll}.dll" \
			"${ffmpeg_dll%%-*}.lib"
	done

	copy_websockets_headers "${bf}"
}

# The macOS client links SDL as a framework, which SDL2 only produces from its Xcode
# project and not from its CMake build, so the layout is assembled here around the
# dylib that was built. The Metal shaders are compiled into the library itself
# (src/render/metal/SDL_shaders_metal_osx.h), so no metallib is needed beside it.
function create_sdl_framework() {
	local bf="$1"
	local lib_dir="$2"
	local framework="ddnet-libs/sdl/${TARGET_PLATFORM}/${lib_dir}/SDL2.framework"
	local triple
	triple="$(desktop_triple_for "${bf}")"

	rm -rf "${framework}"
	mkdir -p "${framework}/Versions/A/Resources"
	cp -Lf "compile_libs/sdl/${bf}/libSDL2-2.0.0.dylib" "${framework}/Versions/A/SDL2"
	cp -R "compile_libs/sdl/${bf}/include/SDL2/." "${framework}/Versions/A/Headers"

	cat > "${framework}/Versions/A/Resources/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleExecutable</key>
	<string>SDL2</string>
	<key>CFBundleGetInfoString</key>
	<string>https://www.libsdl.org</string>
	<key>CFBundleIdentifier</key>
	<string>org.libsdl.SDL2</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>Simple DirectMedia Layer</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>${SDL_VERSION}</string>
	<key>CFBundleSignature</key>
	<string>SDLX</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>MacOSX</string>
	</array>
	<key>CFBundleVersion</key>
	<string>${SDL_VERSION}</string>
	<key>LSMinimumSystemVersion</key>
	<string>${MAC_SDL_DEPLOYMENT_TARGET}</string>
</dict>
</plist>
EOF

	ln -s A "${framework}/Versions/Current"
	ln -s Versions/Current/Headers "${framework}/Headers"
	ln -s Versions/Current/Resources "${framework}/Resources"
	ln -s Versions/Current/SDL2 "${framework}/SDL2"

	"${triple}-install_name_tool" -id "@rpath/SDL2.framework/Versions/A/SDL2" "${framework}/Versions/A/SDL2"
}

function copy_desktop_libs_mac() {
	local bf="$1"
	local lib_dir="$2"
	copy_desktop_file curl "${lib_dir}" "curl/${bf}/lib/libcurl.a"
	copy_desktop_file freetype "${lib_dir}" "freetype/${bf}/libfreetype.6.dylib"
	copy_desktop_file png "${lib_dir}" "png/${bf}/libpng16.16.dylib"
	copy_desktop_file opus "${lib_dir}" "ogg/${bf}/libogg.a"
	copy_desktop_file opus "${lib_dir}" "opus/${bf}/libopus.a"
	copy_desktop_file opus "${lib_dir}" "opusfile/${bf}/libopusfile.a"
	copy_desktop_file websockets "${lib_dir}" "websockets/${bf}/lib/libwebsockets.19.dylib"
	copy_desktop_file wavpack "${lib_dir}" "wavpack/${bf}/libwavpack.a"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavcodec/libavcodec.61.dylib"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavformat/libavformat.61.dylib"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libavutil/libavutil.59.dylib"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswresample/libswresample.5.dylib"
	copy_desktop_file ffmpeg "${lib_dir}" "ffmpeg/${bf}/libswscale/libswscale.8.dylib"
	create_sdl_framework "${bf}" "${lib_dir}"

	# The shipped dylibs are loaded from the app bundle, so their install names and
	# the dependencies between them have to be relative to it rather than absolute
	local folder="ddnet-libs"
	local triple
	triple="$(desktop_triple_for "${bf}")"
	local install_name_tool="${triple}-install_name_tool"
	for dylib in \
		"${folder}/png/${TARGET_PLATFORM}/${lib_dir}/libpng16.16.dylib" \
		"${folder}/websockets/${TARGET_PLATFORM}/${lib_dir}/libwebsockets.19.dylib" \
		"${folder}/freetype/${TARGET_PLATFORM}/${lib_dir}/libfreetype.6.dylib" \
		"${folder}/ffmpeg/${TARGET_PLATFORM}/${lib_dir}"/*.dylib; do
		"${install_name_tool}" -id "@rpath/$(basename "${dylib}")" "${dylib}"
		# The ffmpeg libraries reference each other by their build time paths
		local dependency
		for dependency in $("${triple}-otool" -L "${dylib}" | awk 'NR > 1 {print $1}' | grep -E "^/usr/local/lib/lib(avcodec|avformat|avutil|swresample|swscale)\." || true); do
			"${install_name_tool}" -change "${dependency}" "@rpath/$(basename "${dependency}")" "${dylib}"
		done
	done

	copy_websockets_headers "${bf}"
}

# arm64 Mach-O binaries need at least an ad hoc signature to load at all, and the
# universal ones are signed again after lipo has rewritten them. osxcross cannot
# produce a signature, so this is a no-op when the libraries are cross compiled.
function sign_mac_libs() {
	if ! command -v codesign > /dev/null 2>&1; then
		log_warn "WARNING: codesign was not found, so the arm64 and universal libraries are unsigned."
		log_warn "They have to be signed on macOS before they are shipped:"
		log_warn "  for i in ddnet-libs/*/mac/{libarm64,libfat}/*.dylib ddnet-libs/sdl/mac/{libarm64,libfat}/SDL2.framework; do codesign -s - \"\$i\"; done"
		return
	fi
	log_info_header "Signing macOS libraries..."
	local target
	for target in \
		ddnet-libs/*/"${TARGET_PLATFORM}"/libarm64/*.dylib \
		ddnet-libs/*/"${TARGET_PLATFORM}"/libfat/*.dylib \
		ddnet-libs/sdl/"${TARGET_PLATFORM}"/libarm64/SDL2.framework \
		ddnet-libs/sdl/"${TARGET_PLATFORM}"/libfat/SDL2.framework; do
		[ -e "${target}" ] || continue
		codesign --force -s - "${target}"
	done
}

# Combines the two macOS slices into the universal binaries the app bundle ships.
function create_mac_universal_libs() {
	log_info_header "Creating universal macOS libraries..."
	local lipo="lipo"
	if ! command -v lipo > /dev/null 2>&1; then
		lipo="${MAC_X64_TRIPLE}-lipo"
	fi
	local x64_folder arm64_folder fat_folder
	local library
	for library in curl freetype png opus websockets wavpack ffmpeg sdl; do
		x64_folder="ddnet-libs/${library}/${TARGET_PLATFORM}/lib64"
		arm64_folder="ddnet-libs/${library}/${TARGET_PLATFORM}/libarm64"
		fat_folder="ddnet-libs/${library}/${TARGET_PLATFORM}/libfat"
		[ -d "${x64_folder}" ] || continue
		rm -rf "${fat_folder}"
		mkdir -p "${fat_folder}"
		local slice
		for slice in "${x64_folder}"/*; do
			local name
			name="$(basename "${slice}")"
			if [ -d "${slice}" ]; then
				# A framework, whose binary is the only part that differs per slice
				cp -a "${slice}" "${fat_folder}/${name}"
				local framework_binary="${name%.framework}"
				"${lipo}" -create \
					"${x64_folder}/${name}/Versions/A/${framework_binary}" \
					"${arm64_folder}/${name}/Versions/A/${framework_binary}" \
					-output "${fat_folder}/${name}/Versions/A/${framework_binary}"
			else
				"${lipo}" -create "${slice}" "${arm64_folder}/${name}" -output "${fat_folder}/${name}"
			fi
		done
	done
}

function copy_desktop_libs() {
	local copy_function="copy_desktop_libs_${TARGET_PLATFORM}"
	if [[ "${TARGET_PLATFORM}" == "linux" ]]; then
		"${copy_function}" "${LINUX_X64_BUILD_FOLDER}" lib64
		"${copy_function}" "${LINUX_X86_BUILD_FOLDER}" lib32
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		"${copy_function}" "${WINDOWS_X64_BUILD_FOLDER}" lib64
		"${copy_function}" "${WINDOWS_X86_BUILD_FOLDER}" lib32
		"${copy_function}" "${WINDOWS_ARM64_BUILD_FOLDER}" libarm64
	elif [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		"${copy_function}" "${MAC_X64_BUILD_FOLDER}" lib64
		"${copy_function}" "${MAC_ARM64_BUILD_FOLDER}" libarm64
		create_mac_universal_libs
		sign_mac_libs
	fi

	# Headers that come from the sources rather than from a build
	mkdir -p "ddnet-libs/sdl/include/${TARGET_PLATFORM}"
	cp -R compile_libs/sdl/include/* "ddnet-libs/sdl/include/${TARGET_PLATFORM}/"
	if [[ "${TARGET_PLATFORM}" == "mac" ]]; then
		copy_desktop_headers "${MAC_X64_BUILD_FOLDER}"
	elif [[ "${TARGET_PLATFORM}" == "windows" ]]; then
		copy_desktop_headers "${WINDOWS_X64_BUILD_FOLDER}"
	else
		copy_desktop_headers "${LINUX_X64_BUILD_FOLDER}"
	fi

	write_version_file curl "${CURL_VERSION}"
	write_version_file freetype "${FREETYPE_VERSION}"
	write_version_file png "${PNG_VERSION}"
	write_version_file sdl "${SDL_VERSION}"
	write_version_file sqlite3 "${SQLITE3_RELEASE}"
	write_version_file websockets "${WEBSOCKETS_VERSION}"
	write_version_file wavpack "${WAVPACK_VERSION}"
	write_version_file ffmpeg "${FFMPEG_VERSION}"
	{
		echo "ogg ${OGG_VERSION}"
		echo "opus ${OPUS_DESKTOP_VERSION}"
		echo "opusfile ${OPUSFILE_VERSION}"
	} > ddnet-libs/opus/VERSIONS
}

if [[ "$IS_DESKTOP" == 1 ]]; then
	copy_desktop_libs
	log_info "Done."
	exit 0
fi

function copy_libs_for_arches() {
	if [[ "${TARGET_PLATFORM}" == "android" ]]; then
		${1} "${ANDROID_ARM_BUILD_FOLDER}" libarm
		${1} "${ANDROID_ARM64_BUILD_FOLDER}" libarm64
		${1} "${ANDROID_X86_BUILD_FOLDER}" lib32
		${1} "${ANDROID_X64_BUILD_FOLDER}" lib64
	elif [[ "${TARGET_PLATFORM}" == "ios" ]]; then
		${1} "${IOS_DEVICE_BUILD_FOLDER}" libarm64
		${1} "${IOS_SIM_ARM64_BUILD_FOLDER}" libsimarm64
		${1} "${IOS_SIM_X64_BUILD_FOLDER}" libsimx86_64
	elif [[ "${TARGET_PLATFORM}" == "webasm" ]]; then
		${1} "${EMSCRIPTEN_WASM_BUILD_FOLDER}" libwasm
	fi
}

if [[ "$TARGET_PLATFORM" == "android" ]]; then
	function _copy_boringssl() {
		local target_libs_folder="ddnet-libs/boringssl/$TARGET_PLATFORM/$2"
		local target_include_folder="ddnet-libs/boringssl/include/$TARGET_PLATFORM"
		mkdir -p "$target_libs_folder"
		mkdir -p "$target_include_folder"
		cp compile_libs/boringssl/"$1"/libcrypto.a "$target_libs_folder"/libcrypto.a
		cp compile_libs/boringssl/"$1"/libssl.a "$target_libs_folder"/libssl.a
		cp -R compile_libs/boringssl/include/openssl "$target_include_folder"
	}
	copy_libs_for_arches _copy_boringssl
fi

if [[ "$TARGET_PLATFORM" == "webasm" ]]; then
	function _copy_zlib() {
		local target_libs_folder="ddnet-libs/zlib/$TARGET_PLATFORM/$2"
		local target_include_folder="ddnet-libs/zlib/include/$TARGET_PLATFORM"
		mkdir -p "$target_libs_folder"
		mkdir -p "$target_include_folder"
		cp compile_libs/zlib/"$1"/libz.a "$target_libs_folder"/libz.a
		cp -R compile_libs/zlib/*.h "$target_include_folder"
		cp -R compile_libs/zlib/"$1"/*.h "$target_include_folder"
	}
	copy_libs_for_arches _copy_zlib
fi

function _copy_png() {
	local target_libs_folder="ddnet-libs/png/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/png/"$1"/libpng16.a "$target_libs_folder"/libpng16.a
}
copy_libs_for_arches _copy_png

if [[ "$TARGET_PLATFORM" != "webasm" ]]; then
	function _copy_curl() {
		local target_libs_folder="ddnet-libs/curl/$TARGET_PLATFORM/$2"
		mkdir -p "$target_libs_folder"
		cp compile_libs/curl/"$1"/lib/libcurl.a "$target_libs_folder"/libcurl.a
	}
	copy_libs_for_arches _copy_curl
fi

function _copy_freetype() {
	local target_libs_folder="ddnet-libs/freetype/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/freetype/"$1"/libfreetype.a "$target_libs_folder"/libfreetype.a
}
copy_libs_for_arches _copy_freetype

function _copy_sdl() {
	local target_libs_folder="ddnet-libs/sdl/$TARGET_PLATFORM/$2"
	local target_include_folder="ddnet-libs/sdl/include/$TARGET_PLATFORM"
	mkdir -p "$target_libs_folder"
	mkdir -p "$target_include_folder"
	cp compile_libs/sdl/"$1"/libSDL2.a "$target_libs_folder"/libSDL2.a
	cp -R compile_libs/sdl/include/* "$target_include_folder"
}
copy_libs_for_arches _copy_sdl

# Copy Java code from SDL2 Android project template
if [[ "$TARGET_PLATFORM" == "android" ]]; then
	target_java_folder="ddnet-libs/sdl/java"
	rm -rf "$target_java_folder"
	mkdir -p "$target_java_folder"
	cp -R compile_libs/sdl/android-project/app/src/main/java/org "$target_java_folder"/
fi

function _copy_opus() {
	local target_libs_folder="ddnet-libs/opus/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/ogg/"$1"/libogg.a "$target_libs_folder"/libogg.a
	cp compile_libs/opus/"$1"/libopus.a "$target_libs_folder"/libopus.a
	cp compile_libs/opusfile/"$1"/libopusfile.a "$target_libs_folder"/libopusfile.a
}
copy_libs_for_arches _copy_opus

function _copy_sqlite3() {
	local target_libs_folder="ddnet-libs/sqlite3/$TARGET_PLATFORM/$2"
	mkdir -p "$target_libs_folder"
	cp compile_libs/sqlite3/"$1"/sqlite3.a "$target_libs_folder"/libsqlite3.a
}
copy_libs_for_arches _copy_sqlite3

if [[ "$TARGET_PLATFORM" == "ios" ]]; then
	function _create_ios_xcframework() {
		local library_folder="$1"
		local static_library="$2"

		local ios_folder="${library_folder}/ios"
		local device_library="${ios_folder}/libarm64/${static_library}"
		local simulator_arm64_library="${ios_folder}/libsimarm64/${static_library}"
		local simulator_x64_library="${ios_folder}/libsimx86_64/${static_library}"

		if [[ ! -f "${device_library}" || ! -f "${simulator_arm64_library}" || ! -f "${simulator_x64_library}" ]]; then
			log_error "ERROR: Missing iOS static library slices for ${static_library} in ${ios_folder}"
			exit 1
		fi

		local simulator_universal_folder="${ios_folder}/simulator_universal"
		local simulator_universal_library="${simulator_universal_folder}/${static_library}"
		local xcframework_name="${static_library%.a}.xcframework"
		local xcframework_path="${ios_folder}/${xcframework_name}"
		local xcframework_info_plist="${xcframework_path}/Info.plist"

		rm -rf "${simulator_universal_folder}" "${xcframework_path}"
		mkdir -p "${simulator_universal_folder}"

		lipo -create \
			"${simulator_arm64_library}" \
			"${simulator_x64_library}" \
			-output "${simulator_universal_library}"

		xcodebuild -create-xcframework \
			-library "${device_library}" \
			-library "${simulator_universal_library}" \
			-output "${xcframework_path}" > /dev/null

		plutil -convert json "$xcframework_info_plist" -o - |
			jq '.AvailableLibraries |= sort_by(.LibraryIdentifier)' |
			plutil -convert xml1 -o "$xcframework_info_plist" -

		rm -rf "${simulator_universal_folder}"
		log_info "Created ${xcframework_path}"
	}
	log_info_header "Creating iOS xcframeworks..."
	_create_ios_xcframework "ddnet-libs/curl" "libcurl.a"
	_create_ios_xcframework "ddnet-libs/freetype" "libfreetype.a"
	_create_ios_xcframework "ddnet-libs/opus" "libogg.a"
	_create_ios_xcframework "ddnet-libs/opus" "libopus.a"
	_create_ios_xcframework "ddnet-libs/opus" "libopusfile.a"
	_create_ios_xcframework "ddnet-libs/png" "libpng16.a"
	_create_ios_xcframework "ddnet-libs/sdl" "libSDL2.a"
	_create_ios_xcframework "ddnet-libs/sqlite3" "libsqlite3.a"

	function _cleanup_ios_library() {
		local library_folder="ddnet-libs/$1/ios"
		rm -rf "${library_folder}/libarm64" "${library_folder}/libsimarm64" "${library_folder}/libsimx86_64"
	}
	_cleanup_ios_library curl
	_cleanup_ios_library freetype
	_cleanup_ios_library opus
	_cleanup_ios_library png
	_cleanup_ios_library sdl
	_cleanup_ios_library sqlite3
fi

log_info "Done."
