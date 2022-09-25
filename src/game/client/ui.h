/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_H
#define GAME_CLIENT_UI_H

#include "ui_rect.h"

#include <engine/input.h>
#include <engine/textrender.h>

#include <chrono>
#include <string>
#include <vector>

class IClient;
class IGraphics;
class IKernel;

struct SUIAnimator
{
	bool m_Active;
	bool m_ScaleLabel;
	bool m_RepositionLabel;

	std::chrono::nanoseconds m_Time;
	float m_Value;

	float m_XOffset;
	float m_YOffset;
	float m_WOffset;
	float m_HOffset;
};

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

class CUI;

class CUIElement
{
	friend class CUI;

	CUI *m_pUI;

	CUIElement(CUI *pUI, int RequestedRectCount) { Init(pUI, RequestedRectCount); }

public:
	struct SUIElementRect
	{
		CUIElement *m_pParent;

	public:
		int m_UIRectQuadContainer;
		int m_UITextContainer;

		float m_X;
		float m_Y;
		float m_Width;
		float m_Height;

		std::string m_Text;

		CTextCursor m_Cursor;

		ColorRGBA m_TextColor;
		ColorRGBA m_TextOutlineColor;

		SUIElementRect();

		ColorRGBA m_QuadColor;

		void Reset();
		void Draw(const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding);
	};

protected:
	CUI *UI() const { return m_pUI; }
	std::vector<SUIElementRect> m_vUIRects;

public:
	CUIElement() = default;

	void Init(CUI *pUI, int RequestedRectCount);

	SUIElementRect *Rect(size_t Index)
	{
		return &m_vUIRects[Index];
	}

	bool AreRectsInit()
	{
		return !m_vUIRects.empty();
	}

	void InitRects(int RequestedRectCount);
};

struct SLabelProperties
{
	float m_MaxWidth = -1;
	int m_AlignVertically = 1;
	bool m_StopAtEnd = false;
	class CTextCursor *m_pSelCursor = nullptr;
	bool m_EnableWidthCheck = true;
};

class CUIElementBase
{
private:
	static CUI *s_pUI;

public:
	static void Init(CUI *pUI) { s_pUI = pUI; }

	IClient *Client() const;
	IGraphics *Graphics() const;
	IInput *Input() const;
	ITextRender *TextRender() const;
	CUI *UI() const { return s_pUI; }
};

class CButtonContainer
{
};

class CUI
{
	bool m_Enabled;

	const void *m_pHotItem;
	const void *m_pActiveItem;
	const void *m_pLastActiveItem;
	const void *m_pBecomingHotItem;
	const void *m_pActiveTooltipItem;
	bool m_ActiveItemValid = false;

	float m_MouseX, m_MouseY; // in gui space
	float m_MouseDeltaX, m_MouseDeltaY; // in gui space
	float m_MouseWorldX, m_MouseWorldY; // in world space
	unsigned m_MouseButtons;
	unsigned m_LastMouseButtons;
	bool m_MouseSlow = false;

	IInput::CEvent *m_pInputEventsArray;
	int *m_pInputEventCount;
	unsigned m_HotkeysPressed = 0;

	bool m_MouseIsPress = false;
	bool m_HasSelection = false;

	int m_MousePressX = 0;
	int m_MousePressY = 0;
	int m_MouseCurX = 0;
	int m_MouseCurY = 0;
	int m_CurSelStart = 0;
	int m_CurSelEnd = 0;
	const void *m_pSelItem = nullptr;

	int m_CurCursor = 0;

	CUIRect m_Screen;

	std::vector<CUIRect> m_vClips;
	void UpdateClipping();

	IClient *m_pClient;
	IGraphics *m_pGraphics;
	IInput *m_pInput;
	ITextRender *m_pTextRender;

	std::vector<CUIElement *> m_vpOwnUIElements; // ui elements maintained by CUI class
	std::vector<CUIElement *> m_vpUIElements;

public:
	static const CLinearScrollbarScale ms_LinearScrollbarScale;
	static const CLogarithmicScrollbarScale ms_LogarithmicScrollbarScale;

	static float ms_FontmodHeight;

	void Init(IKernel *pKernel);
	void InitInputs(IInput::CEvent *pInputEventsArray, int *pInputEventCount);
	IClient *Client() const { return m_pClient; }
	IGraphics *Graphics() const { return m_pGraphics; }
	IInput *Input() const { return m_pInput; }
	ITextRender *TextRender() const { return m_pTextRender; }

	CUI();
	~CUI();

	enum EHotkey : unsigned
	{
		HOTKEY_ENTER = 1 << 0,
		HOTKEY_ESCAPE = 1 << 1,
		HOTKEY_UP = 1 << 2,
		HOTKEY_DOWN = 1 << 3,
		HOTKEY_DELETE = 1 << 4,
		HOTKEY_TAB = 1 << 5,
		HOTKEY_SCROLL_UP = 1 << 6,
		HOTKEY_SCROLL_DOWN = 1 << 7,
	};

	void ResetUIElement(CUIElement &UIElement);

	CUIElement *GetNewUIElement(int RequestedRectCount);

	void AddUIElement(CUIElement *pElement);
	void OnElementsReset();
	void OnWindowResize();
	void OnLanguageChange();

	void SetEnabled(bool Enabled) { m_Enabled = Enabled; }
	bool Enabled() const { return m_Enabled; }
	void Update(float MouseX, float MouseY, float MouseWorldX, float MouseWorldY);

	float MouseDeltaX() const { return m_MouseDeltaX; }
	float MouseDeltaY() const { return m_MouseDeltaY; }
	float MouseX() const { return m_MouseX; }
	float MouseY() const { return m_MouseY; }
	float MouseWorldX() const { return m_MouseWorldX; }
	float MouseWorldY() const { return m_MouseWorldY; }
	int MouseButton(int Index) const { return (m_MouseButtons >> Index) & 1; }
	int MouseButtonClicked(int Index) const { return MouseButton(Index) && !((m_LastMouseButtons >> Index) & 1); }
	int MouseButtonReleased(int Index) const { return ((m_LastMouseButtons >> Index) & 1) && !MouseButton(Index); }

	void SetHotItem(const void *pID) { m_pBecomingHotItem = pID; }
	void SetActiveItem(const void *pID)
	{
		m_ActiveItemValid = true;
		m_pActiveItem = pID;
		if(pID)
			m_pLastActiveItem = pID;
	}
	bool CheckActiveItem(const void *pID)
	{
		if(m_pActiveItem == pID)
		{
			m_ActiveItemValid = true;
			return true;
		}
		return false;
	}
	void SetActiveTooltipItem(const void *pID) { m_pActiveTooltipItem = pID; }
	void ClearLastActiveItem() { m_pLastActiveItem = nullptr; }
	const void *HotItem() const { return m_pHotItem; }
	const void *NextHotItem() const { return m_pBecomingHotItem; }
	const void *ActiveItem() const { return m_pActiveItem; }
	const void *ActiveTooltipItem() const { return m_pActiveTooltipItem; }
	const void *LastActiveItem() const { return m_pLastActiveItem; }

	void StartCheck() { m_ActiveItemValid = false; }
	void FinishCheck()
	{
		if(!m_ActiveItemValid && m_pActiveItem != nullptr)
		{
			SetActiveItem(nullptr);
			m_pHotItem = nullptr;
			m_pBecomingHotItem = nullptr;
		}
	}

	bool MouseInside(const CUIRect *pRect) const;
	bool MouseInsideClip() const { return !IsClipped() || MouseInside(ClipArea()); }
	bool MouseHovered(const CUIRect *pRect) const { return MouseInside(pRect) && MouseInsideClip(); }
	void ConvertMouseMove(float *pX, float *pY, IInput::ECursorType CursorType) const;
	void ResetMouseSlow() { m_MouseSlow = false; }

	bool ConsumeHotkey(EHotkey Hotkey);
	void ClearHotkeys() { m_HotkeysPressed = 0; }
	bool OnInput(const IInput::CEvent &Event);

	float ButtonColorMulActive() { return 0.5f; }
	float ButtonColorMulHot() { return 1.5f; }
	float ButtonColorMulDefault() { return 1.0f; }
	float ButtonColorMul(const void *pID);

	CUIRect *Screen();
	void MapScreen();
	float PixelSize();

	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();
	const CUIRect *ClipArea() const;
	inline bool IsClipped() const { return !m_vClips.empty(); }

	int DoButtonLogic(const void *pID, int Checked, const CUIRect *pRect);
	int DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY);
	void DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, float ScrollSpeed = 10.0f);

	float DoTextLabel(float x, float y, float w, float h, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {});
	void DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {});

	void DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen = -1, class CTextCursor *pReadCursor = nullptr);
	void DoLabelStreamed(CUIElement::SUIElementRect &RectEl, float x, float y, float w, float h, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1, bool StopAtEnd = false, int StrLen = -1, class CTextCursor *pReadCursor = nullptr);
	void DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, int AlignVertically = 1, bool StopAtEnd = false, int StrLen = -1, class CTextCursor *pReadCursor = nullptr);

	bool DoEditBox(const void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden = false, int Corners = IGraphics::CORNER_ALL, const SUIExEditBoxProperties &Properties = {});
	bool DoClearableEditBox(const void *pID, const void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden = false, int Corners = IGraphics::CORNER_ALL, const SUIExEditBoxProperties &Properties = {});

	enum
	{
		SCROLLBAR_OPTION_INFINITE = 1,
		SCROLLBAR_OPTION_NOCLAMPVALUE = 2,
	};
	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner = nullptr);
	void DoScrollbarOption(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale = &ms_LinearScrollbarScale, unsigned Flags = 0u);
	void DoScrollbarOptionLabeled(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, const char **ppLabels, int NumLabels, const IScrollbarScale *pScale = &ms_LinearScrollbarScale);
};

#endif
