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

class CTimer
{
public:
	CTimer(int64 StartTime) : m_StartTime(StartTime) { }
	CTimer() : m_StartTime(time_get()) { }
	int64 GetElapsed(int64 CurrentTime)
	{
		return CurrentTime - m_StartTime;
	}
	int64 GetElapsed()
	{
		return GetElapsed(time_get());
	}
	void PrintElapsed(const char *pName, int64 CurrentTime)
	{
		double Elapsed = GetElapsed(CurrentTime) / (double)time_freq();
		dbg_msg("benchmark", "%s: %.2fms", pName, Elapsed * 1000);
	}
	void PrintElapsed(const char *pName)
	{
		PrintElapsed(pName, time_get());
	}
private:
	int64 m_StartTime;
};

#if defined(CONF_PLATFORM_MACOSX) || defined(__ANDROID__)
extern "C" int SDL_main(int argc, char **argv_) // ignore_convention
{
	const char **argv = const_cast<const char **>(argv_);
#else
int main(int argc, const char **argv) // ignore_convention
{
#endif
	CTimer TimerStartup;
	dbg_logger_stdout();

	IKernel *pKernel = IKernel::Create();

	CTimer TimerSDL;
	// init SDL
	{
		if(SDL_Init(0) < 0)
		{
			dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
			return 1;
		}

		atexit(SDL_Quit); // ignore_convention
	}
	TimerSDL.PrintElapsed("sdl");

	CTimer TimerGraphics;
	// init graphics
	IEngineGraphics *pGraphics = CreateEngineGraphicsThreaded();
	pKernel->RegisterInterface(static_cast<IEngineGraphics*>(pGraphics));
	pKernel->RegisterInterface(static_cast<IGraphics*>(pGraphics));
	if(pGraphics->Init() != 0)
	{
		dbg_msg("client", "couldn't init graphics");
		return 1;
	}
	TimerGraphics.PrintElapsed("graphics");

	// init textrender
	IEngineTextRender *pTextRender = CreateEngineTextRender();
	pKernel->RegisterInterface(static_cast<IEngineTextRender*>(pTextRender));
	pKernel->RegisterInterface(static_cast<ITextRender*>(pTextRender));
	pTextRender->Init();

	{
		CTimer TimerFont;
		CFont *pDefaultFont = pTextRender->LoadFont("data/fonts/DejaVuSansCJKName.ttf");
		pTextRender->SetDefaultFont(pDefaultFont);
		TimerFont.PrintElapsed("font");
	}

	pGraphics->BlendNormal();

	TimerStartup.PrintElapsed("startup");

	// text from https://fr.wikibooks.org/wiki/Translinguisme/Par_expression/Bonjour
	static const char s_aaText[][32] =
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
	static const int s_NbText = sizeof(s_aaText) / sizeof(s_aaText[0]);
	static const int s_Duration = 20; // in seconds
	static const int s_Columns = 3;

	float Fps = 0.0f;
	unsigned int FrameCounter = 0;

	array<int64> aFrameTimes;
	aFrameTimes.hint_size(s_Duration * 200); // 200 fps should be enough for anybody.
	{
		int64 CurrentTime = time_get();
		CTimer Timer(CurrentTime);
		CTimer FrameTimer(CurrentTime);
		while(1)
		{
			double RenderTime = 0.1 * Timer.GetElapsed(CurrentTime) / (double)time_freq();

			pGraphics->Clear(0.5f, 0.5f, 0.5f);

			float Height = 300.0f;
			float Width = Height * pGraphics->ScreenAspect();
			pGraphics->MapScreen(0.0f, 0.0f, Width, Height);

			for(int j = 0; j < s_Columns; j++)
			{
				for(int i = 0; i < s_NbText; i++)
				{
					float TimeWrap = fmod(RenderTime + i / (double)(s_NbText), 1.0f);

					float PositionX = j * Width / s_Columns;
					float PositionY = TimeWrap * Height * 0.8f;
					float Size = 8 + TimeWrap * 40.0;

					CTextCursor Cursor;
					pTextRender->SetCursor(&Cursor, PositionX, PositionY, Size, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
					Cursor.m_LineWidth = Width;
					pTextRender->TextEx(&Cursor, s_aaText[i % s_NbText], -1);
				}
			}

			pGraphics->Swap();

			CurrentTime = time_get();
			if(FrameCounter == 0)
			{
				FrameTimer.PrintElapsed("first frame", CurrentTime);
				Timer = CTimer(CurrentTime);
			}
			else
			{
				int64 FrameTime = FrameTimer.GetElapsed(CurrentTime);
				aFrameTimes.add(FrameTime);
				if(argc > 1)
				{
					FrameTimer.PrintElapsed("frame", CurrentTime);
				}
			}
			FrameTimer = CTimer(CurrentTime);
			FrameCounter++;

			int64 TimeDiff = Timer.GetElapsed(CurrentTime);
			if(TimeDiff > time_freq() * s_Duration)
			{
				double Time = TimeDiff / (double)time_freq();
				Fps = (double)(FrameCounter - 1) / Time;
				break;
			}
		}
	}

	sort(aFrameTimes.all());

	dbg_msg("benchmark", "result: %.2fms per frame (%.2f fps)", 1 / Fps * 1000, Fps);

	for(int i = 0; i < 10; i++)
	{
		if(i >= aFrameTimes.size())
		{
			break;
		}
		double Time = aFrameTimes[aFrameTimes.size() - 1 - i] / (double)time_freq() * 1000;
		char aPrefix[6] = "";
		if(i != 0)
		{
			const char *pSuffix = "th";
			if(i == 1)
			{
				pSuffix = "nd";
			}
			else if(i == 2)
			{
				pSuffix = "rd";
			}
			str_format(aPrefix, sizeof(aPrefix), "%d%s ", i + 1, pSuffix);
		}
		dbg_msg("benchmark", "%slongest frame: %.2fms", aPrefix, Time);
	}
	dbg_msg("benchmark", "median frame: %.2fms", aFrameTimes[aFrameTimes.size() / 2] / (double)time_freq() * 1000);

	delete pTextRender;
	delete pGraphics;
	delete pKernel;

	return 0;
}
