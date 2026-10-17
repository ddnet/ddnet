#ifndef GAME_EDITOR_EDITOR_HISTORY_H
#define GAME_EDITOR_EDITOR_HISTORY_H

#include <game/client/ui_listbox.h>
#include <game/editor/map_object.h>

#include <deque>
#include <memory>
#include <vector>

class CEditorHistoryUiState
{
public:
	int m_Category = -1;
	CListBox m_ListBox;
	const char m_aCategoryButtonIds[4] = {0};
	const char m_DeleteButtonId = 0;
	const char m_EntryLimitId = 0;
	const char m_MemoryLimitId = 0;
};

#endif
