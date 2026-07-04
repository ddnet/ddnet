# DDNet Wii U Port Tools

This directory contains utility scripts to patch and prepare the PowerPC binaries for DDNet on the Wii U.

## How to Build

We compile the project using a Dockerized devkitPro toolchain, patch the PowerPC atomic loops and `tweq` instructions, and package everything as a `.wuhb` homebrew bundle.

For Windows, you can simply run **`build_wiiu.bat`** at the repository root!

### Manual Steps / Linux commands

If you want to run the steps manually, perform the following commands in the repository root:

1. **Compile**:
   ```bash
   docker run --rm -v "$(pwd):/src" -w /src/build devkitpro/devkitppc:latest bash -c "make -j4"
   ```

2. **Patch Atomics & Tweq**:
   ```bash
   python wiiu_tools/patch_atomics_universal.py build/DDNet.elf
   python wiiu_tools/patch_elf.py build/DDNet.elf
   ```

3. **Package RPX & WUHB**:
   ```bash
   docker run --rm -v "$(pwd):/src" -w /src devkitpro/devkitppc:latest bash -c "/opt/devkitpro/tools/bin/elf2rpl build/DDNet.elf build/DDNet.rpx && /opt/devkitpro/tools/bin/wuhbtool build/DDNet.rpx build/DDNet.wuhb --content=build/wiiu_content --name='DDraceNetwork' --short-name='DDNet' --author='DDNet team' --icon=build/icon.png --tv-image=build/tv_image.png --drc-image=build/drc_image.png > /dev/null"
   ```

## Why are the Patches Needed?

1. **`patch_atomics_universal.py`**:
   The Wii U PPC compiler generates 32-bit hardware synchronization loops (`lwarx`/`stwcx.`) when compiling `std::atomic` variables. WUT's HLE memory virtualization layer on CafeOS does not fully emulate these reservations, leading to thread lock contention or infinite loops.
   This script parses the `.text` section of the binary, searches for atomic loop instruction sequences, and replaces them with standard instruction loads (`lwz`/`stw`) and a branch `nop` to bypass CPU hardware locking.

2. **`patch_elf.py`**:
   The Wii U CPU has a hardware bug on the `tweq` (Trap Word Equal) instruction that can freeze the console during certain pointer checks or exception triggers. This script replaces all `tweq` opcodes in the `.text` section with `nop` (0x60000000) instructions.
