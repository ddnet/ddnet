[![DDraceNetwork](http://ddnet.tw/ddnet-small.png)](http://ddnet.tw) [![Build Status](https://circleci.com/gh/ddnet/ddnet/tree/master.png)](https://circleci.com/gh/ddnet/ddnet)
================================

Our own flavor of DDRace, a Teeworlds mod. See the [website](http://ddnet.tw) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)).

You can get binary releases on the [DDNet website](http://ddnet.tw/downloads/).

Building
--------

To compile DDNet yourself, you can follow the [instructions for compiling Teeworlds](https://www.teeworlds.com/?page=docs&wiki=compiling_everything).

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86_64). Instead you can install these libraries on your system, remove the `config.lua` and `bam` should use the system-wide libraries by default. You can install all required dependencies on Debian and Ubuntu like this:

    apt-get install libsdl1.2-dev libfreetype6-dev libcurl4-openssl-dev libogg-dev libopus-dev libopusfile-dev

If you have the libraries installed, but still want to use the bundled ones instead, you can specify so by running `bam config curl.use_pkgconfig=false opus.use_pkgconfig=false opusfile.use_pkgconfig=false ogg.use_pkgconfig=false`.

The MySQL server is not included in the binary releases and can be built with `bam server_sql_release`. It requires `libmariadbclient-dev`, `libmysqlcppconn-dev` and `libboost-dev`, which are also bundled for the common platforms.

Note that the bundled MySQL libraries might not work properly on your system. If you run into connection problems with the MySQL server, for example that it connects as root while you chose another user, make sure to install your system libraries for the MySQL client and C++ connector. Make sure that `mysql.use_mysqlconfig` is set to `true` in your config.lua.
