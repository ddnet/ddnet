#ifndef GAME_CLIENT_UI_EX_H
#define GAME_CLIENT_UI_EX_H

#include <base/system.h>
#include <engine/input.h>
#include <engine/kernel.h>
#include <game/client/ui.h>

class IInput;
class ITextRender;
class IKernel;
class IGraphics;

class CRenderTools;

class CUIEx
{
	CUI *m_pUI;
	IInput *m_pInput;
	ITextRender *m_pTextRender;
	IKernel *m_pKernel;
	IGraphics *m_pGraphics;
	CRenderTools *m_pRenderTools;

	IInput::CEvent *m_pInputEventsArray;
	int *m_pInputEventCount;

protected:
	CUI *UI() { return m_pUI; }
	IInput *Input() { return m_pInput; }
	ITextRender *TextRender() { return m_pTextRender; }
	IKernel *Kernel() { return m_pKernel; }
	IGraphics *Graphics() { return m_pGraphics; }
	CRenderTools *RenderTools() { return m_pRenderTools; }

public:
	CUIEx(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools, IInput::CEvent *pInputEventsArray, int *pInputEventCount);
	CUIEx() {}

	void Init(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools, IInput::CEvent *pInputEventsArray, int *pInputEventCount);

	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const char *pEmptyText = "");
};

#endif
