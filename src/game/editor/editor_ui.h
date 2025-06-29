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

// TODO: add and use constants for other special Checked-values in CEditor::GetButtonColor
namespace EditorButtonChecked {
[[maybe_unused]] static constexpr int DANGEROUS_ACTION = 9;
}

namespace EditorFontSizes {
[[maybe_unused]] static constexpr float MENU = 10.0f;
}

#endif
