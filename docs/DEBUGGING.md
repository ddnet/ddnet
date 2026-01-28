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

## Symbolizing a crash dump (.RTP):
Symbolizing converts raw crash data into humanly readable stacktraces.

**Download the crash dump**  
    Make sure it has the original filename because it contains important information.

**Download the debug symbols**  
    Get them from https://ddnet.org/downloads/symbols/ with matching git hash and platform (win32/64, steam/non-steam).  
   - The git hash and platform are found in the filename of the crash dump.
   - If the filename does not contain `steam`, then it is the standalone version.

3. **Install binutils**  
   Install `addr2line` and `objdump` using your systems package manager or from msys2 for Windows:
   - **[MSYS2](https://packages.msys2.org/packages/mingw-w64-x86_64-binutils)**
   - **Ubuntu/Debian**: `sudo apt install binutils`

4. **Run the script**  
```sh
   bash scripts/parse_drmingw.sh <path to server/client symbols exe> <path to server/client crash dump>
```
   - The script is found under `./scripts/`
   - On Windows, you need to explicitly run with `bash` and not in a Windows terminal.
