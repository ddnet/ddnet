#!/bin/bash
# Every third party source the bundled ddnet-libs are built from.
#
# Sources are pinned by commit, not by tag or branch: a tag can be moved and a branch
# always moves, so neither is enough to say which code went into a shipped binary.
# Archives that are not repositories are pinned by SHA-256 instead.
#
# To update a library, change its version and commit here together, then rebuild the
# affected platforms. The VERSION files in ddnet-libs are generated from the versions
# below, so they cannot drift from what is actually built.

# Libraries built for every platform
export CURL_VERSION="8.8.0"
export CURL_URL="https://github.com/curl/curl"
export CURL_COMMIT="fd567d4f06857f4fc8e2f64ea727b1318f76ad33" # curl-8_8_0

export FREETYPE_VERSION="2.13.2"
export FREETYPE_URL="https://gitlab.freedesktop.org/freetype/freetype"
export FREETYPE_COMMIT="920c5502cc3ddda88f6c7d85ee834ac611bb11cc" # VER-2-13-2

export OGG_VERSION="1.3.5"
export OGG_URL="https://github.com/xiph/ogg"
export OGG_COMMIT="e1774cd77f471443541596e09078e78fdc342e4f" # v1.3.5

export OPUSFILE_VERSION="0.12"
export OPUSFILE_URL="https://github.com/xiph/opusfile"
export OPUSFILE_COMMIT="a55c164e9891a9326188b7d4d216ec9a88373739" # v0.12

export PNG_VERSION="1.6.43"
export PNG_URL="https://github.com/pnggroup/libpng"
export PNG_COMMIT="ed217e3e601d8e462f7fd1e04bed43ac42212429" # v1.6.43

export SDL_VERSION="2.32.10"
export SDL_URL="https://github.com/libsdl-org/SDL"
export SDL_COMMIT="5d249570393f7a37e037abf22cd6012a4cc56a71" # release-2.32.10

# The sqlite amalgamation is a generated archive rather than a repository.
export SQLITE3_VERSION="3460000"
export SQLITE3_RELEASE="3.46.0"
export SQLITE3_URL="https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip"
export SQLITE3_SHA256="712a7d09d2a22652fb06a49af516e051979a3984adb067da86760e60ed51a7f5"

# Android and Emscripten build a newer opus than the desktop platforms do. Unifying
# them changes what ships, so it belongs in its own commit rather than in this one.
export OPUS_VERSION="1.5.2"
export OPUS_COMMIT="ddbe48383984d56acd9e1ab6a090c54ca6b735a6" # v1.5.2
export OPUS_DESKTOP_VERSION="1.3.1"
export OPUS_DESKTOP_COMMIT="e85ed7726db5d677c9c0677298ea0cb9c65bdd23" # v1.3.1
export OPUS_URL="https://github.com/xiph/opus"

# Android only
export BORINGSSL_URL="https://boringssl.googlesource.com/boringssl"
export BORINGSSL_COMMIT="a1b6110c3aae7654cd88128186ea826cc27535be"

# Emscripten only, the other platforms get zlib from the NDK or from zlib-rs
export ZLIB_VERSION="1.3.1.2"
export ZLIB_URL="https://github.com/madler/zlib"
export ZLIB_COMMIT="570720b0c24f9686c33f35a1b3165c1f568b96be" # v1.3.1.2

# Desktop only
export FFMPEG_VERSION="7.0.1"
export FFMPEG_URL="https://github.com/FFmpeg/FFmpeg"
export FFMPEG_COMMIT="af25a4bfd2503caf3ee485b27b99b620302f5718" # n7.0.1

export WAVPACK_VERSION="5.9.0"
export WAVPACK_URL="https://github.com/dbry/WavPack"
export WAVPACK_COMMIT="5803634a030e2a11dba602ba057b89cc34486c67" # 5.9.0

# libwebsockets has no release tag on the 4.3 line, so this is the branch tip at the
# time of the last rebuild rather than a released version.
export WEBSOCKETS_VERSION="4.3.10"
export WEBSOCKETS_URL="https://github.com/warmcat/libwebsockets"
export WEBSOCKETS_COMMIT="3df01489ed1b24f606a48e13e892028395b48bcf" # v4.3-stable

# x264 has had no release since 2022 and is shipped from master, so this is a branch
# tip too. It is linked statically into the ffmpeg libraries and not shipped on its own.
export X264_VERSION="master"
export X264_URL="https://code.videolan.org/videolan/x264.git"
export X264_COMMIT="0480cb05fa188d37ae87e8f4fd8f1aea3711f7ee"

# The zlib API that the shipped libpng links against, implemented by zlib-rs.
export ZLIB_RS_VERSION="0.6.7"
