## Cross-compiling on Linux to WebAssembly via Emscripten

Install Emscripten cross-compilers on a modern linux distro. Follow the [instructions to install the emsdk](https://emscripten.org/docs/getting_started/downloads.html). Installing Emscripten from the package manager (e.g. with `sudo apt install emscripten`) may not work, as building some libraries requires a more recent version of Emscripten than available via package manager.

If you need to compile the ddnet-libs for WebAssembly, simply call

```sh
scripts/compile_libs/gen_libs.sh build-webasm-libs webasm
```

from the project's source directory. It will automatically create a directory called `ddnet-libs` in your build directory.
You can then merge this directory with the one in the ddnet source directory:

```sh
find ddnet-libs -type d -name webasm -exec rm -r {} + -prune
cp -r build-webasm-libs/ddnet-libs/. ddnet-libs/
```

Note that using Rust versions 1.90.0 - 1.93.0 does not work. Either use an older or newer version of Rust.
Run `rustup target add wasm32-unknown-emscripten` to install the WASM target for compiling Rust.

Create a new directory to build the client in.
Then run `emcmake cmake .. -G "Unix Makefiles" -DVIDEORECORDER=OFF -DVULKAN=OFF -DSERVER=OFF -DTOOLS=OFF -DPREFER_BUNDLED_LIBS=ON` in your build directory to configure followed by `cmake --build . -j8` to build.
For testing it is highly recommended to build in debug mode by also passing the argument `-DCMAKE_BUILD_TYPE=Debug` when invoking `emcmake cmake`, as this speeds up the build process and adds debug information as well as additional checks.
Note that using the Ninja build system with Emscripten is not currently possible due to [CMake issue 16395](https://gitlab.kitware.com/cmake/cmake/-/issues/16395).

To test the compiled code locally, just use `emrun --browser firefox DDNet.html`.

To host the compiled .html file copy all `.data`, `.html`, `.js`, `.wasm` files to the web server.
See `other/emscripten/minimal.html` for a minimal HTML example. You can also run `other/emscripten/server.py` to host a minimal server for testing using Python without needing to install Emscripten.

Enable cross-origin policies when using a proper web server. Example for apache2 on Debian-based distros:

```sh
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
