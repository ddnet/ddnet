Requirements for building for Android
=====================================

-	At least 10-15 GiB of free disk space.
-	First follow the general instructions for setting up https://github.com/ddnet/ddnet for building on Linux.
	This guide has only been tested on Linux.
-	Note: Use a stable version of Rust. Using the nightly version results in linking errors.
-	Install the Android NDK (version 26) in the same location
	where Android Studio would unpack it (`~/Android/Sdk/ndk/`):
	```shell
	mkdir ~/Android
	cd ~/Android
	mkdir Sdk
	cd Sdk
	mkdir ndk
	cd ndk
	wget https://dl.google.com/android/repository/android-ndk-r26d-linux.zip
	unzip android-ndk-r26d-linux.zip
	unlink android-ndk-r26d-linux.zip
	```
-	Install the Android SDK build tools (version 30.0.3) in the same location
	where Android Studio would unpack them (`~/Android/Sdk/build-tools/`):
	```shell
	# Assuming you already created the Android/Sdk folders in the previous step
	cd ~/Android/Sdk
	mkdir build-tools
	cd build-tools
	wget https://dl.google.com/android/repository/build-tools_r30.0.3-linux.zip
	unzip build-tools_r30.0.3-linux.zip
	unlink build-tools_r30.0.3-linux.zip
	mv android-11 30.0.3
	```
-	Install the Android command-line tools and accept the licenses using the SDK manager,
	otherwise the Gradle build will fail if the licenses have not been accepted:
	```shell
	# Assuming you already created the Android/Sdk folders in the previous step
	cd ~/Android/Sdk
	mkdir cmdline-tools
	cd cmdline-tools
	wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
	unzip commandlinetools-linux-11076708_latest.zip
	unlink commandlinetools-linux-11076708_latest.zip
	mv cmdline-tools latest
	yes | latest/bin/sdkmanager --licenses
	```
-	Install cargo-ndk and add Android targets to rustup to build Rust with the Android NDK:
	```shell
	cargo install cargo-ndk
	rustup target add armv7-linux-androideabi
	rustup target add i686-linux-android
	rustup target add aarch64-linux-android
	rustup target add x86_64-linux-android
	```
-	Install OpenJDK 21:
	```shell
	sudo apt install openjdk-21-jdk
	```
-	Install 7zip for building `ddnet-libs`:
	```shell
	sudo apt install p7zip-full
	```
-	Install ninja:
	```shell
	sudo apt install ninja-build
	```
-	Install curl:
	```shell
	sudo apt install curl
	```
-	*(macOS only)* Install coreutils so `nproc` is available:
	```shell
	brew install coreutils
	```
-	Build the `ddnet-libs` for Android (see below). Follow all above steps first.


How to build the `ddnet-libs` for Android
=========================================

-	There is a script to automatically download and build all repositories,
	this requires an active internet connection and can take around 30 minutes:
	```shell
	mkdir build-android-libs
	scripts/compile_libs/gen_libs.sh build-android-libs android
	```
	**Warning**: Do not choose a directory inside the `src` folder!
-	If you see several red error messages in the first few minutes,
	abort the compilation with repeated Ctrl+C presses.
	Examine the output and ensure that you installed the NDK to the correct location.
-	After the script finished executing, it should have created a `ddnet-libs` directory
	in your selected output folder, which contains all libraries in the correct directory
	format and can be merged with the `ddnet-libs` folder in the source directory:
	```shell
	cp -r build-android-libs/ddnet-libs/. ddnet-libs/
	```


How to build the DDNet client for Android
=========================================

-	Open a terminal inside the `ddnet` project root directory and run the following:
	```shell
	scripts/android/cmake_android.sh <x86/x86_64/arm/arm64/all> <Game name> <Package name> <Debug/Release> <Build folder>
	```
	- The first parameter denotes the architecture.
	  Use `all` to compile for all architectures.
	  Note that all architectures will be compiled in parallel.
	  For testing, only compile for one architecture initially to get readable output.
	- The second parameter denotes the APK name, which must be equal to the library name.
	  If you want to rename the APK, do it after the build.
	- The third parameter denotes the package name of the APK.
	- The fourth parameter denotes the build type.
	- The fifth parameter denotes the build folder.
-	Example to build only for `x86_64` architecture in debug mode:
	```shell
	scripts/android/cmake_android.sh x86_64 DDNet org.ddnet.client Debug build-android-debug
	```
-	To build a signed APK, generate a signing key and export environment variables before running the build script:
	```shell
	keytool -genkey -v -keystore my-release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-alias
	export TW_KEY_NAME=<key name>
	export TW_KEY_PW=<key password>
	export TW_KEY_ALIAS=<key alias>
	```
-	By default, the version code and name of the APK will be determined automatically
	based on the definitions in `src/game/version.h`.
	You can also specify the build version code and name manually before running the build script, e.g.:
	```shell
	export TW_VERSION_CODE=20210819
	export TW_VERSION_NAME="1.0"
	```
	The version code must increase for newer version in order for users to automatically update to them.
	The version name is the string that will be displayed to the user, e.g. `1.2.3-snapshot4`.
-	Example to build a signed APK in release mode for all architectures:
	```shell
	keytool -genkey -v -keystore Teeworlds.jks -keyalg RSA -keysize 2048 -validity 10000 -alias Teeworlds-Key
	# It will prompt for the password, input for example "mypassword"
	export TW_KEY_NAME=Teeworlds.jks
	export TW_KEY_PW=mypassword
	export TW_KEY_ALIAS=Teeworlds-Key
	# Version code and name will be determined automatically
	scripts/android/cmake_android.sh all DDNet org.ddnet.client Release build-android-release
	```
-	Note that you should only generate a signing key once (and make backups).
	Users can only update apps automatically if the same package name and signing key have been used,
	else they must manually uninstall the old app.
