/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_H
#define GAME_CLIENT_UI_H

#include "lineinput.h"
#include "ui_rect.h"

#include <engine/input.h>
#include <engine/textrender.h>

#include <chrono>
#include <set>
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
		return (std::log(AbsoluteValue) - std::log(Min)) / (float)(std::log(Max) - std::log(Min));
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
		return round_to_int(std::exp(RelativeValue * (std::log(Max) - std::log(Min)) + std::log(Min))) + ResultAdjustment;
	}
};

class IButtonColorFunction
{
public:
	virtual ColorRGBA GetColor(bool Active, bool Hovered) const = 0;
};
class CDarkButtonColorFunction : public IButtonColorFunction
{
public:
	ColorRGBA GetColor(bool Active, bool Hovered) const override
	{
		if(Active)
			return ColorRGBA(0.15f, 0.15f, 0.15f, 0.25f);
		else if(Hovered)
			return ColorRGBA(0.5f, 0.5f, 0.5f, 0.25f);
		return ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
	}
};
class CLightButtonColorFunction : public IButtonColorFunction
{
public:
	ColorRGBA GetColor(bool Active, bool Hovered) const override
	{
		if(Active)
			return ColorRGBA(1.0f, 1.0f, 1.0f, 0.4f);
		else if(Hovered)
			return ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f);
		return ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
	}
};
class CScrollBarColorFunction : public IButtonColorFunction
{
public:
	ColorRGBA GetColor(bool Active, bool Hovered) const override
	{
		if(Active)
			return ColorRGBA(0.9f, 0.9f, 0.9f, 1.0f);
		else if(Hovered)
			return ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		return ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
	}
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
		STextContainerIndex m_UITextContainer;

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
	bool m_StopAtEnd = false;
	bool m_EnableWidthCheck = true;
};

struct SMenuButtonProperties
{
	int m_Checked = 0;
	bool m_HintRequiresStringCheck = false;
	bool m_HintCanChangePositionOrSize = false;
	bool m_UseIconFont = false;
	int m_Corners = IGraphics::CORNER_ALL;
	float m_Rounding = 5.0f;
	float m_FontFactor = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
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

/**
 * Type safe UI ID for popup menus.
 */
struct SPopupMenuId
{
};

class CUI
{
public:
	/**
	 * These enum values are returned by popup menu functions to specify the behavior.
	 */
	enum EPopupMenuFunctionResult
	{
		/**
		 * The current popup menu will be kept open.
		 */
		POPUP_KEEP_OPEN = 0,

		/**
		 * The current popup menu will be closed.
		 */
		POPUP_CLOSE_CURRENT = 1,

		/**
		 * The current popup menu and all popup menus above it will be closed.
		 */
		POPUP_CLOSE_CURRENT_AND_DESCENDANTS = 2,
	};

	/**
	 * Callback that draws a popup menu.
	 *
	 * @param pContext The context object of the popup menu.
	 * @param View The UI rect where the popup menu's contents should be drawn.
	 * @param Active Whether this popup is active (the top-most popup).
	 * Only the active popup should handle key and mouse events.
	 *
	 * @return Value from the @link EPopupMenuFunctionResult @endlink enum.
	 */
	typedef EPopupMenuFunctionResult (*FPopupMenuFunction)(void *pContext, CUIRect View, bool Active);

	/**
	 * Callback that is called when one or more popups are closed.
	 */
	typedef std::function<void()> FPopupMenuClosedCallback;

private:
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

	unsigned m_HotkeysPressed = 0;

	CUIRect m_Screen;

	std::vector<CUIRect> m_vClips;
	void UpdateClipping();

	struct SPopupMenu
	{
		static constexpr float POPUP_BORDER = 1.0f;
		static constexpr float POPUP_MARGIN = 4.0f;

		const SPopupMenuId *m_pID;
		CUIRect m_Rect;
		int m_Corners;
		void *m_pContext;
		FPopupMenuFunction m_pfnFunc;
	};
	std::vector<SPopupMenu> m_vPopupMenus;
	FPopupMenuClosedCallback m_pfnPopupMenuClosedCallback = nullptr;

	static CUI::EPopupMenuFunctionResult PopupMessage(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupConfirm(void *pContext, CUIRect View, bool Active);
	static CUI::EPopupMenuFunctionResult PopupSelection(void *pContext, CUIRect View, bool Active);

	IClient *m_pClient;
	IGraphics *m_pGraphics;
	IInput *m_pInput;
	ITextRender *m_pTextRender;

	std::vector<CUIElement *> m_vpOwnUIElements; // ui elements maintained by CUI class
	std::vector<CUIElement *> m_vpUIElements;

public:
	static const CLinearScrollbarScale ms_LinearScrollbarScale;
	static const CLogarithmicScrollbarScale ms_LogarithmicScrollbarScale;
	static const CDarkButtonColorFunction ms_DarkButtonColorFunction;
	static const CLightButtonColorFunction ms_LightButtonColorFunction;
	static const CScrollBarColorFunction ms_ScrollBarColorFunction;

	static const float ms_FontmodHeight;

	void Init(IKernel *pKernel);
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
		HOTKEY_PAGE_UP = 1 << 8,
		HOTKEY_PAGE_DOWN = 1 << 9,
		HOTKEY_HOME = 1 << 10,
		HOTKEY_END = 1 << 11,
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
	vec2 MousePos() const { return vec2(m_MouseX, m_MouseY); }
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

	constexpr float ButtonColorMulActive() const { return 0.5f; }
	constexpr float ButtonColorMulHot() const { return 1.5f; }
	constexpr float ButtonColorMulDefault() const { return 1.0f; }
	float ButtonColorMul(const void *pID);

	const CUIRect *Screen();
	void MapScreen();
	float PixelSize();

	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();
	const CUIRect *ClipArea() const;
	inline bool IsClipped() const { return !m_vClips.empty(); }

	int DoButtonLogic(const void *pID, int Checked, const CUIRect *pRect);
	int DoDraggableButtonLogic(const void *pID, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted);
	int DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY);
	void DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, bool SmoothClamp = false, float ScrollSpeed = 10.0f);
	static vec2 CalcAlignedCursorPos(const CUIRect *pRect, vec2 TextSize, int Align, const float *pBiggestCharHeight = nullptr);

	void DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {});

	void DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {}, int StrLen = -1, const CTextCursor *pReadCursor = nullptr);
	void DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth = -1, bool StopAtEnd = false, int StrLen = -1, const CTextCursor *pReadCursor = nullptr);

	bool DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL);
	bool DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL);

	int DoButton_Menu(CUIElement &UIElement, const CButtonContainer *pID, const std::function<const char *()> &GetTextLambda, const CUIRect *pRect, const SMenuButtonProperties &Props = {});
	// only used for popup menus
	int DoButton_PopupMenu(CButtonContainer *pButtonContainer, const char *pText, const CUIRect *pRect, int Align);

	enum
	{
		SCROLLBAR_OPTION_INFINITE = 1,
		SCROLLBAR_OPTION_NOCLAMPVALUE = 2,
	};
	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner = nullptr);
	void DoScrollbarOption(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale = &ms_LinearScrollbarScale, unsigned Flags = 0u);
	void DoScrollbarOptionLabeled(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, const char **ppLabels, int NumLabels, const IScrollbarScale *pScale = &ms_LinearScrollbarScale);

	// popup menu
	void DoPopupMenu(const SPopupMenuId *pID, int X, int Y, int Width, int Height, void *pContext, FPopupMenuFunction pfnFunc, int Corners = IGraphics::CORNER_ALL);
	void RenderPopupMenus();
	void ClosePopupMenu(const SPopupMenuId *pID, bool IncludeDescendants = false);
	void ClosePopupMenus();
	bool IsPopupOpen() const;
	bool IsPopupOpen(const SPopupMenuId *pID) const;
	bool IsPopupHovered() const;
	void SetPopupMenuClosedCallback(FPopupMenuClosedCallback pfnCallback);

	struct SMessagePopupContext : public SPopupMenuId
	{
		static constexpr float POPUP_MAX_WIDTH = 200.0f;
		static constexpr float POPUP_FONT_SIZE = 10.0f;

		CUI *m_pUI; // set by CUI when popup is shown
		char m_aMessage[1024];
		ColorRGBA m_TextColor;

		void DefaultColor(class ITextRender *pTextRender);
		void ErrorColor();
	};
	void ShowPopupMessage(float X, float Y, SMessagePopupContext *pContext);

	struct SConfirmPopupContext : public SPopupMenuId
	{
		enum EConfirmationResult
		{
			UNSET = 0,
			CONFIRMED,
			CANCELED,
		};
		static constexpr float POPUP_MAX_WIDTH = 200.0f;
		static constexpr float POPUP_FONT_SIZE = 10.0f;
		static constexpr float POPUP_BUTTON_HEIGHT = 12.0f;
		static constexpr float POPUP_BUTTON_SPACING = 5.0f;

		CUI *m_pUI; // set by CUI when popup is shown
		char m_aPositiveButtonLabel[128];
		char m_aNegativeButtonLabel[128];
		char m_aMessage[1024];
		EConfirmationResult m_Result;

		SConfirmPopupContext();
		void Reset();
		void YesNoButtons();
	};
	void ShowPopupConfirm(float X, float Y, SConfirmPopupContext *pContext);

	struct SSelectionPopupContext : public SPopupMenuId
	{
		static constexpr float POPUP_MAX_WIDTH = 300.0f;
		static constexpr float POPUP_FONT_SIZE = 10.0f;
		static constexpr float POPUP_ENTRY_HEIGHT = 12.0f;
		static constexpr float POPUP_ENTRY_SPACING = 5.0f;

		CUI *m_pUI; // set by CUI when popup is shown
		char m_aMessage[256];
		std::set<std::string> m_Entries;
		std::vector<CButtonContainer> m_vButtonContainers;
		const std::string *m_pSelection;

		SSelectionPopupContext();
		void Reset();
	};
	void ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext);
};

#endif
