## Cross-compiling to WebAssembly via Emscripten

### Requirements for building via Emscripten on Linux

-	At least 5-10 GiB of free disk space.
-	First follow the general instructions for setting up https://github.com/ddnet/ddnet for building on Linux.
-	Install and activate the latest 4.x version of the Emscripten SDK (emsdk). To that end, run the following commands within the folder where the emsdk should be installed:
	```sh
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk
	./emsdk install 4.0.22
	./emsdk activate 4.0.22
	```
	Refer to the [Emscripten documentation](https://emscripten.org/docs/getting_started/downloads.html) for details about using and updating the Emscripten SDK.
	Note: Installing Emscripten from the package manager (e.g. with `sudo apt install emscripten`) may not work, as building some libraries requires a different version of Emscripten than is available via package manager.
-	Use a stable version of Rust. Using the nightly version results in linking errors.
	Note that using Rust versions 1.90.0 - 1.93.0 specifically does not work.
	For reproducible builds, use exactly the same version of Rust that is used in the CI:
	```sh
	rustup default 1.89.0
	```
-	Add the WASM targets to rustup to build Rust with Emscripten *after* setting the Rust version:
	```sh
	rustup target add wasm32-unknown-emscripten
	```
-	Build the `ddnet-libs` via Emscripten. See below for instructions on how to compile them locally.
	Alternatively, use the precompiled libraries from https://github.com/ddnet/ddnet-libs/.
	The libraries can also be built by manually running the GitHub workflow [`build-libraries-emscripten`](https://github.com/ddnet/ddnet/blob/master/.github/workflows/build-libraries-emscripten.yml).
-	Important note for reproducible builds: To make the build reproducible, you must use exactly the same versions of the emsdk and Rust.
	Furthermore, exactly CMake version 3.22.1 must be used for configuring.
-	To use the emsdk within a terminal in the later steps, run `source .../emsdk/emsdk_env.sh` once in the terminal (with the correct path to the emsdk) to setup the environment variables.

### Requirements for building via Emscripten on Windows

-	Building the DDNet client via Emscripten is currently not directly possible on Windows, due to [CMake issue 25049](https://gitlab.kitware.com/cmake/cmake/-/issues/25049).
	On Windows, the CMake Ninja and Makefile generators mangle `$` signs in the link options, which are required for the `DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` flag.
	This causes the linking step to either fail due to incorrectly escaped arguments or the client will not launch because SDL cannot find the relevant functions that should have been made accessible by setting the `DEFAULT_LIBRARY_FUNCS_TO_INCLUDE` flag.
-	If you are motivated, it is possible to manually fix up and run the linking command with correct arguments, which allows building and running the client successfully on Windows.

### Building the `ddnet-libs` via Emscripten

-	Note for building on Windows: All commands in this README must be executed in a `bash` terminal (e.g., from MSYS2) and not in `cmd.exe` or PowerShell.
-	On Linux, install the following dependencies:
	```sh
	sudo apt install autoconf automake libtool m4
	```
-	On Windows using MSYS2, install the following dependencies:
	```sh
	pacman -S autoconf-wrapper automake-wrapper libtool unzip
	```
-	There is a script to automatically download and build all libraries.
	This requires an active internet connection and can take around 10-20 minutes:
	```sh
	scripts/compile_libs/gen_libs.sh build-webasm-libs webasm
	```
	**Warning**: Do not choose a directory inside the `src` folder!
-	If you see error messages in the first few minutes, examine the output and ensure that you
	installed the emsdk and other prerequisites correctly.
-	After the script finished executing, it should have created a `ddnet-libs` directory
	in your selected output folder, which contains all libraries in the correct directory
	format and can be merged with the `ddnet-libs` folder in the source directory:
	```sh
	find ddnet-libs -type d -name webasm -exec rm -r {} + -prune
	cp -r build-webasm-libs/ddnet-libs/. ddnet-libs/
	```
-	To force the libraries to be rebuild, delete the individual build folders inside the library folders:
	```sh
	rm -rf build-webasm-libs/compile_libs/*/build_webasm_*
	```
-	To force the libraries to be downloaded again when changing the library versions, delete the entire library build folder:
	```sh
	rm -rf build-webasm-libs
	```

### Building the DDNet client via Emscripten

-	Create a new directory to build the client in.
-	Then run `emcmake cmake .. -G "Unix Makefiles" -DVIDEORECORDER=OFF -DVULKAN=OFF -DSERVER=OFF -DTOOLS=OFF -DPREFER_BUNDLED_LIBS=ON` in your build directory to configure followed by `cmake --build . -j8` to build.
-	For testing it is highly recommended to build in debug mode by also passing the argument `-DCMAKE_BUILD_TYPE=Debug` when invoking `emcmake cmake`, as this speeds up the build process and adds debug information as well as additional checks.
-	Note that using the Ninja build system with Emscripten is not currently possible due to [CMake issue 16395](https://gitlab.kitware.com/cmake/cmake/-/issues/16395).

### Running the client via Emscripten

-	To test the compiled code locally, run `emrun --browser firefox index.html` in the build directory.
-	To host the compiled Emscripten client, copy the `DDNet.data`, `DDNet.js`, `DDNet.wasm` and `index.html` files from the build directory to the web server.
	The file `index.html` in the build folder is copied from `other/emscripten/index.html`.
-	You can also run `other/emscripten/server.py` to host a minimal server for testing using Python without needing to install Emscripten.
-	When using a proper web server, enable cross-origin policies to allow the client to download its components.
	For example for apache2 on Debian-based distributions:
	```sh
	sudo a2enmod header
	```
	Edit the apache2 config to allow `.htaccess` files:
	```sh
	sudo nano /etc/apache2/apache2.conf
	```
	Set `AllowOverride` to `All` for your directory in the editor.
	Then create a `.htaccess` file on the web server (where the `index.html` is) and add these lines it:
	```htaccess
	Header add Cross-Origin-Embedder-Policy "require-corp"
	Header add Cross-Origin-Opener-Policy "same-origin"
	```
	Now restart apache2:
	```sh
	sudo service apache2 restart
	```

### Common problems and solutions

-	Make sure that the `CC` and `CXX` variables are not set in your shell environment, because this will interfere with the compiler selection of the emsdk.
