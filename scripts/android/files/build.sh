#!/bin/bash

[ "$1" == "" ] && {
	printf '\e[31mDid not pass APK name to build script\e[30m\n'
	exit 1
}

[ "$2" == "" ] && {
	printf '\e[31mDid not pass package name to build script\e[30m\n'
	exit 1
}

[ "$3" == "" ] && {
	printf '\e[31mDid not pass build type to build script\e[30m\n'
	exit 1
}

_APK_BASENAME="$1"
_APK_PACKAGE_NAME="$2"
_APK_BUILD_TYPE="$3"

_APK_PACKAGE_FOLDER=$(echo "$_APK_PACKAGE_NAME" | sed 's/\./\//g')

sed -i "s/DDNet/${_APK_BASENAME}/g" settings.gradle

sed -i "s/org.ddnet.client/${_APK_PACKAGE_NAME}/g" build.gradle

TW_KEY_NAME_ESCAPED=$(echo "$TW_KEY_NAME" | sed 's/\//\\\//g')
TW_KEY_PW_ESCAPED=$(echo "$TW_KEY_PW" | sed 's/\//\\\//g')
TW_KEY_ALIAS_ESCAPED=$(echo "$TW_KEY_ALIAS" | sed 's/\//\\\//g')

sed -i "s/TW_KEY_NAME/${TW_KEY_NAME_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_PW/${TW_KEY_PW_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_ALIAS/${TW_KEY_ALIAS_ESCAPED}/g" build.gradle

sed -i "s/TW_NDK_VERSION/${ANDROID_NDK_VERSION}/g" build.gradle
sed -i "s/TW_VERSION_CODE/${TW_VERSION_CODE}/g" build.gradle
sed -i "s/TW_VERSION_NAME/${TW_VERSION_NAME}/g" build.gradle

sed -i "s/DDNet/${_APK_BASENAME}/g" src/main/res/values/strings.xml

sed -i "s/\"DDNet\"/\"${_APK_BASENAME}\"/g" src/main/AndroidManifest.xml
sed -i "s/org.ddnet.client/${_APK_PACKAGE_NAME}/g" src/main/AndroidManifest.xml

mv src/main/java/org/ddnet/client src/main/java/"${_APK_PACKAGE_FOLDER}"

sed -i "s/org.ddnet.client/${_APK_PACKAGE_NAME}/g" src/main/java/"${_APK_PACKAGE_FOLDER}"/NativeMain.java
sed -i "s/org.ddnet.client/${_APK_PACKAGE_NAME}/g" proguard-rules.pro

# disable hid manager for now
sed -i "s/mHIDDeviceManager = HIDDeviceManager.acquire(this);/mHIDDeviceManager=null;/g" src/main/java/org/libsdl/app/SDLActivity.java

if [[ "${_APK_BUILD_TYPE}" == "Debug" ]]; then
	sed -i "s/android.enableR8.fullMode=true/android.enableR8.fullMode=false/g" gradle.properties
fi

if [[ -z ${GE_NO_APK_BUILD} || "${GE_NO_APK_BUILD}" != "1" ]]; then
	_RELEASE_TYPE_NAME=debug
	_RELEASE_TYPE_APK_NAME=
	if [[ "${_APK_BUILD_TYPE}" == "Debug" ]]; then
		_RELEASE_TYPE_NAME=debug
	fi

	if [[ "${_APK_BUILD_TYPE}" == "Release" ]]; then
		_RELEASE_TYPE_NAME=release
		_RELEASE_TYPE_APK_NAME=
	fi

	APP_BASE_NAME=Gradle
	CLASSPATH=gradle-wrapper.jar
	java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all
	if [[ "${_APK_BUILD_TYPE}" == "Debug" ]]; then
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all builddebug
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all assembleDebug
	else
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all buildrelease
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all assembleRelease
	fi
	cp build/outputs/apk/"$_RELEASE_TYPE_NAME"/"$_APK_BASENAME"-"$_RELEASE_TYPE_NAME""$_RELEASE_TYPE_APK_NAME".apk "$_APK_BASENAME".apk

	if [[ "${_APK_BUILD_TYPE}" == "Release" ]]; then
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all bundleRelease

		cp build/outputs/bundle/"$_RELEASE_TYPE_NAME"/"$_APK_BASENAME"-"$_RELEASE_TYPE_NAME""$_RELEASE_TYPE_APK_NAME".aab "$_APK_BASENAME".aab
	fi
fi
