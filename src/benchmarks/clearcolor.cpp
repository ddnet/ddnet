#include "SDL.h"
#ifdef main
#undef main
#endif

#include <base/tl/array.h>
#include <base/tl/algorithm.h>
#include <base/math.h>
#include <engine/graphics.h>
#include <engine/kernel.h>
#include <engine/textrender.h>

#include "tools.h"

#if defined(CONF_PLATFORM_MACOSX) || defined(__ANDROID__)
extern "C" int SDL_main(int argc, char **argv_) // ignore_convention
{
	const char **argv = const_cast<const char **>(argv_);
#else
int main(int argc, const char **argv) // ignore_convention
{
#endif
	dbg_logger_stdout();

	CTimer TimerStartup;

	IKernel *pKernel = IKernel::Create();
	IEngineGraphics *pGraphics = 0;
	
	// init SDL
	{
		CTimer Timer;
		if(SDL_Init(0) < 0)
		{
			dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
			return 1;
		}

		atexit(SDL_Quit); // ignore_convention
		Timer.PrintElapsed("sdl");
	}

	// init graphics
	{
		CTimer Timer;
		pGraphics = CreateEngineGraphicsThreaded();
		pKernel->RegisterInterface(static_cast<IEngineGraphics*>(pGraphics));
		pKernel->RegisterInterface(static_cast<IGraphics*>(pGraphics));
		if(pGraphics->Init() != 0)
		{
			dbg_msg("client", "couldn't init graphics");
			return 1;
		}
		Timer.PrintElapsed("graphics");
	}

	TimerStartup.PrintElapsed("startup");

	CFrameLoopAnalyser Analyser(20, 512); // 512 frames per seconds can be view as a maximum

	while(Analyser.Continue())
	{
		pGraphics->Clear(0.5f, 0.5f, 0.5f);
		pGraphics->Swap();
	}

	Analyser.PrintReport();

	delete pGraphics;
	delete pKernel;

	return 0;
}
