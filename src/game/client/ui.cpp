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

#include <game/client/lineinput.h>

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
	m_UITextContainer = -1;
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
	if(m_UIRectQuadContainer == -1 || m_X != pRect->x || m_Y != pRect->y || m_Width != pRect->w || m_Height != pRect->h || mem_comp(&m_QuadColor, &Color, sizeof(Color)) != 0)
	{
		if(m_UIRectQuadContainer != -1)
			m_pParent->UI()->Graphics()->DeleteQuadContainer(m_UIRectQuadContainer);
		NeedsRecreate = true;
	}
	if(NeedsRecreate)
	{
		m_X = pRect->x;
		m_Y = pRect->y;
		m_Width = pRect->w;
		m_Height = pRect->h;
		m_QuadColor = Color;

		m_pParent->UI()->Graphics()->SetColor(Color);
		m_UIRectQuadContainer = m_pParent->UI()->Graphics()->CreateRectQuadContainer(pRect->x, pRect->y, pRect->w, pRect->h, Rounding, Corners);
		m_pParent->UI()->Graphics()->SetColor(1, 1, 1, 1);
	}

	m_pParent->UI()->Graphics()->TextureClear();
	m_pParent->UI()->Graphics()->RenderQuadContainer(m_UIRectQuadContainer, -1);
}

/********************************************************
 UI
*********************************************************/

const CLinearScrollbarScale CUI::ms_LinearScrollbarScale;
const CLogarithmicScrollbarScale CUI::ms_LogarithmicScrollbarScale(25);
float CUI::ms_FontmodHeight = 0.8f;

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
	InitInputs(m_pInput->GetEventsRaw(), m_pInput->GetEventCountRaw());
	CUIRect::Init(m_pGraphics);
	CUIElementBase::Init(this);
}

void CUI::InitInputs(IInput::CEvent *pInputEventsArray, int *pInputEventCount)
{
	m_pInputEventsArray = pInputEventsArray;
	m_pInputEventCount = pInputEventCount;
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
		if(Rect.m_UIRectQuadContainer != -1)
			Graphics()->DeleteQuadContainer(Rect.m_UIRectQuadContainer);
		Rect.m_UIRectQuadContainer = -1;
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
	if(!Enabled())
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

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		unsigned LastHotkeysPressed = m_HotkeysPressed;
		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
			m_HotkeysPressed |= HOTKEY_ENTER;
		else if(Event.m_Key == KEY_ESCAPE)
			m_HotkeysPressed |= HOTKEY_ESCAPE;
		else if(Event.m_Key == KEY_TAB && !Input()->KeyIsPressed(KEY_LALT) && !Input()->KeyIsPressed(KEY_RALT))
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

CUIRect *CUI::Screen()
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

	if(pX)
		*pX = clamp(m_MouseX - pRect->x, 0.0f, pRect->w);
	if(pY)
		*pY = clamp(m_MouseY - pRect->y, 0.0f, pRect->h);

	return 1;
}

void CUI::DoSmoothScrollLogic(float *pScrollOffset, float *pScrollOffsetChange, float ViewPortSize, float TotalSize, float ScrollSpeed)
{
	// instant scrolling if distance too long
	if(absolute(*pScrollOffsetChange) > ViewPortSize)
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
		*pScrollOffset = 0.0f;
		*pScrollOffsetChange = 0.0f;
	}
	// clamp to last item
	if(TotalSize > ViewPortSize && *pScrollOffset > TotalSize - ViewPortSize)
	{
		*pScrollOffset = TotalSize - ViewPortSize;
		*pScrollOffsetChange = 0.0f;
	}
}

float CUI::DoTextLabel(float x, float y, float w, float h, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)
{
	float AlignedSize = 0;
	float MaxCharacterHeightInLine = 0;
	float tw = std::numeric_limits<float>::max();
	float MaxTextWidth = LabelProps.m_MaxWidth != -1 ? LabelProps.m_MaxWidth : w;
	tw = TextRender()->TextWidth(0, Size, pText, -1, LabelProps.m_MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	while(tw > MaxTextWidth + 0.001f)
	{
		if(!LabelProps.m_EnableWidthCheck)
			break;
		if(Size < 4.0f)
			break;
		Size -= 1.0f;
		tw = TextRender()->TextWidth(0, Size, pText, -1, LabelProps.m_MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	}

	int Flags = TEXTFLAG_RENDER | (LabelProps.m_StopAtEnd ? TEXTFLAG_STOP_AT_END : 0);

	float AlignmentVert = y + (h - AlignedSize) / 2.f;
	float AlignmentHori = 0;
	if(LabelProps.m_AlignVertically == 0)
	{
		AlignmentVert = y + (h - AlignedSize) / 2.f - (AlignedSize - MaxCharacterHeightInLine) / 2.f;
	}
	// if(Align == 0)
	if(Align & TEXTALIGN_CENTER)
	{
		AlignmentHori = x + (w - tw) / 2.f;
	}
	// else if(Align < 0)
	else if(Align & TEXTALIGN_LEFT)
	{
		AlignmentHori = x;
	}
	// else if(Align > 0)
	else if(Align & TEXTALIGN_RIGHT)
	{
		AlignmentHori = x + w - tw;
	}

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, AlignmentHori, AlignmentVert, Size, Flags);
	Cursor.m_LineWidth = (float)LabelProps.m_MaxWidth;
	if(LabelProps.m_pSelCursor)
	{
		Cursor.m_CursorMode = LabelProps.m_pSelCursor->m_CursorMode;
		Cursor.m_CursorCharacter = LabelProps.m_pSelCursor->m_CursorCharacter;
		Cursor.m_CalculateSelectionMode = LabelProps.m_pSelCursor->m_CalculateSelectionMode;
		Cursor.m_PressMouseX = LabelProps.m_pSelCursor->m_PressMouseX;
		Cursor.m_PressMouseY = LabelProps.m_pSelCursor->m_PressMouseY;
		Cursor.m_ReleaseMouseX = LabelProps.m_pSelCursor->m_ReleaseMouseX;
		Cursor.m_ReleaseMouseY = LabelProps.m_pSelCursor->m_ReleaseMouseY;

		Cursor.m_SelectionStart = LabelProps.m_pSelCursor->m_SelectionStart;
		Cursor.m_SelectionEnd = LabelProps.m_pSelCursor->m_SelectionEnd;
	}

	TextRender()->TextEx(&Cursor, pText, -1);

	if(LabelProps.m_pSelCursor)
	{
		*LabelProps.m_pSelCursor = Cursor;
	}

	return tw;
}

void CUI::DoLabel(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)
{
	DoTextLabel(pRect->x, pRect->y, pRect->w, pRect->h, pText, Size, Align, LabelProps);
}

void CUI::DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, CTextCursor *pReadCursor)
{
	float AlignedSize = 0;
	float MaxCharacterHeightInLine = 0;
	float tw = std::numeric_limits<float>::max();
	float MaxTextWidth = LabelProps.m_MaxWidth != -1 ? LabelProps.m_MaxWidth : pRect->w;
	tw = TextRender()->TextWidth(0, Size, pText, -1, LabelProps.m_MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	while(tw > MaxTextWidth + 0.001f)
	{
		if(!LabelProps.m_EnableWidthCheck)
			break;
		if(Size < 4.0f)
			break;
		Size -= 1.0f;
		tw = TextRender()->TextWidth(0, Size, pText, -1, LabelProps.m_MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	}
	float AlignmentVert = pRect->y + (pRect->h - AlignedSize) / 2.f;
	float AlignmentHori = 0;

	CTextCursor Cursor;

	int Flags = TEXTFLAG_RENDER | (LabelProps.m_StopAtEnd ? TEXTFLAG_STOP_AT_END : 0);

	if(LabelProps.m_AlignVertically == 0)
	{
		AlignmentVert = pRect->y + (pRect->h - AlignedSize) / 2.f - (AlignedSize - MaxCharacterHeightInLine) / 2.f;
	}
	// if(Align == 0)
	if(Align & TEXTALIGN_CENTER)
	{
		AlignmentHori = pRect->x + (pRect->w - tw) / 2.f;
	}
	// else if(Align < 0)
	else if(Align & TEXTALIGN_LEFT)
	{
		AlignmentHori = pRect->x;
	}
	// else if(Align > 0)
	else if(Align & TEXTALIGN_RIGHT)
	{
		AlignmentHori = pRect->x + pRect->w - tw;
	}

	if(pReadCursor)
	{
		Cursor = *pReadCursor;
	}
	else
	{
		TextRender()->SetCursor(&Cursor, AlignmentHori, AlignmentVert, Size, Flags);
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

void CUI::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, float x, float y, float w, float h, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically, bool StopAtEnd, int StrLen, CTextCursor *pReadCursor)
{
	bool NeedsRecreate = false;
	bool ColorChanged = RectEl.m_TextColor != TextRender()->GetTextColor() || RectEl.m_TextOutlineColor != TextRender()->GetTextOutlineColor();
	if(RectEl.m_UITextContainer == -1 || RectEl.m_X != x || RectEl.m_Y != y || RectEl.m_Width != w || RectEl.m_Height != h || ColorChanged)
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
	if(NeedsRecreate)
	{
		TextRender()->DeleteTextContainer(RectEl.m_UITextContainer);

		RectEl.m_X = x;
		RectEl.m_Y = y;
		RectEl.m_Width = w;
		RectEl.m_Height = h;

		if(StrLen > 0)
			RectEl.m_Text = std::string(pText, StrLen);
		else if(StrLen < 0)
			RectEl.m_Text = pText;
		else
			RectEl.m_Text.clear();

		CUIRect TmpRect;
		TmpRect.x = x;
		TmpRect.y = y;
		TmpRect.w = w;
		TmpRect.h = h;

		SLabelProperties Props;
		Props.m_MaxWidth = MaxWidth;
		Props.m_AlignVertically = AlignVertically;
		Props.m_StopAtEnd = StopAtEnd;
		DoLabel(RectEl, &TmpRect, pText, Size, Align, Props, StrLen, pReadCursor);
	}

	ColorRGBA ColorText(RectEl.m_TextColor);
	ColorRGBA ColorTextOutline(RectEl.m_TextOutlineColor);
	if(RectEl.m_UITextContainer != -1)
		TextRender()->RenderTextContainer(RectEl.m_UITextContainer, ColorText, ColorTextOutline);
}

void CUI::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically, bool StopAtEnd, int StrLen, CTextCursor *pReadCursor)
{
	DoLabelStreamed(RectEl, pRect->x, pRect->y, pRect->w, pRect->h, pText, Size, Align, MaxWidth, AlignVertically, StopAtEnd, StrLen, pReadCursor);
}

bool CUI::DoEditBox(const void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners, const SUIExEditBoxProperties &Properties)
{
	const bool Inside = MouseHovered(pRect);
	bool ReturnValue = false;
	bool UpdateOffset = false;

	auto &&SetHasSelection = [&](bool HasSelection) {
		m_HasSelection = HasSelection;
		m_pSelItem = m_HasSelection ? pID : nullptr;
	};

	auto &&SelectAllText = [&]() {
		m_CurSelStart = 0;
		int StrLen = str_length(pStr);
		TextRender()->UTF8OffToDecodedOff(pStr, StrLen, m_CurSelEnd);
		SetHasSelection(true);
		m_CurCursor = StrLen;
	};

	if(LastActiveItem() == pID)
	{
		if(m_HasSelection && m_pSelItem != pID)
		{
			SetHasSelection(false);
		}

		m_CurCursor = minimum(str_length(pStr), m_CurCursor);

		bool IsShiftPressed = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT);
		bool IsModPressed = Input()->ModifierIsPressed();

		if(Enabled() && !IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_V))
		{
			const char *pText = Input()->GetClipboardText();
			if(pText)
			{
				int OffsetL = clamp(m_CurCursor, 0, str_length(pStr));
				int OffsetR = OffsetL;

				if(m_HasSelection)
				{
					int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
					int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
					int UTF8SelLeft = -1;
					int UTF8SelRight = -1;
					if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft, SelRight, UTF8SelLeft, UTF8SelRight))
					{
						OffsetL = UTF8SelLeft;
						OffsetR = UTF8SelRight;
						SetHasSelection(false);
					}
				}

				std::string NewStr(pStr, OffsetL);

				int WrittenChars = 0;

				const char *pIt = pText;
				while(*pIt)
				{
					const char *pTmp = pIt;
					int Character = str_utf8_decode(&pTmp);
					if(Character == -1 || Character == 0)
						break;

					if(Character == '\r' || Character == '\n')
					{
						NewStr.append(1, ' ');
						++WrittenChars;
					}
					else
					{
						NewStr.append(pIt, (std::intptr_t)(pTmp - pIt));
						WrittenChars += (int)(std::intptr_t)(pTmp - pIt);
					}

					pIt = pTmp;
				}

				NewStr.append(pStr + OffsetR);

				str_copy(pStr, NewStr.c_str(), StrSize);

				m_CurCursor = OffsetL + WrittenChars;
				ReturnValue = true;
			}
		}

		if(Enabled() && !IsShiftPressed && IsModPressed && (Input()->KeyPress(KEY_C) || Input()->KeyPress(KEY_X)))
		{
			if(m_HasSelection)
			{
				int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
				int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
				int UTF8SelLeft = -1;
				int UTF8SelRight = -1;
				if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft, SelRight, UTF8SelLeft, UTF8SelRight))
				{
					std::string NewStr(&pStr[UTF8SelLeft], UTF8SelRight - UTF8SelLeft);
					Input()->SetClipboardText(NewStr.c_str());
					if(Input()->KeyPress(KEY_X))
					{
						NewStr = std::string(pStr, UTF8SelLeft) + std::string(pStr + UTF8SelRight);
						str_copy(pStr, NewStr.c_str(), StrSize);
						SetHasSelection(false);
						if(m_CurCursor > UTF8SelLeft)
							m_CurCursor = maximum(0, m_CurCursor - (UTF8SelRight - UTF8SelLeft));
						else
							m_CurCursor = UTF8SelLeft;
					}
				}
			}
			else
				Input()->SetClipboardText(pStr);
		}

		if(Properties.m_SelectText || (Enabled() && !IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_A)))
		{
			SelectAllText();
		}

		if(Enabled() && !IsShiftPressed && IsModPressed && Input()->KeyPress(KEY_U))
		{
			pStr[0] = '\0';
			m_CurCursor = 0;
			SetHasSelection(false);
			ReturnValue = true;
		}

		for(int i = 0; i < *m_pInputEventCount; i++)
		{
			int LastCursor = m_CurCursor;
			int Len, NumChars;
			str_utf8_stats(pStr, StrSize, StrSize, &Len, &NumChars);
			int32_t ManipulateChanges = CLineInput::Manipulate(m_pInputEventsArray[i], pStr, StrSize, StrSize, &Len, &m_CurCursor, &NumChars, m_HasSelection ? CLineInput::LINE_INPUT_MODIFY_DONT_DELETE : 0, IsModPressed ? KEY_LCTRL : 0);
			ReturnValue |= (ManipulateChanges & (CLineInput::LINE_INPUT_CHANGE_STRING | CLineInput::LINE_INPUT_CHANGE_CHARACTERS_DELETE)) != 0;

			// if cursor changed, reset selection
			if(ManipulateChanges != 0)
			{
				if(m_HasSelection && (ManipulateChanges & (CLineInput::LINE_INPUT_CHANGE_STRING | CLineInput::LINE_INPUT_CHANGE_CHARACTERS_DELETE)) != 0)
				{
					int OffsetL = 0;
					int OffsetR = 0;

					bool IsReverseSel = m_CurSelStart > m_CurSelEnd;

					int ExtraNew = 0;
					int ExtraOld = 0;
					// selection correction from added chars
					if(IsReverseSel)
					{
						TextRender()->UTF8OffToDecodedOff(pStr, m_CurCursor, ExtraNew);
						TextRender()->UTF8OffToDecodedOff(pStr, LastCursor, ExtraOld);
					}

					int SelLeft = minimum(m_CurSelStart, m_CurSelEnd);
					int SelRight = maximum(m_CurSelStart, m_CurSelEnd);
					int UTF8SelLeft = -1;
					int UTF8SelRight = -1;
					if(TextRender()->SelectionToUTF8OffSets(pStr, SelLeft + (ExtraNew - ExtraOld), SelRight + (ExtraNew - ExtraOld), UTF8SelLeft, UTF8SelRight))
					{
						OffsetL = UTF8SelLeft;
						OffsetR = UTF8SelRight;
						SetHasSelection(false);
					}

					std::string NewStr(pStr, OffsetL);

					NewStr.append(pStr + OffsetR);

					str_copy(pStr, NewStr.c_str(), StrSize);

					if(!IsReverseSel)
						m_CurCursor = clamp<int>(m_CurCursor - (UTF8SelRight - UTF8SelLeft), 0, NewStr.length());
				}

				if(IsShiftPressed && (ManipulateChanges & CLineInput::LINE_INPUT_CHANGE_STRING) == 0)
				{
					int CursorPosDecoded = -1;
					int LastCursorPosDecoded = -1;

					if(!m_HasSelection)
					{
						m_CurSelStart = -1;
						m_CurSelEnd = -1;
					}

					if(TextRender()->UTF8OffToDecodedOff(pStr, m_CurCursor, CursorPosDecoded))
					{
						if(TextRender()->UTF8OffToDecodedOff(pStr, LastCursor, LastCursorPosDecoded))
						{
							if(!m_HasSelection)
							{
								m_CurSelStart = LastCursorPosDecoded;
								m_CurSelEnd = LastCursorPosDecoded;
							}
							m_CurSelEnd += (CursorPosDecoded - LastCursorPosDecoded);
						}
					}
					if(m_CurSelStart == m_CurSelEnd)
						SetHasSelection(false);
					else
						SetHasSelection(true);
				}
				else
				{
					if(m_HasSelection && (ManipulateChanges & CLineInput::LINE_INPUT_CHANGE_CURSOR) != 0)
					{
						if(m_CurSelStart < m_CurSelEnd)
						{
							if(m_CurCursor >= LastCursor)
								m_CurCursor = LastCursor;
							else
								TextRender()->DecodedOffToUTF8Off(pStr, m_CurSelStart, m_CurCursor);
						}
						else
						{
							if(m_CurCursor <= LastCursor)
								m_CurCursor = LastCursor;
							else
								TextRender()->DecodedOffToUTF8Off(pStr, m_CurSelStart, m_CurCursor);
						}
					}
					SetHasSelection(false);
				}
			}
		}
	}

	if(Inside)
	{
		SetHotItem(pID);
	}

	CUIRect Textbox = *pRect;
	Textbox.Draw(ColorRGBA(1, 1, 1, 0.5f), Corners, 3.0f);
	Textbox.Margin(2.0f, &Textbox);

	const char *pDisplayStr = pStr;
	char aStars[128];

	if(Hidden)
	{
		unsigned s = str_length(pDisplayStr);
		if(s >= sizeof(aStars))
			s = sizeof(aStars) - 1;
		for(unsigned int i = 0; i < s; ++i)
			aStars[i] = '*';
		aStars[s] = 0;
		pDisplayStr = aStars;
	}

	char aDispEditingText[128 + IInput::INPUT_TEXT_SIZE + 2] = {0};
	int DispCursorPos = m_CurCursor;
	if(LastActiveItem() == pID && Input()->GetIMEEditingTextLength() > -1)
	{
		int EditingTextCursor = Input()->GetEditingCursor();
		str_copy(aDispEditingText, pDisplayStr);
		char aEditingText[IInput::INPUT_TEXT_SIZE + 2];
		if(Hidden)
		{
			// Do not show editing text in password field
			str_copy(aEditingText, "[*]");
			EditingTextCursor = 1;
		}
		else
		{
			str_format(aEditingText, sizeof(aEditingText), "[%s]", Input()->GetIMEEditingText());
		}
		int NewTextLen = str_length(aEditingText);
		int CharsLeft = (int)sizeof(aDispEditingText) - str_length(aDispEditingText) - 1;
		int FillCharLen = minimum(NewTextLen, CharsLeft);
		for(int i = str_length(aDispEditingText) - 1; i >= m_CurCursor; i--)
			aDispEditingText[i + FillCharLen] = aDispEditingText[i];
		for(int i = 0; i < FillCharLen; i++)
			aDispEditingText[m_CurCursor + i] = aEditingText[i];
		DispCursorPos = m_CurCursor + EditingTextCursor + 1;
		pDisplayStr = aDispEditingText;
		UpdateOffset = true;
	}

	bool IsEmptyText = false;
	if(pDisplayStr[0] == '\0')
	{
		pDisplayStr = Properties.m_pEmptyText;
		IsEmptyText = true;
		TextRender()->TextColor(1, 1, 1, 0.75f);
	}

	DispCursorPos = minimum(DispCursorPos, str_length(pDisplayStr));

	bool JustGotActive = false;
	if(CheckActiveItem(pID))
	{
		if(!MouseButton(0))
		{
			SetActiveItem(nullptr);
		}
	}
	else if(HotItem() == pID)
	{
		if(MouseButton(0))
		{
			if(LastActiveItem() != pID)
				JustGotActive = true;
			SetActiveItem(pID);
		}
	}

	// check if the text has to be moved
	if(LastActiveItem() == pID && !JustGotActive && (UpdateOffset || *m_pInputEventCount))
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		if(w - *pOffset > Textbox.w)
		{
			// move to the left
			float wt = TextRender()->TextWidth(0, FontSize, pDisplayStr, -1, std::numeric_limits<float>::max());
			do
			{
				*pOffset += minimum(wt - *pOffset - Textbox.w, Textbox.w / 3);
			} while(w - *pOffset > Textbox.w + 0.0001f);
		}
		else if(w - *pOffset < 0.0f)
		{
			// move to the right
			do
			{
				*pOffset = maximum(0.0f, *pOffset - Textbox.w / 3);
			} while(w - *pOffset < -0.0001f);
		}
	}
	ClipEnable(pRect);
	Textbox.x -= *pOffset;

	CTextCursor SelCursor;
	TextRender()->SetCursor(&SelCursor, 0, 0, 16, 0);

	bool HasMouseSel = false;
	if(LastActiveItem() == pID)
	{
		if(!m_MouseIsPress && MouseButtonClicked(0))
		{
			m_MouseIsPress = true;
			m_MousePressX = MouseX();
			m_MousePressY = MouseY();
		}
	}

	if(m_MouseIsPress)
	{
		m_MouseCurX = MouseX();
		m_MouseCurY = MouseY();
	}
	HasMouseSel = m_MouseIsPress && !IsEmptyText;
	if(m_MouseIsPress && MouseButtonReleased(0))
	{
		m_MouseIsPress = false;
	}

	if(LastActiveItem() == pID)
	{
		int CursorPos = -1;
		TextRender()->UTF8OffToDecodedOff(pDisplayStr, DispCursorPos, CursorPos);

		SelCursor.m_CursorMode = HasMouseSel ? TEXT_CURSOR_CURSOR_MODE_CALCULATE : TEXT_CURSOR_CURSOR_MODE_SET;
		SelCursor.m_CursorCharacter = CursorPos;
		SelCursor.m_CalculateSelectionMode = HasMouseSel ? TEXT_CURSOR_SELECTION_MODE_CALCULATE : (m_HasSelection ? TEXT_CURSOR_SELECTION_MODE_SET : TEXT_CURSOR_SELECTION_MODE_NONE);
		SelCursor.m_PressMouseX = m_MousePressX;
		SelCursor.m_PressMouseY = m_MousePressY;
		SelCursor.m_ReleaseMouseX = m_MouseCurX;
		SelCursor.m_ReleaseMouseY = m_MouseCurY;
		SelCursor.m_SelectionStart = m_CurSelStart;
		SelCursor.m_SelectionEnd = m_CurSelEnd;
	}

	SLabelProperties Props;
	Props.m_pSelCursor = &SelCursor;
	Props.m_EnableWidthCheck = IsEmptyText;
	DoLabel(&Textbox, pDisplayStr, FontSize, TEXTALIGN_LEFT, Props);

	if(LastActiveItem() == pID)
	{
		if(SelCursor.m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
		{
			m_CurSelStart = SelCursor.m_SelectionStart;
			m_CurSelEnd = SelCursor.m_SelectionEnd;
			SetHasSelection(m_CurSelStart != m_CurSelEnd);
		}
		if(SelCursor.m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE)
		{
			TextRender()->DecodedOffToUTF8Off(pDisplayStr, SelCursor.m_CursorCharacter, DispCursorPos);
			m_CurCursor = DispCursorPos;
		}
	}

	TextRender()->TextColor(1, 1, 1, 1);

	// set the ime cursor
	if(LastActiveItem() == pID && !JustGotActive)
	{
		float w = TextRender()->TextWidth(0, FontSize, pDisplayStr, DispCursorPos, std::numeric_limits<float>::max());
		Textbox.x += w;
		Input()->SetEditingPosition(Textbox.x, Textbox.y + FontSize);
	}

	ClipDisable();

	return ReturnValue;
}

bool CUI::DoClearableEditBox(const void *pID, const void *pClearID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *pOffset, bool Hidden, int Corners, const SUIExEditBoxProperties &Properties)
{
	CUIRect EditBox;
	CUIRect ClearButton;
	pRect->VSplitRight(15.0f, &EditBox, &ClearButton);
	bool ReturnValue = DoEditBox(pID, &EditBox, pStr, StrSize, FontSize, pOffset, Hidden, Corners & ~IGraphics::CORNER_R, Properties);

	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT);
	ClearButton.Draw(ColorRGBA(1, 1, 1, 0.33f * ButtonColorMul(pClearID)), Corners & ~IGraphics::CORNER_L, 3.0f);

	SLabelProperties Props;
	Props.m_AlignVertically = 0;
	DoLabel(&ClearButton, "×", ClearButton.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
	TextRender()->SetRenderFlags(0);
	if(DoButtonLogic(pClearID, 0, &ClearButton))
	{
		pStr[0] = 0;
		SetActiveItem(pID);
		ReturnValue = true;
	}
	return ReturnValue;
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
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
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

	float ColorSlider;
	if(CheckActiveItem(pID))
		ColorSlider = 0.9f;
	else if(HotItem() == pID)
		ColorSlider = 1.0f;
	else
		ColorSlider = 0.8f;

	Handle.Draw(ColorRGBA(ColorSlider, ColorSlider, ColorSlider, 1.0f), IGraphics::CORNER_ALL, Handle.w / 2.0f);

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
			if(Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT))
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

		float ColorSlider;
		if(CheckActiveItem(pID))
			ColorSlider = 0.9f;
		else if(HotItem() == pID)
			ColorSlider = 1.0f;
		else
			ColorSlider = 0.8f;

		Handle.Draw(ColorRGBA(ColorSlider, ColorSlider, ColorSlider, 1.0f), IGraphics::CORNER_ALL, Handle.h / 2.0f);
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
	float VSplitVal = 10.0f + maximum(TextRender()->TextWidth(0, FontSize, aBuf, -1, std::numeric_limits<float>::max()), TextRender()->TextWidth(0, FontSize, aBufMax, -1, std::numeric_limits<float>::max()));

	CUIRect Label, ScrollBar;
	pRect->VSplitLeft(VSplitVal, &Label, &ScrollBar);
	DoLabel(&Label, aBuf, FontSize, TEXTALIGN_LEFT);

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
	DoLabel(&Label, aBuf, FontSize, TEXTALIGN_LEFT);

	Value = pScale->ToAbsolute(DoScrollbarH(pID, &ScrollBar, pScale->ToRelative(Value, 0, Max)), 0, Max);

	if(HotItem() != pID && !CheckActiveItem(pID) && MouseHovered(pRect) && MouseButtonClicked(0))
		Value = (Value + 1) % NumLabels;

	*pOption = clamp(Value, 0, Max);
}
