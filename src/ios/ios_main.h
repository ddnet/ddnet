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

struct SDL_Window;

/**
 * Determines the insets of the drawable area of a window which are covered by
 * the cutout of the display, in pixels.
 *
 * @ingroup iOS
 *
 * Only the cutout hides content. The home indicator is drawn on top of the
 * content instead, so the entire height of the display stays usable.
 *
 * @param pWindow The window to determine the insets of.
 * @param pLeft Pointer to variable that will receive the left inset.
 * @param pRight Pointer to variable that will receive the right inset.
 */
void IosDisplayCutoutInsets(SDL_Window *pWindow, int *pLeft, int *pRight);

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
