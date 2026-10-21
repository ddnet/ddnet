FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        curl \
        glslang-tools \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libclang-rt-dev \
        libcurl4-openssl-dev \
        libfreetype6-dev \
        libglew-dev \
        libnotify-dev \
        libogg-dev \
        libopus-dev \
        libopusfile-dev \
        libpng-dev \
        libsdl2-dev \
        libsqlite3-dev \
        libssl-dev \
        libswresample-dev \
        libswscale-dev \
        libvulkan-dev \
        libwavpack-dev \
        libx264-dev \
        ninja-build \
        pkg-config \
        spirv-tools \
    && rm -rf /var/lib/apt/lists/*

RUN curl https://sh.rustup.rs -sSf | sh -s -- --default-toolchain stable -y \
    && ~/.cargo/bin/rustup toolchain install stable \
    && ~/.cargo/bin/rustup default stable
ENV PATH=/root/.cargo/bin:$PATH

COPY . /src
WORKDIR /src

RUN cmake -S /src -B /src/build -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_C_FLAGS="-g -fno-omit-frame-pointer" \
      -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer" \
    && cmake --build /src/build -j"$(nproc)"

RUN cmake -S /src -B /src/build --target run_tests || echo "WARNING: tests reported failures (regular build)"

RUN cmake -S /src -B /src/build-asan -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fsanitize-recover=all -fno-omit-frame-pointer" \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fsanitize-recover=all -fno-omit-frame-pointer" \
    && cmake --build /src/build-asan -j"$(nproc)"

RUN cmake --build /src/build-asan --target run_tests || echo "WARNING: tests reported failures (asan build)"
