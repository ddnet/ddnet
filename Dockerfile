FROM debian:11

RUN apt-get update && apt-get install -y mingw-w64 \
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
        cmake

RUN git clone --depth=1 https://github.com/tpoechtrager/osxcross.git /osxcross
RUN /osxcross/tools/get_dependencies.sh

ARG OSX_SDK_FILE
COPY $OSX_SDK_FILE /osxcross/tarballs/
RUN ls -la /osxcross/tarballs
RUN UNATTENDED=1 /osxcross/build.sh

RUN printf '#!/bin/bash\n \
        set -x\n \
        export PATH=$PATH:/osxcross/target/bin\n \
        o32-clang++ -v\n \
        mkdir /build/linux\n \
        cd /build/linux\n \
        pwd\n \
        cmake /ddnet && make\n \
        mkdir /build/win64\n \
        cd /build/win64\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw64.toolchain /ddnet && make\n \
        mkdir /build/win32\n \
        cd /build/win32\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw32.toolchain /ddnet && make\n \
        mkdir /build/macos\n \
        cd /build/macos\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/darwin.toolchain -DCMAKE_OSX_SYSROOT=/osxcross/target/SDK/MacOSX10.11.sdk/ /ddnet && make' \
        >> build-all.sh
RUN chmod +x build-all.sh
RUN mkdir /build

ADD . /ddnet
