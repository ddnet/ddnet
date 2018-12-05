# ChillerBot User Xp
<img src="https://github.com/chillerbot/chillerbot-ux/blob/master/data/chillerbot_ux.png" height="86" width="86">


Made for main client usage. Based on DDNet which is based on DDrace which is based on Teeworlds.

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/chillerbot/chillerbot-ux

To clone the libraries if you have previously cloned chillerbot without them:

    git submodule update --init --recursive

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
