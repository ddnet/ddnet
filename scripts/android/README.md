Requirements for building for Android on Linux
==============================================

-	At least 10-15 GiB of free disk space.
-	First follow the general instructions for setting up https://github.com/ddnet/ddnet for building on Linux.
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
	Alternatively, use the precompiled libraries from https://github.com/ddnet/ddnet-libs/.

Requirements for building for Android on Windows using MSYS2
============================================================

-	At least 50 GiB of free disk space if you start from scratch.
-	First install MSYS2 (https://www.msys2.org/wiki/MSYS2-installation/) as well as all required packages for building DDNet using MSYS2 on Windows.
	(There is currently no more detailed guide for this.)
-	Install cargo-ndk and add Android targets to rustup to build Rust with the Android NDK:
	```shell
	cargo install cargo-ndk
	rustup target add armv7-linux-androideabi
	rustup target add i686-linux-android
	rustup target add aarch64-linux-android
	rustup target add x86_64-linux-android
	```
-	Install JDK 21, e.g. from https://adoptium.net/temurin/releases/?package=jdk&os=windows&version=21
-	Install ninja:
	```shell
	pacman -S mingw-w64-x86_64-ninja
	```
-	Install curl:
	```shell
	pacman -S mingw-w64-x86_64-curl
	```
-	Install coreutils so `nproc` is available:
	```shell
	pacman -S coreutils
	```
-	Compiling the libraries is not supported on Windows yet. Use the precompiled libraries from https://github.com/ddnet/ddnet-libs/,
	i.e. make sure to also clone the ddnet-libs submodule, or compile the libraries on a separate Linux system.
-	Set the `ANDROID_HOME` environment variable to override the location where the Android SDK will be installed, e.g. `C:/Android/SDK`. Make sure to only use forward slashes.
-	Install either Android Studio (which includes an SDK manager GUI) from https://developer.android.com/studio or the standalone command-line tools (which include the `sdkmanager` tool) from https://developer.android.com/studio/#command-line-tools-only.
-	When using the command-line tools: Ensure the command-line tools are installed at the expected location, so `%ANDROID_HOME%/cmdline-tools/latest/bin` should contain `sdkmanager.bat`.
Accept the licenses using the SDK manager, otherwise the Gradle build will fail if the licenses have not been accepted:
	```shell
	yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat --licenses
	```
-	Install the following using the SDK Manager in Android Studio (Tools menu) or the `sdkmanager` command-line tool:
	-	SDK Platform for API Level 34
	-	NDK (Side by side)
	-	Android SDK Build-Tools (latest version)

How to build the `ddnet-libs` for Android
=========================================

-	Note: This has only been tested on Linux.
-	Install 7-Zip:
	```shell
	sudo apt install p7zip-full
	```
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

-	These steps are identical on Linux and Windows, except on Windows `bash` must be used as terminal and not `cmd.exe` or PowerShell.
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

Common problems and solutions
=============================

-	If the Gradle build fails with errors messages indicating bugs relating to files in the Gradle cache, try to clear the Gradle cache by deleting the contents of the folder `~/.gradle/caches` (`%USERPROFILE%/.gradle/caches` on Windows).
-	The Gradle build may show a message that the JDK version could not be determined but this can safely be ignored.
-	The Gradle build will fail with errors messages indicating an unsupported class file version if a different version of the JDK is used than specified in `build.gradle`.
	When incrementing the supported JDK version, the Gradle version also has to be incremented according to https://docs.gradle.org/current/userguide/compatibility.html.
	If you have multiple JDKs installed, you can set the JDK version for Gradle using the property `org.gradle.java.home` in the `gradle.properties` file in your Gradle home directory.
