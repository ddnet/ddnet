## Using AddressSanitizer + UndefinedBehaviourSanitizer or Valgrind's Memcheck

ASan+UBSan and Memcheck are useful to find code problems more easily. Please use them to test your changes if you can.

For ASan+UBSan compile with:

```sh
CC=clang CXX=clang++ CXXFLAGS="-fsanitize=address,undefined -fsanitize-recover=all -fno-omit-frame-pointer" CFLAGS="-fsanitize=address,undefined -fsanitize-recover=all -fno-omit-frame-pointer" cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -GNinja
cmake --build build
```

and run with:

```sh
UBSAN_OPTIONS=suppressions=./ubsan.supp:log_path=./SAN:print_stacktrace=1:halt_on_errors=0 ASAN_OPTIONS=log_path=./SAN:print_stacktrace=1:check_initialization_order=1:detect_leaks=1:halt_on_errors=0:handle_abort=1 LSAN_OPTIONS=suppressions=./lsan.supp ./DDNet
```

Check the SAN.\* files afterwards. This finds more problems than memcheck, runs faster, but requires a modern GCC/Clang compiler.

For valgrind's memcheck compile a normal Debug build and run with: `valgrind --tool=memcheck ./DDNet`
Expect a large slow down.

## Symbolizing a crash dump (`.RTP`) for an official Windows release

Symbolizing converts raw crash data into humanly readable stacktraces.
The following assumes that the crash dump is from an official DDNet release, release candidate or nightly version, for which debug symbols are available.

If you build the client on Windows with `-DEXCEPTION_HANDLING=ON` in debug mode, then the crash dumps will immediately contain readable stacktraces.

1. Requirements:
    - Install `binutils` (in particular the `addr2line` and `objdump` tools) using your system's package manager or with MSYS2 on Windows:
        - MSYS2: `pacman -S mingw-w64-x86_64-binutils`
        - Ubuntu/Debian: `sudo apt install binutils`
    - Install [Python](https://www.python.org/downloads/).
    - Download/clone the [ddnet](https://github.com/ddnet/ddnet) repository.

2. Download the crash dump (`.RTP` file).
    - Make sure it has the original filename because it contains important information to identify the matching debug symbols.

3. Run the script to symbolize the crash dump.
    - Open a terminal in the `ddnet` project folder and run the following:
    ```sh
    python scripts/parse_crashlog_drmingw.py "<path to crash dump>"
    ```
    - The script will automatically download the correct debug symbols for recent crash dumps. If this does not work, refer to step 4.

4. (Optional) Manually download and specify debug symbols to symbolize the crash dump.
    - Download and unpack the debug symbols archive from https://ddnet.org/downloads/symbols/ with matching git commit revision and platform (win32/win64/win-arm64, steam/non-steam).
    - The git commit revision and platform are found in the filename of the crash dump.
    - If the filename does not contain `steam`, then it is the standalone version.
    - Open a terminal in the `ddnet` project folder and run the following:
    ```sh
    python scripts/parse_crashlog_drmingw.py "<path to server/client crash dump>" --executable "<path to server/client symbols exe>"
    ```

5. Examine the output.
    - Check whether the stacktrace is theoretically possible. If the order of function calls is nonsensical or if many lines contain `?? ??:0`, you likely paired the crash dump with the wrong debug symbols.
