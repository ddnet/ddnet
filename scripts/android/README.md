Requirements for building:
==========================
-	Android NDK (tested with NDK 23), must be in the same location in which Android studio would unpack it (~/Android/Sdk/ndk/)
	atleast version 23
-	Android SDK build tools
	version 30.0.3
-	ddnet-libs with Android libs
-	Java -- JDK 11+
-	7zip (for ddnet-libs building)
-	ninja
-	curl runtime

How to build:
=============
-	run a terminal inside the source directory:
	`scripts/android/cmake_android.sh <x86/x86_64/arm/arm64/all> <Game name> <Debug/Release>`
	where the first parameter is the arch (all for all arches), the second is the apk name, which must be equal to the library name (if you want to rename the APK do it after the build)
	and the third parameter which simply defines the build type
	
-	if you build with a signing key for the APK
	Generate one with
	`keytool -genkey -v -keystore my-release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-alias`
	export environment variables for the script
	```
	export TW_KEY_NAME=<key name>
	export TW_KEY_PW=<key password>
	export TW_KEY_ALIAS=<key alias>
	```
	so for example:
	```
	keytool -genkey -v -keystore Teeworlds.jks -keyalg RSA -keysize 2048 -validity 10000 -alias Teeworlds-Key
	(it will prompt an input:)
	Input keystore-password: mypassword
	
	export TW_KEY_NAME=Teeworlds.jks
	export TW_KEY_PW=mypassword
	export TW_KEY_ALIAS=Teeworlds-Key
	scripts/android/cmake_android.sh all DDNet Release
	```

	You can also specify the build version code and build version string before running the build script, e.g.:
	```
	export TW_VERSION_CODE=20210819
	export TW_VERSION_NAME="1.0"
	```

How to build the ddnet-libs for Android:
========================================
-	There is a script to automatically download and build all repositories, this requires an active internet connection:
	`scripts/compile_libs/gen_libs.sh <directory to build in> android`
	Warning!: DO NOT CHOOSE A DIRECTORY INSIDE THE SOURCE TREE
	
	After the script finished executing it should have created a ddnet-libs directory which created all libs in the right directory format and can be merged with ddnet-libs in the source directory
