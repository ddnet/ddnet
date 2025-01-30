/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui.h"
#include "ui_scrollregion.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/localization.h>

#include <limits>

using namespace FontIcons;

void CUIElement::Init(CUi *pUI, int RequestedRectCount)
{
	m_pUI = pUI;
	pUI->AddUIElement(this);
	if(RequestedRectCount > 0)
		InitRects(RequestedRectCount);
}

void CUIElement::InitRects(int RequestedRectCount)
{
	dbg_assert(m_vUIRects.empty(), "UI rects can only be initialized once, create another ui element instead.");
	m_vUIRects.resize(RequestedRectCount);
	for(auto &Rect : m_vUIRects)
		Rect.m_pParent = this;
}

CUIElement::SUIElementRect::SUIElementRect() { Reset(); }

void CUIElement::SUIElementRect::Reset()
{
	m_UIRectQuadContainer = -1;
	m_UITextContainer.Reset();
	m_X = -1;
	m_Y = -1;
	m_Width = -1;
	m_Height = -1;
	m_Rounding = -1.0f;
	m_Corners = -1;
	m_Text.clear();
	m_Cursor.Reset();
	m_TextColor = ColorRGBA(-1, -1, -1, -1);
	m_TextOutlineColor = ColorRGBA(-1, -1, -1, -1);
	m_QuadColor = ColorRGBA(-1, -1, -1, -1);
	m_ReadCursorGlyphCount = -1;
}

void CUIElement::SUIElementRect::Draw(const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding)
{
	bool NeedsRecreate = false;
	if(m_UIRectQuadContainer == -1 || m_Width != pRect->w || m_Height != pRect->h || m_QuadColor != Color)
	{
		m_pParent->Ui()->Graphics()->DeleteQuadContainer(m_UIRectQuadContainer);
		NeedsRecreate = true;
	}
	m_X = pRect->x;
	m_Y = pRect->y;
	if(NeedsRecreate)
	{
		m_Width = pRect->w;
		m_Height = pRect->h;
		m_QuadColor = Color;

		m_pParent->Ui()->Graphics()->SetColor(Color);
		m_UIRectQuadContainer = m_pParent->Ui()->Graphics()->CreateRectQuadContainer(0, 0, pRect->w, pRect->h, Rounding, Corners);
		m_pParent->Ui()->Graphics()->SetColor(1, 1, 1, 1);
	}

	m_pParent->Ui()->Graphics()->TextureClear();
	m_pParent->Ui()->Graphics()->RenderQuadContainerEx(m_UIRectQuadContainer,
		0, -1, m_X, m_Y, 1, 1);
}

/********************************************************
 UI
*********************************************************/

const CLinearScrollbarScale CUi::ms_LinearScrollbarScale;
const CLogarithmicScrollbarScale CUi::ms_LogarithmicScrollbarScale(25);
const CDarkButtonColorFunction CUi::ms_DarkButtonColorFunction;
const CLightButtonColorFunction CUi::ms_LightButtonColorFunction;
const CScrollBarColorFunction CUi::ms_ScrollBarColorFunction;
const float CUi::ms_FontmodHeight = 0.8f;

CUi *CUIElementBase::s_pUI = nullptr;

IClient *CUIElementBase::Client() const { return s_pUI->Client(); }
IGraphics *CUIElementBase::Graphics() const { return s_pUI->Graphics(); }
IInput *CUIElementBase::Input() const { return s_pUI->Input(); }
ITextRender *CUIElementBase::TextRender() const { return s_pUI->TextRender(); }

void CUi::Init(IKernel *pKernel)
{
	m_pClient = pKernel->RequestInterface<IClient>();
	m_pGraphics = pKernel->RequestInterface<IGraphics>();
	m_pInput = pKernel->RequestInterface<IInput>();
	m_pTextRender = pKernel->RequestInterface<ITextRender>();
	CUIRect::Init(m_pGraphics);
	CLineInput::Init(m_pClient, m_pGraphics, m_pInput, m_pTextRender);
	CUIElementBase::Init(this);
}

CUi::CUi()
{
	m_Enabled = true;

	m_Screen.x = 0.0f;
	m_Screen.y = 0.0f;
}

CUi::~CUi()
{
	for(CUIElement *&pEl : m_vpOwnUIElements)
	{
		delete pEl;
	}
	m_vpOwnUIElements.clear();
}

CUIElement *CUi::GetNewUIElement(int RequestedRectCount)
{
	CUIElement *pNewEl = new CUIElement(this, RequestedRectCount);

	m_vpOwnUIElements.push_back(pNewEl);

	return pNewEl;
}

void CUi::AddUIElement(CUIElement *pElement)
{
	m_vpUIElements.push_back(pElement);
}

void CUi::ResetUIElement(CUIElement &UIElement) const
{
	for(CUIElement::SUIElementRect &Rect : UIElement.m_vUIRects)
	{
		Graphics()->DeleteQuadContainer(Rect.m_UIRectQuadContainer);
		TextRender()->DeleteTextContainer(Rect.m_UITextContainer);
		Rect.Reset();
	}
}

void CUi::OnElementsReset()
{
	for(CUIElement *pEl : m_vpUIElements)
	{
		ResetUIElement(*pEl);
	}
}

void CUi::OnWindowResize()
{
	OnElementsReset();
}

void CUi::OnCursorMove(float X, float Y)
{
	if(!CheckMouseLock())
	{
		m_UpdatedMousePos.x = clamp(m_UpdatedMousePos.x + X, 0.0f, Graphics()->WindowWidth() - 1.0f);
		m_UpdatedMousePos.y = clamp(m_UpdatedMousePos.y + Y, 0.0f, Graphics()->WindowHeight() - 1.0f);
	}

	m_UpdatedMouseDelta += vec2(X, Y);
}

void CUi::Update(vec2 MouseWorldPos)
{
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pScreen = Screen();

	unsigned UpdatedMouseButtonsNext = 0;
	if(Enabled())
	{
		// Update mouse buttons based on mouse keys
		for(int MouseKey = KEY_MOUSE_1; MouseKey <= KEY_MOUSE_3; ++MouseKey)
		{
			if(Input()->KeyIsPressed(MouseKey))
			{
				m_UpdatedMouseButtons |= 1 << (MouseKey - KEY_MOUSE_1);
			}
		}

		// Update mouse position and buttons based on touch finger state
		UpdateTouchState(m_TouchState);
		if(m_TouchState.m_AnyPressed)
		{
			if(!CheckMouseLock())
			{
				m_UpdatedMousePos = m_TouchState.m_PrimaryPosition * WindowSize;
				m_UpdatedMousePos.x = clamp(m_UpdatedMousePos.x, 0.0f, WindowSize.x - 1.0f);
				m_UpdatedMousePos.y = clamp(m_UpdatedMousePos.y, 0.0f, WindowSize.y - 1.0f);
			}
			m_UpdatedMouseDelta += m_TouchState.m_PrimaryDelta * WindowSize;

			// Scroll currently hovered scroll region with touch scroll gesture.
			if(m_TouchState.m_ScrollAmount != vec2(0.0f, 0.0f))
			{
				if(m_pHotScrollRegion != nullptr)
				{
					m_pHotScrollRegion->ScrollRelativeDirect(-m_TouchState.m_ScrollAmount.y * pScreen->h);
				}
				m_TouchState.m_ScrollAmount = vec2(0.0f, 0.0f);
			}

			// We need to delay the click until the next update or it's not possible to use UI
			// elements because click and hover would happen at the same time for touch events.
			if(m_TouchState.m_PrimaryPressed)
			{
				UpdatedMouseButtonsNext |= 1;
			}
			if(m_TouchState.m_SecondaryPressed)
			{
				UpdatedMouseButtonsNext |= 2;
			}
		}
	}

	m_MousePos = m_UpdatedMousePos * vec2(pScreen->w, pScreen->h) / WindowSize;
	m_MouseDelta = m_UpdatedMouseDelta;
	m_UpdatedMouseDelta = vec2(0.0f, 0.0f);
	m_MouseWorldPos = MouseWorldPos;
	m_LastMouseButtons = m_MouseButtons;
	m_MouseButtons = m_UpdatedMouseButtons;
	m_UpdatedMouseButtons = UpdatedMouseButtonsNext;

	m_pHotItem = m_pBecomingHotItem;
	if(m_pActiveItem)
		m_pHotItem = m_pActiveItem;
	m_pBecomingHotItem = nullptr;
	m_pHotScrollRegion = m_pBecomingHotScrollRegion;
	m_pBecomingHotScrollRegion = nullptr;

	if(Enabled())
	{
		CLineInput *pActiveInput = CLineInput::GetActiveInput();
		if(pActiveInput && m_pLastActiveItem && pActiveInput != m_pLastActiveItem)
			pActiveInput->Deactivate();
	}
	else
	{
		m_pHotItem = nullptr;
		m_pActiveItem = nullptr;
		m_pHotScrollRegion = nullptr;
	}

	m_ProgressSpinnerOffset += Client()->RenderFrameTime() * 1.5f;
	m_ProgressSpinnerOffset = std::fmod(m_ProgressSpinnerOffset, 1.0f);
}

void CUi::DebugRender(float X, float Y)
{
	MapScreen();

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "hot=%p nexthot=%p active=%p lastactive=%p", HotItem(), NextHotItem(), ActiveItem(), m_pLastActiveItem);
	TextRender()->Text(X, Y, 10.0f, aBuf);
}

bool CUi::MouseInside(const CUIRect *pRect) const
{
	return pRect->Inside(MousePos());
}

void CUi::ConvertMouseMove(float *pX, float *pY, IInput::ECursorType CursorType) const
{
	float Factor = 1.0f;
	switch(CursorType)
	{
	case IInput::CURSOR_MOUSE:
		Factor = g_Config.m_UiMousesens / 100.0f;
		break;
	case IInput::CURSOR_JOYSTICK:
		Factor = g_Config.m_UiControllerSens / 100.0f;
		break;
	default:
		dbg_msg("assert", "CUi::ConvertMouseMove CursorType %d", (int)CursorType);
		dbg_break();
		break;
	}

	if(m_MouseSlow)
		Factor *= 0.05f;

	*pX *= Factor;
	*pY *= Factor;
}

void CUi::UpdateTouchState(CTouchState &State) const
{
	const std::vector<IInput::CTouchFingerState> &vTouchFingerStates = Input()->TouchFingerStates();

	// Updated touch position as long as any finger is beinged pressed.
	const bool WasAnyPressed = State.m_AnyPressed;
	State.m_AnyPressed = !vTouchFingerStates.empty();
	if(State.m_AnyPressed)
	{
		// We always use the position of first finger being pressed down. Multi-touch UI is
		// not possible and always choosing the last finger would cause the cursor to briefly
		// warp without having any effect if multiple fingers are used.
		const IInput::CTouchFingerState &PrimaryTouchFingerState = vTouchFingerStates.front();
		State.m_PrimaryPosition = PrimaryTouchFingerState.m_Position;
		State.m_PrimaryDelta = PrimaryTouchFingerState.m_Delta;
	}

	// Update primary (left click) and secondary (right click) action.
	if(State.m_SecondaryPressedNext)
	{
		// The secondary action is delayed by one frame until the primary has been released,
		// otherwise most UI elements cannot be activated by the secondary action because they
		// never become the hot-item unless all mouse buttons are released for one frame.
		State.m_SecondaryPressedNext = false;
		State.m_SecondaryPressed = true;
	}
	else if(vTouchFingerStates.size() != 1)
	{
		// Consider primary and secondary to be pressed only when exactly one finger is pressed,
		// to avoid UI elements and console text selection being activated while scrolling.
		State.m_PrimaryPressed = false;
		State.m_SecondaryPressed = false;
	}
	else if(!WasAnyPressed)
	{
		State.m_PrimaryPressed = true;
		State.m_SecondaryActivationTime = Client()->GlobalTime();
		State.m_SecondaryActivationDelta = vec2(0.0f, 0.0f);
	}
	else if(State.m_PrimaryPressed)
	{
		// Activate secondary by pressing and holding roughly on the same position for some time.
		const float SecondaryActivationDelay = 0.5f;
		const float SecondaryActivationMaxDistance = 0.001f;
		State.m_SecondaryActivationDelta += State.m_PrimaryDelta;
		if(Client()->GlobalTime() - State.m_SecondaryActivationTime >= SecondaryActivationDelay &&
			length(State.m_SecondaryActivationDelta) <= SecondaryActivationMaxDistance)
		{
			State.m_PrimaryPressed = false;
			State.m_SecondaryPressedNext = true;
		}
	}

	// Handle two fingers being moved roughly in same direction as a scrolling gesture.
	if(vTouchFingerStates.size() == 2)
	{
		const vec2 Delta0 = vTouchFingerStates[0].m_Delta;
		const vec2 Delta1 = vTouchFingerStates[1].m_Delta;
		const float Similarity = dot(normalize(Delta0), normalize(Delta1));
		const float SimilarityThreshold = 0.8f; // How parallel the deltas have to be (1.0f being completely parallel)
		if(Similarity > SimilarityThreshold)
		{
			const float DirectionThreshold = 3.0f; // How much longer the delta of one axis has to be compared to other axis

			// Vertical scrolling (y-delta must be larger than x-delta)
			if(absolute(Delta0.y) > DirectionThreshold * absolute(Delta0.x) &&
				absolute(Delta1.y) > DirectionThreshold * absolute(Delta1.x) &&
				Delta0.y * Delta1.y > 0.0f) // Same y direction required
			{
				// Accumulate average delta of the two fingers
				State.m_ScrollAmount.y += (Delta0.y + Delta1.y) / 2.0f;
			}
		}
	}
	else
	{
		// Scrolling gesture should start from zero again if released.
		State.m_ScrollAmount = vec2(0.0f, 0.0f);
	}
}

bool CUi::ConsumeHotkey(EHotkey Hotkey)
{
	const bool Pressed = m_HotkeysPressed & Hotkey;
	m_HotkeysPressed &= ~Hotkey;
	return Pressed;
}

bool CUi::OnInput(const IInput::CEvent &Event)
{
	if(!Enabled())
		return false;

	CLineInput *pActiveInput = CLineInput::GetActiveInput();
	if(pActiveInput && pActiveInput->ProcessInput(Event))
		return true;

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		unsigned LastHotkeysPressed = m_HotkeysPressed;
		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
			m_HotkeysPressed |= HOTKEY_ENTER;
		else if(Event.m_Key == KEY_ESCAPE)
			m_HotkeysPressed |= HOTKEY_ESCAPE;
		else if(Event.m_Key == KEY_TAB && !Input()->AltIsPressed())
			m_HotkeysPressed |= HOTKEY_TAB;
		else if(Event.m_Key == KEY_DELETE)
			m_HotkeysPressed |= HOTKEY_DELETE;
		else if(Event.m_Key == KEY_UP)
			m_HotkeysPressed |= HOTKEY_UP;
		else if(Event.m_Key == KEY_DOWN)
			m_HotkeysPressed |= HOTKEY_DOWN;
		else if(Event.m_Key == KEY_LEFT)
			m_HotkeysPressed |= HOTKEY_LEFT;
		else if(Event.m_Key == KEY_RIGHT)
			m_HotkeysPressed |= HOTKEY_RIGHT;
		else if(Event.m_Key == KEY_MOUSE_WHEEL_UP)
			m_HotkeysPressed |= HOTKEY_SCROLL_UP;
		else if(Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
			m_HotkeysPressed |= HOTKEY_SCROLL_DOWN;
		else if(Event.m_Key == KEY_PAGEUP)
			m_HotkeysPressed |= HOTKEY_PAGE_UP;
		else if(Event.m_Key == KEY_PAGEDOWN)
			m_HotkeysPressed |= HOTKEY_PAGE_DOWN;
		else if(Event.m_Key == KEY_HOME)
			m_HotkeysPressed |= HOTKEY_HOME;
		else if(Event.m_Key == KEY_END)
			m_HotkeysPressed |= HOTKEY_END;
		return LastHotkeysPressed != m_HotkeysPressed;
	}
	return false;
}

float CUi::ButtonColorMul(const void *pId)
{
	if(CheckActiveItem(pId))
		return ButtonColorMulActive();
	else if(HotItem() == pId)
		return ButtonColorMulHot();
	return ButtonColorMulDefault();
}

const CUIRect *CUi::Screen()
{
	m_Screen.h = 600.0f;
	m_Screen.w = Graphics()->ScreenAspect() * m_Screen.h;
	return &m_Screen;
}

void CUi::MapScreen()
{
	const CUIRect *pScreen = Screen();
	Graphics()->MapScreen(pScreen->x, pScreen->y, pScreen->w, pScreen->h);
}

float CUi::PixelSize()
{
	return Screen()->w / Graphics()->ScreenWidth();
}

void CUi::ClipEnable(const CUIRect *pRect)
{
	if(IsClipped())
	{
		const CUIRect *pOldRect = ClipArea();
		CUIRect Intersection;
		Intersection.x = std::max(pRect->x, pOldRect->x);
		Intersection.y = std::max(pRect->y, pOldRect->y);
		Intersection.w = std::min(pRect->x + pRect->w, pOldRect->x + pOldRect->w) - pRect->x;
		Intersection.h = std::min(pRect->y + pRect->h, pOldRect->y + pOldRect->h) - pRect->y;
		m_vClips.push_back(Intersection);
	}
	else
	{
		m_vClips.push_back(*pRect);
	}
	UpdateClipping();
}

void CUi::ClipDisable()
{
	dbg_assert(IsClipped(), "no clip region");
	m_vClips.pop_back();
	UpdateClipping();
}

const CUIRect *CUi::ClipArea() const
{
	dbg_assert(IsClipped(), "no clip region");
	return &m_vClips.back();
}

void CUi::UpdateClipping()
{
	if(IsClipped())
	{
		const CUIRect *pRect = ClipArea();
		const float XScale = Graphics()->ScreenWidth() / Screen()->w;
		const float YScale = Graphics()->ScreenHeight() / Screen()->h;
		Graphics()->ClipEnable((int)(pRect->x * XScale), (int)(pRect->y * YScale), (int)(pRect->w * XScale), (int)(pRect->h * YScale));
	}
	else
	{
		Graphics()->ClipDisable();
	}
}

int CUi::DoButtonLogic(const void *pId, int Checked, const CUIRect *pRect)
{
	// logic
	int ReturnValue = 0;
	const bool Inside = MouseHovered(pRect);

	if(CheckActiveItem(pId))
	{
		dbg_assert(m_ActiveButtonLogicButton >= 0, "m_ActiveButtonLogicButton invalid");
		if(!MouseButton(m_ActiveButtonLogicButton))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1 + m_ActiveButtonLogicButton;
			SetActiveItem(nullptr);
			m_ActiveButtonLogicButton = -1;
		}
	}
	else if(HotItem() == pId)
	{
		for(int i = 0; i < 3; ++i)
		{
			if(MouseButton(i))
			{
				SetActiveItem(pId);
				m_ActiveButtonLogicButton = i;
			}
		}
	}

	if(Inside && !MouseButton(0) && !MouseButton(1) && !MouseButton(2))
		SetHotItem(pId);

	return ReturnValue;
}

int CUi::DoDraggableButtonLogic(const void *pId, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted)
{
	// logic
	int ReturnValue = 0;
	const bool Inside = MouseHovered(pRect);

	if(pClicked != nullptr)
		*pClicked = false;
	if(pAbrupted != nullptr)
		*pAbrupted = false;

	if(CheckActiveItem(pId))
	{
		dbg_assert(m_ActiveDraggableButtonLogicButton >= 0, "m_ActiveDraggableButtonLogicButton invalid");
		if(m_ActiveDraggableButtonLogicButton == 0)
		{
			if(Checked >= 0)
				ReturnValue = 1 + m_ActiveDraggableButtonLogicButton;
			if(!MouseButton(m_ActiveDraggableButtonLogicButton))
			{
				if(pClicked != nullptr)
					*pClicked = true;
				SetActiveItem(nullptr);
				m_ActiveDraggableButtonLogicButton = -1;
			}
			if(MouseButton(1))
			{
				if(pAbrupted != nullptr)
					*pAbrupted = true;
				SetActiveItem(nullptr);
				m_ActiveDraggableButtonLogicButton = -1;
			}
		}
		else if(!MouseButton(m_ActiveDraggableButtonLogicButton))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1 + m_ActiveDraggableButtonLogicButton;
			if(pClicked != nullptr)
				*pClicked = true;
			SetActiveItem(nullptr);
			m_ActiveDraggableButtonLogicButton = -1;
		}
	}
	else if(HotItem() == pId)
	{
		for(int i = 0; i < 3; ++i)
		{
			if(MouseButton(i))
			{
				SetActiveItem(pId);
				m_ActiveDraggableButtonLogicButton = i;
			}
		}
	}

	if(Inside && !MouseButton(0) && !MouseButton(1) && !MouseButton(2))
		SetHotItem(pId);

	return ReturnValue;
}

bool CUi::DoDoubleClickLogic(const void *pId)
{
	if(m_DoubleClickState.m_pLastClickedId == pId &&
		Client()->GlobalTime() - m_DoubleClickState.m_LastClickTime < 0.5f &&
		distance(m_DoubleClickState.m_LastClickPos, MousePos()) <= 32.0f * Screen()->h / Graphics()->ScreenHeight())
	{
		m_DoubleClickState.m_pLastClickedId = nullptr;
		return true;
	}
	m_DoubleClickState.m_pLastClickedId = pId;
	m_DoubleClickState.m_LastClickTime = Client()->GlobalTime();
	m_DoubleClickState.m_LastClickPos = MousePos();
	return false;
}

EEditState CUi::DoPickerLogic(const void *pId, const CUIRect *pRect, float *pX, float *pY)
{
	if(MouseHovered(pRect))
		SetHotItem(pId);

	EEditState Res = EEditState::EDITING;

	if(HotItem() == pId && MouseButtonClicked(0))
	{
		SetActiveItem(pId);
		if(!m_pLastEditingItem)
		{
			m_pLastEditingItem = pId;
			Res = EEditState::START;
		}
	}

	if(CheckActiveItem(pId) && !MouseButton(0))
	{
		SetActiveItem(nullptr);
		if(m_pLastEditingItem == pId)
		{
			m_pLastEditingItem = nullptr;
			Res = EEditState::END;
		}
	}

	if(!CheckActiveItem(pId) && Res == EEditState::EDITING)
		return EEditState::NONE;

	if(Input()->ShiftIsPressed())
		m_MouseSlow = true;

	if(pX)
		*pX = clamp(MouseX() - pRect->x, 0.0f, pRect->w);
	if(pY)
		*pY = clamp(MouseY() - pRect->y, 0.0f, pRect->h);

	return Res;
}

void CUi::DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, bool SmoothClamp, float ScrollSpeed) const
{
	// reset scrolling if it's not necessary anymore
	if(TotalSize < ViewPortSize)
	{
		*pScrollOffsetChange = -*pScrollOffset;
	}

	// instant scrolling if distance too long
	if(absolute(*pScrollOffsetChange) > 2.0f * ViewPortSize)
	{
		*pScrollOffset += *pScrollOffsetChange;
		*pScrollOffsetChange = 0.0f;
	}

	// smooth scrolling
	if(*pScrollOffsetChange)
	{
		const float Delta = *pScrollOffsetChange * clamp(Client()->RenderFrameTime() * ScrollSpeed, 0.0f, 1.0f);
		*pScrollOffset += Delta;
		*pScrollOffsetChange -= Delta;
	}

	// clamp to first item
	if(*pScrollOffset < 0.0f)
	{
		if(SmoothClamp && *pScrollOffset < -0.1f)
		{
			*pScrollOffsetChange = -*pScrollOffset;
		}
		else
		{
			*pScrollOffset = 0.0f;
			*pScrollOffsetChange = 0.0f;
		}
	}

	// clamp to last item
	if(TotalSize > ViewPortSize && *pScrollOffset > TotalSize - ViewPortSize)
	{
		if(SmoothClamp && *pScrollOffset - (TotalSize - ViewPortSize) > 0.1f)
		{
			*pScrollOffsetChange = (TotalSize - ViewPortSize) - *pScrollOffset;
		}
		else
		{
			*pScrollOffset = TotalSize - ViewPortSize;
			*pScrollOffsetChange = 0.0f;
		}
	}
}

struct SCursorAndBoundingBox
{
	vec2 m_TextSize;
	float m_BiggestCharacterHeight;
	int m_LineCount;
};

static SCursorAndBoundingBox CalcFontSizeCursorHeightAndBoundingBox(ITextRender *pTextRender, const char *pText, int Flags, float &Size, float MaxWidth, const SLabelProperties &LabelProps)
{
	const float MinFontSize = 5.0f;
	const float MaxTextWidth = LabelProps.m_MaxWidth != -1.0f ? LabelProps.m_MaxWidth : MaxWidth;
	const int FlagsWithoutStop = Flags & ~(TEXTFLAG_STOP_AT_END | TEXTFLAG_ELLIPSIS_AT_END);
	const float MaxTextWidthWithoutStop = Flags == FlagsWithoutStop ? LabelProps.m_MaxWidth : -1.0f;

	float TextBoundingHeight = 0.0f;
	float TextHeight = 0.0f;
	int LineCount = 0;
	STextSizeProperties TextSizeProps{};
	TextSizeProps.m_pHeight = &TextHeight;
	TextSizeProps.m_pMaxCharacterHeightInLine = &TextBoundingHeight;
	TextSizeProps.m_pLineCount = &LineCount;

	float TextWidth;
	do
	{
		Size = maximum(Size, MinFontSize);
		// Only consider stop-at-end and ellipsis-at-end when minimum font size reached or font scaling disabled
		if((Size == MinFontSize || !LabelProps.m_EnableWidthCheck) && Flags != FlagsWithoutStop)
			TextWidth = pTextRender->TextWidth(Size, pText, -1, LabelProps.m_MaxWidth, Flags, TextSizeProps);
		else
			TextWidth = pTextRender->TextWidth(Size, pText, -1, MaxTextWidthWithoutStop, FlagsWithoutStop, TextSizeProps);
		if(TextWidth <= MaxTextWidth + 0.001f || !LabelProps.m_EnableWidthCheck || Size == MinFontSize)
			break;
		Size--;
	} while(true);

	SCursorAndBoundingBox Res{};
	Res.m_TextSize = vec2(TextWidth, TextHeight);
	Res.m_BiggestCharacterHeight = TextBoundingHeight;
	Res.m_LineCount = LineCount;
	return Res;
}

static int GetFlagsForLabelProperties(const SLabelProperties &LabelProps, const CTextCursor *pReadCursor)
{
	if(pReadCursor != nullptr)
		return pReadCursor->m_Flags & ~TEXTFLAG_RENDER;

	int Flags = 0;
	Flags |= LabelProps.m_StopAtEnd ? TEXTFLAG_STOP_AT_END : 0;
	Flags |= LabelProps.m_EllipsisAtEnd ? TEXTFLAG_ELLIPSIS_AT_END : 0;
	return Flags;
}

vec2 CUi::CalcAlignedCursorPos(const CUIRect *pRect, vec2 TextSize, int Align, const float *pBiggestCharHeight)
{
	vec2 Cursor(pRect->x, pRect->y);

	const int HorizontalAlign = Align & TEXTALIGN_MASK_HORIZONTAL;
	if(HorizontalAlign == TEXTALIGN_CENTER)
	{
		Cursor.x += (pRect->w - TextSize.x) / 2.0f;
	}
	else if(HorizontalAlign == TEXTALIGN_RIGHT)
	{
		Cursor.x += pRect->w - TextSize.x;
	}

	const int VerticalAlign = Align & TEXTALIGN_MASK_VERTICAL;
	if(VerticalAlign == TEXTALIGN_MIDDLE)
	{
		Cursor.y += pBiggestCharHeight != nullptr ? ((pRect->h - *pBiggestCharHeight) / 2.0f - (TextSize.y - *pBiggestCharHeight)) : (pRect->h - TextSize.y) / 2.0f;
	}
	else if(VerticalAlign == TEXTALIGN_BOTTOM)
	{
		Cursor.y += pRect->h - TextSize.y;
	}

	return Cursor;
}

void CUi::DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps) const
{
	const int Flags = GetFlagsForLabelProperties(LabelProps, nullptr);
	const SCursorAndBoundingBox TextBounds = CalcFontSizeCursorHeightAndBoundingBox(TextRender(), pText, Flags, Size, pRect->w, LabelProps);
	const vec2 CursorPos = CalcAlignedCursorPos(pRect, TextBounds.m_TextSize, Align, TextBounds.m_LineCount == 1 ? &TextBounds.m_BiggestCharacterHeight : nullptr);

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, CursorPos.x, CursorPos.y, Size, TEXTFLAG_RENDER | Flags);
	Cursor.m_vColorSplits = LabelProps.m_vColorSplits;
	Cursor.m_LineWidth = (float)LabelProps.m_MaxWidth;
	TextRender()->TextEx(&Cursor, pText, -1);
}

void CUi::DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor) const
{
	const int Flags = GetFlagsForLabelProperties(LabelProps, pReadCursor);
	const SCursorAndBoundingBox TextBounds = CalcFontSizeCursorHeightAndBoundingBox(TextRender(), pText, Flags, Size, pRect->w, LabelProps);

	CTextCursor Cursor;
	if(pReadCursor)
	{
		Cursor = *pReadCursor;
	}
	else
	{
		const vec2 CursorPos = CalcAlignedCursorPos(pRect, TextBounds.m_TextSize, Align);
		TextRender()->SetCursor(&Cursor, CursorPos.x, CursorPos.y, Size, TEXTFLAG_RENDER | Flags);
	}
	Cursor.m_LineWidth = LabelProps.m_MaxWidth;

	RectEl.m_TextColor = TextRender()->GetTextColor();
	RectEl.m_TextOutlineColor = TextRender()->GetTextOutlineColor();
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->CreateTextContainer(RectEl.m_UITextContainer, &Cursor, pText, StrLen);
	TextRender()->TextColor(RectEl.m_TextColor);
	TextRender()->TextOutlineColor(RectEl.m_TextOutlineColor);
	RectEl.m_Cursor = Cursor;
}

void CUi::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor) const
{
	const int ReadCursorGlyphCount = pReadCursor == nullptr ? -1 : pReadCursor->m_GlyphCount;
	bool NeedsRecreate = false;
	bool ColorChanged = RectEl.m_TextColor != TextRender()->GetTextColor() || RectEl.m_TextOutlineColor != TextRender()->GetTextOutlineColor();
	if((!RectEl.m_UITextContainer.Valid() && pText[0] != '\0' && StrLen != 0) || RectEl.m_Width != pRect->w || RectEl.m_Height != pRect->h || ColorChanged || RectEl.m_ReadCursorGlyphCount != ReadCursorGlyphCount)
	{
		NeedsRecreate = true;
	}
	else
	{
		if(StrLen <= -1)
		{
			if(str_comp(RectEl.m_Text.c_str(), pText) != 0)
				NeedsRecreate = true;
		}
		else
		{
			if(StrLen != (int)RectEl.m_Text.size() || str_comp_num(RectEl.m_Text.c_str(), pText, StrLen) != 0)
				NeedsRecreate = true;
		}
	}
	RectEl.m_X = pRect->x;
	RectEl.m_Y = pRect->y;
	if(NeedsRecreate)
	{
		TextRender()->DeleteTextContainer(RectEl.m_UITextContainer);

		RectEl.m_Width = pRect->w;
		RectEl.m_Height = pRect->h;

		if(StrLen > 0)
			RectEl.m_Text = std::string(pText, StrLen);
		else if(StrLen < 0)
			RectEl.m_Text = pText;
		else
			RectEl.m_Text.clear();

		RectEl.m_ReadCursorGlyphCount = ReadCursorGlyphCount;

		CUIRect TmpRect;
		TmpRect.x = 0;
		TmpRect.y = 0;
		TmpRect.w = pRect->w;
		TmpRect.h = pRect->h;

		DoLabel(RectEl, &TmpRect, pText, Size, TEXTALIGN_TL, LabelProps, StrLen, pReadCursor);
	}

	if(RectEl.m_UITextContainer.Valid())
	{
		const vec2 CursorPos = CalcAlignedCursorPos(pRect, vec2(RectEl.m_Cursor.m_LongestLineWidth, RectEl.m_Cursor.Height()), Align);
		TextRender()->RenderTextContainer(RectEl.m_UITextContainer, RectEl.m_TextColor, RectEl.m_TextOutlineColor, CursorPos.x, CursorPos.y);
	}
}

bool CUi::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const std::vector<STextColorSplit> &vColorSplits)
{
	const bool Inside = MouseHovered(pRect);
	const bool Active = m_pLastActiveItem == pLineInput;
	const bool Changed = pLineInput->WasChanged();
	const bool CursorChanged = pLineInput->WasCursorChanged();

	const float VSpacing = 2.0f;
	CUIRect Textbox;
	pRect->VMargin(VSpacing, &Textbox);

	bool JustGotActive = false;
	if(CheckActiveItem(pLineInput))
	{
		if(MouseButton(0))
		{
			if(pLineInput->IsActive() && (Input()->HasComposition() || Input()->GetCandidateCount()))
			{
				// Clear IME composition/candidates on mouse press
				Input()->StopTextInput();
				Input()->StartTextInput();
			}
		}
		else
		{
			SetActiveItem(nullptr);
		}
	}
	else if(HotItem() == pLineInput)
	{
		if(MouseButton(0))
		{
			if(!Active)
				JustGotActive = true;
			SetActiveItem(pLineInput);
		}
	}

	if(Inside && !MouseButton(0))
		SetHotItem(pLineInput);

	if(Enabled() && Active && !JustGotActive)
		pLineInput->Activate(EInputPriority::UI);
	else
		pLineInput->Deactivate();

	float ScrollOffset = pLineInput->GetScrollOffset();
	float ScrollOffsetChange = pLineInput->GetScrollOffsetChange();

	// Update mouse selection information
	CLineInput::SMouseSelection *pMouseSelection = pLineInput->GetMouseSelection();
	if(Inside)
	{
		if(!pMouseSelection->m_Selecting && MouseButtonClicked(0))
		{
			pMouseSelection->m_Selecting = true;
			pMouseSelection->m_PressMouse = MousePos();
			pMouseSelection->m_Offset.x = ScrollOffset;
		}
	}
	if(pMouseSelection->m_Selecting)
	{
		pMouseSelection->m_ReleaseMouse = MousePos();
		if(!MouseButton(0))
		{
			pMouseSelection->m_Selecting = false;
		}
	}
	if(ScrollOffset != pMouseSelection->m_Offset.x)
	{
		// When the scroll offset is changed, update the position that the mouse was pressed at,
		// so the existing text selection still stays mostly the same.
		// TODO: The selection may change by one character temporarily, due to different character widths.
		//       Needs text render adjustment: keep selection start based on character.
		pMouseSelection->m_PressMouse.x -= ScrollOffset - pMouseSelection->m_Offset.x;
		pMouseSelection->m_Offset.x = ScrollOffset;
	}

	// Render
	pRect->Draw(ms_LightButtonColorFunction.GetColor(Active, HotItem() == pLineInput), Corners, 3.0f);
	ClipEnable(pRect);
	Textbox.x -= ScrollOffset;
	const STextBoundingBox BoundingBox = pLineInput->Render(&Textbox, FontSize, TEXTALIGN_ML, Changed || CursorChanged, -1.0f, 0.0f, vColorSplits);
	ClipDisable();

	// Scroll left or right if necessary
	if(Active && !JustGotActive && (Changed || CursorChanged || Input()->HasComposition()))
	{
		const float CaretPositionX = pLineInput->GetCaretPosition().x - Textbox.x - ScrollOffset - ScrollOffsetChange;
		if(CaretPositionX > Textbox.w)
			ScrollOffsetChange += CaretPositionX - Textbox.w;
		else if(CaretPositionX < 0.0f)
			ScrollOffsetChange += CaretPositionX;
	}

	DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, Textbox.w, BoundingBox.m_W, true);

	pLineInput->SetScrollOffset(ScrollOffset);
	pLineInput->SetScrollOffsetChange(ScrollOffsetChange);

	return Changed;
}

bool CUi::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const std::vector<STextColorSplit> &vColorSplits)
{
	CUIRect EditBox, ClearButton;
	pRect->VSplitRight(pRect->h, &EditBox, &ClearButton);

	bool ReturnValue = DoEditBox(pLineInput, &EditBox, FontSize, Corners & ~IGraphics::CORNER_R, vColorSplits);

	ClearButton.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f * ButtonColorMul(pLineInput->GetClearButtonId())), Corners & ~IGraphics::CORNER_L, 3.0f);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	DoLabel(&ClearButton, "×", ClearButton.h * CUi::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	if(DoButtonLogic(pLineInput->GetClearButtonId(), 0, &ClearButton))
	{
		pLineInput->Clear();
		SetActiveItem(pLineInput);
		ReturnValue = true;
	}

	return ReturnValue;
}

bool CUi::DoEditBox_Search(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, bool HotkeyEnabled)
{
	CUIRect QuickSearch = *pRect;
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	DoLabel(&QuickSearch, FONT_ICON_MAGNIFYING_GLASS, FontSize, TEXTALIGN_ML);
	const float SearchWidth = TextRender()->TextWidth(FontSize, FONT_ICON_MAGNIFYING_GLASS);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	QuickSearch.VSplitLeft(SearchWidth + 5.0f, nullptr, &QuickSearch);
	if(HotkeyEnabled && Input()->ModifierIsPressed() && Input()->KeyPress(KEY_F))
	{
		SetActiveItem(pLineInput);
		pLineInput->SelectAll();
	}
	pLineInput->SetEmptyText(Localize("Search"));
	return DoClearableEditBox(pLineInput, &QuickSearch, FontSize);
}

int CUi::DoButton_Menu(CUIElement &UIElement, const CButtonContainer *pId, const std::function<const char *()> &GetTextLambda, const CUIRect *pRect, const SMenuButtonProperties &Props)
{
	CUIRect Text = *pRect, DropDownIcon;
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * Props.m_FontFactor) / 2.0f, &Text);
	if(Props.m_ShowDropDownIcon)
	{
		Text.VSplitRight(pRect->h * 0.25f, &Text, nullptr);
		Text.VSplitRight(pRect->h * 0.75f, &Text, &DropDownIcon);
	}

	if(!UIElement.AreRectsInit() || Props.m_HintRequiresStringCheck || Props.m_HintCanChangePositionOrSize || !UIElement.Rect(0)->m_UITextContainer.Valid())
	{
		bool NeedsRecalc = !UIElement.AreRectsInit() || !UIElement.Rect(0)->m_UITextContainer.Valid();
		if(Props.m_HintCanChangePositionOrSize)
		{
			if(UIElement.AreRectsInit())
			{
				if(UIElement.Rect(0)->m_X != pRect->x || UIElement.Rect(0)->m_Y != pRect->y || UIElement.Rect(0)->m_Width != pRect->w || UIElement.Rect(0)->m_Height != pRect->h || UIElement.Rect(0)->m_Rounding != Props.m_Rounding || UIElement.Rect(0)->m_Corners != Props.m_Corners)
				{
					NeedsRecalc = true;
				}
			}
		}
		const char *pText = nullptr;
		if(Props.m_HintRequiresStringCheck)
		{
			if(UIElement.AreRectsInit())
			{
				pText = GetTextLambda();
				if(str_comp(UIElement.Rect(0)->m_Text.c_str(), pText) != 0)
				{
					NeedsRecalc = true;
				}
			}
		}
		if(NeedsRecalc)
		{
			if(!UIElement.AreRectsInit())
			{
				UIElement.InitRects(3);
			}
			ResetUIElement(UIElement);

			for(int i = 0; i < 3; ++i)
			{
				ColorRGBA Color = Props.m_Color;
				if(i == 0)
					Color.a *= ButtonColorMulActive();
				else if(i == 1)
					Color.a *= ButtonColorMulHot();
				else if(i == 2)
					Color.a *= ButtonColorMulDefault();
				Graphics()->SetColor(Color);

				CUIElement::SUIElementRect &NewRect = *UIElement.Rect(i);
				NewRect.m_UIRectQuadContainer = Graphics()->CreateRectQuadContainer(pRect->x, pRect->y, pRect->w, pRect->h, Props.m_Rounding, Props.m_Corners);

				NewRect.m_X = pRect->x;
				NewRect.m_Y = pRect->y;
				NewRect.m_Width = pRect->w;
				NewRect.m_Height = pRect->h;
				NewRect.m_Rounding = Props.m_Rounding;
				NewRect.m_Corners = Props.m_Corners;
				if(i == 0)
				{
					if(pText == nullptr)
						pText = GetTextLambda();
					NewRect.m_Text = pText;
					if(Props.m_UseIconFont)
						TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					DoLabel(NewRect, &Text, pText, Text.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
					if(Props.m_UseIconFont)
						TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				}
			}
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	// render
	size_t Index = 2;
	if(CheckActiveItem(pId))
		Index = 0;
	else if(HotItem() == pId)
		Index = 1;
	Graphics()->TextureClear();
	Graphics()->RenderQuadContainer(UIElement.Rect(Index)->m_UIRectQuadContainer, -1);
	if(Props.m_ShowDropDownIcon)
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		DoLabel(&DropDownIcon, FONT_ICON_CIRCLE_CHEVRON_DOWN, DropDownIcon.h * CUi::ms_FontmodHeight, TEXTALIGN_MR);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
	ColorRGBA ColorText(TextRender()->DefaultTextColor());
	ColorRGBA ColorTextOutline(TextRender()->DefaultTextOutlineColor());
	if(UIElement.Rect(0)->m_UITextContainer.Valid())
		TextRender()->RenderTextContainer(UIElement.Rect(0)->m_UITextContainer, ColorText, ColorTextOutline);
	return DoButtonLogic(pId, Props.m_Checked, pRect);
}

int CUi::DoButton_PopupMenu(CButtonContainer *pButtonContainer, const char *pText, const CUIRect *pRect, float Size, int Align, float Padding, bool TransparentInactive, bool Enabled)
{
	if(!TransparentInactive || CheckActiveItem(pButtonContainer) || HotItem() == pButtonContainer)
		pRect->Draw(Enabled ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * ButtonColorMul(pButtonContainer)) : ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Label;
	pRect->Margin(Padding, &Label);
	DoLabel(&Label, pText, Size, Align);

	return Enabled ? DoButtonLogic(pButtonContainer, 0, pRect) : 0;
}

int64_t CUi::DoValueSelector(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props)
{
	return DoValueSelectorWithState(pId, pRect, pLabel, Current, Min, Max, Props).m_Value;
}

SEditResult<int64_t> CUi::DoValueSelectorWithState(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props)
{
	// logic
	const bool Inside = MouseInside(pRect);
	const int Base = Props.m_IsHex ? 16 : 10;

	if(HotItem() == pId && m_ActiveValueSelectorState.m_Button >= 0 && !MouseButton(m_ActiveValueSelectorState.m_Button))
	{
		DisableMouseLock();
		if(CheckActiveItem(pId))
		{
			SetActiveItem(nullptr);
		}
		if(Inside && ((m_ActiveValueSelectorState.m_Button == 0 && !m_ActiveValueSelectorState.m_DidScroll && DoDoubleClickLogic(pId)) || m_ActiveValueSelectorState.m_Button == 1))
		{
			m_ActiveValueSelectorState.m_pLastTextId = pId;
			m_ActiveValueSelectorState.m_NumberInput.SetInteger64(Current, Base, Props.m_HexPrefix);
			m_ActiveValueSelectorState.m_NumberInput.SelectAll();
		}
		m_ActiveValueSelectorState.m_Button = -1;
	}

	if(m_ActiveValueSelectorState.m_pLastTextId == pId)
	{
		SetActiveItem(&m_ActiveValueSelectorState.m_NumberInput);
		DoEditBox(&m_ActiveValueSelectorState.m_NumberInput, pRect, 10.0f);

		if(ConsumeHotkey(HOTKEY_ENTER) || ((MouseButtonClicked(1) || MouseButtonClicked(0)) && !Inside))
		{
			Current = clamp(m_ActiveValueSelectorState.m_NumberInput.GetInteger64(Base), Min, Max);
			DisableMouseLock();
			SetActiveItem(nullptr);
			m_ActiveValueSelectorState.m_pLastTextId = nullptr;
		}

		if(ConsumeHotkey(HOTKEY_ESCAPE))
		{
			DisableMouseLock();
			SetActiveItem(nullptr);
			m_ActiveValueSelectorState.m_pLastTextId = nullptr;
		}
	}
	else
	{
		if(CheckActiveItem(pId))
		{
			dbg_assert(m_ActiveValueSelectorState.m_Button >= 0, "m_ActiveValueSelectorState.m_Button invalid");
			if(Props.m_UseScroll && m_ActiveValueSelectorState.m_Button == 0 && MouseButton(0))
			{
				m_ActiveValueSelectorState.m_ScrollValue += MouseDeltaX() * (Input()->ShiftIsPressed() ? 0.05f : 1.0f);

				if(absolute(m_ActiveValueSelectorState.m_ScrollValue) > Props.m_Scale)
				{
					const int64_t Count = (int64_t)(m_ActiveValueSelectorState.m_ScrollValue / Props.m_Scale);
					m_ActiveValueSelectorState.m_ScrollValue = std::fmod(m_ActiveValueSelectorState.m_ScrollValue, Props.m_Scale);
					Current += Props.m_Step * Count;
					Current = clamp(Current, Min, Max);
					m_ActiveValueSelectorState.m_DidScroll = true;

					// Constrain to discrete steps
					if(Count > 0)
						Current = Current / Props.m_Step * Props.m_Step;
					else
						Current = std::ceil(Current / (float)Props.m_Step) * Props.m_Step;
				}
			}
		}
		else if(HotItem() == pId)
		{
			if(MouseButton(0))
			{
				m_ActiveValueSelectorState.m_Button = 0;
				m_ActiveValueSelectorState.m_DidScroll = false;
				m_ActiveValueSelectorState.m_ScrollValue = 0.0f;
				SetActiveItem(pId);
				if(Props.m_UseScroll)
					EnableMouseLock(pId);
			}
			else if(MouseButton(1))
			{
				m_ActiveValueSelectorState.m_Button = 1;
				SetActiveItem(pId);
			}
		}

		// render
		char aBuf[128];
		if(pLabel[0] != '\0')
		{
			if(Props.m_IsHex)
				str_format(aBuf, sizeof(aBuf), "%s #%0*" PRIX64, pLabel, Props.m_HexPrefix, Current);
			else
				str_format(aBuf, sizeof(aBuf), "%s %" PRId64, pLabel, Current);
		}
		else
		{
			if(Props.m_IsHex)
				str_format(aBuf, sizeof(aBuf), "#%0*" PRIX64, Props.m_HexPrefix, Current);
			else
				str_format(aBuf, sizeof(aBuf), "%" PRId64, Current);
		}
		pRect->Draw(Props.m_Color, IGraphics::CORNER_ALL, 3.0f);
		DoLabel(pRect, aBuf, 10.0f, TEXTALIGN_MC);
	}

	if(Inside && !MouseButton(0) && !MouseButton(1))
		SetHotItem(pId);

	EEditState State = EEditState::NONE;
	if(m_pLastEditingItem == pId)
	{
		State = EEditState::EDITING;
	}
	if(((CheckActiveItem(pId) && CheckMouseLock()) || m_ActiveValueSelectorState.m_pLastTextId == pId) && m_pLastEditingItem != pId)
	{
		State = EEditState::START;
		m_pLastEditingItem = pId;
	}
	if(!CheckMouseLock() && m_ActiveValueSelectorState.m_pLastTextId != pId && m_pLastEditingItem == pId)
	{
		State = EEditState::END;
		m_pLastEditingItem = nullptr;
	}

	return SEditResult<int64_t>{State, Current};
}

float CUi::DoScrollbarV(const void *pId, const CUIRect *pRect, float Current)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	pRect->Margin(5.0f, &Rail);

	CUIRect Handle;
	Rail.HSplitTop(clamp(33.0f, Rail.w, Rail.h / 3.0f), &Handle, nullptr);
	Handle.y = Rail.y + (Rail.h - Handle.h) * Current;

	// logic
	const bool InsideRail = MouseHovered(&Rail);
	const bool InsideHandle = MouseHovered(&Handle);
	bool Grabbed = false; // whether to apply the offset

	if(CheckActiveItem(pId))
	{
		if(MouseButton(0))
		{
			Grabbed = true;
			if(Input()->ShiftIsPressed())
				m_MouseSlow = true;
		}
		else
		{
			SetActiveItem(nullptr);
		}
	}
	else if(HotItem() == pId)
	{
		if(InsideHandle)
		{
			if(MouseButton(0))
			{
				SetActiveItem(pId);
				m_ActiveScrollbarOffset = MouseY() - Handle.y;
				Grabbed = true;
			}
		}
		else if(MouseButtonClicked(0))
		{
			SetActiveItem(pId);
			m_ActiveScrollbarOffset = Handle.h / 2.0f;
			Grabbed = true;
		}
	}

	if(InsideRail && !MouseButton(0))
	{
		SetHotItem(pId);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.y;
		const float Max = Rail.h - Handle.h;
		const float Cur = MouseY() - m_ActiveScrollbarOffset;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, Rail.w / 2.0f);
	Handle.Draw(ms_ScrollBarColorFunction.GetColor(CheckActiveItem(pId), HotItem() == pId), IGraphics::CORNER_ALL, Handle.w / 2.0f);

	return ReturnValue;
}

float CUi::DoScrollbarH(const void *pId, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	if(pColorInner)
		Rail = *pRect;
	else
		pRect->HMargin(5.0f, &Rail);

	CUIRect Handle;
	Rail.VSplitLeft(pColorInner ? 8.0f : clamp(33.0f, Rail.h, Rail.w / 3.0f), &Handle, nullptr);
	Handle.x += (Rail.w - Handle.w) * Current;

	CUIRect HandleArea = Handle;
	if(!pColorInner)
	{
		HandleArea.h = pRect->h * 0.9f;
		HandleArea.y = pRect->y + pRect->h * 0.05f;
		HandleArea.w += 6.0f;
		HandleArea.x -= 3.0f;
	}

	// logic
	const bool InsideRail = MouseHovered(&Rail);
	const bool InsideHandle = MouseHovered(&HandleArea);
	bool Grabbed = false; // whether to apply the offset

	if(CheckActiveItem(pId))
	{
		if(MouseButton(0))
		{
			Grabbed = true;
			if(Input()->ShiftIsPressed())
				m_MouseSlow = true;
		}
		else
		{
			SetActiveItem(nullptr);
		}
	}
	else if(HotItem() == pId)
	{
		if(InsideHandle)
		{
			if(MouseButton(0))
			{
				SetActiveItem(pId);
				m_ActiveScrollbarOffset = MouseX() - Handle.x;
				Grabbed = true;
			}
		}
		else if(MouseButtonClicked(0))
		{
			SetActiveItem(pId);
			m_ActiveScrollbarOffset = Handle.w / 2.0f;
			Grabbed = true;
		}
	}

	if(!pColorInner && (InsideHandle || Grabbed) && (CheckActiveItem(pId) || HotItem() == pId))
	{
		Handle.h += 3.0f;
		Handle.y -= 1.5f;
	}

	if(InsideRail && !MouseButton(0))
	{
		SetHotItem(pId);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.x;
		const float Max = Rail.w - Handle.w;
		const float Cur = MouseX() - m_ActiveScrollbarOffset;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	const ColorRGBA HandleColor = ms_ScrollBarColorFunction.GetColor(CheckActiveItem(pId), HotItem() == pId);
	if(pColorInner)
	{
		CUIRect Slider;
		Handle.VMargin(-2.0f, &Slider);
		Slider.HMargin(-3.0f, &Slider);
		Slider.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f).Multiply(HandleColor), IGraphics::CORNER_ALL, 5.0f);
		Slider.Margin(2.0f, &Slider);
		Slider.Draw(pColorInner->Multiply(HandleColor), IGraphics::CORNER_ALL, 3.0f);
	}
	else
	{
		Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, Rail.h / 2.0f);
		Handle.Draw(HandleColor, IGraphics::CORNER_ALL, Rail.h / 2.0f);
	}

	return ReturnValue;
}

bool CUi::DoScrollbarOption(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool Infinite = Flags & CUi::SCROLLBAR_OPTION_INFINITE;
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;
	const bool MultiLine = Flags & CUi::SCROLLBAR_OPTION_MULTILINE;

	int Value = *pOption;
	if(Infinite)
	{
		Max += 1;
		if(Value == 0)
			Value = Max;
	}

	char aBuf[256];
	if(!Infinite || Value != Max)
		str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value, pSuffix);
	else
		str_format(aBuf, sizeof(aBuf), "%s: ∞", pStr);

	if(NoClampValue)
	{
		// clamp the value internally for the scrollbar
		Value = clamp(Value, Min, Max);
	}

	CUIRect Label, ScrollBar;
	if(MultiLine)
		pRect->HSplitMid(&Label, &ScrollBar);
	else
		pRect->VSplitMid(&Label, &ScrollBar, minimum(10.0f, pRect->w * 0.05f));

	const float FontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	DoLabel(&Label, aBuf, FontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
	{
		Value = *pOption; // use previous out of range value instead if the scrollbar is at the edge
	}
	else if(Infinite)
	{
		if(Value == Max)
			Value = 0;
	}

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}

void CUi::RenderProgressBar(CUIRect ProgressBar, float Progress)
{
	const float Rounding = minimum(5.0f, ProgressBar.h / 2.0f);
	ProgressBar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, Rounding);
	ProgressBar.w = maximum(ProgressBar.w * Progress, 2 * Rounding);
	ProgressBar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), IGraphics::CORNER_ALL, Rounding);
}

void CUi::RenderProgressSpinner(vec2 Center, float OuterRadius, const SProgressSpinnerProperties &Props) const
{
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	// The filled and unfilled segments need to begin at the same angle offset
	// or the differences in pixel alignment will make the filled segments flicker.
	const float SegmentsAngle = 2.0f * pi / Props.m_Segments;
	const float InnerRadius = OuterRadius * 0.75f;
	const float AngleOffset = -0.5f * pi;
	Graphics()->SetColor(Props.m_Color.WithMultipliedAlpha(0.5f));
	for(int i = 0; i < Props.m_Segments; ++i)
	{
		const float Angle1 = AngleOffset + i * SegmentsAngle;
		const float Angle2 = AngleOffset + (i + 1) * SegmentsAngle;
		IGraphics::CFreeformItem Item = IGraphics::CFreeformItem(
			Center.x + std::cos(Angle1) * InnerRadius, Center.y + std::sin(Angle1) * InnerRadius,
			Center.x + std::cos(Angle2) * InnerRadius, Center.y + std::sin(Angle2) * InnerRadius,
			Center.x + std::cos(Angle1) * OuterRadius, Center.y + std::sin(Angle1) * OuterRadius,
			Center.x + std::cos(Angle2) * OuterRadius, Center.y + std::sin(Angle2) * OuterRadius);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}

	const float FilledRatio = Props.m_Progress < 0.0f ? 0.333f : Props.m_Progress;
	const int FilledSegmentOffset = Props.m_Progress < 0.0f ? round_to_int(m_ProgressSpinnerOffset * Props.m_Segments) : 0;
	const int FilledNumSegments = minimum<int>(Props.m_Segments * FilledRatio + (Props.m_Progress < 0.0f ? 0 : 1), Props.m_Segments);
	Graphics()->SetColor(Props.m_Color);
	for(int i = 0; i < FilledNumSegments; ++i)
	{
		const float Angle1 = AngleOffset + (i + FilledSegmentOffset) * SegmentsAngle;
		const float Angle2 = AngleOffset + ((i + 1 == FilledNumSegments && Props.m_Progress >= 0.0f) ? (2.0f * pi * Props.m_Progress) : ((i + FilledSegmentOffset + 1) * SegmentsAngle));
		IGraphics::CFreeformItem Item = IGraphics::CFreeformItem(
			Center.x + std::cos(Angle1) * InnerRadius, Center.y + std::sin(Angle1) * InnerRadius,
			Center.x + std::cos(Angle2) * InnerRadius, Center.y + std::sin(Angle2) * InnerRadius,
			Center.x + std::cos(Angle1) * OuterRadius, Center.y + std::sin(Angle1) * OuterRadius,
			Center.x + std::cos(Angle2) * OuterRadius, Center.y + std::sin(Angle2) * OuterRadius);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	}

	Graphics()->QuadsEnd();
}

void CUi::DoPopupMenu(const SPopupMenuId *pId, float X, float Y, float Width, float Height, void *pContext, FPopupMenuFunction pfnFunc, const SPopupMenuProperties &Props)
{
	constexpr float Margin = SPopupMenu::POPUP_BORDER + SPopupMenu::POPUP_MARGIN;
	if(X + Width > Screen()->w - Margin)
		X = maximum<float>(X - Width, Margin);
	if(Y + Height > Screen()->h - Margin)
		Y = maximum<float>(Y - Height, Margin);

	m_vPopupMenus.emplace_back();
	SPopupMenu *pNewMenu = &m_vPopupMenus.back();
	pNewMenu->m_pId = pId;
	pNewMenu->m_Props = Props;
	pNewMenu->m_Rect.x = X;
	pNewMenu->m_Rect.y = Y;
	pNewMenu->m_Rect.w = Width;
	pNewMenu->m_Rect.h = Height;
	pNewMenu->m_pContext = pContext;
	pNewMenu->m_pfnFunc = pfnFunc;
}

void CUi::RenderPopupMenus()
{
	for(size_t i = 0; i < m_vPopupMenus.size(); ++i)
	{
		const SPopupMenu &PopupMenu = m_vPopupMenus[i];
		const SPopupMenuId *pId = PopupMenu.m_pId;
		const bool Inside = MouseInside(&PopupMenu.m_Rect);
		const bool Active = i == m_vPopupMenus.size() - 1;

		if(Active)
		{
			// Prevent UI elements below the popup menu from being activated.
			SetHotItem(pId);
		}

		if(CheckActiveItem(pId))
		{
			if(!MouseButton(0))
			{
				if(!Inside)
				{
					ClosePopupMenu(pId);
					--i;
					continue;
				}
				SetActiveItem(nullptr);
			}
		}
		else if(HotItem() == pId)
		{
			if(MouseButton(0))
				SetActiveItem(pId);
		}

		if(Inside)
		{
			// Prevent scroll regions directly behind popup menus from using the mouse scroll events.
			SetHotScrollRegion(nullptr);
		}

		CUIRect PopupRect = PopupMenu.m_Rect;
		PopupRect.Draw(PopupMenu.m_Props.m_BorderColor, PopupMenu.m_Props.m_Corners, 3.0f);
		PopupRect.Margin(SPopupMenu::POPUP_BORDER, &PopupRect);
		PopupRect.Draw(PopupMenu.m_Props.m_BackgroundColor, PopupMenu.m_Props.m_Corners, 3.0f);
		PopupRect.Margin(SPopupMenu::POPUP_MARGIN, &PopupRect);

		// The popup render function can open/close popups, which may resize the vector and thus
		// invalidate the variable PopupMenu. We therefore store pId in a separate variable.
		EPopupMenuFunctionResult Result = PopupMenu.m_pfnFunc(PopupMenu.m_pContext, PopupRect, Active);
		if(Result != POPUP_KEEP_OPEN || (Active && ConsumeHotkey(HOTKEY_ESCAPE)))
			ClosePopupMenu(pId, Result == POPUP_CLOSE_CURRENT_AND_DESCENDANTS);
	}
}

void CUi::ClosePopupMenu(const SPopupMenuId *pId, bool IncludeDescendants)
{
	auto PopupMenuToClose = std::find_if(m_vPopupMenus.begin(), m_vPopupMenus.end(), [pId](const SPopupMenu PopupMenu) { return PopupMenu.m_pId == pId; });
	if(PopupMenuToClose != m_vPopupMenus.end())
	{
		if(IncludeDescendants)
			m_vPopupMenus.erase(PopupMenuToClose, m_vPopupMenus.end());
		else
			m_vPopupMenus.erase(PopupMenuToClose);
		SetActiveItem(nullptr);
		if(m_pfnPopupMenuClosedCallback)
			m_pfnPopupMenuClosedCallback();
	}
}

void CUi::ClosePopupMenus()
{
	if(m_vPopupMenus.empty())
		return;

	m_vPopupMenus.clear();
	SetActiveItem(nullptr);
	if(m_pfnPopupMenuClosedCallback)
		m_pfnPopupMenuClosedCallback();
}

bool CUi::IsPopupOpen() const
{
	return !m_vPopupMenus.empty();
}

bool CUi::IsPopupOpen(const SPopupMenuId *pId) const
{
	return std::any_of(m_vPopupMenus.begin(), m_vPopupMenus.end(), [pId](const SPopupMenu PopupMenu) { return PopupMenu.m_pId == pId; });
}

bool CUi::IsPopupHovered() const
{
	return std::any_of(m_vPopupMenus.begin(), m_vPopupMenus.end(), [this](const SPopupMenu PopupMenu) { return MouseHovered(&PopupMenu.m_Rect); });
}

void CUi::SetPopupMenuClosedCallback(FPopupMenuClosedCallback pfnCallback)
{
	m_pfnPopupMenuClosedCallback = std::move(pfnCallback);
}

void CUi::SMessagePopupContext::DefaultColor(ITextRender *pTextRender)
{
	m_TextColor = pTextRender->DefaultTextColor();
}

void CUi::SMessagePopupContext::ErrorColor()
{
	m_TextColor = ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f);
}

CUi::EPopupMenuFunctionResult CUi::PopupMessage(void *pContext, CUIRect View, bool Active)
{
	SMessagePopupContext *pMessagePopup = static_cast<SMessagePopupContext *>(pContext);
	CUi *pUI = pMessagePopup->m_pUI;

	pUI->TextRender()->TextColor(pMessagePopup->m_TextColor);
	pUI->TextRender()->Text(View.x, View.y, SMessagePopupContext::POPUP_FONT_SIZE, pMessagePopup->m_aMessage, View.w);
	pUI->TextRender()->TextColor(pUI->TextRender()->DefaultTextColor());

	return (Active && pUI->ConsumeHotkey(HOTKEY_ENTER)) ? CUi::POPUP_CLOSE_CURRENT : CUi::POPUP_KEEP_OPEN;
}

void CUi::ShowPopupMessage(float X, float Y, SMessagePopupContext *pContext)
{
	const float TextWidth = minimum(std::ceil(TextRender()->TextWidth(SMessagePopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, -1.0f) + 0.5f), SMessagePopupContext::POPUP_MAX_WIDTH);
	float TextHeight = 0.0f;
	STextSizeProperties TextSizeProps{};
	TextSizeProps.m_pHeight = &TextHeight;
	TextRender()->TextWidth(SMessagePopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, TextWidth, 0, TextSizeProps);
	pContext->m_pUI = this;
	DoPopupMenu(pContext, X, Y, TextWidth + 10.0f, TextHeight + 10.0f, pContext, PopupMessage);
}

CUi::SConfirmPopupContext::SConfirmPopupContext()
{
	Reset();
}

void CUi::SConfirmPopupContext::Reset()
{
	m_Result = SConfirmPopupContext::UNSET;
}

void CUi::SConfirmPopupContext::YesNoButtons()
{
	str_copy(m_aPositiveButtonLabel, Localize("Yes"));
	str_copy(m_aNegativeButtonLabel, Localize("No"));
}

void CUi::ShowPopupConfirm(float X, float Y, SConfirmPopupContext *pContext)
{
	const float TextWidth = minimum(std::ceil(TextRender()->TextWidth(SConfirmPopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, -1.0f) + 0.5f), SConfirmPopupContext::POPUP_MAX_WIDTH);
	float TextHeight = 0.0f;
	STextSizeProperties TextSizeProps{};
	TextSizeProps.m_pHeight = &TextHeight;
	TextRender()->TextWidth(SConfirmPopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, TextWidth, 0, TextSizeProps);
	const float PopupHeight = TextHeight + SConfirmPopupContext::POPUP_BUTTON_HEIGHT + SConfirmPopupContext::POPUP_BUTTON_SPACING + 10.0f;
	pContext->m_pUI = this;
	pContext->m_Result = SConfirmPopupContext::UNSET;
	DoPopupMenu(pContext, X, Y, TextWidth + 10.0f, PopupHeight, pContext, PopupConfirm);
}

CUi::EPopupMenuFunctionResult CUi::PopupConfirm(void *pContext, CUIRect View, bool Active)
{
	SConfirmPopupContext *pConfirmPopup = static_cast<SConfirmPopupContext *>(pContext);
	CUi *pUI = pConfirmPopup->m_pUI;

	CUIRect Label, ButtonBar, CancelButton, ConfirmButton;
	View.HSplitBottom(SConfirmPopupContext::POPUP_BUTTON_HEIGHT, &Label, &ButtonBar);
	ButtonBar.VSplitMid(&CancelButton, &ConfirmButton, SConfirmPopupContext::POPUP_BUTTON_SPACING);

	pUI->TextRender()->Text(Label.x, Label.y, SConfirmPopupContext::POPUP_FONT_SIZE, pConfirmPopup->m_aMessage, Label.w);

	if(pUI->DoButton_PopupMenu(&pConfirmPopup->m_CancelButton, pConfirmPopup->m_aNegativeButtonLabel, &CancelButton, SConfirmPopupContext::POPUP_FONT_SIZE, TEXTALIGN_MC))
	{
		pConfirmPopup->m_Result = SConfirmPopupContext::CANCELED;
		return CUi::POPUP_CLOSE_CURRENT;
	}

	if(pUI->DoButton_PopupMenu(&pConfirmPopup->m_ConfirmButton, pConfirmPopup->m_aPositiveButtonLabel, &ConfirmButton, SConfirmPopupContext::POPUP_FONT_SIZE, TEXTALIGN_MC) || (Active && pUI->ConsumeHotkey(HOTKEY_ENTER)))
	{
		pConfirmPopup->m_Result = SConfirmPopupContext::CONFIRMED;
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::SSelectionPopupContext::SSelectionPopupContext()
{
	Reset();
}

void CUi::SSelectionPopupContext::Reset()
{
	m_Props = SPopupMenuProperties();
	m_aMessage[0] = '\0';
	m_pSelection = nullptr;
	m_SelectionIndex = -1;
	m_vEntries.clear();
	m_vButtonContainers.clear();
	m_EntryHeight = 12.0f;
	m_EntryPadding = 0.0f;
	m_EntrySpacing = 5.0f;
	m_FontSize = 10.0f;
	m_Width = 300.0f + (SPopupMenu::POPUP_BORDER + SPopupMenu::POPUP_MARGIN) * 2;
	m_AlignmentHeight = -1.0f;
	m_TransparentButtons = false;
}

CUi::EPopupMenuFunctionResult CUi::PopupSelection(void *pContext, CUIRect View, bool Active)
{
	SSelectionPopupContext *pSelectionPopup = static_cast<SSelectionPopupContext *>(pContext);
	CUi *pUI = pSelectionPopup->m_pUI;
	CScrollRegion *pScrollRegion = pSelectionPopup->m_pScrollRegion;

	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = SPopupMenu::POPUP_MARGIN;
	ScrollParams.m_ScrollbarNoMarginRight = true;
	ScrollParams.m_ScrollUnit = 3 * (pSelectionPopup->m_EntryHeight + pSelectionPopup->m_EntrySpacing);
	pScrollRegion->Begin(&View, &ScrollOffset, &ScrollParams);
	View.y += ScrollOffset.y;

	CUIRect Slot;
	if(pSelectionPopup->m_aMessage[0] != '\0')
	{
		const STextBoundingBox TextBoundingBox = pUI->TextRender()->TextBoundingBox(pSelectionPopup->m_FontSize, pSelectionPopup->m_aMessage, -1, pSelectionPopup->m_Width);
		View.HSplitTop(TextBoundingBox.m_H, &Slot, &View);
		if(pScrollRegion->AddRect(Slot))
		{
			pUI->TextRender()->Text(Slot.x, Slot.y, pSelectionPopup->m_FontSize, pSelectionPopup->m_aMessage, Slot.w);
		}
	}

	pSelectionPopup->m_vButtonContainers.resize(pSelectionPopup->m_vEntries.size());

	size_t Index = 0;
	for(const auto &Entry : pSelectionPopup->m_vEntries)
	{
		if(pSelectionPopup->m_aMessage[0] != '\0' || Index != 0)
			View.HSplitTop(pSelectionPopup->m_EntrySpacing, nullptr, &View);
		View.HSplitTop(pSelectionPopup->m_EntryHeight, &Slot, &View);
		if(pScrollRegion->AddRect(Slot))
		{
			if(pUI->DoButton_PopupMenu(&pSelectionPopup->m_vButtonContainers[Index], Entry.c_str(), &Slot, pSelectionPopup->m_FontSize, TEXTALIGN_ML, pSelectionPopup->m_EntryPadding, pSelectionPopup->m_TransparentButtons))
			{
				pSelectionPopup->m_pSelection = &Entry;
				pSelectionPopup->m_SelectionIndex = Index;
			}
		}
		++Index;
	}

	pScrollRegion->End();

	return pSelectionPopup->m_pSelection == nullptr ? CUi::POPUP_KEEP_OPEN : CUi::POPUP_CLOSE_CURRENT;
}

void CUi::ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext)
{
	const STextBoundingBox TextBoundingBox = TextRender()->TextBoundingBox(pContext->m_FontSize, pContext->m_aMessage, -1, pContext->m_Width);
	const float PopupHeight = minimum((pContext->m_aMessage[0] == '\0' ? -pContext->m_EntrySpacing : TextBoundingBox.m_H) + pContext->m_vEntries.size() * (pContext->m_EntryHeight + pContext->m_EntrySpacing) + (SPopupMenu::POPUP_BORDER + SPopupMenu::POPUP_MARGIN) * 2 + CScrollRegion::HEIGHT_MAGIC_FIX, Screen()->h * 0.4f);
	pContext->m_pUI = this;
	pContext->m_pSelection = nullptr;
	pContext->m_SelectionIndex = -1;
	pContext->m_Props.m_Corners = IGraphics::CORNER_ALL;
	if(pContext->m_AlignmentHeight >= 0.0f)
	{
		constexpr float Margin = SPopupMenu::POPUP_BORDER + SPopupMenu::POPUP_MARGIN;
		if(X + pContext->m_Width > Screen()->w - Margin)
		{
			X = maximum<float>(X - pContext->m_Width, Margin);
		}
		if(Y + pContext->m_AlignmentHeight + PopupHeight > Screen()->h - Margin)
		{
			Y -= PopupHeight;
			pContext->m_Props.m_Corners = IGraphics::CORNER_T;
		}
		else
		{
			Y += pContext->m_AlignmentHeight;
			pContext->m_Props.m_Corners = IGraphics::CORNER_B;
		}
	}
	DoPopupMenu(pContext, X, Y, pContext->m_Width, PopupHeight, pContext, PopupSelection, pContext->m_Props);
}

int CUi::DoDropDown(CUIRect *pRect, int CurSelection, const char **pStrs, int Num, SDropDownState &State)
{
	if(!State.m_Init)
	{
		State.m_UiElement.Init(this, -1);
		State.m_Init = true;
	}

	const auto LabelFunc = [CurSelection, pStrs]() {
		return CurSelection > -1 ? pStrs[CurSelection] : "";
	};

	SMenuButtonProperties Props;
	Props.m_HintRequiresStringCheck = true;
	Props.m_HintCanChangePositionOrSize = true;
	Props.m_ShowDropDownIcon = true;
	if(IsPopupOpen(&State.m_SelectionPopupContext))
		Props.m_Corners = IGraphics::CORNER_ALL & (~State.m_SelectionPopupContext.m_Props.m_Corners);
	if(DoButton_Menu(State.m_UiElement, &State.m_ButtonContainer, LabelFunc, pRect, Props))
	{
		State.m_SelectionPopupContext.Reset();
		State.m_SelectionPopupContext.m_Props.m_BorderColor = ColorRGBA(0.7f, 0.7f, 0.7f, 0.9f);
		State.m_SelectionPopupContext.m_Props.m_BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
		for(int i = 0; i < Num; ++i)
			State.m_SelectionPopupContext.m_vEntries.emplace_back(pStrs[i]);
		State.m_SelectionPopupContext.m_EntryHeight = pRect->h;
		State.m_SelectionPopupContext.m_EntryPadding = pRect->h >= 20.0f ? 2.0f : 1.0f;
		State.m_SelectionPopupContext.m_FontSize = (State.m_SelectionPopupContext.m_EntryHeight - 2 * State.m_SelectionPopupContext.m_EntryPadding) * CUi::ms_FontmodHeight;
		State.m_SelectionPopupContext.m_Width = pRect->w;
		State.m_SelectionPopupContext.m_AlignmentHeight = pRect->h;
		State.m_SelectionPopupContext.m_TransparentButtons = true;
		ShowPopupSelection(pRect->x, pRect->y, &State.m_SelectionPopupContext);
	}

	if(State.m_SelectionPopupContext.m_SelectionIndex >= 0)
	{
		const int NewSelection = State.m_SelectionPopupContext.m_SelectionIndex;
		State.m_SelectionPopupContext.Reset();
		return NewSelection;
	}

	return CurSelection;
}

CUi::EPopupMenuFunctionResult CUi::PopupColorPicker(void *pContext, CUIRect View, bool Active)
{
	SColorPickerPopupContext *pColorPicker = static_cast<SColorPickerPopupContext *>(pContext);
	CUi *pUI = pColorPicker->m_pUI;
	pColorPicker->m_State = EEditState::NONE;

	CUIRect ColorsArea, HueArea, BottomArea, ModeButtonArea, HueRect, SatRect, ValueRect, HexRect, AlphaRect;

	View.HSplitTop(140.0f, &ColorsArea, &BottomArea);
	ColorsArea.VSplitRight(20.0f, &ColorsArea, &HueArea);

	BottomArea.HSplitTop(3.0f, nullptr, &BottomArea);
	HueArea.VSplitLeft(3.0f, nullptr, &HueArea);

	BottomArea.HSplitTop(20.0f, &HueRect, &BottomArea);
	BottomArea.HSplitTop(3.0f, nullptr, &BottomArea);

	constexpr float ValuePadding = 5.0f;
	const float HsvValueWidth = (HueRect.w - ValuePadding * 2) / 3.0f;
	const float HexValueWidth = HsvValueWidth * 2 + ValuePadding;

	HueRect.VSplitLeft(HsvValueWidth, &HueRect, &SatRect);
	SatRect.VSplitLeft(ValuePadding, nullptr, &SatRect);
	SatRect.VSplitLeft(HsvValueWidth, &SatRect, &ValueRect);
	ValueRect.VSplitLeft(ValuePadding, nullptr, &ValueRect);

	BottomArea.HSplitTop(20.0f, &HexRect, &BottomArea);
	BottomArea.HSplitTop(3.0f, nullptr, &BottomArea);
	HexRect.VSplitLeft(HexValueWidth, &HexRect, &AlphaRect);
	AlphaRect.VSplitLeft(ValuePadding, nullptr, &AlphaRect);
	BottomArea.HSplitTop(20.0f, &ModeButtonArea, &BottomArea);

	const ColorRGBA BlackColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f);

	HueArea.Draw(BlackColor, IGraphics::CORNER_NONE, 0.0f);
	HueArea.Margin(1.0f, &HueArea);

	ColorsArea.Draw(BlackColor, IGraphics::CORNER_NONE, 0.0f);
	ColorsArea.Margin(1.0f, &ColorsArea);

	ColorHSVA PickerColorHSV = pColorPicker->m_HsvaColor;
	ColorRGBA PickerColorRGB = pColorPicker->m_RgbaColor;
	ColorHSLA PickerColorHSL = pColorPicker->m_HslaColor;

	// Color Area
	ColorRGBA TL, TR, BL, BR;
	TL = BL = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 0.0f, 1.0f));
	TR = BR = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f));
	ColorsArea.Draw4(TL, TR, BL, BR, IGraphics::CORNER_NONE, 0.0f);

	TL = TR = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
	BL = BR = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
	ColorsArea.Draw4(TL, TR, BL, BR, IGraphics::CORNER_NONE, 0.0f);

	// Hue Area
	static const float s_aaColorIndices[7][3] = {
		{1.0f, 0.0f, 0.0f}, // red
		{1.0f, 0.0f, 1.0f}, // magenta
		{0.0f, 0.0f, 1.0f}, // blue
		{0.0f, 1.0f, 1.0f}, // cyan
		{0.0f, 1.0f, 0.0f}, // green
		{1.0f, 1.0f, 0.0f}, // yellow
		{1.0f, 0.0f, 0.0f}, // red
	};

	const float HuePickerOffset = HueArea.h / 6.0f;
	CUIRect HuePartialArea = HueArea;
	HuePartialArea.h = HuePickerOffset;

	for(size_t j = 0; j < std::size(s_aaColorIndices) - 1; j++)
	{
		TL = ColorRGBA(s_aaColorIndices[j][0], s_aaColorIndices[j][1], s_aaColorIndices[j][2], 1.0f);
		BL = ColorRGBA(s_aaColorIndices[j + 1][0], s_aaColorIndices[j + 1][1], s_aaColorIndices[j + 1][2], 1.0f);

		HuePartialArea.y = HueArea.y + HuePickerOffset * j;
		HuePartialArea.Draw4(TL, TL, BL, BL, IGraphics::CORNER_NONE, 0.0f);
	}

	const auto &&RenderAlphaSelector = [&](unsigned OldA) -> SEditResult<int64_t> {
		if(pColorPicker->m_Alpha)
		{
			return pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[3], &AlphaRect, "A:", OldA, 0, 255);
		}
		else
		{
			char aBuf[8];
			str_format(aBuf, sizeof(aBuf), "A: %d", OldA);
			pUI->DoLabel(&AlphaRect, aBuf, 10.0f, TEXTALIGN_MC);
			AlphaRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.65f), IGraphics::CORNER_ALL, 3.0f);
			return {EEditState::NONE, OldA};
		}
	};

	// Editboxes Area
	if(pColorPicker->m_ColorMode == SColorPickerPopupContext::MODE_HSVA)
	{
		const unsigned OldH = round_to_int(PickerColorHSV.h * 255.0f);
		const unsigned OldS = round_to_int(PickerColorHSV.s * 255.0f);
		const unsigned OldV = round_to_int(PickerColorHSV.v * 255.0f);
		const unsigned OldA = round_to_int(PickerColorHSV.a * 255.0f);

		const auto [StateH, H] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[0], &HueRect, "H:", OldH, 0, 255);
		const auto [StateS, S] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[1], &SatRect, "S:", OldS, 0, 255);
		const auto [StateV, V] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[2], &ValueRect, "V:", OldV, 0, 255);
		const auto [StateA, A] = RenderAlphaSelector(OldA);

		if(OldH != H || OldS != S || OldV != V || OldA != A)
		{
			PickerColorHSV = ColorHSVA(H / 255.0f, S / 255.0f, V / 255.0f, A / 255.0f);
			PickerColorHSL = color_cast<ColorHSLA>(PickerColorHSV);
			PickerColorRGB = color_cast<ColorRGBA>(PickerColorHSL);
		}

		for(auto State : {StateH, StateS, StateV, StateA})
		{
			if(State != EEditState::NONE)
			{
				pColorPicker->m_State = State;
				break;
			}
		}
	}
	else if(pColorPicker->m_ColorMode == SColorPickerPopupContext::MODE_RGBA)
	{
		const unsigned OldR = round_to_int(PickerColorRGB.r * 255.0f);
		const unsigned OldG = round_to_int(PickerColorRGB.g * 255.0f);
		const unsigned OldB = round_to_int(PickerColorRGB.b * 255.0f);
		const unsigned OldA = round_to_int(PickerColorRGB.a * 255.0f);

		const auto [StateR, R] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[0], &HueRect, "R:", OldR, 0, 255);
		const auto [StateG, G] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[1], &SatRect, "G:", OldG, 0, 255);
		const auto [StateB, B] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[2], &ValueRect, "B:", OldB, 0, 255);
		const auto [StateA, A] = RenderAlphaSelector(OldA);

		if(OldR != R || OldG != G || OldB != B || OldA != A)
		{
			PickerColorRGB = ColorRGBA(R / 255.0f, G / 255.0f, B / 255.0f, A / 255.0f);
			PickerColorHSL = color_cast<ColorHSLA>(PickerColorRGB);
			PickerColorHSV = color_cast<ColorHSVA>(PickerColorHSL);
		}

		for(auto State : {StateR, StateG, StateB, StateA})
		{
			if(State != EEditState::NONE)
			{
				pColorPicker->m_State = State;
				break;
			}
		}
	}
	else if(pColorPicker->m_ColorMode == SColorPickerPopupContext::MODE_HSLA)
	{
		const unsigned OldH = round_to_int(PickerColorHSL.h * 255.0f);
		const unsigned OldS = round_to_int(PickerColorHSL.s * 255.0f);
		const unsigned OldL = round_to_int(PickerColorHSL.l * 255.0f);
		const unsigned OldA = round_to_int(PickerColorHSL.a * 255.0f);

		const auto [StateH, H] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[0], &HueRect, "H:", OldH, 0, 255);
		const auto [StateS, S] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[1], &SatRect, "S:", OldS, 0, 255);
		const auto [StateL, L] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[2], &ValueRect, "L:", OldL, 0, 255);
		const auto [StateA, A] = RenderAlphaSelector(OldA);

		if(OldH != H || OldS != S || OldL != L || OldA != A)
		{
			PickerColorHSL = ColorHSLA(H / 255.0f, S / 255.0f, L / 255.0f, A / 255.0f);
			PickerColorHSV = color_cast<ColorHSVA>(PickerColorHSL);
			PickerColorRGB = color_cast<ColorRGBA>(PickerColorHSL);
		}

		for(auto State : {StateH, StateS, StateL, StateA})
		{
			if(State != EEditState::NONE)
			{
				pColorPicker->m_State = State;
				break;
			}
		}
	}
	else
	{
		dbg_assert(false, "Color picker mode invalid");
	}

	SValueSelectorProperties Props;
	Props.m_UseScroll = false;
	Props.m_IsHex = true;
	Props.m_HexPrefix = pColorPicker->m_Alpha ? 8 : 6;
	const unsigned OldHex = PickerColorRGB.PackAlphaLast(pColorPicker->m_Alpha);
	auto [HexState, Hex] = pUI->DoValueSelectorWithState(&pColorPicker->m_aValueSelectorIds[4], &HexRect, "Hex:", OldHex, 0, pColorPicker->m_Alpha ? 0xFFFFFFFFll : 0xFFFFFFll, Props);
	if(OldHex != Hex)
	{
		const float OldAlpha = PickerColorRGB.a;
		PickerColorRGB = ColorRGBA::UnpackAlphaLast<ColorRGBA>(Hex, pColorPicker->m_Alpha);
		if(!pColorPicker->m_Alpha)
			PickerColorRGB.a = OldAlpha;
		PickerColorHSL = color_cast<ColorHSLA>(PickerColorRGB);
		PickerColorHSV = color_cast<ColorHSVA>(PickerColorHSL);
	}

	if(HexState != EEditState::NONE)
		pColorPicker->m_State = HexState;

	// Logic
	float PickerX, PickerY;
	EEditState ColorPickerRes = pUI->DoPickerLogic(&pColorPicker->m_ColorPickerId, &ColorsArea, &PickerX, &PickerY);
	if(ColorPickerRes != EEditState::NONE)
	{
		PickerColorHSV.y = PickerX / ColorsArea.w;
		PickerColorHSV.z = 1.0f - PickerY / ColorsArea.h;
		PickerColorHSL = color_cast<ColorHSLA>(PickerColorHSV);
		PickerColorRGB = color_cast<ColorRGBA>(PickerColorHSL);
		pColorPicker->m_State = ColorPickerRes;
	}

	EEditState HuePickerRes = pUI->DoPickerLogic(&pColorPicker->m_HuePickerId, &HueArea, &PickerX, &PickerY);
	if(HuePickerRes != EEditState::NONE)
	{
		PickerColorHSV.x = 1.0f - PickerY / HueArea.h;
		PickerColorHSL = color_cast<ColorHSLA>(PickerColorHSV);
		PickerColorRGB = color_cast<ColorRGBA>(PickerColorHSL);
		pColorPicker->m_State = HuePickerRes;
	}

	// Marker Color Area
	const float MarkerX = ColorsArea.x + ColorsArea.w * PickerColorHSV.y;
	const float MarkerY = ColorsArea.y + ColorsArea.h * (1.0f - PickerColorHSV.z);

	const float MarkerOutlineInd = PickerColorHSV.z > 0.5f ? 0.0f : 1.0f;
	const ColorRGBA MarkerOutline = ColorRGBA(MarkerOutlineInd, MarkerOutlineInd, MarkerOutlineInd, 1.0f);

	pUI->Graphics()->TextureClear();
	pUI->Graphics()->QuadsBegin();
	pUI->Graphics()->SetColor(MarkerOutline);
	pUI->Graphics()->DrawCircle(MarkerX, MarkerY, 4.5f, 32);
	pUI->Graphics()->SetColor(PickerColorRGB);
	pUI->Graphics()->DrawCircle(MarkerX, MarkerY, 3.5f, 32);
	pUI->Graphics()->QuadsEnd();

	// Marker Hue Area
	CUIRect HueMarker;
	HueArea.Margin(-2.5f, &HueMarker);
	HueMarker.h = 6.5f;
	HueMarker.y = (HueArea.y + HueArea.h * (1.0f - PickerColorHSV.x)) - HueMarker.h / 2.0f;

	const ColorRGBA HueMarkerColor = color_cast<ColorRGBA>(ColorHSVA(PickerColorHSV.x, 1.0f, 1.0f, 1.0f));
	const float HueMarkerOutlineColor = PickerColorHSV.x > 0.75f ? 1.0f : 0.0f;
	const ColorRGBA HueMarkerOutline = ColorRGBA(HueMarkerOutlineColor, HueMarkerOutlineColor, HueMarkerOutlineColor, 1.0f);

	HueMarker.Draw(HueMarkerOutline, IGraphics::CORNER_ALL, 1.2f);
	HueMarker.Margin(1.2f, &HueMarker);
	HueMarker.Draw(HueMarkerColor, IGraphics::CORNER_ALL, 1.2f);

	pColorPicker->m_HsvaColor = PickerColorHSV;
	pColorPicker->m_RgbaColor = PickerColorRGB;
	pColorPicker->m_HslaColor = PickerColorHSL;
	if(pColorPicker->m_pHslaColor != nullptr)
		*pColorPicker->m_pHslaColor = PickerColorHSL.Pack(pColorPicker->m_Alpha);

	static const SColorPickerPopupContext::EColorPickerMode s_aModes[] = {SColorPickerPopupContext::MODE_HSVA, SColorPickerPopupContext::MODE_RGBA, SColorPickerPopupContext::MODE_HSLA};
	static const char *s_apModeLabels[std::size(s_aModes)] = {"HSVA", "RGBA", "HSLA"};
	for(SColorPickerPopupContext::EColorPickerMode Mode : s_aModes)
	{
		CUIRect ModeButton;
		ModeButtonArea.VSplitLeft(HsvValueWidth, &ModeButton, &ModeButtonArea);
		ModeButtonArea.VSplitLeft(ValuePadding, nullptr, &ModeButtonArea);
		if(pUI->DoButton_PopupMenu(&pColorPicker->m_aModeButtons[(int)Mode], s_apModeLabels[Mode], &ModeButton, 10.0f, TEXTALIGN_MC, 2.0f, false, pColorPicker->m_ColorMode != Mode))
		{
			pColorPicker->m_ColorMode = Mode;
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CUi::ShowPopupColorPicker(float X, float Y, SColorPickerPopupContext *pContext)
{
	pContext->m_pUI = this;
	if(pContext->m_ColorMode == SColorPickerPopupContext::MODE_UNSET)
		pContext->m_ColorMode = SColorPickerPopupContext::MODE_HSVA;
	DoPopupMenu(pContext, X, Y, 160.0f + 10.0f, 209.0f + 10.0f, pContext, PopupColorPicker);
}
