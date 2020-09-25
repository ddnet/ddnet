# ChillerBot User Xp
<img src="https://github.com/chillerbot/chillerbot-ux/blob/chillerbot/chillerbot_ux.png" height="86" width="86">


Made for main client usage. Based on DDNet which is based on DDrace which is based on Teeworlds.

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/chillerbot/chillerbot-ux

To clone the libraries if you have previously cloned chillerbot without them:

    git submodule update --init --recursive

Dependencies on Linux
---------------------

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install build-essential cmake git libcurl4-openssl-dev libssl-dev libfreetype6-dev libglew-dev libnotify-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libsqlite3-dev libwavpack-dev python

Or on Arch Linux like this:

    sudo pacman -S --needed base-devel cmake curl freetype2 git glew libnotify opusfile python sdl2 sqlite wavpack

There is an [AUR package for pnglite](https://aur.archlinux.org/packages/pnglite/). For instructions on installing it, see [AUR packages installation instructions on ArchWiki](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages).

If you don't want to use the system libraries, you can pass the `-DPREFER_BUNDLED_LIBS=ON` parameter to cmake.

Building on Linux and macOS
---------------------------

To compile DDNet yourself, execute the following commands in the source root:

    mkdir build
    cd build
    cmake ..
    make


Building on windows
-------------------

    mkdir build
    cd build
    cmake ..
    cmake --build .

If you use MinGW as a compiler the client is in build/DDNet.exe if vs in build/Debug/DDNet.exe
