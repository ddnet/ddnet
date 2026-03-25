# How to locally build the `ddnet-libs` for iOS

Install Xcode on a macOS computer, install iOS sdk for device, arm64 simulator and x86_64 simulator.

Required build tools can be installed with homebrew:

```sh
brew update
brew install autoconf automake cmake libtool ninja pkg-config
```

Set to use GNU m4 for iOS build, or opusfile would fail to build on first run.

```sh
export M4="$(brew --prefix m4)/bin/m4"
```

Call build script

```sh
scripts/compile_libs/gen_libs.sh build-ios-libs ios
```

from the project's source directory.

To merge with the one in the source directory:

```sh
rsync -a build-ios-libs/ddnet-libs ddnet-libs
```
