/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include "ui.h"
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

void CUIElement::Init(CUI *pUI)
{
	pUI->AddUIElement(this);
}

CUIElement::SUIElementRect::SUIElementRect() :
	m_UIRectQuadContainer(-1), m_UITextContainer(-1), m_X(-1), m_Y(-1), m_Width(-1), m_Height(-1)
{
	mem_zero(&m_Cursor, sizeof(m_Cursor));
	mem_zero(&m_TextColor, sizeof(m_TextColor));
	mem_zero(&m_TextOutlineColor, sizeof(m_TextOutlineColor));
}

/********************************************************
 UI
*********************************************************/

CUI::CUI()
{
	m_pHotItem = 0;
	m_pActiveItem = 0;
	m_pLastActiveItem = 0;
	m_pBecommingHotItem = 0;

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

CUIElement *CUI::GetNewUIElement()
{
	CUIElement *pNewEl = new CUIElement(this);

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
	}

	UIElement.Clear();
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
	m_pHotItem = m_pBecommingHotItem;
	if(m_pActiveItem)
		m_pHotItem = m_pActiveItem;
	m_pBecommingHotItem = 0;
	return 0;
}

int CUI::MouseInside(const CUIRect *r)
{
	if(m_MouseX >= r->x && m_MouseX < r->x + r->w && m_MouseY >= r->y && m_MouseY < r->y + r->h)
		return 1;
	return 0;
}

void CUI::ConvertMouseMove(float *x, float *y)
{
	float Fac = (float)(g_Config.m_UiMousesens) / g_Config.m_InpMousesens;
	*x = *x * Fac;
	*y = *y * Fac;
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

float CUI::PixelSize()
{
	return Screen()->w / Graphics()->ScreenWidth();
}

void CUI::SetScale(float s)
{
	g_Config.m_UiScale = (int)(s * 100.0f);
}

float CUI::Scale()
{
	return g_Config.m_UiScale / 100.0f;
}

float CUIRect::Scale() const
{
	return g_Config.m_UiScale / 100.0f;
}

void CUI::ClipEnable(const CUIRect *r)
{
	float XScale = Graphics()->ScreenWidth() / Screen()->w;
	float YScale = Graphics()->ScreenHeight() / Screen()->h;
	Graphics()->ClipEnable((int)(r->x * XScale), (int)(r->y * YScale), (int)(r->w * XScale), (int)(r->h * YScale));
}

void CUI::ClipDisable()
{
	Graphics()->ClipDisable();
}

void CUIRect::HSplitMid(CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;
	float Cut = r.h / 2;

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

void CUIRect::VSplitMid(CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;
	float Cut = r.w / 2;
	//	Cut *= Scale();

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

/*
int CUI::DoButton(const void *id, const char *text, int checked, const CUIRect *r, ui_draw_button_func draw_func, const void *extra)
{
	// logic
	int ret = 0;
	int inside = ui_MouseInside(r);
	static int button_used = 0;

	if(ui_ActiveItem() == id)
	{
		if(!ui_MouseButton(button_used))
		{
			if(inside && checked >= 0)
				ret = 1+button_used;
			ui_SetActiveItem(0);
		}
	}
	else if(ui_HotItem() == id)
	{
		if(ui_MouseButton(0))
		{
			ui_SetActiveItem(id);
			button_used = 0;
		}

		if(ui_MouseButton(1))
		{
			ui_SetActiveItem(id);
			button_used = 1;
		}
	}

	if(inside)
		ui_SetHotItem(id);

	if(draw_func)
		draw_func(id, text, checked, r, extra);
	return ret;
}*/

void CUI::DoLabel(const CUIRect *r, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically)
{
	float AlignedSize = 0;
	float MaxCharacterHeightInLine = 0;
	float tw = TextRender()->TextWidth(0, Size, pText, -1, MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	float AlignmentVert = r->y + (r->h - AlignedSize) / 2.f;
	if(AlignVertically == 0)
	{
		AlignmentVert = r->y + (r->h - AlignedSize) / 2.f - (AlignedSize - MaxCharacterHeightInLine) / 2.f;
	}
	if(Align == 0)
	{
		TextRender()->Text(0, r->x + (r->w - tw) / 2.f, AlignmentVert, Size, pText, MaxWidth);
	}
	else if(Align < 0)
	{
		TextRender()->Text(0, r->x, AlignmentVert, Size, pText, MaxWidth);
	}
	else if(Align > 0)
	{
		TextRender()->Text(0, r->x + r->w - tw, AlignmentVert, Size, pText, MaxWidth);
	}
}

void CUI::DoLabelScaled(const CUIRect *r, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically)
{
	DoLabel(r, pText, Size * Scale(), Align, MaxWidth, AlignVertically);
}

void CUI::DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically, bool StopAtEnd, int StrLen, CTextCursor *pReadCursor)
{
	float AlignedSize = 0;
	float MaxCharacterHeightInLine = 0;
	float tw = TextRender()->TextWidth(0, Size, pText, -1, MaxWidth, &AlignedSize, &MaxCharacterHeightInLine);
	float AlignmentVert = pRect->y + (pRect->h - AlignedSize) / 2.f;
	float AlignmentHori = 0;

	CTextCursor Cursor;

	int Flags = TEXTFLAG_RENDER | (StopAtEnd ? TEXTFLAG_STOP_AT_END : 0);

	if(AlignVertically == 0)
	{
		AlignmentVert = pRect->y + (pRect->h - AlignedSize) / 2.f - (AlignedSize - MaxCharacterHeightInLine) / 2.f;
	}
	if(Align == 0)
	{
		AlignmentHori = pRect->x + (pRect->w - tw) / 2.f;
	}
	else if(Align < 0)
	{
		AlignmentHori = pRect->x;
	}
	else if(Align > 0)
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
	Cursor.m_LineWidth = MaxWidth;

	RectEl.m_UITextContainer = TextRender()->CreateTextContainer(&Cursor, pText, StrLen);
	RectEl.m_Cursor = Cursor;
	RectEl.m_TextColor = TextRender()->GetTextColor();
	RectEl.m_TextOutlineColor = TextRender()->GetTextOutlineColor();
}

void CUI::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, float MaxWidth, int AlignVertically, bool StopAtEnd, int StrLen, CTextCursor *pReadCursor)
{
	bool NeedsRecreate = false;
	bool ColorChanged = RectEl.m_TextColor != TextRender()->GetTextColor() || RectEl.m_TextOutlineColor != TextRender()->GetTextOutlineColor();
	if(RectEl.m_UITextContainer == -1 || RectEl.m_X != pRect->x || RectEl.m_Y != pRect->y || RectEl.m_Width != pRect->w || RectEl.m_Height != pRect->h || ColorChanged)
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

		RectEl.m_X = pRect->x;
		RectEl.m_Y = pRect->y;
		RectEl.m_Width = pRect->w;
		RectEl.m_Height = pRect->h;

		if(StrLen > 0)
			RectEl.m_Text = std::string(pText, StrLen);
		else if(StrLen < 0)
			RectEl.m_Text = pText;
		else
			RectEl.m_Text.clear();

		DoLabel(RectEl, pRect, pText, Size, Align, MaxWidth, AlignVertically, StopAtEnd, StrLen, pReadCursor);
	}

	STextRenderColor ColorText(TextRender()->DefaultTextColor());
	STextRenderColor ColorTextOutline(TextRender()->DefaultTextOutlineColor());
	if(RectEl.m_UITextContainer != -1)
		TextRender()->RenderTextContainer(RectEl.m_UITextContainer, &ColorText, &ColorTextOutline);
}
