#ifndef ANDROID_ANDROID_MAIN_H
#define ANDROID_ANDROID_MAIN_H

#include <base/detect.h>
#if !defined(CONF_PLATFORM_ANDROID)
#error "This header should only be included when compiling for Android"
#endif

/**
 * @defgroup Android
 *
 * Android-specific functions to interact with the ClientActivity.
 *
 * Important note: These functions may only be called from the main native thread
 * which is created by the SDLActivity (super class of ClientActivity), otherwise
 * JNI calls are not possible because the JNI environment is not attached to that
 * thread. See https://developer.android.com/training/articles/perf-jni#threads
 */

/**
 * Initializes the Android storage. Must be called on Android-systems
 * before using any of the I/O and storage functions.
 *
 * @ingroup Android
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
 * @ingroup Android
 *
 * This will restart the main activity in a new task. The current process
 * must immediately terminate after this function is called.
 */
void RestartAndroidApp();

/**
 * Starts the local server as an Android service.
 *
 * @ingroup Android
 *
 * This will request the notification-permission as it is required for
 * foreground services to show a notification.
 *
 * @return `true` on success, `false` on error.
 */
bool StartAndroidServer();

/**
 * Adds a command to the execution queue of the local server.
 *
 * @ingroup Android
 *
 * @param pCommand The command to enqueue.
 */
void ExecuteAndroidServerCommand(const char *pCommand);

/**
 * Returns whether the local server and its Android service are running.
 *
 * @ingroup Android
 *
 * @return `true` if the server is running, `false` if the server is stopped.
 */
bool IsAndroidServerRunning();

#endif // ANDROID_ANDROID_MAIN_H
