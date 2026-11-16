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
-	You need the `ddnet-libs` for Emscripten.
	The easiest way is to use the precompiled libraries from https://github.com/ddnet/ddnet-libs/ by cloning the `ddnet-libs` submodule.
	Alternatively, you can build the `ddnet-libs` for Emscripten yourself. See below for instructions on how to compile them locally.
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
-	Note that you need to clone the `ddnet-libs` submodule first, before you merge the self-build `ddnet-libs` with it.
	Otherwise, the `ddnet-libs` folder will be missing common files shared between platforms (e.g., header files).
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

### Building the DDNet server via Emscripten

-	The server can be compiled to WebAssembly to run it with Node.js or in a [Cloudflare Durable Object](https://developers.cloudflare.com/durable-objects/).
	Clients connect to it with WebSockets, which is supported by the web client.
	Native clients cannot connect, as UDP is not supported.
-	The client and the server cannot be built at the same time with Emscripten, so use a separate build directory for the server.
-	Run `emcmake cmake .. -G "Unix Makefiles" -DCLIENT=OFF -DTOOLS=OFF -DPREFER_BUNDLED_LIBS=ON` in your build directory to configure followed by `cmake --build . -j8` to build.
-	The server is built without threads, as they are not available in Cloudflare Workers.
	Work that would run on other threads, like database queries, runs synchronously instead.
	HTTP requests are not supported, so the server cannot register itself at the master server, which cannot list servers that are only reachable with WebSockets anyway.
-	The build directory contains the files `DDNet-Server.mjs` and `DDNet-Server.wasm` as well as the scripts to run the server, which are copied from `other/emscripten/server`.

### Running the server with Node.js

-	Node.js 20 or newer is required.
-	Copy the files `DDNet-Server.mjs`, `DDNet-Server.wasm`, `package.json` and `server.mjs` from the build directory to the server.
-	Run `npm install --omit=dev` to install the dependencies.
-	Run `node server.mjs [arguments]` in the directory which contains the `data` directory to start the server.
	The arguments are passed to the server and it uses the same configuration files and directories as the native server.
-	Clients connect with WebSockets to the server port (`sv_port`), so enter `<host>:<port>` in the web client to connect.
	The web client uses secure WebSockets (`wss://`) when it is served via HTTPS, because browsers do not allow insecure connections from secure pages.
	To accept secure WebSockets, set the environment variables `DDNET_TLS_CERT` and `DDNET_TLS_KEY` to the paths of a certificate and its private key.
	Alternatively, use a reverse proxy and set `DDNET_TRUST_PROXY=1` to use the client address from its `X-Forwarded-For` header.

### Running the server in a Cloudflare Durable Object

-	Copy the files `DDNet-Server.mjs`, `DDNet-Server.wasm`, `empty.mjs`, `package.json`, `worker.mjs` and `wrangler.toml` from the build directory into a new directory and run `npm install` in it.
-	Create the directory `files` with the files of the server, which are copied into its working directory when it starts.
	For example, copy the maps from `data/maps` to `files/data/maps` and create the config file `files/autoexec_server.cfg`.
	The files are only accessible to the server and they are not served to the public.
-	Optionally, set additional console commands that are executed when the server starts, like the rcon password, as a secret with `npx wrangler secret put DDNET_COMMANDS`.
	Multiple commands are separated with `;`.
-	Run `npx wrangler dev` to test the server locally or run `npx wrangler deploy` to deploy it to Cloudflare.
-	Clients connect with secure WebSockets (`wss://`) to port 443 of the Worker, so enter `ddnet-server.<account>.workers.dev:443` in the web client when it is served via HTTPS.
	The path of the WebSocket URL selects the Durable Object, which allows running multiple servers with one Worker. The root path is the server named `default`.
-	The server starts when the first client connects and keeps running while clients are connected.
	The home directory of the server, which contains the user directory with the ranks database, is stored in the Durable Object storage.

### Common problems and solutions

-	Make sure that the `CC` and `CXX` variables are not set in your shell environment, because this will interfere with the compiler selection of the emsdk.
