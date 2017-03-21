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

typedef char TestString[64];

int ExecTest(const TestString* pStrings, int NumStrings)
{
	CTimer TimerStartup;

	IKernel *pKernel = IKernel::Create();
	IEngineGraphics *pGraphics = 0;
	IEngineTextRender *pTextRender = 0;

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

	// init textrender
	{
		pTextRender = CreateEngineTextRender();
		pKernel->RegisterInterface(static_cast<IEngineTextRender*>(pTextRender));
		pKernel->RegisterInterface(static_cast<ITextRender*>(pTextRender));
		pTextRender->Init();

		{
			CTimer Timer;
			CFont *pDefaultFont = pTextRender->LoadFont("data/fonts/DejaVuSansCJKName.ttf");
			pTextRender->SetDefaultFont(pDefaultFont);
			Timer.PrintElapsed("font");
		}
	}

	pGraphics->BlendNormal();

	TimerStartup.PrintElapsed("startup");

	static const int s_Columns = 3;

	CFrameLoopAnalyser Analyser(20, 512);

	while(Analyser.Continue())
	{
		int64 Time = Analyser.GetFrameTime();
		double RenderTime = 0.1 * Time / (double)time_freq();

		pGraphics->Clear(0.5f, 0.5f, 0.5f);

		float Height = 300.0f;
		float Width = Height * pGraphics->ScreenAspect();
		pGraphics->MapScreen(0.0f, 0.0f, Width, Height);

		for(int j = 0; j < s_Columns; j++)
		{
			for(int i = 0; i < NumStrings; i++)
			{
				float TimeWrap = fmod(RenderTime + i / (double)(NumStrings), 1.0f);

				float PositionX = j * Width / s_Columns;
				float PositionY = TimeWrap * Height * 0.8f;
				float Size = 8 + TimeWrap * 40.0;

				CTextCursor Cursor;
				pTextRender->SetCursor(&Cursor, PositionX, PositionY, Size, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Width;
				pTextRender->TextEx(&Cursor, pStrings[i % NumStrings], -1);
			}
		}

		pGraphics->Swap();
	}

	Analyser.PrintReport();

	delete pTextRender;
	delete pGraphics;
	delete pKernel;
	
	return 0;
}

#if defined(CONF_PLATFORM_MACOSX) || defined(__ANDROID__)
extern "C" int SDL_main(int argc, char **argv_) // ignore_convention
{
	const char **argv = const_cast<const char **>(argv_);
#else
int main(int argc, const char **argv) // ignore_convention
{
#endif
	dbg_logger_stdout();
	
	// this set contains different glyphs, including CJK
	static const TestString s_aaCjk[] =
	{
		"azertyuiop",
		"ըթժիլխծկհձ",
		"ءحآخغأدؤذإ",
		"ႠႡႢႣႤႥႦႧႨႩ",
		"ΑΒΓΔΕΖΗΘΙΚ",
		"㮖㯙㿄㾩䋤䋬䙐䘂䤠䣤",
		"ЀЎМЪЁЏНЫЂА",
		"QSDFGHJKLM",
		"アコイザェナボゼハク", 
		"رئزاسبشفةص",
		"ﬡﬢﬣﬤﬥﬦﬧﬨ﬩שׁ",
		"αβγδεζηθικ",
		"ႭႮႯႰႱႲႳႴႵႶ",
		"ՋՍՌՎՏՐՑՒՓՔ",
		"ОЬЃБПЭЄВРЮ",
		"あつさうじほぬぜむん",
		"😀😖😱😻😴😳🙀🙃🙊🙏",
		"قتضكثطلجظم",
		"갥겎뙱뫋민봴빹쀔앂챠",
		"ნოპჟრსტუფქ",
		"еужфзхицйч",
		"ԱԲԳԴԵԶԷԸԹԺ",
		"כלםמןנסעףפ",
	};

	// this set contains only ascii characters
	static const TestString s_aaAscii[] =
	{
		"Jumping",
		"the gun",
		"A retro",
		"multiplayer",
		"shooter",
		"Teeworlds is",
		"a free online",
		"multiplayer game,",
		"available for",
		"all major",
		"operating",
		"systems.",
		"Battle with",
		"up to 16 players",
		"in a variety",
		"of game modes,",
		"including",
		"Team Deathmatch",
		"and Capture",
		"The Flag",
		"You can even",
		"design your",
		"own maps!",
	};

	// this set contains glyphs available in the default font
	static const TestString s_aaMonofont[] =
	{
		"azertyuiop",
		"ըթժիլխծկհձ",
		"ءحآخغأدؤذإ",
		"ႠႡႢႣႤႥႦႧႨႩ",
		"ΑΒΓΔΕΖΗΘΙΚ",
		"٠١٢٣٤٥٦٧٨٩",
		"ЀЎМЪЁЏНЫЂА",
		"QSDFGHJKLM",
		"رئزاسبشفةص",
		"ﬡﬢﬣﬤﬥﬦﬧﬨ﬩שׁ",
		"αβγδεζηθικ",
		"ႭႮႯႰႱႲႳႴႵႶ",
		"ՋՍՌՎՏՐՑՒՓՔ",
		"ОЬЃБПЭЄВРЮ",
		"0123456789",
		"wxcvbnqsdf",
		"قتضكثطلجظم",
		"ΛΜΝΞΟΠΡΣΤΥ",
		"ნოპჟრსტუფქ",
		"еужфзхицйч",
		"ԱԲԳԴԵԶԷԸԹԺ",
		"כלםמןנסעףפ",
		"AZERTYUIOP",
	};

	enum
	{
		TEST_DEFAULT = 0,
		TEST_ASCII,
		TEST_MONOFONT,
		TEST_CJK,
	};

	int Test = TEST_DEFAULT;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp(argv[i], "--help") == 0 || str_comp(argv[i], "-h") == 0)
		{
			dbg_msg("benchmark", "Usage: %s --test <testname>", argv[0]);
			dbg_msg("benchmark", "Available tests: \"ascii\", \"monofont\" or \"cjk\"", argv[0]);
			return 0;
		}
		else if(str_comp(argv[i], "--test") == 0 || str_comp(argv[i], "-t") == 0)
		{
			if(Test != TEST_DEFAULT)
				continue;
			else if(i+1 < argc)
			{
				if(str_comp(argv[i+1], "ascii") == 0)
				{
					Test = TEST_ASCII;
				}
				else if(str_comp(argv[i+1], "monofont") == 0)
				{
					Test = TEST_MONOFONT;
				}
				else if(str_comp(argv[i+1], "cjk") == 0)
				{
					Test = TEST_CJK;
				}
				else
				{
					dbg_msg("benchmark", "Error: unknown value for parameter \"test\". Possible values: \"ascii\", \"monofont\" or \"cjk\"");
					return 1;
				}
				
				i++;
			}
			else
			{
				dbg_msg("benchmark", "Error: missing value for parameter \"test\". possible values: \"ascii\", \"monofont\" or \"cjk\"");
				return 1;
			}
		}
	}

	switch(Test)
	{
		case TEST_DEFAULT:
		case TEST_MONOFONT:
			return ExecTest(s_aaMonofont, sizeof(s_aaMonofont) / sizeof(TestString));
		case TEST_CJK:
			return ExecTest(s_aaCjk, sizeof(s_aaCjk) / sizeof(TestString));
		case TEST_ASCII:
			return ExecTest(s_aaAscii, sizeof(s_aaAscii) / sizeof(TestString));
	}
	
	return 0;
}
