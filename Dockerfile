# Docker image to build DDNet for Linux and Windows (32 bit, 64 bit)
# Usage:
# 1. Build image
# docker build -t ddnet - < Dockerfile
# 2. Run the DDNet build script
# docker run -it -v PATH_TO_DDNET:/ddnet:ro -v PATH_TO_OUTPUT_DIRECTORY:/build ddnet ./build-all.sh
FROM debian:12

RUN apt-get update && apt-get install -y gcc-mingw-w64-x86-64-posix \
        g++-mingw-w64-x86-64-posix \
        gcc-mingw-w64-i686-posix \
        g++-mingw-w64-i686-posix \
        wget \
        git \
        ca-certificates \
        build-essential \
        python3 \
        libcurl4-openssl-dev \
        libfreetype6-dev \
        libglew-dev \
        libogg-dev \
        libopus-dev \
        libpng-dev \
        libwavpack-dev \
        libopusfile-dev \
        libsdl2-dev \
        cmake \
        glslang-tools \
        libavcodec-extra \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libcurl4-openssl-dev \
        libnotify-dev \
        libsqlite3-dev \
        libssl-dev \
        libvulkan-dev \
        libx264-dev \
        spirv-tools \
        curl

RUN curl https://sh.rustup.rs -sSf | \
    sh -s -- --default-toolchain stable -y

RUN ~/.cargo/bin/rustup toolchain install stable
RUN ~/.cargo/bin/rustup target add i686-pc-windows-gnu
RUN ~/.cargo/bin/rustup target add x86_64-pc-windows-gnu

RUN printf '#!/bin/bash\n \
        export PATH=$PATH:$HOME/.cargo/bin\n \
        set -x\n \
        mkdir /build\n \
        mkdir /build/linux\n \
        cd /build/linux\n \
        pwd\n \
        cmake /ddnet && make -j$(nproc) \n \
        mkdir /build/win64\n \
        cd /build/win64\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw64.toolchain /ddnet && make -j$(nproc) \n \
        mkdir /build/win32\n \
        cd /build/win32\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw32.toolchain /ddnet && make -j$(nproc) \n' \
        > build-all.sh
RUN chmod +x build-all.sh
RUN mkdir /build
