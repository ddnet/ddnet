FROM debian:8

WORKDIR /ddnet

ADD . /ddnet

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
        libpnglite-dev \
        libwavpack-dev \
        libopusfile-dev \
        libsdl2-dev

RUN update-alternatives --install /usr/bin/python python /usr/bin/python2.7 1 && \
        update-alternatives --install /usr/bin/python python /usr/bin/python3 2

RUN wget https://cmake.org/files/v3.11/cmake-3.11.0-rc2-Linux-x86_64.sh && \
        chmod +x cmake-3.11.0-rc2-Linux-x86_64.sh && \
        ./cmake-3.11.0-rc2-Linux-x86_64.sh --skip-license --prefix=/usr/local

RUN mkdir /build
VOLUME /build

RUN printf '#!/bin/bash\n \
        set -x\n \
        mkdir /build/linux && cd /build/linux\n \
        pwd\n \
        cmake /ddnet && make\n \
        mkdir /build/win64 && cd /build/win64\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw64.toolchain /ddnet && make\n \
        mkdir /build/win32 && cd /build/win32\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw32.toolchain /ddnet && make' \
        >> build-all.sh

RUN chmod +x build-all.sh

