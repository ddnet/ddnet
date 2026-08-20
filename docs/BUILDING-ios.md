# Requirements for building for iOS on macOS

-	Xcode with the iOS SDK and command line tools installed.
-	CMake 3.20 or newer.
-	Rust (stable). For reproducible builds, use the same version as the CI.
-	Install the iOS Rust targets:
	```shell
	rustup target add aarch64-apple-ios
	rustup target add aarch64-apple-ios-sim
	rustup target add x86_64-apple-ios
	```
-	Build the `ddnet-libs` for iOS (see below) or use precompiled ones from https://github.com/ddnet/ddnet-libs/.

# How to locally build the `ddnet-libs` for iOS

-	Install dependencies:
	```shell
	brew install autoconf automake cmake libtool m4 ninja pkg-config
	```
-	Set to use GNU m4 for the build, or opusfile would fail to build on first run:
	```shell
	export M4="$(brew --prefix m4)/bin/m4"
	```
-	Run the iOS library build script:
	```shell
	scripts/compile_libs/gen_libs.sh build-ios-libs ios
	```
	**Warning**: Do not choose a directory inside the `src` folder!
-	After the script finished executing, it should have created a `ddnet-libs` directory
	in your selected output folder, which contains all libraries in the correct directory
	format and can be merged with the `ddnet-libs` folder in the source directory:
	```shell
	find ddnet-libs -type d -name ios -exec rm -r {} + -prune
	cp -r build-ios-libs/ddnet-libs/. ddnet-libs/
	```

# How to build the DDNet client for iOS

-	Open a terminal inside the project root and run:
	```shell
	scripts/ios/cmake_ios.sh <device/sim-arm64/sim-x86_64/sim> <App name> <Bundle id> <Debug/Release> <Build folder>
	```
	- `device` builds an arm64 iPhoneOS app.
	- `sim` uses the host architecture to build the iOS simulator app.
-	Example to build a simulator app on Apple Silicon:
	```shell
	scripts/ios/cmake_ios.sh sim DDNet org.ddnet.client Debug build-ios-sim
	```
-	Device builds are unsigned unless `IOS_DEVELOPMENT_TEAM` is set to your team ID,
	which is enough to check that the app compiles, but such an app cannot be
	installed on a device. Set the variable to use automatic signing:
	```shell
	IOS_DEVELOPMENT_TEAM=XXXXXXXXXX scripts/ios/cmake_ios.sh device DDNet org.ddnet.client Debug build-ios-device
	```
	The device must be registered with your team before this works. Xcode only
	does that for devices that it has prepared for development, so run the app
	from Xcode once if the build fails with "Your team has no devices".
