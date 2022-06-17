#ifndef GAME_CLIENT_UI_EX_H
#define GAME_CLIENT_UI_EX_H

#include <engine/input.h>
#include <game/client/ui.h>

class CRenderTools;
class IGraphics;
class IKernel;
class ITextRender;

class IScrollbarScale
{
public:
	virtual float ToRelative(int AbsoluteValue, int Min, int Max) const = 0;
	virtual int ToAbsolute(float RelativeValue, int Min, int Max) const = 0;
};
class CLinearScrollbarScale : public IScrollbarScale
{
public:
	float ToRelative(int AbsoluteValue, int Min, int Max) const override
	{
		return (AbsoluteValue - Min) / (float)(Max - Min);
	}
	int ToAbsolute(float RelativeValue, int Min, int Max) const override
	{
		return round_to_int(RelativeValue * (Max - Min) + Min + 0.1f);
	}
};
class CLogarithmicScrollbarScale : public IScrollbarScale
{
private:
	int m_MinAdjustment;

public:
	CLogarithmicScrollbarScale(int MinAdjustment)
	{
		m_MinAdjustment = maximum(MinAdjustment, 1); // must be at least 1 to support Min == 0 with logarithm
	}
	float ToRelative(int AbsoluteValue, int Min, int Max) const override
	{
		if(Min < m_MinAdjustment)
		{
			AbsoluteValue += m_MinAdjustment;
			Min += m_MinAdjustment;
			Max += m_MinAdjustment;
		}
		return (log(AbsoluteValue) - log(Min)) / (float)(log(Max) - log(Min));
	}
	int ToAbsolute(float RelativeValue, int Min, int Max) const override
	{
		int ResultAdjustment = 0;
		if(Min < m_MinAdjustment)
		{
			Min += m_MinAdjustment;
			Max += m_MinAdjustment;
			ResultAdjustment = -m_MinAdjustment;
		}
		return round_to_int(exp(RelativeValue * (log(Max) - log(Min)) + log(Min))) + ResultAdjustment;
	}
};

struct SUIExEditBoxProperties
{
	bool m_SelectText = false;
	const char *m_pEmptyText = "";
};

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

	bool m_MouseIsPress = false;
	bool m_HasSelection = false;

	int m_MousePressX = 0;
	int m_MousePressY = 0;
	int m_MouseCurX = 0;
	int m_MouseCurY = 0;
	bool m_MouseSlow;
	int m_CurSelStart = 0;
	int m_CurSelEnd = 0;
	const void *m_pSelItem = nullptr;

	int m_CurCursor = 0;

protected:
	CUI *UI() const { return m_pUI; }
	IInput *Input() const { return m_pInput; }
	ITextRender *TextRender() const { return m_pTextRender; }
	IKernel *Kernel() const { return m_pKernel; }
	IGraphics *Graphics() const { return m_pGraphics; }
	CRenderTools *RenderTools() const { return m_pRenderTools; }

public:
	static const CLinearScrollbarScale ms_LinearScrollbarScale;
	static const CLogarithmicScrollbarScale ms_LogarithmicScrollbarScale;

	CUIEx();

	void Init(CUI *pUI, IKernel *pKernel, CRenderTools *pRenderTools, IInput::CEvent *pInputEventsArray, int *pInputEventCount);

	void ConvertMouseMove(float *pX, float *pY, IInput::ECursorType CursorType) const;
	void ResetMouseSlow() { m_MouseSlow = false; }

	enum
	{
		SCROLLBAR_OPTION_INFINITE = 1,
		SCROLLBAR_OPTION_NOCLAMPVALUE = 2,
	};
	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner = nullptr);
	void DoScrollbarOption(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale = &ms_LinearScrollbarScale, unsigned Flags = 0u);
	void DoScrollbarOptionLabeled(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, const char **ppLabels, int NumLabels, const IScrollbarScale *pScale = &ms_LinearScrollbarScale);

	bool DoEditBox(const void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const SUIExEditBoxProperties &Properties = {});
	bool DoClearableEditBox(const void *pID, const void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden = false, int Corners = CUI::CORNER_ALL, const SUIExEditBoxProperties &Properties = {});
};

#endif
