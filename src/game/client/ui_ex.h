#ifndef GAME_CLIENT_UI_EX_H
#define GAME_CLIENT_UI_EX_H

#include "engine/kernel.h"
#include <base/system.h>
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

protected:
	CUI *UI() { return m_pUI; }
	IInput *Input() { return m_pInput; }
	ITextRender *TextRender() { return m_pTextRender; }
	IKernel *Kernel() { return m_pKernel; }
	IGraphics *Graphics() { return m_pGraphics; }
	CRenderTools *RenderTools() { return m_pRenderTools; }

public:
	CUIEx(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools);
	CUIEx() {}

	void Init(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools);

	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const char *pEmptyText = "");
};

#endif
