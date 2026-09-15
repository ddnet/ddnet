#import <UIKit/UIKit.h>

#include "ios_main.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/log.h>

// Keep our own main, SDL_main.h would rename it to SDL_main otherwise.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>

extern "C" int SDL_main(int argc, char **argv);

int main(int argc, char **argv)
{
	return SDL_RunApp(argc, argv, SDL_main, nullptr);
}

void IosDisplayCutoutInsets(SDL_Window *pWindow, int *pLeft, int *pRight)
{
	*pLeft = 0;
	*pRight = 0;

	UIWindow *pUiWindow = (UIWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(pWindow), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
	dbg_assert(pUiWindow != nil, "Failed to determine window information: %s", SDL_GetError());

	// SDL uses the root view controller's view to render to, so its scale factor
	// converts the insets from points to the pixels of the drawable area.
	UIView *pView = pUiWindow.rootViewController.view;
	const UIEdgeInsets Insets = pView.safeAreaInsets;
	const CGFloat Scale = pView.contentScaleFactor;

	// The same inset is reported for both sides so that content stays centered, but
	// only the side that the cutout is on actually hides content.
	if(pUiWindow.windowScene.interfaceOrientation == UIInterfaceOrientationLandscapeRight)
	{
		*pLeft = std::ceil(Insets.left * Scale);
	}
	else
	{
		*pRight = std::ceil(Insets.right * Scale);
	}
}

const char *InitIos()
{
	const char *pBasePath = SDL_GetBasePath();
	if(!pBasePath)
	{
		return "Failed to determine the app base path.";
	}
	if(fs_chdir(pBasePath) != 0)
	{
		return "Failed to change current directory to the app bundle.";
	}
	log_info("ios", "Changed current directory to '%s'", pBasePath);

	if(!fs_is_dir("data"))
	{
		return "Missing data directory in app bundle. Ensure data is packaged into the app.";
	}

	return nullptr;
}
