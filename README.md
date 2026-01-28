[![DDraceNetwork](https://ddnet.org/ddnet-small.png)](https://ddnet.org)

[![Build status](https://github.com/ddnet/ddnet/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/ddnet/ddnet/actions/workflows/build.yml?branch=master)
[![Code coverage](https://codecov.io/gh/ddnet/ddnet/branch/master/graph/badge.svg)](https://codecov.io/gh/ddnet/ddnet/branch/master)
[![Translation status](https://hosted.weblate.org/widget/ddnet/ddnet/svg-badge.svg)](https://hosted.weblate.org/engage/ddnet/)

Our own flavor of DDRace, a Teeworlds mod. See the [website](https://ddnet.org) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)) or on [Discord in the developer channel](https://discord.gg/xsEd9xu).

You can get binary releases on the [DDNet website](https://ddnet.org/downloads/), find it on [Steam](https://store.steampowered.com/app/412220/DDraceNetwork/) or [install from repository](#installation-from-repository).

- [Code Browser](https://ddnet.org/codebrowser/DDNet/)
- [Source Code Documentation](https://codedoc.ddnet.org/) (very incomplete, only a few items are documented)
- [Building Guide](docs/BUILDING.md)
- [Debugging Guide](docs/DEBUGGING.md)
- [Contributing Guide](docs/CONTRIBUTING.md)

If you want to learn about the source code, you can check the [Development](https://wiki.ddnet.org/wiki/Development) article on the wiki.

## Cloning

To clone this repository with external libraries and no history (~700 MiB):

```sh
git clone --depth 1 --recursive --shallow-submodules https://github.com/ddnet/ddnet
```

To clone this repository when you have the necessary libraries on your system already with no history (~150 MiB):

```sh
git clone --depth 1 https://github.com/ddnet/ddnet
```

To clone this repository with external libraries and full history (~1 GiB):

```sh
git clone --recursive https://github.com/ddnet/ddnet
```

To clone this repository when you have the necessary libraries on your system already with full history (~450 MiB):

```sh
git clone https://github.com/ddnet/ddnet
```

To clone this repository since we moved the libraries to https://github.com/ddnet/ddnet-libs with history (~250 MiB):

```sh
git clone --shallow-exclude=included-libs https://github.com/ddnet/ddnet
```

To clone the libraries if you have previously cloned DDNet without them, or if you require the ddnet-libs history instead of a shallow clone:

```sh
git submodule update --init --recursive
```

## Running tests (Debian/Ubuntu)

In order to run the tests, you need to install the following library `libgtest-dev`.

This library isn't compiled, so you have to do it:
```sh
sudo apt install libgtest-dev
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make

# copy or symlink libgtest.a and libgtest_main.a to your /usr/lib folder
sudo cp lib/*.a /usr/lib
```

To run the tests you can build the target `run_tests` using

```sh
cmake --build build --target run_tests
```

## Importing the official DDNet Database

```sh
$ wget https://ddnet.org/stats/ddnet-sql.zip # (~2.5 GiB)
$ unzip ddnet-sql.zip
$ yay -S mariadb mysql-connector-c++
$ sudo mysql_install_db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
$ sudo systemctl start mariadb
$ sudo mysqladmin -u root password 'PW'
$ mysql -u root -p'PW'
MariaDB [(none)]> create database teeworlds; create user 'teeworlds'@'localhost' identified by 'PW2'; grant all privileges on teeworlds.* to 'teeworlds'@'localhost'; flush privileges;
# this takes a while, you can remove the KEYs in record_race.sql to trade performance in queries
$ mysql -u teeworlds -p'PW2' teeworlds < ddnet-sql/record_*.sql

$ cat mine.cfg
sv_use_sql 1
add_sqlserver r teeworlds record teeworlds "PW2" "localhost" "3306"
add_sqlserver w teeworlds record teeworlds "PW2" "localhost" "3306"

$ cmake -Bbuild -DMYSQL=ON -GNinja
$ cmake --build build --target DDNet-Server
$ build/DDNet-Server -f mine.cfg
```

<a href="https://repology.org/metapackage/ddnet/versions">
	<img src="https://repology.org/badge/vertical-allrepos/ddnet.svg?header=" alt="Packaging status" align="right">
</a>

## Installation from Repository

Debian/Ubuntu

```sh
sudo apt-get install ddnet
```

MacOS

```sh
brew install --cask ddnet
```

Fedora

```sh
sudo dnf install ddnet
```

Arch Linux

```sh
yay -S ddnet
```

FreeBSD

```sh
sudo pkg install DDNet
```

Windows (Scoop)
```cmd
scoop bucket add games
scoop install games/ddnet
```

## Benchmarking

DDNet is available in the [Phoronix Test Suite](https://openbenchmarking.org/test/pts/ddnet). If you have PTS installed you can easily benchmark DDNet on your own system like this:

```sh
phoronix-test-suite benchmark ddnet
```

## Better Git Blame

First, use a better tool than `git blame` itself, e.g. [`tig`](https://jonas.github.io/tig/). There's probably a good UI for Windows, too. Alternatively, use the GitHub UI, click "Blame" in any file view.

For `tig`, use `tig blame path/to/file.cpp` to open the blame view, you can navigate with arrow keys or kj, press comma to go to the previous revision of the current line, q to quit.

Only then you could also set up git to ignore specific formatting revisions:

```sh
git config blame.ignoreRevsFile formatting-revs.txt
```

## (Neo)Vim Syntax Highlighting for config files

Copy the file detection and syntax files to your vim config folder:

```sh
# vim
cp -R other/vim/* ~/.vim/

# neovim
cp -R other/vim/* ~/.config/nvim/
```
