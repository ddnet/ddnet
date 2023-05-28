/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <game/localization.h>

#include <limits>

void CUIElement::Init(CUI *pUI, int RequestedRectCount)
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
	m_Text.clear();
	mem_zero(&m_Cursor, sizeof(m_Cursor));
	m_TextColor = ColorRGBA(-1, -1, -1, -1);
	m_TextOutlineColor = ColorRGBA(-1, -1, -1, -1);
	m_QuadColor = ColorRGBA(-1, -1, -1, -1);
}

void CUIElement::SUIElementRect::Draw(const CUIRect *pRect, ColorRGBA Color, int Corners, float Rounding)
{
	bool NeedsRecreate = false;
	if(m_UIRectQuadContainer == -1 || m_Width != pRect->w || m_Height != pRect->h || mem_comp(&m_QuadColor, &Color, sizeof(Color)) != 0)
	{
		m_pParent->UI()->Graphics()->DeleteQuadContainer(m_UIRectQuadContainer);
		NeedsRecreate = true;
	}
	m_X = pRect->x;
	m_Y = pRect->y;
	if(NeedsRecreate)
	{
		m_Width = pRect->w;
		m_Height = pRect->h;
		m_QuadColor = Color;

		m_pParent->UI()->Graphics()->SetColor(Color);
		m_UIRectQuadContainer = m_pParent->UI()->Graphics()->CreateRectQuadContainer(0, 0, pRect->w, pRect->h, Rounding, Corners);
		m_pParent->UI()->Graphics()->SetColor(1, 1, 1, 1);
	}

	m_pParent->UI()->Graphics()->TextureClear();
	m_pParent->UI()->Graphics()->RenderQuadContainerEx(m_UIRectQuadContainer,
		0, -1, m_X, m_Y, 1, 1);
}

/********************************************************
 UI
*********************************************************/

const CLinearScrollbarScale CUI::ms_LinearScrollbarScale;
const CLogarithmicScrollbarScale CUI::ms_LogarithmicScrollbarScale(25);
const CDarkButtonColorFunction CUI::ms_DarkButtonColorFunction;
const CLightButtonColorFunction CUI::ms_LightButtonColorFunction;
const CScrollBarColorFunction CUI::ms_ScrollBarColorFunction;
const float CUI::ms_FontmodHeight = 0.8f;

CUI *CUIElementBase::s_pUI = nullptr;

IClient *CUIElementBase::Client() const { return s_pUI->Client(); }
IGraphics *CUIElementBase::Graphics() const { return s_pUI->Graphics(); }
IInput *CUIElementBase::Input() const { return s_pUI->Input(); }
ITextRender *CUIElementBase::TextRender() const { return s_pUI->TextRender(); }

void CUI::Init(IKernel *pKernel)
{
	m_pClient = pKernel->RequestInterface<IClient>();
	m_pGraphics = pKernel->RequestInterface<IGraphics>();
	m_pInput = pKernel->RequestInterface<IInput>();
	m_pTextRender = pKernel->RequestInterface<ITextRender>();
	CUIRect::Init(m_pGraphics);
	CLineInput::Init(m_pClient, m_pGraphics, m_pInput, m_pTextRender);
	CUIElementBase::Init(this);
}

CUI::CUI()
{
	m_Enabled = true;

	m_pHotItem = 0;
	m_pActiveItem = 0;
	m_pLastActiveItem = 0;
	m_pBecomingHotItem = 0;
	m_pActiveTooltipItem = 0;

	m_MouseX = 0;
	m_MouseY = 0;
	m_MouseWorldX = 0;
	m_MouseWorldY = 0;
	m_MouseButtons = 0;
	m_LastMouseButtons = 0;

	m_Screen.x = 0;
	m_Screen.y = 0;
	m_Screen.w = 848.0f;
	m_Screen.h = 480.0f;
}

CUI::~CUI()
{
	for(CUIElement *&pEl : m_vpOwnUIElements)
	{
		delete pEl;
	}
	m_vpOwnUIElements.clear();
}

CUIElement *CUI::GetNewUIElement(int RequestedRectCount)
{
	CUIElement *pNewEl = new CUIElement(this, RequestedRectCount);

	m_vpOwnUIElements.push_back(pNewEl);

	return pNewEl;
}

void CUI::AddUIElement(CUIElement *pElement)
{
	m_vpUIElements.push_back(pElement);
}

void CUI::ResetUIElement(CUIElement &UIElement)
{
	for(CUIElement::SUIElementRect &Rect : UIElement.m_vUIRects)
	{
		Graphics()->DeleteQuadContainer(Rect.m_UIRectQuadContainer);
		TextRender()->DeleteTextContainer(Rect.m_UITextContainer);
		Rect.Reset();
	}
}

void CUI::OnElementsReset()
{
	for(CUIElement *pEl : m_vpUIElements)
	{
		ResetUIElement(*pEl);
	}
}

void CUI::OnWindowResize()
{
	OnElementsReset();
}

void CUI::OnLanguageChange()
{
	OnElementsReset();
}

void CUI::Update(float MouseX, float MouseY, float MouseWorldX, float MouseWorldY)
{
	unsigned MouseButtons = 0;
	if(Enabled())
	{
		if(Input()->KeyIsPressed(KEY_MOUSE_1))
			MouseButtons |= 1;
		if(Input()->KeyIsPressed(KEY_MOUSE_2))
			MouseButtons |= 2;
		if(Input()->KeyIsPressed(KEY_MOUSE_3))
			MouseButtons |= 4;
	}

	m_MouseDeltaX = MouseX - m_MouseX;
	m_MouseDeltaY = MouseY - m_MouseY;
	m_MouseX = MouseX;
	m_MouseY = MouseY;
	m_MouseWorldX = MouseWorldX;
	m_MouseWorldY = MouseWorldY;
	m_LastMouseButtons = m_MouseButtons;
	m_MouseButtons = MouseButtons;
	m_pHotItem = m_pBecomingHotItem;
	if(m_pActiveItem)
		m_pHotItem = m_pActiveItem;
	m_pBecomingHotItem = 0;

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
	}
}

bool CUI::MouseInside(const CUIRect *pRect) const
{
	return pRect->Inside(m_MouseX, m_MouseY);
}

void CUI::ConvertMouseMove(float *pX, float *pY, IInput::ECursorType CursorType) const
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
		dbg_msg("assert", "CUI::ConvertMouseMove CursorType %d", (int)CursorType);
		dbg_break();
		break;
	}

	if(m_MouseSlow)
		Factor *= 0.05f;

	*pX *= Factor;
	*pY *= Factor;
}

bool CUI::ConsumeHotkey(EHotkey Hotkey)
{
	const bool Pressed = m_HotkeysPressed & Hotkey;
	m_HotkeysPressed &= ~Hotkey;
	return Pressed;
}

bool CUI::OnInput(const IInput::CEvent &Event)
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

float CUI::ButtonColorMul(const void *pID)
{
	if(CheckActiveItem(pID))
		return ButtonColorMulActive();
	else if(HotItem() == pID)
		return ButtonColorMulHot();
	return ButtonColorMulDefault();
}

const CUIRect *CUI::Screen()
{
	float Aspect = Graphics()->ScreenAspect();
	float w, h;

	h = 600;
	w = Aspect * h;

	m_Screen.w = w;
	m_Screen.h = h;

	return &m_Screen;
}

void CUI::MapScreen()
{
	const CUIRect *pScreen = Screen();
	Graphics()->MapScreen(pScreen->x, pScreen->y, pScreen->w, pScreen->h);
}

float CUI::PixelSize()
{
	return Screen()->w / Graphics()->ScreenWidth();
}

void CUI::ClipEnable(const CUIRect *pRect)
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

void CUI::ClipDisable()
{
	dbg_assert(IsClipped(), "no clip region");
	m_vClips.pop_back();
	UpdateClipping();
}

const CUIRect *CUI::ClipArea() const
{
	dbg_assert(IsClipped(), "no clip region");
	return &m_vClips.back();
}

void CUI::UpdateClipping()
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

int CUI::DoButtonLogic(const void *pID, int Checked, const CUIRect *pRect)
{
	// logic
	int ReturnValue = 0;
	const bool Inside = MouseHovered(pRect);
	static int s_ButtonUsed = -1;

	if(CheckActiveItem(pID))
	{
		if(s_ButtonUsed >= 0 && !MouseButton(s_ButtonUsed))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1 + s_ButtonUsed;
			SetActiveItem(nullptr);
			s_ButtonUsed = -1;
		}
	}
	else if(HotItem() == pID)
	{
		for(int i = 0; i < 3; ++i)
		{
			if(MouseButton(i))
			{
				SetActiveItem(pID);
				s_ButtonUsed = i;
			}
		}
	}

	if(Inside && !MouseButton(0) && !MouseButton(1) && !MouseButton(2))
		SetHotItem(pID);

	return ReturnValue;
}

int CUI::DoDraggableButtonLogic(const void *pID, int Checked, const CUIRect *pRect, bool *pClicked, bool *pAbrupted)
{
	// logic
	int ReturnValue = 0;
	const bool Inside = MouseHovered(pRect);
	static int s_ButtonUsed = -1;

	if(pClicked != nullptr)
		*pClicked = false;
	if(pAbrupted != nullptr)
		*pAbrupted = false;

	if(CheckActiveItem(pID))
	{
		if(s_ButtonUsed == 0)
		{
			if(Checked >= 0)
				ReturnValue = 1 + s_ButtonUsed;
			if(!MouseButton(s_ButtonUsed))
			{
				if(pClicked != nullptr)
					*pClicked = true;
				SetActiveItem(nullptr);
				s_ButtonUsed = -1;
			}
			if(MouseButton(1))
			{
				if(pAbrupted != nullptr)
					*pAbrupted = true;
				SetActiveItem(nullptr);
				s_ButtonUsed = -1;
			}
		}
		else if(s_ButtonUsed > 0 && !MouseButton(s_ButtonUsed))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1 + s_ButtonUsed;
			if(pClicked != nullptr)
				*pClicked = true;
			SetActiveItem(nullptr);
			s_ButtonUsed = -1;
		}
	}
	else if(HotItem() == pID)
	{
		for(int i = 0; i < 3; ++i)
		{
			if(MouseButton(i))
			{
				SetActiveItem(pID);
				s_ButtonUsed = i;
			}
		}
	}

	if(Inside && !MouseButton(0) && !MouseButton(1) && !MouseButton(2))
		SetHotItem(pID);

	return ReturnValue;
}

int CUI::DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY)
{
	if(MouseHovered(pRect))
		SetHotItem(pID);

	if(HotItem() == pID && MouseButtonClicked(0))
		SetActiveItem(pID);

	if(CheckActiveItem(pID) && !MouseButton(0))
		SetActiveItem(nullptr);

	if(!CheckActiveItem(pID))
		return 0;

	if(Input()->ShiftIsPressed())
		m_MouseSlow = true;

	if(pX)
		*pX = clamp(m_MouseX - pRect->x, 0.0f, pRect->w);
	if(pY)
		*pY = clamp(m_MouseY - pRect->y, 0.0f, pRect->h);

	return 1;
}

void CUI::DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, bool SmoothClamp, float ScrollSpeed)
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
	float TextBoundingHeight = 0.0f;
	float TextHeight = 0.0f;
	int LineCount = 0;
	float MaxTextWidth = LabelProps.m_MaxWidth != -1 ? LabelProps.m_MaxWidth : MaxWidth;
	STextSizeProperties TextSizeProps{};
	TextSizeProps.m_pHeight = &TextHeight;
	TextSizeProps.m_pMaxCharacterHeightInLine = &TextBoundingHeight;
	TextSizeProps.m_pLineCount = &LineCount;
	float TextWidth = pTextRender->TextWidth(Size, pText, -1, LabelProps.m_MaxWidth, Flags, TextSizeProps);
	while(TextWidth > MaxTextWidth + 0.001f)
	{
		if(!LabelProps.m_EnableWidthCheck)
			break;
		if(Size < 4.0f)
			break;
		Size -= 1.0f;
		TextWidth = pTextRender->TextWidth(Size, pText, -1, LabelProps.m_MaxWidth, Flags, TextSizeProps);
	}
	SCursorAndBoundingBox Res{};
	Res.m_TextSize = vec2(TextWidth, TextHeight);
	Res.m_BiggestCharacterHeight = TextBoundingHeight;
	Res.m_LineCount = LineCount;
	return Res;
}

vec2 CUI::CalcAlignedCursorPos(const CUIRect *pRect, vec2 TextSize, int Align, const float *pBiggestCharHeight)
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

void CUI::DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)
{
	const int Flags = LabelProps.m_StopAtEnd ? TEXTFLAG_STOP_AT_END : 0;
	const SCursorAndBoundingBox TextBounds = CalcFontSizeCursorHeightAndBoundingBox(TextRender(), pText, Flags, Size, pRect->w, LabelProps);
	const vec2 CursorPos = CalcAlignedCursorPos(pRect, TextBounds.m_TextSize, Align, TextBounds.m_LineCount == 1 ? &TextBounds.m_BiggestCharacterHeight : nullptr);

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, CursorPos.x, CursorPos.y, Size, TEXTFLAG_RENDER | Flags);
	Cursor.m_LineWidth = (float)LabelProps.m_MaxWidth;
	TextRender()->TextEx(&Cursor, pText, -1);
}

void CUI::DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor)
{
	const int Flags = pReadCursor ? (pReadCursor->m_Flags & ~TEXTFLAG_RENDER) : LabelProps.m_StopAtEnd ? TEXTFLAG_STOP_AT_END : 0;
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

void CUI::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth, bool StopAtEnd, int StrLen, const CTextCursor *pReadCursor)
{
	bool NeedsRecreate = false;
	bool ColorChanged = RectEl.m_TextColor != TextRender()->GetTextColor() || RectEl.m_TextOutlineColor != TextRender()->GetTextOutlineColor();
	if(!RectEl.m_UITextContainer.Valid() || RectEl.m_Width != pRect->w || RectEl.m_Height != pRect->h || ColorChanged)
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

		CUIRect TmpRect;
		TmpRect.x = 0;
		TmpRect.y = 0;
		TmpRect.w = pRect->w;
		TmpRect.h = pRect->h;

		SLabelProperties Props;
		Props.m_MaxWidth = MaxWidth;
		Props.m_StopAtEnd = StopAtEnd;
		DoLabel(RectEl, &TmpRect, pText, Size, TEXTALIGN_TL, Props, StrLen, pReadCursor);
	}

	ColorRGBA ColorText(RectEl.m_TextColor);
	ColorRGBA ColorTextOutline(RectEl.m_TextOutlineColor);
	if(RectEl.m_UITextContainer.Valid())
	{
		const vec2 CursorPos = CalcAlignedCursorPos(pRect, vec2(RectEl.m_Cursor.m_LongestLineWidth, RectEl.m_Cursor.Height()), Align);
		TextRender()->RenderTextContainer(RectEl.m_UITextContainer, ColorText, ColorTextOutline, CursorPos.x, CursorPos.y);
	}
}

bool CUI::DoEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners)
{
	const bool Inside = MouseHovered(pRect);
	const bool Active = LastActiveItem() == pLineInput;
	const bool Changed = pLineInput->WasChanged();

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
			SetActiveItem(0);
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

	if(Inside)
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
		if(MouseButtonReleased(0))
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
	pRect->Draw(ms_LightButtonColorFunction.GetColor(Active, Inside), Corners, 3.0f);
	ClipEnable(pRect);
	Textbox.x -= ScrollOffset;
	const STextBoundingBox BoundingBox = pLineInput->Render(&Textbox, FontSize, TEXTALIGN_ML, Changed, -1.0f);
	ClipDisable();

	// Scroll left or right if necessary
	if(Active && !JustGotActive && (Changed || Input()->HasComposition()))
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

bool CUI::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners)
{
	CUIRect EditBox, ClearButton;
	pRect->VSplitRight(pRect->h, &EditBox, &ClearButton);

	bool ReturnValue = DoEditBox(pLineInput, &EditBox, FontSize, Corners & ~IGraphics::CORNER_R);

	ClearButton.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f * ButtonColorMul(pLineInput->GetClearButtonId())), Corners & ~IGraphics::CORNER_L, 3.0f);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	DoLabel(&ClearButton, "×", ClearButton.h * CUI::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	if(DoButtonLogic(pLineInput->GetClearButtonId(), 0, &ClearButton))
	{
		pLineInput->Clear();
		SetActiveItem(pLineInput);
		ReturnValue = true;
	}

	return ReturnValue;
}

int CUI::DoButton_Menu(CUIElement &UIElement, const CButtonContainer *pID, const std::function<const char *()> &GetTextLambda, const CUIRect *pRect, const SMenuButtonProperties &Props)
{
	CUIRect Text = *pRect;
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * Props.m_FontFactor) / 2.0f, &Text);

	if(!UIElement.AreRectsInit() || Props.m_HintRequiresStringCheck || Props.m_HintCanChangePositionOrSize || !UIElement.Rect(0)->m_UITextContainer.Valid())
	{
		bool NeedsRecalc = !UIElement.AreRectsInit() || !UIElement.Rect(0)->m_UITextContainer.Valid();
		if(Props.m_HintCanChangePositionOrSize)
		{
			if(UIElement.AreRectsInit())
			{
				if(UIElement.Rect(0)->m_X != pRect->x || UIElement.Rect(0)->m_Y != pRect->y || UIElement.Rect(0)->m_Width != pRect->w || UIElement.Rect(0)->m_Y != pRect->h)
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
				if(i == 0)
				{
					if(pText == nullptr)
						pText = GetTextLambda();
					NewRect.m_Text = pText;
					if(Props.m_UseIconFont)
						TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
					DoLabel(NewRect, &Text, pText, Text.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);
					if(Props.m_UseIconFont)
						TextRender()->SetCurFont(nullptr);
				}
			}
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
	// render
	size_t Index = 2;
	if(CheckActiveItem(pID))
		Index = 0;
	else if(HotItem() == pID)
		Index = 1;
	Graphics()->TextureClear();
	Graphics()->RenderQuadContainer(UIElement.Rect(Index)->m_UIRectQuadContainer, -1);
	ColorRGBA ColorText(TextRender()->DefaultTextColor());
	ColorRGBA ColorTextOutline(TextRender()->DefaultTextOutlineColor());
	if(UIElement.Rect(0)->m_UITextContainer.Valid())
		TextRender()->RenderTextContainer(UIElement.Rect(0)->m_UITextContainer, ColorText, ColorTextOutline);
	return DoButtonLogic(pID, Props.m_Checked, pRect);
}

int CUI::DoButton_PopupMenu(CButtonContainer *pButtonContainer, const char *pText, const CUIRect *pRect, int Align)
{
	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f * ButtonColorMul(pButtonContainer)), IGraphics::CORNER_ALL, 3.0f);

	CUIRect Label;
	pRect->VMargin(2.0f, &Label);
	DoLabel(&Label, pText, 10.0f, Align);

	return DoButtonLogic(pButtonContainer, 0, pRect);
}

float CUI::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	pRect->Margin(5.0f, &Rail);

	CUIRect Handle;
	Rail.HSplitTop(clamp(33.0f, Rail.w, Rail.h / 3.0f), &Handle, 0);
	Handle.y = Rail.y + (Rail.h - Handle.h) * Current;

	// logic
	static float s_OffsetY;
	const bool InsideRail = MouseHovered(&Rail);
	const bool InsideHandle = MouseHovered(&Handle);
	bool Grabbed = false; // whether to apply the offset

	if(CheckActiveItem(pID))
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
	else if(HotItem() == pID)
	{
		if(MouseButton(0))
		{
			SetActiveItem(pID);
			s_OffsetY = MouseY() - Handle.y;
			Grabbed = true;
		}
	}
	else if(MouseButtonClicked(0) && !InsideHandle && InsideRail)
	{
		SetActiveItem(pID);
		s_OffsetY = Handle.h / 2.0f;
		Grabbed = true;
	}

	if(InsideHandle)
	{
		SetHotItem(pID);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.y;
		const float Max = Rail.h - Handle.h;
		const float Cur = MouseY() - s_OffsetY;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, Rail.w / 2.0f);
	Handle.Draw(ms_ScrollBarColorFunction.GetColor(CheckActiveItem(pID), HotItem() == pID), IGraphics::CORNER_ALL, Handle.w / 2.0f);

	return ReturnValue;
}

float CUI::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current, const ColorRGBA *pColorInner)
{
	Current = clamp(Current, 0.0f, 1.0f);

	// layout
	CUIRect Rail;
	if(pColorInner)
		Rail = *pRect;
	else
		pRect->HMargin(5.0f, &Rail);

	CUIRect Handle;
	Rail.VSplitLeft(pColorInner ? 8.0f : clamp(33.0f, Rail.h, Rail.w / 3.0f), &Handle, 0);
	Handle.x += (Rail.w - Handle.w) * Current;

	// logic
	static float s_OffsetX;
	const bool InsideRail = MouseHovered(&Rail);
	const bool InsideHandle = MouseHovered(&Handle);
	bool Grabbed = false; // whether to apply the offset

	if(CheckActiveItem(pID))
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
	else if(HotItem() == pID)
	{
		if(MouseButton(0))
		{
			SetActiveItem(pID);
			s_OffsetX = MouseX() - Handle.x;
			Grabbed = true;
		}
	}
	else if(MouseButtonClicked(0) && !InsideHandle && InsideRail)
	{
		SetActiveItem(pID);
		s_OffsetX = Handle.w / 2.0f;
		Grabbed = true;
	}

	if(InsideHandle)
	{
		SetHotItem(pID);
	}

	float ReturnValue = Current;
	if(Grabbed)
	{
		const float Min = Rail.x;
		const float Max = Rail.w - Handle.w;
		const float Cur = MouseX() - s_OffsetX;
		ReturnValue = clamp((Cur - Min) / Max, 0.0f, 1.0f);
	}

	// render
	if(pColorInner)
	{
		CUIRect Slider;
		Handle.VMargin(-2.0f, &Slider);
		Slider.HMargin(-3.0f, &Slider);
		Slider.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 5.0f);
		Slider.Margin(2.0f, &Slider);
		Slider.Draw(*pColorInner, IGraphics::CORNER_ALL, 3.0f);
	}
	else
	{
		Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, Rail.h / 2.0f);
		Handle.Draw(ms_ScrollBarColorFunction.GetColor(CheckActiveItem(pID), HotItem() == pID), IGraphics::CORNER_ALL, Handle.h / 2.0f);
	}

	return ReturnValue;
}

void CUI::DoScrollbarOption(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags)
{
	const bool Infinite = Flags & CUI::SCROLLBAR_OPTION_INFINITE;
	const bool NoClampValue = Flags & CUI::SCROLLBAR_OPTION_NOCLAMPVALUE;
	dbg_assert(!(Infinite && NoClampValue), "cannot combine SCROLLBAR_OPTION_INFINITE and SCROLLBAR_OPTION_NOCLAMPVALUE");

	int Value = *pOption;
	if(Infinite)
	{
		Min += 1;
		Max += 1;
		if(Value == 0)
			Value = Max;
	}

	char aBufMax[256];
	str_format(aBufMax, sizeof(aBufMax), "%s: %i", pStr, Max);
	char aBuf[256];
	if(!Infinite || Value != Max)
		str_format(aBuf, sizeof(aBuf), "%s: %i", pStr, Value);
	else
		str_format(aBuf, sizeof(aBuf), "%s: ∞", pStr);

	if(NoClampValue)
	{
		// clamp the value internally for the scrollbar
		Value = clamp(Value, Min, Max);
	}

	float FontSize = pRect->h * CUI::ms_FontmodHeight * 0.8f;
	float VSplitVal = 10.0f + maximum(TextRender()->TextWidth(FontSize, aBuf, -1, std::numeric_limits<float>::max()), TextRender()->TextWidth(FontSize, aBufMax, -1, std::numeric_limits<float>::max()));

	CUIRect Label, ScrollBar;
	pRect->VSplitLeft(VSplitVal, &Label, &ScrollBar);
	DoLabel(&Label, aBuf, FontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(DoScrollbarH(pID, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(Infinite)
	{
		if(Value == Max)
			Value = 0;
	}
	else if(NoClampValue)
	{
		if((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max))
			Value = *pOption; // use previous out of range value instead if the scrollbar is at the edge
	}

	*pOption = Value;
}

void CUI::DoScrollbarOptionLabeled(const void *pID, int *pOption, const CUIRect *pRect, const char *pStr, const char **ppLabels, int NumLabels, const IScrollbarScale *pScale)
{
	const int Max = NumLabels - 1;
	int Value = clamp(*pOption, 0, Max);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s", pStr, ppLabels[Value]);

	float FontSize = pRect->h * CUI::ms_FontmodHeight * 0.8f;

	CUIRect Label, ScrollBar;
	pRect->VSplitRight(60.0f, &Label, &ScrollBar);
	Label.VSplitRight(10.0f, &Label, 0);
	DoLabel(&Label, aBuf, FontSize, TEXTALIGN_MC);

	Value = pScale->ToAbsolute(DoScrollbarH(pID, &ScrollBar, pScale->ToRelative(Value, 0, Max)), 0, Max);

	if(HotItem() != pID && !CheckActiveItem(pID) && MouseHovered(pRect) && MouseButtonClicked(0))
		Value = (Value + 1) % NumLabels;

	*pOption = clamp(Value, 0, Max);
}

void CUI::DoPopupMenu(const SPopupMenuId *pID, int X, int Y, int Width, int Height, void *pContext, FPopupMenuFunction pfnFunc, int Corners)
{
	constexpr float Margin = SPopupMenu::POPUP_BORDER + SPopupMenu::POPUP_MARGIN;
	if(X + Width > Screen()->w - Margin)
		X = maximum<float>(X - Width, Margin);
	if(Y + Height > Screen()->h - Margin)
		Y = maximum<float>(Y - Height, Margin);

	m_vPopupMenus.emplace_back();
	SPopupMenu *pNewMenu = &m_vPopupMenus.back();
	pNewMenu->m_pID = pID;
	pNewMenu->m_Rect.x = X;
	pNewMenu->m_Rect.y = Y;
	pNewMenu->m_Rect.w = Width;
	pNewMenu->m_Rect.h = Height;
	pNewMenu->m_Corners = Corners;
	pNewMenu->m_pContext = pContext;
	pNewMenu->m_pfnFunc = pfnFunc;
}

void CUI::RenderPopupMenus()
{
	for(size_t i = 0; i < m_vPopupMenus.size(); ++i)
	{
		const SPopupMenu &PopupMenu = m_vPopupMenus[i];
		const bool Inside = MouseInside(&PopupMenu.m_Rect);
		const bool Active = i == m_vPopupMenus.size() - 1;

		if(Active)
			SetHotItem(PopupMenu.m_pID);

		if(CheckActiveItem(PopupMenu.m_pID))
		{
			if(!MouseButton(0))
			{
				if(!Inside)
				{
					ClosePopupMenu(PopupMenu.m_pID);
				}
				SetActiveItem(nullptr);
			}
		}
		else if(HotItem() == PopupMenu.m_pID)
		{
			if(MouseButton(0))
				SetActiveItem(PopupMenu.m_pID);
		}

		CUIRect PopupRect = PopupMenu.m_Rect;
		PopupRect.Draw(ColorRGBA(0.5f, 0.5f, 0.5f, 0.75f), PopupMenu.m_Corners, 3.0f);
		PopupRect.Margin(SPopupMenu::POPUP_BORDER, &PopupRect);
		PopupRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.75f), PopupMenu.m_Corners, 3.0f);
		PopupRect.Margin(SPopupMenu::POPUP_MARGIN, &PopupRect);

		EPopupMenuFunctionResult Result = PopupMenu.m_pfnFunc(PopupMenu.m_pContext, PopupRect, Active);
		if(Result != POPUP_KEEP_OPEN || (Active && ConsumeHotkey(HOTKEY_ESCAPE)))
			ClosePopupMenu(PopupMenu.m_pID, Result == POPUP_CLOSE_CURRENT_AND_DESCENDANTS);
	}
}

void CUI::ClosePopupMenu(const SPopupMenuId *pID, bool IncludeDescendants)
{
	auto PopupMenuToClose = std::find_if(m_vPopupMenus.begin(), m_vPopupMenus.end(), [pID](const SPopupMenu PopupMenu) { return PopupMenu.m_pID == pID; });
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

void CUI::ClosePopupMenus()
{
	if(m_vPopupMenus.empty())
		return;

	m_vPopupMenus.clear();
	SetActiveItem(nullptr);
	if(m_pfnPopupMenuClosedCallback)
		m_pfnPopupMenuClosedCallback();
}

bool CUI::IsPopupOpen() const
{
	return !m_vPopupMenus.empty();
}

bool CUI::IsPopupOpen(const SPopupMenuId *pID) const
{
	return std::any_of(m_vPopupMenus.begin(), m_vPopupMenus.end(), [pID](const SPopupMenu PopupMenu) { return PopupMenu.m_pID == pID; });
}

bool CUI::IsPopupHovered() const
{
	return std::any_of(m_vPopupMenus.begin(), m_vPopupMenus.end(), [this](const SPopupMenu PopupMenu) { return MouseHovered(&PopupMenu.m_Rect); });
}

void CUI::SetPopupMenuClosedCallback(FPopupMenuClosedCallback pfnCallback)
{
	m_pfnPopupMenuClosedCallback = std::move(pfnCallback);
}

void CUI::SMessagePopupContext::DefaultColor(ITextRender *pTextRender)
{
	m_TextColor = pTextRender->DefaultTextColor();
}

void CUI::SMessagePopupContext::ErrorColor()
{
	m_TextColor = ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f);
}

CUI::EPopupMenuFunctionResult CUI::PopupMessage(void *pContext, CUIRect View, bool Active)
{
	SMessagePopupContext *pMessagePopup = static_cast<SMessagePopupContext *>(pContext);
	CUI *pUI = pMessagePopup->m_pUI;

	pUI->TextRender()->TextColor(pMessagePopup->m_TextColor);
	pUI->TextRender()->Text(View.x, View.y, SMessagePopupContext::POPUP_FONT_SIZE, pMessagePopup->m_aMessage, View.w);
	pUI->TextRender()->TextColor(pUI->TextRender()->DefaultTextColor());

	return (Active && pUI->ConsumeHotkey(HOTKEY_ENTER)) ? CUI::POPUP_CLOSE_CURRENT : CUI::POPUP_KEEP_OPEN;
}

void CUI::ShowPopupMessage(float X, float Y, SMessagePopupContext *pContext)
{
	const float TextWidth = minimum(std::ceil(TextRender()->TextWidth(SMessagePopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, -1.0f) + 0.5f), SMessagePopupContext::POPUP_MAX_WIDTH);
	float TextHeight = 0.0f;
	STextSizeProperties TextSizeProps{};
	TextSizeProps.m_pHeight = &TextHeight;
	TextRender()->TextWidth(SMessagePopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, TextWidth, 0, TextSizeProps);
	pContext->m_pUI = this;
	DoPopupMenu(pContext, X, Y, TextWidth + 10.0f, TextHeight + 10.0f, pContext, PopupMessage);
}

CUI::SConfirmPopupContext::SConfirmPopupContext()
{
	Reset();
}

void CUI::SConfirmPopupContext::Reset()
{
	m_Result = SConfirmPopupContext::UNSET;
}

void CUI::SConfirmPopupContext::YesNoButtons()
{
	str_copy(m_aPositiveButtonLabel, Localize("Yes"));
	str_copy(m_aNegativeButtonLabel, Localize("No"));
}

void CUI::ShowPopupConfirm(float X, float Y, SConfirmPopupContext *pContext)
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

CUI::EPopupMenuFunctionResult CUI::PopupConfirm(void *pContext, CUIRect View, bool Active)
{
	SConfirmPopupContext *pConfirmPopup = static_cast<SConfirmPopupContext *>(pContext);
	CUI *pUI = pConfirmPopup->m_pUI;

	CUIRect Label, ButtonBar, CancelButton, ConfirmButton;
	View.HSplitBottom(SConfirmPopupContext::POPUP_BUTTON_HEIGHT, &Label, &ButtonBar);
	ButtonBar.VSplitMid(&CancelButton, &ConfirmButton, SConfirmPopupContext::POPUP_BUTTON_SPACING);

	pUI->TextRender()->Text(Label.x, Label.y, SConfirmPopupContext::POPUP_FONT_SIZE, pConfirmPopup->m_aMessage, Label.w);

	static CButtonContainer s_CancelButton;
	if(pUI->DoButton_PopupMenu(&s_CancelButton, pConfirmPopup->m_aNegativeButtonLabel, &CancelButton, TEXTALIGN_MC))
	{
		pConfirmPopup->m_Result = SConfirmPopupContext::CANCELED;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	static CButtonContainer s_ConfirmButton;
	if(pUI->DoButton_PopupMenu(&s_ConfirmButton, pConfirmPopup->m_aPositiveButtonLabel, &ConfirmButton, TEXTALIGN_MC) || (Active && pUI->ConsumeHotkey(HOTKEY_ENTER)))
	{
		pConfirmPopup->m_Result = SConfirmPopupContext::CONFIRMED;
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

CUI::SSelectionPopupContext::SSelectionPopupContext()
{
	Reset();
}

void CUI::SSelectionPopupContext::Reset()
{
	m_pSelection = nullptr;
	m_Entries.clear();
}

CUI::EPopupMenuFunctionResult CUI::PopupSelection(void *pContext, CUIRect View, bool Active)
{
	SSelectionPopupContext *pSelectionPopup = static_cast<SSelectionPopupContext *>(pContext);
	CUI *pUI = pSelectionPopup->m_pUI;

	CUIRect Slot;
	const STextBoundingBox TextBoundingBox = pUI->TextRender()->TextBoundingBox(SSelectionPopupContext::POPUP_FONT_SIZE, pSelectionPopup->m_aMessage, -1, SSelectionPopupContext::POPUP_MAX_WIDTH);
	View.HSplitTop(TextBoundingBox.m_H, &Slot, &View);

	pUI->TextRender()->Text(Slot.x, Slot.y, SSelectionPopupContext::POPUP_FONT_SIZE, pSelectionPopup->m_aMessage, Slot.w);

	pSelectionPopup->m_vButtonContainers.resize(pSelectionPopup->m_Entries.size());

	size_t Index = 0;
	for(const auto &Entry : pSelectionPopup->m_Entries)
	{
		View.HSplitTop(SSelectionPopupContext::POPUP_ENTRY_SPACING, nullptr, &View);
		View.HSplitTop(SSelectionPopupContext::POPUP_ENTRY_HEIGHT, &Slot, &View);
		if(pUI->DoButton_PopupMenu(&pSelectionPopup->m_vButtonContainers[Index], Entry.c_str(), &Slot, TEXTALIGN_ML))
			pSelectionPopup->m_pSelection = &Entry;
		++Index;
	}

	return pSelectionPopup->m_pSelection == nullptr ? CUI::POPUP_KEEP_OPEN : CUI::POPUP_CLOSE_CURRENT;
}

void CUI::ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext)
{
	const STextBoundingBox TextBoundingBox = TextRender()->TextBoundingBox(SSelectionPopupContext::POPUP_FONT_SIZE, pContext->m_aMessage, -1, SSelectionPopupContext::POPUP_MAX_WIDTH);
	const float PopupHeight = TextBoundingBox.m_H + pContext->m_Entries.size() * (SSelectionPopupContext::POPUP_ENTRY_HEIGHT + SSelectionPopupContext::POPUP_ENTRY_SPACING) + 10.0f;
	pContext->m_pUI = this;
	pContext->m_pSelection = nullptr;
	DoPopupMenu(pContext, X, Y, SSelectionPopupContext::POPUP_MAX_WIDTH + 10.0f, PopupHeight, pContext, PopupSelection);
}
