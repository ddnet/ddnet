/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/shared/config.h>

#include <game/localization.h>

#include "ui_listbox.h"

CListBox::CListBox()
{
	m_ScrollOffset = vec2(0.0f, 0.0f);
	m_ListBoxUpdateScroll = false;
	m_FilterOffset = 0.0f;
	m_HasHeader = false;
	m_AutoSpacing = 0.0f;
	m_ScrollbarIsShown = false;
	m_Active = true;
}

void CListBox::DoBegin(const CUIRect *pRect)
{
	// setup the variables
	m_ListBoxView = *pRect;
}

void CListBox::DoHeader(const CUIRect *pRect, const char *pTitle, float HeaderHeight, float Spacing)
{
	CUIRect View = *pRect;
	CUIRect Header;

	// background
	View.HSplitTop(HeaderHeight + Spacing, &Header, 0);
	Header.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), m_BackgroundCorners & IGraphics::CORNER_T, 5.0f);

	// draw header
	View.HSplitTop(HeaderHeight, &Header, &View);
	UI()->DoLabel(&Header, pTitle, Header.h * CUI::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);

	View.HSplitTop(Spacing, &Header, &View);

	// setup the variables
	m_ListBoxView = View;
	m_HasHeader = true;
}

void CListBox::DoSpacing(float Spacing)
{
	CUIRect View = m_ListBoxView;
	View.HSplitTop(Spacing, 0, &View);
	m_ListBoxView = View;
}

void CListBox::DoFooter(const char *pBottomText, float FooterHeight)
{
	m_pBottomText = pBottomText;
	m_FooterHeight = FooterHeight;
}

void CListBox::DoStart(float RowHeight, int NumItems, int ItemsPerRow, int RowsPerScroll, int SelectedIndex, const CUIRect *pRect, bool Background, int BackgroundCorners, bool ForceShowScrollbar)
{
	CUIRect View;
	if(pRect)
		View = *pRect;
	else
		View = m_ListBoxView;

	// background
	m_BackgroundCorners = BackgroundCorners;
	if(Background)
		View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), m_BackgroundCorners & (m_HasHeader ? IGraphics::CORNER_B : IGraphics::CORNER_ALL), 5.0f);

	// draw footers
	if(m_pBottomText)
	{
		CUIRect Footer;
		View.HSplitBottom(m_FooterHeight, &View, &Footer);
		Footer.VSplitLeft(10.0f, 0, &Footer);
		UI()->DoLabel(&Footer, m_pBottomText, Footer.h * CUI::ms_FontmodHeight * 0.8f, TEXTALIGN_MC);
	}

	// setup the variables
	m_ListBoxView = View;
	m_RowView = {};
	m_ListBoxSelectedIndex = SelectedIndex;
	m_ListBoxNewSelected = SelectedIndex;
	m_ListBoxNewSelOffset = 0;
	m_ListBoxItemIndex = 0;
	m_ListBoxRowHeight = RowHeight;
	m_ListBoxNumItems = NumItems;
	m_ListBoxItemsPerRow = ItemsPerRow;
	m_ListBoxDoneEvents = false;
	m_ListBoxItemActivated = false;
	m_ListBoxItemSelected = false;

	// handle input
	if(m_Active && !Input()->ModifierIsPressed() && !Input()->ShiftIsPressed() && !Input()->AltIsPressed())
	{
		if(UI()->ConsumeHotkey(CUI::HOTKEY_DOWN))
			m_ListBoxNewSelOffset += 1;
		else if(UI()->ConsumeHotkey(CUI::HOTKEY_UP))
			m_ListBoxNewSelOffset -= 1;
		else if(UI()->ConsumeHotkey(CUI::HOTKEY_PAGE_UP))
			m_ListBoxNewSelOffset = -ItemsPerRow * RowsPerScroll * 4;
		else if(UI()->ConsumeHotkey(CUI::HOTKEY_PAGE_DOWN))
			m_ListBoxNewSelOffset = ItemsPerRow * RowsPerScroll * 4;
		else if(UI()->ConsumeHotkey(CUI::HOTKEY_HOME))
			m_ListBoxNewSelOffset = 1 - m_ListBoxNumItems;
		else if(UI()->ConsumeHotkey(CUI::HOTKEY_END))
			m_ListBoxNewSelOffset = m_ListBoxNumItems - 1;
	}

	// setup the scrollbar
	m_ScrollOffset = vec2(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_Active = m_Active;
	ScrollParams.m_ScrollbarWidth = ScrollbarWidthMax();
	ScrollParams.m_ScrollUnit = (m_ListBoxRowHeight + m_AutoSpacing) * RowsPerScroll;
	ScrollParams.m_Flags = ForceShowScrollbar ? CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH : 0;
	m_ScrollRegion.Begin(&m_ListBoxView, &m_ScrollOffset, &ScrollParams);
	m_ListBoxView.y += m_ScrollOffset.y;
}

CListboxItem CListBox::DoNextRow()
{
	CListboxItem Item = {};

	if(m_ListBoxItemIndex % m_ListBoxItemsPerRow == 0)
		m_ListBoxView.HSplitTop(m_ListBoxRowHeight, &m_RowView, &m_ListBoxView);
	m_ScrollRegion.AddRect(m_RowView);
	if(m_ListBoxUpdateScroll && m_ListBoxSelectedIndex == m_ListBoxItemIndex)
	{
		m_ScrollRegion.ScrollHere(CScrollRegion::SCROLLHERE_KEEP_IN_VIEW);
		m_ListBoxUpdateScroll = false;
	}

	m_RowView.VSplitLeft(m_RowView.w / (m_ListBoxItemsPerRow - m_ListBoxItemIndex % m_ListBoxItemsPerRow), &Item.m_Rect, &m_RowView);

	Item.m_Selected = m_ListBoxSelectedIndex == m_ListBoxItemIndex;
	Item.m_Visible = !m_ScrollRegion.IsRectClipped(Item.m_Rect);

	m_ListBoxItemIndex++;
	return Item;
}

CListboxItem CListBox::DoNextItem(const void *pId, bool Selected)
{
	if(m_AutoSpacing > 0.0f && m_ListBoxItemIndex > 0)
		DoSpacing(m_AutoSpacing);

	const int ThisItemIndex = m_ListBoxItemIndex;
	if(Selected)
	{
		if(m_ListBoxSelectedIndex == m_ListBoxNewSelected)
			m_ListBoxNewSelected = ThisItemIndex;
		m_ListBoxSelectedIndex = ThisItemIndex;
	}

	CListboxItem Item = DoNextRow();
	bool ItemClicked = false;

	if(Item.m_Visible && UI()->DoButtonLogic(pId, 0, &Item.m_Rect))
	{
		ItemClicked = true;
		m_ListBoxNewSelected = ThisItemIndex;
		m_ListBoxItemSelected = true;
		m_Active = true;
	}
	else
		ItemClicked = false;

	// process input, regard selected index
	if(m_ListBoxSelectedIndex == ThisItemIndex)
	{
		if(m_Active && !m_ListBoxDoneEvents)
		{
			m_ListBoxDoneEvents = true;

			if(UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || (ItemClicked && Input()->MouseDoubleClick()))
			{
				m_ListBoxItemActivated = true;
				UI()->SetActiveItem(nullptr);
			}
		}

		Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, m_Active ? 0.5f : 0.33f), IGraphics::CORNER_ALL, 5.0f);
	}
	if(UI()->HotItem() == pId && !m_ScrollRegion.IsAnimating())
	{
		Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f), IGraphics::CORNER_ALL, 5.0f);
	}

	return Item;
}

CListboxItem CListBox::DoSubheader()
{
	CListboxItem Item = DoNextRow();
	Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.2f), IGraphics::CORNER_NONE, 0.0f);
	return Item;
}

int CListBox::DoEnd()
{
	m_ScrollRegion.End();
	m_Active |= m_ScrollRegion.Params().m_Active;

	m_ScrollbarIsShown = m_ScrollRegion.IsScrollbarShown();
	if(m_ListBoxNewSelOffset != 0 && m_ListBoxNumItems > 0 && m_ListBoxSelectedIndex == m_ListBoxNewSelected)
	{
		m_ListBoxNewSelected = clamp((m_ListBoxNewSelected == -1 ? 0 : m_ListBoxNewSelected) + m_ListBoxNewSelOffset, 0, m_ListBoxNumItems - 1);
		ScrollToSelected();
	}
	return m_ListBoxNewSelected;
}
