#!/bin/bash

COLOR_RED="\e[1;31m"
COLOR_RESET="\e[0m"

if [ -z ${1+x} ]; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass APK name to build script"
	exit 1
fi

if [ -z ${2+x} ]; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass package name to build script"
	exit 1
fi

if [ -z ${3+x} ]; then
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build type to build script: Debug, Release"
	exit 1
fi

APK_BASENAME="$1"
APK_PACKAGE_NAME="$2"
APK_BUILD_TYPE="$3"

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
	RELEASE_TYPE_NAME=debug
elif [[ "${APK_BUILD_TYPE}" == "Release" ]]; then
	RELEASE_TYPE_NAME=release
else
	printf "${COLOR_RED}%s${COLOR_RESET}\n" "Did not pass build type to build script: Debug, Release"
	exit 1
fi

APK_PACKAGE_FOLDER=$(echo "$APK_PACKAGE_NAME" | sed 's/\./\//g')

sed -i "s/DDNet/${APK_BASENAME}/g" settings.gradle

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" build.gradle

TW_KEY_NAME_ESCAPED=$(echo "$TW_KEY_NAME" | sed 's/\//\\\//g')
TW_KEY_PW_ESCAPED=$(echo "$TW_KEY_PW" | sed 's/\//\\\//g')
TW_KEY_ALIAS_ESCAPED=$(echo "$TW_KEY_ALIAS" | sed 's/\//\\\//g')

sed -i "s/TW_KEY_NAME/${TW_KEY_NAME_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_PW/${TW_KEY_PW_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_ALIAS/${TW_KEY_ALIAS_ESCAPED}/g" build.gradle

sed -i "s/TW_VERSION_CODE/${TW_VERSION_CODE}/g" build.gradle
sed -i "s/TW_VERSION_NAME/${TW_VERSION_NAME}/g" build.gradle

for f in src/main/res/values*; do
	sed -i "s/DDNet/${APK_BASENAME}/g" "$f/strings.xml"
done

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/res/xml/shortcuts.xml

sed -i "s/\"DDNet\"/\"${APK_BASENAME}\"/g" src/main/AndroidManifest.xml
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/AndroidManifest.xml

if [ "${APK_PACKAGE_FOLDER}" != "org/ddnet/client" ]; then
	mv src/main/java/org/ddnet/client src/main/java/"${APK_PACKAGE_FOLDER}"
fi

sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/java/"${APK_PACKAGE_FOLDER}"/ClientActivity.java
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" src/main/java/"${APK_PACKAGE_FOLDER}"/ServerService.java
sed -i "s/org.ddnet.client/${APK_PACKAGE_NAME}/g" proguard-rules.pro

# disable hid manager for now
sed -i "s/mHIDDeviceManager = HIDDeviceManager.acquire(this);/mHIDDeviceManager=null;/g" src/main/java/org/libsdl/app/SDLActivity.java

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
	sed -i "s/android.enableR8.fullMode=true/android.enableR8.fullMode=false/g" gradle.properties
fi

function build_gradle() {
	./gradlew --warning-mode all "$1"
}

if [[ "${APK_BUILD_TYPE}" == "Debug" ]]; then
	build_gradle builddebug
	build_gradle assembleDebug
else
	build_gradle buildrelease
	build_gradle assembleRelease
fi
cp build/outputs/apk/"$RELEASE_TYPE_NAME"/"$APK_BASENAME"-"$RELEASE_TYPE_NAME".apk "$APK_BASENAME".apk

if [[ "${APK_BUILD_TYPE}" == "Release" ]]; then
	build_gradle bundleRelease
	cp build/outputs/bundle/"$RELEASE_TYPE_NAME"/"$APK_BASENAME"-"$RELEASE_TYPE_NAME".aab "$APK_BASENAME".aab
fi
