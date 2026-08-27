#include "x11_error_handler.h"

#include <base/detect.h>

#if defined(CONF_X11)

#include <base/str.h>

#include <SDL_loadso.h>
#include <SDL_video.h>
#include <X11/Xlib.h>
#include <X11/extensions/randr.h>

typedef int (*FX11ErrorHandler)(Display *, XErrorEvent *);

static FX11ErrorHandler s_pfnPreviousErrorHandler = nullptr;
static int s_RandrMajorOpcode;
static int s_RandrErrorBase;

static int HandleX11Error(Display *pDisplay, XErrorEvent *pError)
{
	// SDL queries RandR outputs that disconnecting a monitor or putting it to sleep has
	// already destroyed, which the server answers with BadRROutput. Xlib ends the process
	// over an unhandled error, while SDL treats a query that returns nothing like a
	// disconnected output.
	if(pError->request_code == s_RandrMajorOpcode && pError->error_code == s_RandrErrorBase + BadRROutput)
		return 0;
	return s_pfnPreviousErrorHandler(pDisplay, pError);
}

void X11IgnoreStaleOutputErrors()
{
	if(s_pfnPreviousErrorHandler != nullptr)
		return;

	const char *pVideoDriver = SDL_GetCurrentVideoDriver();
	if(pVideoDriver == nullptr || str_comp(pVideoDriver, "x11") != 0)
		return;

	// Xlib is resolved through SDL instead of being linked, so that the client still runs
	// on systems that ship no X libraries at all. The library stays loaded for as long as
	// the error handler is installed, which is until the process ends.
	auto *pXlib = SDL_LoadObject("libX11.so.6");
	if(pXlib == nullptr)
		return;
	const auto pfnOpenDisplay = (Display * (*)(const char *)) SDL_LoadFunction(pXlib, "XOpenDisplay");
	const auto pfnCloseDisplay = (int (*)(Display *))SDL_LoadFunction(pXlib, "XCloseDisplay");
	const auto pfnQueryExtension = (Bool (*)(Display *, const char *, int *, int *, int *))SDL_LoadFunction(pXlib, "XQueryExtension");
	const auto pfnSetErrorHandler = (FX11ErrorHandler (*)(FX11ErrorHandler))SDL_LoadFunction(pXlib, "XSetErrorHandler");
	if(pfnOpenDisplay == nullptr || pfnCloseDisplay == nullptr || pfnQueryExtension == nullptr || pfnSetErrorHandler == nullptr)
	{
		SDL_UnloadObject(pXlib);
		return;
	}

	// The server assigns the opcode and the error base of an extension once, so they are
	// the same on every connection to it.
	Display *pDisplay = pfnOpenDisplay(nullptr);
	if(pDisplay == nullptr)
	{
		SDL_UnloadObject(pXlib);
		return;
	}
	int EventBase;
	const Bool HasRandr = pfnQueryExtension(pDisplay, "RANDR", &s_RandrMajorOpcode, &EventBase, &s_RandrErrorBase);
	pfnCloseDisplay(pDisplay);
	if(!HasRandr)
	{
		SDL_UnloadObject(pXlib);
		return;
	}

	s_pfnPreviousErrorHandler = pfnSetErrorHandler(HandleX11Error);
}

#else

void X11IgnoreStaleOutputErrors()
{
}

#endif
