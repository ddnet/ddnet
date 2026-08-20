#ifndef IOS_IOS_MAIN_H
#define IOS_IOS_MAIN_H

#include <base/detect.h>
#if !defined(CONF_PLATFORM_IOS)
#error "This header should only be included when compiling for iOS"
#endif

/**
 * @defgroup iOS iOS
 *
 * iOS-specific functions.
 */

/**
 * Initializes iOS specific runtime settings.
 *
 * @ingroup iOS
 *
 * This changes the current working directory to the app bundle so the data
 * files can be read directly from the packaged app.
 *
 * Failure must be handled by exiting the app.
 *
 * @return `nullptr` on success, error message on failure.
 */
const char *InitIos();

#endif // IOS_IOS_MAIN_H
