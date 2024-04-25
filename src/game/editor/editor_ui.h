#ifndef GAME_EDITOR_EDITOR_UI_H
#define GAME_EDITOR_EDITOR_UI_H

#include <game/client/ui_listbox.h>

struct SEditBoxDropdownContext
{
	bool m_Visible = false;
	int m_Selected = -1;
	CListBox m_ListBox;
	bool m_ShortcutUsed = false;
	bool m_DidBecomeVisible = false;
	bool m_MousePressedInside = false;
	bool m_ShouldHide = false;
	int m_Width = 0;
};

namespace EditorFontSizes {
MAYBE_UNUSED static constexpr float MENU = 10.0f;
}

#endif
