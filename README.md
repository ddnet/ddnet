[![DDraceNetwork](https://ddnet.org/ddnet-small.png)](https://ddnet.org) [![](https://github.com/ddnet/ddnet/workflows/Build/badge.svg)](https://github.com/ddnet/ddnet/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster) [![](https://codecov.io/gh/ddnet/ddnet/branch/master/graph/badge.svg)](https://codecov.io/gh/ddnet/ddnet/branch/master)

Our own flavor of DDRace, a Teeworlds mod. See the [website](https://ddnet.org) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)) or on [Discord in the developer channel](https://discord.gg/xsEd9xu).

You can get binary releases on the [DDNet website](https://ddnet.org/downloads/), find it on [Steam](https://store.steampowered.com/app/412220/DDraceNetwork/) or [install from repository](#installation-from-repository).

- [Code Browser](https://ddnet.org/codebrowser/DDNet/)
- [Source Code Documentation](https://codedoc.ddnet.org/) (very incomplete, only a few items are documented)
- [Contributing Guide](CONTRIBUTING.md)

If you want to learn about the source code, you can check the [Development](https://wiki.ddnet.org/wiki/Development) article on the wiki.

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/ddnet/ddnet

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/ddnet/ddnet

To clone this repository with history since we moved the libraries to https://github.com/ddnet/ddnet-libs (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/ddnet/ddnet

To clone the libraries if you have previously cloned DDNet without them, or if you require the ddnet-libs history instead of a shallow clone:

    git submodule update --init --recursive

Dependencies on Linux / macOS
-----------------------------

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install build-essential cargo cmake git glslang-tools google-mock libavcodec-extra libavdevice-dev libavfilter-dev libavformat-dev libavutil-dev libcurl4-openssl-dev libfreetype6-dev libglew-dev libnotify-dev libogg-dev libopus-dev libopusfile-dev libpng-dev libsdl2-dev libsqlite3-dev libssl-dev libvulkan-dev libwavpack-dev libx264-dev python3 rustc spirv-tools

On older distributions like Ubuntu 18.04 don't install `google-mock`, but instead set `-DDOWNLOAD_GTEST=ON` when building to get a more recent gtest/gmock version.

On older distributions `rustc` version might be too old, to get an up-to-date Rust compiler you can use [rustup](https://rustup.rs/) with stable channel instead or try the `rustc-mozilla` package.

Or on CentOS, RedHat and AlmaLinux like this:

    sudo yum install cargo cmake ffmpeg-devel freetype-devel gcc gcc-c++ git glew-devel glslang gmock-devel gtest-devel libcurl-devel libnotify-devel libogg-devel libpng-devel libx264-devel make openssl-devel opus-devel opusfile-devel python2 rust SDL2-devel spirv-tools sqlite-devel vulkan-devel wavpack-devel

Or on Fedora like this:

    sudo dnf install cargo cmake ffmpeg-devel freetype-devel gcc gcc-c++ git glew-devel glslang gmock-devel gtest-devel libcurl-devel libnotify-devel libogg-devel libpng-devel make openssl-devel opus-devel opusfile-devel python2 SDL2-devel spirv-tools sqlite-devel vulkan-devel wavpack-devel x264-devel

Or on Arch Linux like this:

    sudo pacman -S --needed base-devel cmake curl ffmpeg freetype2 git glew glslang gmock libnotify libpng opusfile python rust sdl2 spirv-tools sqlite vulkan-headers vulkan-icd-loader wavpack x264

Or on Gentoo like this:

    emerge --ask dev-db/sqlite dev-lang/rust-bin dev-libs/glib dev-libs/openssl dev-util/glslang dev-util/spirv-headers dev-util/spirv-tools media-libs/freetype media-libs/glew media-libs/libglvnd media-libs/libogg media-libs/libpng media-libs/libsdl2 media-libs/libsdl2[vulkan] media-libs/opus media-libs/opusfile media-libs/pnglite media-libs/vulkan-loader[layers] media-sound/wavpack media-video/ffmpeg net-misc/curl x11-libs/gdk-pixbuf x11-libs/libnotify

On macOS you can use [homebrew](https://brew.sh/) to install build dependencies like this:

    brew install cmake ffmpeg freetype glew glslang googletest libpng molten-vk opusfile rust SDL2 spirv-tools vulkan-headers wavpack x264

If you don't want to use the system libraries, you can pass the `-DPREFER_BUNDLED_LIBS=ON` parameter to cmake.

Building on Linux and macOS
---------------------------

To compile DDNet yourself, execute the following commands in the source root:

    cmake -Bbuild
    cmake --build build

DDNet requires additional libraries, some of which are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86\_64) for convenience and the official builds. The bundled libraries for official builds are now in the ddnet-libs submodule. Note that when you build and develop locally, you should ideally use your system's package manager to install the dependencies, instead of relying on ddnet-libs submodule, which does not contain all dependencies anyway (e.g. openssl, vulkan). See the previous section for how to get the dependencies. Alternatively see the following build arguments for how to disable some features and their dependencies (`-DVULKAN=OFF` won't require Vulkan for example).

The following is a non-exhaustive list of build arguments that can be passed to the `cmake` command-line tool in order to enable or disable options in build time:

* **-DCMAKE_BUILD_TYPE=[Release|Debug|RelWithDebInfo|MinSizeRel]** <br>
  An optional CMake variable for setting the build type. If not set, defaults to "Release" if `-DDEV=ON` is **not** used, and "Debug" if `-DDEV=ON` is used. See `CMAKE_BUILD_TYPE` in CMake Documentation for more information.

* **-DPREFER_BUNDLED_LIBS=[ON|OFF]** <br>
  Whether to prefer bundled libraries over system libraries. Setting to ON will make DDNet use third party libraries available in the `ddnet-libs` folder, which is the git-submodule target of the [ddnet-libs](https://github.com/ddnet/ddnet-libs) repository mentioned above -- Useful if you do not have those libraries installed and want to avoid building them. If set to OFF, will only use bundled libraries when system libraries are not found. Default value is OFF.

* **-DWEBSOCKETS=[ON|OFF]** <br>
  Whether to enable WebSocket support for server. Setting to ON requires the `libwebsockets-dev` library installed. Default value is OFF.

* **-DMYSQL=[ON|OFF]** <br>
  Whether to enable MySQL/MariaDB support for server. Requires at least MySQL 8.0 or MariaDB 10.2. Setting to ON requires the `libmariadbclient-dev` library installed, which are also provided as bundled libraries for the common platforms. Default value is OFF.

  Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client. Make sure that the CMake configuration summary says that it found MySQL libs that were not bundled (no "using bundled libs").

* **-DTEST_MYSQL=[ON|OFF]** <br>
  Whether to test MySQL/MariaDB support in GTest based tests. Default value is OFF.

  Note that this requires a running MySQL/MariaDB database on localhost with this setup:

```
CREATE DATABASE ddnet;
CREATE USER 'ddnet'@'localhost' IDENTIFIED BY 'thebestpassword';
GRANT ALL PRIVILEGES ON ddnet.* TO 'ddnet'@'localhost';
FLUSH PRIVILEGES;
```

* **-DAUTOUPDATE=[ON|OFF]** <br>
  Whether to enable the autoupdater. Packagers may want to disable this for their packages. Default value is ON for Windows and Linux.

* **-DCLIENT=[ON|OFF]** <br>
  Whether to enable client compilation. If set to OFF, DDNet will not depend on Curl, Freetype, Ogg, Opus, Opusfile, and SDL2. Default value is ON.

* **-DVIDEORECORDER=[ON|OFF]** <br>
  Whether to add video recording support using FFmpeg to the client. Default value is ON.

* **-DDOWNLOAD_GTEST=[ON|OFF]** <br>
  Whether to download and compile GTest. Useful if GTest is not installed and, for Linux users, there is no suitable package providing it. Default value is OFF.

* **-DDEV=[ON|OFF]** <br>
  Whether to optimize for development, speeding up the compilation process a little. If enabled, don't generate stuff necessary for packaging. Setting to ON will set CMAKE\_BUILD\_TYPE to Debug by default. Default value is OFF.

* **-DUPNP=[ON|OFF]** <br>
  Whether to enable UPnP support for the server.
  You need to install `libminiupnpc-dev` on Debian, `miniupnpc` on Arch Linux.
  Default value is OFF.

* **-DVULKAN=[ON|OFF]** <br>
  Whether to enable the vulkan backend.
  On Windows you need to install the Vulkan SDK and set the `VULKAN_SDK` environment flag accordingly.
  Default value is ON for Windows x86\_64 and Linux, and OFF for Windows x86 and macOS.

* **-GNinja** <br>
  Use the Ninja build system instead of Make. This automatically parallelizes the build and is generally faster. Compile with `ninja` instead of `make`. Install Ninja with `sudo apt install ninja-build` on Debian, `sudo pacman -S --needed ninja` on Arch Linux.

* **-DCMAKE_CXX_LINK_FLAGS=[FLAGS]** <br>
  Custom flags to set for compiler when linking.

* **-DEXCEPTION_HANDLING=[ON|OFF]** <br>
  Enable exception handling (only works with Windows as of now, uses DrMingw there). Default value is OFF.

* **-DIPO=[ON|OFF]** <br>
  Enable interprocedural optimizations, also known as Link Time Optimization (LTO). Default value is OFF.

* **-DFUSE_LD=[OFF|LINKER]** <br>
  Linker to use. Default value is OFF to try mold, lld, gold.

* **-DSECURITY_COMPILER_FLAGS=[ON|OFF]** <br>
  Whether to set security-relevant compiler flags like `-D_FORTIFY_SOURCE=2` and `-fstack-protector-all`. Default Value is ON.

Running tests (Debian/Ubuntu)
-----------------------------

In order to run the tests, you need to install the following library `libgtest-dev`.

This library isn't compiled, so you have to do it:
```bash
sudo apt install libgtest-dev
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make

# copy or symlink libgtest.a and libgtest_main.a to your /usr/lib folder
sudo cp lib/*.a /usr/lib
```

To run the tests you must target `run_tests` with make:
`make run_tests`

Code formatting
---------------
We use clang-format 10 to format the C++ code of this project. Execute `scripts/fix_style.py` after changing the code to ensure code is formatted properly, a GitHub central style checker will do the same and prevent your change from being submitted.

On Arch Linux you can install clang-format 10 using the [clang-format-static-bin AUR package](https://aur.archlinux.org/packages/clang-format-static-bin/) with [yay](https://github.com/Jguer/yay#Binary):
```bash
yay -S clang-format-static-bin
```

Or on macOS you can install clang-format 10 using a [homebrew tap](https://github.com/r-lib/homebrew-taps):
```bash
brew install r-lib/taps/clang-format@10
sudo ln -s /opt/homebrew/Cellar/clang-format@10/10.0.1/bin/clang-format /opt/homebrew/bin/clang-format-10
```

Using AddressSanitizer + UndefinedBehaviourSanitizer or Valgrind's Memcheck
---------------------------------------------------------------------------
ASan+UBSan and Memcheck are useful to find code problems more easily. Please use them to test your changes if you can.

For ASan+UBSan compile with:
```bash
CC=clang CXX=clang++ CXXFLAGS="-fsanitize=address,undefined -fsanitize-recover=address,undefined -fno-omit-frame-pointer" CFLAGS="-fsanitize=address,undefined -fsanitize-recover=address,undefined -fno-omit-frame-pointer" cmake -DCMAKE_BUILD_TYPE=Debug .
make
```
and run with:
```bash
UBSAN_OPTIONS=suppressions=./ubsan.supp:log_path=./SAN:print_stacktrace=1:halt_on_errors=0 ASAN_OPTIONS=log_path=./SAN:print_stacktrace=1:check_initialization_order=1:detect_leaks=1:halt_on_errors=0 LSAN_OPTIONS=suppressions=./lsan.supp ./DDNet
```

Check the SAN.\* files afterwards. This finds more problems than memcheck, runs faster, but requires a modern GCC/Clang compiler.

For valgrind's memcheck compile a normal Debug build and run with: `valgrind --tool=memcheck ./DDNet`
Expect a large slow down.

Building on Windows with the Visual Studio IDE
--------------------------------------

Download and install some version of [Microsoft Visual Studio](https://www.visualstudio.com/) (At the time of writing, MSVS Community 2022) with **C++ support**.

You'll have to install both [Python 3](https://www.python.org/downloads/) and [Rust](https://rustup.rs/) as well.

Make sure the MSVC build tools, C++ CMake-Tools and the latest Windows SDK version appropriate to your windows version are selected in the installer.

Now open up your Project folder, Visual Studio should automatically detect and configure your project using CMake.

On your tools hotbar next to the triangular "Run" Button, you can now select what you want to start (e.g game-client or game-server) and build it.

Building on Windows with standalone MSVC build tools 
--------------------------------------

First off you will need to install the MSVC [Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/), [Python 3](https://www.python.org/downloads/) as well as [Rust](https://www.rust-lang.org/tools/install).

To compile and build DDNet on Windows, use your IDE of choice either with a CMake integration (e.g Visual Studio Code), or by ~~**deprecated**~~ using the CMake GUI.

Configure CMake to use the MSVC Build Tools appropriate to your System by your IDE's instructions.

If you're using Visual Studio Code, you can use the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension to configure and build the project.

You can then open the project folder in VSC and press `Ctrl+Shift+P` to open the command palette, then search for `CMake: Configure`

This will open up a prompt for you to select a kit, select your `Visual Studio` version and save it. You can now use the GUI (bottom left) to compile and build your project.


Cross-compiling on Linux to Windows x86/x86\_64
-----------------------------------------------

Install MinGW cross-compilers of the form `i686-w64-mingw32-gcc` (32 bit) or
`x86_64-w64-mingw32-gcc` (64 bit). This is probably the hard part. ;)

Then add `-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mingw64.toolchain` to the
**initial** CMake command line.

Cross-compiling on Linux to WebAssembly via Emscripten
--------------------------------------------------------

Install Emscripten cross-compilers (e.g. `sudo apt install emscripten`) on a modern linux distro.

If you need to compile the ddnet-libs for WebAssembly, simply call

```bash
# <directory to build in> should be a directory outside of the project's source directory
scripts/compile_libs/gen_libs.sh <directory to build in> webasm
```

from the project's source directory. It will automatically create a directory called `ddnet-libs`.
You can then manually merge this directory with the one in the ddnet source directory.

Then run `emcmake cmake .. -DVIDEORECORDER=OFF -DVULKAN=OFF -DSERVER=OFF -DTOOLS=OFF -DPREFER_BUNDLED_LIBS=ON` in your build directory.

To test the compiled code locally, just use `emrun --browser firefox DDNet.html`

To host the compiled .html file copy all `.data`, `.html`, `.js`, `.wasm` files to the web server. (see /other/emscripten/minimal.html for a minimal html example)

Then enable cross origin policies. Example for apache2 on debian based distros:
```bash
sudo a2enmod header

# edit the apache2 config to allow .htaccess files
sudo nano /etc/apache2/apache2.conf

# set AllowOverride to All for your directory
# then create a .htaccess file on the web server (where the .html is)
# and add these lines
Header add Cross-Origin-Embedder-Policy "require-corp"
Header add Cross-Origin-Opener-Policy "same-origin"

# now restart apache2
sudo service apache2 restart
```

Cross-compiling on Linux to macOS
---------------------------------

Install [osxcross](https://github.com/tpoechtrager/osxcross), then add
`-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/darwin.toolchain` and
`-DCMAKE_OSX_SYSROOT=/path/to/osxcross/target/SDK/MacOSX10.11.sdk/` to the
**initial** CMake command line.

Install `dmg` and `hfsplus` from
[libdmg-hfsplus](https://github.com/mozilla/libdmg-hfsplus) and `newfs_hfs`
from
[diskdev\_cmds](http://pkgs.fedoraproject.org/repo/pkgs/hfsplus-tools/diskdev_cmds-540.1.linux3.tar.gz/0435afc389b919027b69616ad1b05709/diskdev_cmds-540.1.linux3.tar.gz)
to unlock the `package_dmg` target that outputs a macOS disk image.

Importing the official DDNet Database
-------------------------------------

```bash
$ wget https://ddnet.org/stats/ddnet-sql.zip
$ unzip ddnet-sql.zip
$ yaourt -S mariadb mysql-connector-c++
$ mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
$ systemctl start mariadb
$ mysqladmin -u root password 'PW'
$ mysql -u root -p'PW'
MariaDB [(none)]> create database teeworlds; create user 'teeworlds'@'localhost' identified by 'PW2'; grant all privileges on teeworlds.* to 'teeworlds'@'localhost'; flush privileges;
# this takes a while, you can remove the KEYs in record_race.sql to trade performance in queries
$ mysql -u teeworlds -p'PW2' teeworlds < ddnet-sql/record_*.sql

$ cat mine.cfg
sv_use_sql 1
add_sqlserver r teeworlds record teeworlds "PW2" "localhost" "3306"
add_sqlserver w teeworlds record teeworlds "PW2" "localhost" "3306"

$ cmake -Bbuild -DMYSQL=ON
$ cmake --build build --target DDNet-Server
$ build/DDNet-Server -f mine.cfg
```

<a href="https://repology.org/metapackage/ddnet/versions">
    <img src="https://repology.org/badge/vertical-allrepos/ddnet.svg?header=" alt="Packaging status" align="right">
</a>

Installation from Repository
----------------------------

Debian/Ubuntu

```bash
$ apt-get install ddnet

```

MacOS

```bash
$ brew install --cask ddnet
```

Fedora

```bash
$ dnf install ddnet
```

Arch Linux

```bash
$ yay -S ddnet
```

FreeBSD

```bash
$ pkg install DDNet
```

Windows (Scoop)
```
scoop bucket add games
scoop install games/ddnet
```

Benchmarking
------------

DDNet is available in the [Phoronix Test Suite](https://openbenchmarking.org/test/pts/ddnet). If you have PTS installed you can easily benchmark DDNet on your own system like this:

```bash
$ phoronix-test-suite benchmark ddnet
```

Better Git Blame
----------------

First, use a better tool than `git blame` itself, e.g. [`tig`](https://jonas.github.io/tig/). There's probably a good UI for Windows, too. Alternatively, use the GitHub UI, click "Blame" in any file view.

For `tig`, use `tig blame path/to/file.cpp` to open the blame view, you can navigate with arrow keys or kj, press comma to go to the previous revision of the current line, q to quit.

Only then you could also set up git to ignore specific formatting revisions:
```bash
git config blame.ignoreRevsFile formatting-revs.txt
```

(Neo)Vim Syntax Highlighting for config files
----------------------------------------
Copy the file detection and syntax files to your vim config folder:

```bash
# vim
cp -R other/vim/* ~/.vim/

# neovim
cp -R other/vim/* ~/.config/nvim/
```
