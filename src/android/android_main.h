#ifndef ANDROID_ANDROID_MAIN_H
#define ANDROID_ANDROID_MAIN_H

#include <base/detect.h>
#if !defined(CONF_PLATFORM_ANDROID)
#error "This header should only be included when compiling for Android"
#endif

/**
 * Initializes the Android storage. Must be called on Android-systems
 * before using any of the I/O and storage functions.
 *
 * This will change the current working directory to the app specific external
 * storage location and unpack the assets from the APK file to the `data` folder.
 * The folder `user` is created in the external storage to store the user data.
 *
 * Failure must be handled by exiting the app.
 *
 * @return `nullptr` on success, error message on failure.
 */
const char *InitAndroid();

/**
 * Sends an intent to the Android system to restart the app.
 *
 * This will restart the main activity in a new task. The current process
 * must immediately terminate after this function is called.
 */
void RestartAndroidApp();

#endif // ANDROID_ANDROID_MAIN_H
