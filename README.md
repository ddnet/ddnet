[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![CircleCI Build Status](https://circleci.com/gh/ddnet/ddnet/tree/master.png)](https://circleci.com/gh/ddnet/ddnet) [![Travis CI Build Status](https://travis-ci.org/ddnet/ddnet.svg?branch=master)](https://travis-ci.org/ddnet/ddnet) [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/foeer8wbynqaqqho?svg=true)](https://ci.appveyor.com/project/def-/ddnet)

Our own flavor of DDRace, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)) or on [Discord in the developer channel](https://discord.gg/xsEd9xu).

You can get binary releases on the [DDNet website](https://ddnet.tw/downloads/).

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/ddnet/ddnet

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/ddnet/ddnet

To clone this repository with history since we moved the libraries to https://github.com/ddnet/ddnet-libs (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/ddnet/ddnet

To clone the libraries if you have previously cloned ddnet without them:

    git submodule update --init --recursive

Building on Linux and macOS
---------------------------

To compile DDNet yourself, you can follow the [instructions for compiling Teeworlds](https://www.teeworlds.com/?page=docs&wiki=compiling_everything). Alternatively we also support CMake, so something like this works:

    mkdir build
    cd build
    cmake ..
    make

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86\_64). The bundled libraries are now in the ddnet-libs submodule.

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install cmake libcurl4-openssl-dev libfreetype6-dev libogg-dev libopus-dev libopusfile-dev libsdl2-dev

Or on Arch Linux like this:

    pacman -S cmake curl freetype2 opusfile sdl2

If you have the libraries installed, but still want to use the bundled ones instead, you can do so by removing your build directory and re-running CMake with `-DPREFER_BUNDLED_LIBS=ON`, e.g. `cmake -DPREFER_BUNDLED_LIBS=ON ..`.

MySQL (or MariaDB) support in the server is not included in the binary releases but it can be built by specifying `-DMYSQL=ON`, like `cmake -DMYSQL=ON ..`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that the CMake configuration summary says that it found MySQL libs that were not bundled (no "using bundled libs").

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
sudo cp *.a /usr/lib
```

To run the tests you must target `run_tests` with make:
`make run_tests`

Building on Windows with Visual Studio
--------------------------------------

Download and install some version of [Microsoft Visual Studio](https://www.visualstudio.com/) (as of writing, MSVS Community 2017) with **C++ support**, install [Python 3](https://www.python.org/downloads/) **for all users** and install [CMake](https://cmake.org/download/#latest).

Start CMake and select the source code folder (where DDNet resides, the directory with `CMakeLists.txt`). Additionally select a build folder, e.g. create a build subdirectory in the source code directory. Click "Configure" and select the Visual Studio generator (it should be pre-selected, so pressing "Finish" will suffice). After configuration finishes and the "Generate" reactivates, click it. When that finishes, click "Open Project". Visual Studio should open. You can compile the DDNet client by right-clicking the DDNet project (not the solution) and select "Select as StartUp project". Now you should be able to compile DDNet by clicking the green, triangular "Run" button.

Cross-compiling to Windows x86/x86\_64
--------------------------------------

Install MinGW cross-compilers of the form `i686-w64-mingw32-gcc` (32 bit) or
`x86_64-w64-mingw32-gcc` (64 bit). This is probably the hard part. ;)

Then add `-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mingw64.toolchain` to the
**initial** CMake command line.

Importing the official DDNet Database
-------------------------------------

```bash
$ wget https://ddnet.tw/stats/ddnet-sql.zip
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

$ bam server_sql_release
$ ./DDNet-Server_sql -f mine.cfg
```
