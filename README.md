[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![CircleCI Build Status](https://circleci.com/gh/ddnet/ddnet/tree/master.png)](https://circleci.com/gh/ddnet/ddnet) [![Travis CI Build Status](https://travis-ci.org/ddnet/ddnet.svg?branch=master)](https://travis-ci.org/ddnet/ddnet) [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/foeer8wbynqaqqho?svg=true)](https://ci.appveyor.com/project/def-/ddnet)

================================

Our own flavor of DDRace, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)).

You can get binary releases on the [DDNet website](https://ddnet.tw/downloads/).

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/ddnet/ddnet

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/ddnet/ddnet

To clone this repository with history since we moved the libraries to https://github.com/ddnet/ddnet-libs (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/ddnet/ddnet

Building
--------

To compile DDNet yourself, you can follow the [instructions for compiling Teeworlds](https://www.teeworlds.com/?page=docs&wiki=compiling_everything).

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86_64). The bundled libraries are now in the ddnet-libs submodule.

You can install the required libraries on your system, remove the `config.lua` and `bam` should use the system-wide libraries by default. You can install all required dependencies and bam on Debian and Ubuntu like this:

    apt-get install libsdl2-dev libfreetype6-dev libcurl4-openssl-dev libogg-dev libopus-dev libopusfile-dev bam

Or on Arch Linux like this:

    pacman -S sdl2 freetype2 curl opusfile bam

If you have the libraries installed, but still want to use the bundled ones instead, you can specify so by running `bam config curl.use_pkgconfig=false opus.use_pkgconfig=false opusfile.use_pkgconfig=false ogg.use_pkgconfig=false`.

The MySQL server is not included in the binary releases and can be built with `bam server_sql_release`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that `mysql.use_mysqlconfig` is set to `true` in your config.lua.

Importing the official DDNet Database
--------

```
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
