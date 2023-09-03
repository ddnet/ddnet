/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_UI_LISTBOX_H
#define GAME_CLIENT_UI_LISTBOX_H

#include "ui_scrollregion.h"

struct CListboxItem
{
	bool m_Visible;
	bool m_Selected;
	CUIRect m_Rect;
};

// Instances of CListBox must be static, as member addresses are used as UI item IDs
class CListBox : private CUIElementBase
{
private:
	CUIRect m_ListBoxView;
	CUIRect m_RowView;
	float m_ListBoxRowHeight;
	int m_ListBoxItemIndex;
	int m_ListBoxSelectedIndex;
	int m_ListBoxNewSelected;
	int m_ListBoxNewSelOffset;
	bool m_ListBoxUpdateScroll;
	bool m_ListBoxDoneEvents;
	int m_ListBoxNumItems;
	int m_ListBoxItemsPerRow;
	bool m_ListBoxItemSelected;
	bool m_ListBoxItemActivated;
	bool m_ScrollbarIsShown;
	const char *m_pBottomText;
	float m_FooterHeight;
	float m_AutoSpacing;
	CScrollRegion m_ScrollRegion;
	vec2 m_ScrollOffset;
	float m_FilterOffset;
	int m_BackgroundCorners;
	bool m_HasHeader;
	bool m_Active;

protected:
	CListboxItem DoNextRow();

public:
	CListBox();

	void DoBegin(const CUIRect *pRect);
	void DoHeader(const CUIRect *pRect, const char *pTitle, float HeaderHeight = 20.0f, float Spacing = 2.0f);
	void DoAutoSpacing(float Spacing = 20.0f) { m_AutoSpacing = Spacing; }
	void DoSpacing(float Spacing = 20.0f);
	void DoFooter(const char *pBottomText, float FooterHeight = 20.0f); // call before DoStart to create a footer
	void DoStart(float RowHeight, int NumItems, int ItemsPerRow, int RowsPerScroll, int SelectedIndex, const CUIRect *pRect = nullptr, bool Background = true, int BackgroundCorners = IGraphics::CORNER_ALL, bool ForceShowScrollbar = false);
	void ScrollToSelected() { m_ListBoxUpdateScroll = true; }
	CListboxItem DoNextItem(const void *pID, bool Selected = false);
	CListboxItem DoSubheader();
	int DoEnd();

	// Active state must be set before calling DoStart.
	bool Active() const { return m_Active; }
	void SetActive(bool Active) { m_Active = Active; }

	bool WasItemSelected() const { return m_ListBoxItemSelected; }
	bool WasItemActivated() const { return m_ListBoxItemActivated; }

	bool ScrollbarIsShown() const { return m_ScrollbarIsShown; }
	float ScrollbarWidth() const { return ScrollbarIsShown() ? ScrollbarWidthMax() : 0.0f; }
	float ScrollbarWidthMax() const { return 20.0f; }
};

#endif
