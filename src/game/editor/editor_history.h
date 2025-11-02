#ifndef GAME_EDITOR_EDITOR_HISTORY_H
#define GAME_EDITOR_EDITOR_HISTORY_H

#include "editor_action.h"

#include <game/editor/map_object.h>

#include <deque>
#include <memory>
#include <vector>

class CEditorHistory : public CMapObject
{
public:
	explicit CEditorHistory(CEditorMap *pMap) :
		CMapObject(pMap)
	{
	}

	~CEditorHistory() override
	{
		Clear();
	}

	void RecordAction(const std::shared_ptr<IEditorAction> &pAction);
	void RecordAction(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay);
	void Execute(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay = nullptr);

	bool Undo();
	bool Redo();

	void Clear();
	bool CanUndo() const { return !m_vpUndoActions.empty(); }
	bool CanRedo() const { return !m_vpRedoActions.empty(); }

	void BeginBulk();
	void EndBulk(const char *pDisplay = nullptr);
	void EndBulk(int DisplayToUse);

	std::deque<std::shared_ptr<IEditorAction>> m_vpUndoActions;
	std::deque<std::shared_ptr<IEditorAction>> m_vpRedoActions;

private:
	std::vector<std::shared_ptr<IEditorAction>> m_vpBulkActions;
	bool m_IsBulk = false;
};

#endif
