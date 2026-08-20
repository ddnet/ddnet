#include "ios_main.h"

#include <base/fs.h>
#include <base/log.h>

// Keep our own main, SDL_main.h would rename it to SDL_main otherwise.
#define SDL_MAIN_HANDLED
#include <SDL.h>

extern "C" int SDL_UIKitRunApp(int argc, char **argv, int (*mainFunction)(int, char **));
extern "C" int SDL_main(int argc, char **argv);

int main(int argc, char **argv)
{
	return SDL_UIKitRunApp(argc, argv, SDL_main);
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
