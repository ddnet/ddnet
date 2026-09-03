#include "ios_main.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/log.h>

// Keep our own main, SDL_main.h would rename it to SDL_main otherwise.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>

#include <cmath>

extern "C" int SDL_UIKitRunApp(int argc, char **argv, int (*mainFunction)(int, char **));
extern "C" int SDL_main(int argc, char **argv);

int main(int argc, char **argv)
{
	return SDL_UIKitRunApp(argc, argv, SDL_main);
}

void IosDisplayCutoutInsets(SDL_Window *pWindow, int *pLeft, int *pRight)
{
	*pLeft = 0;
	*pRight = 0;

	SDL_SysWMinfo Info;
	SDL_VERSION(&Info.version);
	dbg_assert(SDL_GetWindowWMInfo(pWindow, &Info) == SDL_TRUE, "Failed to determine window information: %s", SDL_GetError());

	// SDL uses the root view controller's view to render to, so its scale factor
	// converts the insets from points to the pixels of the drawable area.
	UIWindow *pUiWindow = Info.info.uikit.window;
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
	char *pBasePath = SDL_GetBasePath();
	if(!pBasePath)
	{
		return "Failed to determine the app base path.";
	}
	if(fs_chdir(pBasePath) != 0)
	{
		SDL_free(pBasePath);
		return "Failed to change current directory to the app bundle.";
	}
	log_info("ios", "Changed current directory to '%s'", pBasePath);
	SDL_free(pBasePath);

	if(!fs_is_dir("data"))
	{
		return "Missing data directory in app bundle. Ensure data is packaged into the app.";
	}

	return nullptr;
}
