#include "SDL.h"
#ifdef main
#undef main
#endif

#include <engine/kernel.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <base/math.h>

#if defined(CONF_PLATFORM_MACOSX) || defined(__ANDROID__)
extern "C" int SDL_main(int argc, char **argv_) // ignore_convention
{
	const char **argv = const_cast<const char **>(argv_);
#else
int main(int argc, const char **argv) // ignore_convention
{
#endif
	dbg_logger_stdout();
	
	IKernel* pKernel = IKernel::Create();

	// init SDL
	{
		if(SDL_Init(0) < 0)
		{
			dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
			return 1;
		}

		atexit(SDL_Quit); // ignore_convention
	}

	// init graphics
	IEngineGraphics* pGraphics = CreateEngineGraphicsThreaded();
	pKernel->RegisterInterface(static_cast<IEngineGraphics*>(pGraphics));
	pKernel->RegisterInterface(static_cast<IGraphics*>(pGraphics));
	if(pGraphics->Init() != 0)
	{
		dbg_msg("client", "couldn't init graphics");
		return 1;
	}
	
	// init textrender
	IEngineTextRender* pTextRender = CreateEngineTextRender();
	pKernel->RegisterInterface(static_cast<IEngineTextRender*>(pTextRender));
	pKernel->RegisterInterface(static_cast<ITextRender*>(pTextRender));
	pTextRender->Init();
	
	static CFont *pDefaultFont = pTextRender->LoadFont("data/fonts/DejaVuSansCJKName.ttf");
	pTextRender->SetDefaultFont(pDefaultFont);
	
	pGraphics->BlendNormal();
	
	dbg_msg("benchmark", "ready!");
	
	// text from https://fr.wikibooks.org/wiki/Translinguisme/Par_expression/Bonjour
	static const char aaText[][1024] = 
	{
		"Hello DDNet",
		"Bonjour",
		"صَباح الخير",
		"早晨",
		"안녕하세요",
		"नमस्कार",
		"Günaydın",
		"Καλιμέρα",
		"おはようございます",
		"สวัสดี ครับ",
		"Здравствуй",
		"בוקר טוב",
		"Paakuinôgwzian",
	};
	static const int NbText = 13;
	
	int64 StartTime = time_get();
	int FrameCounter = 0;
	while(1)
	{		
		int64 CurrentTime = time_get();
		double RenderTime = (CurrentTime - StartTime)/(double)time_freq();
		
		pGraphics->Clear(0.5f, 0.5f, 0.5f);
		
		float Height = 300.0f;
		float Width = Height*pGraphics->ScreenAspect();
		pGraphics->MapScreen(0.0f, 0.0f, Width, Height);
		
		//~ for(int i=0; i<NbText; i++)
		//~ {
			//~ float Angle = RenderTime*pi/2.0f + i*2.0f*pi/NbText;
			//~ CTextCursor Cursor;
			//~ pTextRender->SetCursor(&Cursor, Width/4, Height/2 + (i - NbText/2)*15.0f , 40.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
			//~ Cursor.m_LineWidth = Width/2;
			//~ pTextRender->TextEx(&Cursor, aaText[i], -1);
		//~ }
		//~ for(int i=0; i<NbText; i++)
		//~ {
			//~ float Angle = RenderTime*pi/2.0f + i*2.0f*pi/NbText;
			//~ CTextCursor Cursor;
			//~ pTextRender->SetCursor(&Cursor, Width/2, Height/2 + (i - NbText/2)*15.0f , 8.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
			//~ Cursor.m_LineWidth = Width/2;
			//~ pTextRender->TextEx(&Cursor, aaText[i], -1);
		//~ }
		for(int i=0; i<2*NbText; i++)
		{
			float Angle = RenderTime*pi/2.0f + i*pi/NbText;
			CTextCursor Cursor;
			pTextRender->SetCursor(&Cursor, Width/4 + sin(Angle)*100.f, Height/2 + cos(Angle)*100.f, 8.0f + (sin(Angle)+1)*20.0, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Width;
			pTextRender->TextEx(&Cursor, aaText[i%NbText], -1);
		}
		
		pGraphics->Swap();
		
		FrameCounter++;
		CurrentTime = time_get();
		if(CurrentTime - StartTime > time_freq()*10)
		{
			double Time = (CurrentTime - StartTime)/(double)time_freq();
			dbg_msg("benchmark", "result: %lf fps", (double)FrameCounter / Time);
			break;
		}
	}
	
	delete pTextRender;
	delete pGraphics;
	delete pKernel;

	return 0;
}
