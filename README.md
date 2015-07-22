[![DDraceNetwork](http://ddnet.tw/ddnet-small.png)](http://ddnet.tw) [![Build Status](https://circleci.com/gh/def-/ddnet.png)](https://circleci.com/gh/def-/ddnet)
================================

Our own flavor of DDRace, a Teeworlds mod. See the [website](http://ddnet.tw) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)).

Building
--------

You can get binary releases on the [DDNet website](http://ddnet.tw/downloads/).

To compile DDNet yourself, you can follow the [instructions for compiling Teeworlds](https://www.teeworlds.com/?page=docs&wiki=compiling_everything).

DDNet requires additional libraries, that are bundled for the most common platforms (Windows, Mac, Linux, all x86 and x86_64). Instead you can install these libraries on your system, remove the `config.lua` and `bam` should use the system-wide libraries by default: `libcurl, libogg, libopus, libopusfile`

The MySQL server can be built with `bam server_sql_release`. It requires `libmariadbclient-dev` and `libmysqlcppconn-dev`, which are also bundled for the common platforms.
