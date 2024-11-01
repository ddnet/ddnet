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

class CScrollRegion;
class IClient;
class IGraphics;
class IKernel;

enum class EEditState
{
	NONE,
	START,
	EDITING,
	END,
	ONE_GO
};

template<typename T>
struct SEditResult
{
	EEditState m_State;
	T m_Value;
};

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

class CUi;

class CUIElement
{
	friend class CUi;

	CUi *m_pUI;

	CUIElement(CUi *pUI, int RequestedRectCount) { Init(pUI, RequestedRectCount); }

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
		float m_Rounding;
		int m_Corners;

		std::string m_Text;
		int m_ReadCursorGlyphCount;

		CTextCursor m_Cursor;

		ColorRGBA m_TextColor;
		ColorRGBA m_TextOutlineColor;

		SUIElementRect();

		ColorRGBA m_QuadColor;

		void Reset();
		void Draw(const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding);
	};

protected:
	CUi *Ui() const { return m_pUI; }
	std::vector<SUIElementRect> m_vUIRects;

public:
	CUIElement() = default;

	void Init(CUi *pUI, int RequestedRectCount);

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
	bool m_EllipsisAtEnd = false;
	bool m_EnableWidthCheck = true;
	std::vector<STextColorSplit> m_vColorSplits = {};
};

struct SMenuButtonProperties
{
	int m_Checked = 0;
	bool m_HintRequiresStringCheck = false;
	bool m_HintCanChangePositionOrSize = false;
	bool m_UseIconFont = false;
	bool m_ShowDropDownIcon = false;
	int m_Corners = IGraphics::CORNER_ALL;
	float m_Rounding = 5.0f;
	float m_FontFactor = 0.0f;
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
};

class CUIElementBase
{
private:
	static CUi *s_pUI;

public:
	static void Init(CUi *pUI) { s_pUI = pUI; }

	IClient *Client() const;
	IGraphics *Graphics() const;
	IInput *Input() const;
	ITextRender *TextRender() const;
	CUi *Ui() const { return s_pUI; }
};

class CButtonContainer
{
};

struct SValueSelectorProperties
{
	bool m_UseScroll = true;
	int64_t m_Step = 1;
	float m_Scale = 1.0f;
	bool m_IsHex = false;
	int m_HexPrefix = 6;
	ColorRGBA m_Color = ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f);
};

struct SProgressSpinnerProperties
{
	float m_Progress = -1.0f; // between 0.0f and 1.0f, or negative for indeterminate progress
	ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	int m_Segments = 64;
};

/**
 * Type safe UI ID for popup menus.
 */
struct SPopupMenuId
{
};

struct SPopupMenuProperties
{
	int m_Corners = IGraphics::CORNER_ALL;
	ColorRGBA m_BorderColor = ColorRGBA(0.5f, 0.5f, 0.5f, 0.75f);
	ColorRGBA m_BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.75f);
};

class CUi
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

	/**
	 * Represents the aggregated state of current touch events to control a user interface.
	 */
	class CTouchState
	{
		friend class CUi;

		bool m_SecondaryPressedNext = false;
		float m_SecondaryActivationTime = 0.0f;
		vec2 m_SecondaryActivationDelta = vec2(0.0f, 0.0f);

	public:
		bool m_AnyPressed = false;
		bool m_PrimaryPressed = false;
		bool m_SecondaryPressed = false;
		vec2 m_PrimaryPosition = vec2(-1.0f, -1.0f);
		vec2 m_PrimaryDelta = vec2(0.0f, 0.0f);
		vec2 m_ScrollAmount = vec2(0.0f, 0.0f);
	};

private:
	bool m_Enabled;

	const void *m_pHotItem = nullptr;
	const void *m_pActiveItem = nullptr;
	const void *m_pLastActiveItem = nullptr; // only used internally to track active CLineInput
	const void *m_pBecomingHotItem = nullptr;
	CScrollRegion *m_pHotScrollRegion = nullptr;
	CScrollRegion *m_pBecomingHotScrollRegion = nullptr;
	bool m_ActiveItemValid = false;

	int m_ActiveButtonLogicButton = -1;
	int m_ActiveDraggableButtonLogicButton = -1;
	class CDoubleClickState
	{
	public:
		const void *m_pLastClickedId = nullptr;
		float m_LastClickTime = -1.0f;
		vec2 m_LastClickPos = vec2(-1.0f, -1.0f);
	};
	CDoubleClickState m_DoubleClickState;
	const void *m_pLastEditingItem = nullptr;
	float m_ActiveScrollbarOffset = 0.0f;
	float m_ProgressSpinnerOffset = 0.0f;
	class CValueSelectorState
	{
	public:
		int m_Button = -1;
		bool m_DidScroll = false;
		float m_ScrollValue = 0.0f;
		CLineInputNumber m_NumberInput;
		const void *m_pLastTextId = nullptr;
	};
	CValueSelectorState m_ActiveValueSelectorState;

	vec2 m_UpdatedMousePos = vec2(0.0f, 0.0f); // in window screen space
	vec2 m_UpdatedMouseDelta = vec2(0.0f, 0.0f); // in window screen space
	vec2 m_MousePos = vec2(0.0f, 0.0f); // in gui space
	vec2 m_MouseDelta = vec2(0.0f, 0.0f); // in gui space
	vec2 m_MouseWorldPos = vec2(-1.0f, -1.0f); // in world space
	unsigned m_UpdatedMouseButtons = 0;
	unsigned m_MouseButtons = 0;
	unsigned m_LastMouseButtons = 0;
	CTouchState m_TouchState;
	bool m_MouseSlow = false;
	bool m_MouseLock = false;
	const void *m_pMouseLockId = nullptr;

	unsigned m_HotkeysPressed = 0;

	CUIRect m_Screen;

	std::vector<CUIRect> m_vClips;
	void UpdateClipping();

	struct SPopupMenu
	{
		static constexpr float POPUP_BORDER = 1.0f;
		static constexpr float POPUP_MARGIN = 4.0f;

		const SPopupMenuId *m_pId;
		SPopupMenuProperties m_Props;
		CUIRect m_Rect;
		void *m_pContext;
		FPopupMenuFunction m_pfnFunc;
	};
	std::vector<SPopupMenu> m_vPopupMenus;
	FPopupMenuClosedCallback m_pfnPopupMenuClosedCallback = nullptr;

	static CUi::EPopupMenuFunctionResult PopupMessage(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupConfirm(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupSelection(void *pContext, CUIRect View, bool Active);
	static CUi::EPopupMenuFunctionResult PopupColorPicker(void *pContext, CUIRect View, bool Active);

	IClient *m_pClient;
	IGraphics *m_pGraphics;
	IInput *m_pInput;
	ITextRender *m_pTextRender;

	std::vector<CUIElement *> m_vpOwnUIElements; // ui elements maintained by CUi class
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

	CUi();
	~CUi();

	enum EHotkey : unsigned
	{
		HOTKEY_ENTER = 1 << 0,
		HOTKEY_ESCAPE = 1 << 1,
		HOTKEY_UP = 1 << 2,
		HOTKEY_DOWN = 1 << 3,
		HOTKEY_LEFT = 1 << 4,
		HOTKEY_RIGHT = 1 << 5,
		HOTKEY_DELETE = 1 << 6,
		HOTKEY_TAB = 1 << 7,
		HOTKEY_SCROLL_UP = 1 << 8,
		HOTKEY_SCROLL_DOWN = 1 << 9,
		HOTKEY_PAGE_UP = 1 << 10,
		HOTKEY_PAGE_DOWN = 1 << 11,
		HOTKEY_HOME = 1 << 12,
		HOTKEY_END = 1 << 13,
	};

	void ResetUIElement(CUIElement &UIElement) const;

	CUIElement *GetNewUIElement(int RequestedRectCount);

	void AddUIElement(CUIElement *pElement);
	void OnElementsReset();
	void OnWindowResize();
	void OnCursorMove(float X, float Y);

	void SetEnabled(bool Enabled) { m_Enabled = Enabled; }
	bool Enabled() const { return m_Enabled; }
	void Update(vec2 MouseWorldPos = vec2(-1.0f, -1.0f));
	void DebugRender(float X, float Y);

	vec2 MousePos() const { return m_MousePos; }
	float MouseX() const { return m_MousePos.x; }
	float MouseY() const { return m_MousePos.y; }
	vec2 MouseDelta() const { return m_MouseDelta; }
	float MouseDeltaX() const { return m_MouseDelta.x; }
	float MouseDeltaY() const { return m_MouseDelta.y; }
	vec2 MouseWorldPos() const { return m_MouseWorldPos; }
	float MouseWorldX() const { return m_MouseWorldPos.x; }
	float MouseWorldY() const { return m_MouseWorldPos.y; }
	vec2 UpdatedMousePos() const { return m_UpdatedMousePos; }
	vec2 UpdatedMouseDelta() const { return m_UpdatedMouseDelta; }
	int MouseButton(int Index) const { return (m_MouseButtons >> Index) & 1; }
	int MouseButtonClicked(int Index) const { return MouseButton(Index) && !((m_LastMouseButtons >> Index) & 1); }
	bool CheckMouseLock()
	{
		if(m_MouseLock && ActiveItem() != m_pMouseLockId)
			DisableMouseLock();
		return m_MouseLock;
	}
	void EnableMouseLock(const void *pId)
	{
		m_MouseLock = true;
		m_pMouseLockId = pId;
	}
	void DisableMouseLock() { m_MouseLock = false; }

	void SetHotItem(const void *pId) { m_pBecomingHotItem = pId; }
	void SetActiveItem(const void *pId)
	{
		m_ActiveItemValid = true;
		m_pActiveItem = pId;
		if(pId)
			m_pLastActiveItem = pId;
	}
	bool CheckActiveItem(const void *pId)
	{
		if(m_pActiveItem == pId)
		{
			m_ActiveItemValid = true;
			return true;
		}
		return false;
	}
	void SetHotScrollRegion(CScrollRegion *pId) { m_pBecomingHotScrollRegion = pId; }
	const void *HotItem() const { return m_pHotItem; }
	const void *NextHotItem() const { return m_pBecomingHotItem; }
	const void *ActiveItem() const { return m_pActiveItem; }
	const CScrollRegion *HotScrollRegion() const { return m_pHotScrollRegion; }

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
	void UpdateTouchState(CTouchState &State) const;
	void ResetMouseSlow() { m_MouseSlow = false; }

	bool ConsumeHotkey(EHotkey Hotkey);
	void ClearHotkeys() { m_HotkeysPressed = 0; }
	bool OnInput(const IInput::CEvent &Event);

	constexpr float ButtonColorMulActive() const { return 0.5f; }
	constexpr float ButtonColorMulHot() const { return 1.5f; }
	constexpr float ButtonColorMulDefault() const { return 1.0f; }
	float ButtonColorMul(const void *pId);

	const CUIRect *Screen();
	void MapScreen();
	float PixelSize();

	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();
	const CUIRect *ClipArea() const;
	inline bool IsClipped() const { return !m_vClips.empty(); }

	int DoButtonLogic(const void *pId, int Checked, const CUIRect *pRect);
	int DoDraggableButtonLogic(const void *pId, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted);
	bool DoDoubleClickLogic(const void *pId);
	EEditState DoPickerLogic(const void *pId, const CUIRect *pRect, float *pX, float *pY);
	void DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, bool SmoothClamp = false, float ScrollSpeed = 10.0f) const;
	static vec2 CalcAlignedCursorPos(const CUIRect *pRect, vec2 TextSize, int Align, const float *pBiggestCharHeight = nullptr);

	void DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {}) const;

	void DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {}, int StrLen = -1, const CTextCursor *pReadCursor = nullptr) const;
	void DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps = {}, int StrLen = -1, const CTextCursor *pReadCursor = nullptr) const;

	/**
	 * Creates an input field.
	 *
	 * @see DoClearableEditBox
	 *
	 * @param pLineInput This pointer will be stored and written to on next user input.
	 *                   So you can not pass in a pointer that goes out of scope such as a local variable.
	 *                   Pass in either a member variable of the current class or a static variable.
	 *                   For example ```static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_MyInput;```
	 * @param pRect the UI rect it will attach to with a 2.0f margin
	 * @param FontSize Size of the font (`10.0f`, `12.0f` and `14.0f` are commonly used here)
	 * @param Corners Number of corners (default: `IGraphics::CORNER_ALL`)
	 * @param vColorSplits Sets color splits of the `CTextCursor` to allow multicolored text
	 *
	 * @return true if the value of the input field changed since the last call.
	 */
	bool DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const std::vector<STextColorSplit> &vColorSplits = {});

	/**
	 * Creates an input field with a clear [x] button attached to it.
	 *
	 * @see DoEditBox
	 *
	 * @param pLineInput This pointer will be stored and written to on next user input.
	 *                   So you can not pass in a pointer that goes out of scope such as a local variable.
	 *                   Pass in either a member variable of the current class or a static variable.
	 *                   For example ```static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_MyInput;```
	 * @param pRect the UI rect it will attach to
	 * @param FontSize Size of the font (`10.0f`, `12.0f` and `14.0f` are commonly used here)
	 * @param Corners Number of corners (default: `IGraphics::CORNER_ALL`)
	 * @param vColorSplits Sets color splits of the `CTextCursor` to allow multicolored text
	 *
	 * @return true if the value of the input field changed since the last call.
	 */
	bool DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners = IGraphics::CORNER_ALL, const std::vector<STextColorSplit> &vColorSplits = {});

	/**
	 * Creates an input field with a search icon and a clear [x] button attached to it.
	 * The input will have default text "Search" and the hotkey Ctrl+F can be used to activate the input.
	 *
	 * @see DoEditBox
	 *
	 * @param pLineInput This pointer will be stored and written to on next user input.
	 *                   So you can not pass in a pointer that goes out of scope such as a local variable.
	 *                   Pass in either a member variable of the current class or a static variable.
	 *                   For example ```static CLineInputBuffered<IO_MAX_PATH_LENGTH> s_MyInput;```
	 * @param pRect the UI rect it will attach to
	 * @param FontSize Size of the font (`10.0f`, `12.0f` and `14.0f` are commonly used here)
	 * @param HotkeyEnabled Whether the hotkey to enable this editbox is currently enabled.
	 *
	 * @return true if the value of the input field changed since the last call.
	 */
	bool DoEditBox_Search(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, bool HotkeyEnabled);

	int DoButton_Menu(CUIElement &UIElement, const CButtonContainer *pId, const std::function<const char *()> &GetTextLambda, const CUIRect *pRect, const SMenuButtonProperties &Props = {});
	// only used for popup menus
	int DoButton_PopupMenu(CButtonContainer *pButtonContainer, const char *pText, const CUIRect *pRect, float Size, int Align, float Padding = 0.0f, bool TransparentInactive = false, bool Enabled = true);

	// value selector
	SEditResult<int64_t> DoValueSelectorWithState(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props = {});
	int64_t DoValueSelector(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props = {});

	// scrollbars
	enum
	{
		SCROLLBAR_OPTION_INFINITE = 1 << 0,
		SCROLLBAR_OPTION_NOCLAMPVALUE = 1 << 1,
		SCROLLBAR_OPTION_MULTILINE = 1 << 2,
	};
	float DoScrollbarV(const void *pId, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pId, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner = nullptr);
	bool DoScrollbarOption(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale = &ms_LinearScrollbarScale, unsigned Flags = 0u, const char *pSuffix = "");

	// progress bar
	void RenderProgressBar(CUIRect ProgressBar, float Progress);

	// progress spinner
	void RenderProgressSpinner(vec2 Center, float OuterRadius, const SProgressSpinnerProperties &Props = {}) const;

	// popup menu
	void DoPopupMenu(const SPopupMenuId *pId, float X, float Y, float Width, float Height, void *pContext, FPopupMenuFunction pfnFunc, const SPopupMenuProperties &Props = {});
	void RenderPopupMenus();
	void ClosePopupMenu(const SPopupMenuId *pId, bool IncludeDescendants = false);
	void ClosePopupMenus();
	bool IsPopupOpen() const;
	bool IsPopupOpen(const SPopupMenuId *pId) const;
	bool IsPopupHovered() const;
	void SetPopupMenuClosedCallback(FPopupMenuClosedCallback pfnCallback);

	struct SMessagePopupContext : public SPopupMenuId
	{
		static constexpr float POPUP_MAX_WIDTH = 200.0f;
		static constexpr float POPUP_FONT_SIZE = 10.0f;

		CUi *m_pUI; // set by CUi when popup is shown
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

		CUi *m_pUI; // set by CUi when popup is shown
		char m_aPositiveButtonLabel[128];
		char m_aNegativeButtonLabel[128];
		char m_aMessage[1024];
		EConfirmationResult m_Result;

		CButtonContainer m_CancelButton;
		CButtonContainer m_ConfirmButton;

		SConfirmPopupContext();
		void Reset();
		void YesNoButtons();
	};
	void ShowPopupConfirm(float X, float Y, SConfirmPopupContext *pContext);

	struct SSelectionPopupContext : public SPopupMenuId
	{
		CUi *m_pUI; // set by CUi when popup is shown
		CScrollRegion *m_pScrollRegion;
		SPopupMenuProperties m_Props;
		char m_aMessage[256];
		std::vector<std::string> m_vEntries;
		std::vector<CButtonContainer> m_vButtonContainers;
		const std::string *m_pSelection;
		int m_SelectionIndex;
		float m_EntryHeight;
		float m_EntryPadding;
		float m_EntrySpacing;
		float m_FontSize;
		float m_Width;
		float m_AlignmentHeight;
		bool m_TransparentButtons;

		SSelectionPopupContext();
		void Reset();
	};
	void ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext);

	struct SColorPickerPopupContext : public SPopupMenuId
	{
		enum EColorPickerMode
		{
			MODE_UNSET = -1,
			MODE_HSVA,
			MODE_RGBA,
			MODE_HSLA,
		};

		CUi *m_pUI; // set by CUi when popup is shown
		EColorPickerMode m_ColorMode = MODE_UNSET;
		bool m_Alpha = false;
		unsigned int *m_pHslaColor = nullptr; // may be nullptr
		ColorHSVA m_HsvaColor;
		ColorRGBA m_RgbaColor;
		ColorHSLA m_HslaColor;
		// UI element IDs
		const char m_HuePickerId = 0;
		const char m_ColorPickerId = 0;
		const char m_aValueSelectorIds[5] = {0};
		CButtonContainer m_aModeButtons[(int)MODE_HSLA + 1];
		EEditState m_State;
	};
	void ShowPopupColorPicker(float X, float Y, SColorPickerPopupContext *pContext);

	// dropdown menu
	struct SDropDownState
	{
		SSelectionPopupContext m_SelectionPopupContext;
		CUIElement m_UiElement;
		CButtonContainer m_ButtonContainer;
		bool m_Init = false;
	};
	int DoDropDown(CUIRect *pRect, int CurSelection, const char **pStrs, int Num, SDropDownState &State);
};

#endif
