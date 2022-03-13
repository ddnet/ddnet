/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include "ui.h"
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <limits>

void CUIElement::Init(CUI *pUI, int RequestedRectCount)
{
	pUI->AddUIElement(this);
	if(RequestedRectCount > 0)
		m_UIRects.resize(RequestedRectCount);
}

void CUIElement::InitRects(int RequestedRectCount)
{
	dbg_assert(m_UIRects.empty(), "UI rects can only be initialized once, create another ui element instead.");
	m_UIRects.resize(RequestedRectCount);
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
	m_TextColor.Set(-1, -1, -1, -1);
	m_TextOutlineColor.Set(-1, -1, -1, -1);
	m_QuadColor = ColorRGBA(-1, -1, -1, -1);
}

/********************************************************
 UI
*********************************************************/

float CUI::ms_FontmodHeight = 0.8f;

void CUI::Init(class IGraphics *pGraphics, class ITextRender *pTextRender)
{
	m_pGraphics = pGraphics;
	m_pTextRender = pTextRender;
}

CUI::CUI()
{
	m_pHotItem = 0;
	m_pActiveItem = 0;
	m_pLastActiveItem = 0;
	m_pBecomingHotItem = 0;

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
	for(CUIElement *&pEl : m_OwnUIElements)
	{
		delete pEl;
	}
	m_OwnUIElements.clear();
}

CUIElement *CUI::GetNewUIElement(int RequestedRectCount)
{
	CUIElement *pNewEl = new CUIElement(this, RequestedRectCount);

	m_OwnUIElements.push_back(pNewEl);

	return pNewEl;
}

void CUI::AddUIElement(CUIElement *pElement)
{
	m_UIElements.push_back(pElement);
}

void CUI::ResetUIElement(CUIElement &UIElement)
{
	for(CUIElement::SUIElementRect &Rect : UIElement.m_UIRects)
	{
		if(Rect.m_UIRectQuadContainer != -1)
			Graphics()->DeleteQuadContainer(Rect.m_UIRectQuadContainer);
		Rect.m_UIRectQuadContainer = -1;
		if(Rect.m_UITextContainer != -1)
			TextRender()->DeleteTextContainer(Rect.m_UITextContainer);
		Rect.m_UITextContainer = -1;

		Rect.Reset();
	}
}

void CUI::OnElementsReset()
{
	for(CUIElement *pEl : m_UIElements)
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

int CUI::Update(float Mx, float My, float Mwx, float Mwy, int Buttons)
{
	m_MouseDeltaX = Mx - m_MouseX;
	m_MouseDeltaY = My - m_MouseY;
	m_MouseX = Mx;
	m_MouseY = My;
	m_MouseWorldX = Mwx;
	m_MouseWorldY = Mwy;
	m_LastMouseButtons = m_MouseButtons;
	m_MouseButtons = Buttons;
	m_pHotItem = m_pBecomingHotItem;
	if(m_pActiveItem)
		m_pHotItem = m_pActiveItem;
	m_pBecomingHotItem = 0;
	return 0;
}

bool CUI::MouseInside(const CUIRect *pRect) const
{
	return pRect->Inside(m_MouseX, m_MouseY);
}

void CUI::ConvertMouseMove(float *x, float *y) const
{
	float Fac = (float)(g_Config.m_UiMousesens) / g_Config.m_InpMousesens;
	*x = *x * Fac;
	*y = *y * Fac;
}

float CUI::ButtonColorMul(const void *pID)
{
	if(ActiveItem() == pID)
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

void CUI::SetScale(float s)
{
	g_Config.m_UiScale = (int)(s * 100.0f);
}

float CUI::Scale() const
{
	return g_Config.m_UiScale / 100.0f;
}

float CUIRect::Scale() const
{
	return g_Config.m_UiScale / 100.0f;
}

void CUI::ClipEnable(const CUIRect *pRect)
{
	float XScale = Graphics()->ScreenWidth() / Screen()->w;
	float YScale = Graphics()->ScreenHeight() / Screen()->h;
	Graphics()->ClipEnable((int)(pRect->x * XScale), (int)(pRect->y * YScale), (int)(pRect->w * XScale), (int)(pRect->h * YScale));
}

void CUI::ClipDisable()
{
	Graphics()->ClipDisable();
}

void CUIRect::HSplitMid(CUIRect *pTop, CUIRect *pBottom, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.h / 2;
	const float HalfSpacing = Spacing / 2;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut - HalfSpacing;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut + HalfSpacing;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut - HalfSpacing;
	}
}

void CUIRect::HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut;
	}
}

void CUIRect::HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = r.h - Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + r.h - Cut;
		pBottom->w = r.w;
		pBottom->h = Cut;
	}
}

void CUIRect::VSplitMid(CUIRect *pLeft, CUIRect *pRight, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.w / 2;
	const float HalfSpacing = Spacing / 2;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut - HalfSpacing;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut + HalfSpacing;
		pRight->y = r.y;
		pRight->w = r.w - Cut - HalfSpacing;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut;
		pRight->y = r.y;
		pRight->w = r.w - Cut;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	Cut *= Scale();

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = r.w - Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + r.w - Cut;
		pRight->y = r.y;
		pRight->w = Cut;
		pRight->h = r.h;
	}
}

void CUIRect::Margin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x + Cut;
	pOtherRect->y = r.y + Cut;
	pOtherRect->w = r.w - 2 * Cut;
	pOtherRect->h = r.h - 2 * Cut;
}

void CUIRect::VMargin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x + Cut;
	pOtherRect->y = r.y;
	pOtherRect->w = r.w - 2 * Cut;
	pOtherRect->h = r.h;
}

void CUIRect::HMargin(float Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;
	Cut *= Scale();

	pOtherRect->x = r.x;
	pOtherRect->y = r.y + Cut;
	pOtherRect->w = r.w;
	pOtherRect->h = r.h - 2 * Cut;
}

bool CUIRect::Inside(float x, float y) const
{
	return x >= this->x && x < this->x + this->w && y >= this->y && y < this->y + this->h;
}

int CUI::DoButtonLogic(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButtonLogic(pID, Checked, pRect);
}

int CUI::DoButtonLogic(const void *pID, int Checked, const CUIRect *pRect)
{
	// logic
	int ReturnValue = 0;
	int Inside = MouseInside(pRect);
	static int ButtonUsed = 0;

	if(ActiveItem() == pID)
	{
		if(!MouseButton(ButtonUsed))
		{
			if(Inside && Checked >= 0)
				ReturnValue = 1 + ButtonUsed;
			SetActiveItem(0);
		}
	}
	else if(HotItem() == pID)
	{
		for(int i = 0; i < 3; ++i)
		{
			if(MouseButton(i))
			{
				SetActiveItem(pID);
				ButtonUsed = i;
			}
		}
	}

	if(Inside)
		SetHotItem(pID);

	return ReturnValue;
}

int CUI::DoPickerLogic(const void *pID, const CUIRect *pRect, float *pX, float *pY)
{
	int Inside = MouseInside(pRect);

	if(Inside)
		SetHotItem(pID);

	if(HotItem() == pID && MouseButtonClicked(0))
		SetActiveItem(pID);

	if(ActiveItem() == pID && !MouseButton(0))
		SetActiveItem(0);

	if(ActiveItem() != pID)
		return 0;

	if(pX)
		*pX = clamp(m_MouseX - pRect->x, 0.0f, pRect->w) / Scale();
	if(pY)
		*pY = clamp(m_MouseY - pRect->y, 0.0f, pRect->h) / Scale();

	return 1;
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

void CUI::DoLabelScaled(const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)
{
	DoLabel(pRect, pText, Size * Scale(), Align, LabelProps);
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
	RectEl.m_UITextContainer = TextRender()->CreateTextContainer(&Cursor, pText, StrLen);
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
		if(RectEl.m_UITextContainer != -1)
			TextRender()->DeleteTextContainer(RectEl.m_UITextContainer);
		RectEl.m_UITextContainer = -1;

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

	STextRenderColor ColorText(RectEl.m_TextColor);
	STextRenderColor ColorTextOutline(RectEl.m_TextOutlineColor);
	if(RectEl.m_UITextContainer != -1)
		TextRender()->RenderTextContainer(RectEl.m_UITextContainer, &ColorText, &ColorTextOutline);
}

void CUI::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically, bool StopAtEnd, int StrLen, CTextCursor *pReadCursor)
{
	DoLabelStreamed(RectEl, pRect->x, pRect->y, pRect->w, pRect->h, pText, Size, Align, MaxWidth, AlignVertically, StopAtEnd, StrLen, pReadCursor);
}
