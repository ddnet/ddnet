#!/bin/bash

[ "$1" == "" ] && {
    printf '\e[31mDid not pass ANDROID_SDK_ROOT to build script\e[30m\n'
	exit 1
}

[ "$2" == "" ] && {
    printf '\e[31mDid not pass APK name to build script\e[30m\n'
	exit 1
}

[ "$3" == "" ] && {
    printf '\e[31mDid not pass build type to build script\e[30m\n'
	exit 1
}

_APK_BASENAME="$2"

sed -i "s/DDNet/${2}/g" settings.gradle

_REPLACE_PACKAGE_NAME_STR="tw.${2,,}"

sed -i "s/tw.DDNet/${_REPLACE_PACKAGE_NAME_STR}/g" build.gradle

TW_KEY_NAME_ESCAPED=$(echo "$TW_KEY_NAME"|sed 's/\//\\\//g')
TW_KEY_PW_ESCAPED=$(echo "$TW_KEY_PW"|sed 's/\//\\\//g')
TW_KEY_ALIAS_ESCAPED=$(echo "$TW_KEY_ALIAS"|sed 's/\//\\\//g')

sed -i "s/TW_KEY_NAME/${TW_KEY_NAME_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_PW/${TW_KEY_PW_ESCAPED}/g" build.gradle
sed -i "s/TW_KEY_ALIAS/${TW_KEY_ALIAS_ESCAPED}/g" build.gradle

sed -i "s/DDNet/${2}/g" src/main/res/values/strings.xml

sed -i "s/\"DDNet\"/\"${2}\"/g" src/main/AndroidManifest.xml
sed -i "s/tw.DDNet/${_REPLACE_PACKAGE_NAME_STR}/g" src/main/AndroidManifest.xml

__TW_HOME_DIR=$(echo "$HOME"|sed 's/\//\\\//g')

sed -i "s/TW_HOME_DIR/${__TW_HOME_DIR}/g" local.properties
sed -i "s/TW_NDK_VERSION/${ANDROID_NDK_VERSION}/g" build.gradle
sed -i "s/TW_VERSION_CODE/${TW_VERSION_CODE}/g" build.gradle
sed -i "s/TW_VERSION_NAME/${TW_VERSION_NAME}/g" build.gradle

mv src/main/java/tw/DDNet src/main/java/tw/"${2}"

sed -i "s/tw.DDNet/${_REPLACE_PACKAGE_NAME_STR}/g" src/main/java/tw/"${2}"/NativeMain.java
sed -i "s/tw.DDNet/${_REPLACE_PACKAGE_NAME_STR}/g" proguard-rules.pro

# disable hid manager for now
sed -i "s/mHIDDeviceManager = HIDDeviceManager.acquire(this);/mHIDDeviceManager=null;/g" src/main/java/org/libsdl/app/SDLActivity.java

if [[ "${3}" == "Debug" ]]; then
	sed -i "s/android.enableR8.fullMode=true/android.enableR8.fullMode=false/g" gradle.properties
fi

if [[ -z ${GE_NO_APK_BUILD} || "${GE_NO_APK_BUILD}" != "1" ]]; then
	_RELEASE_TYPE_NAME=debug
	_RELEASE_TYPE_APK_NAME=
	if [[ "${3}" == "Debug" ]]; then
		_RELEASE_TYPE_NAME=debug
	fi

	if [[ "${3}" == "Release" ]]; then
		_RELEASE_TYPE_NAME=release
		_RELEASE_TYPE_APK_NAME=
	fi
	
	APP_BASE_NAME=Gradle
	CLASSPATH=gradle-wrapper.jar
	java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all
	if [[ "${3}" == "Debug" ]]; then
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all builddebug
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all assembleDebug
	else
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all buildrelease
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all assembleRelease
	fi
	cp build/outputs/apk/"$_RELEASE_TYPE_NAME"/"$_APK_BASENAME"-"$_RELEASE_TYPE_NAME""$_RELEASE_TYPE_APK_NAME".apk "$_APK_BASENAME".apk
	
	
	if [[ "${3}" == "Release" ]]; then
		java "-Dorg.gradle.appname=${APP_BASE_NAME}" -classpath "${CLASSPATH}" org.gradle.wrapper.GradleWrapperMain --warning-mode all bundleRelease
		
		cp build/outputs/bundle/"$_RELEASE_TYPE_NAME"/"$_APK_BASENAME"-"$_RELEASE_TYPE_NAME""$_RELEASE_TYPE_APK_NAME".aab "$_APK_BASENAME".aab
	fi
fi
