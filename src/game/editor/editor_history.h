#ifndef GAME_EDITOR_EDITOR_HISTORY_H
#define GAME_EDITOR_EDITOR_HISTORY_H

#include "editor_action.h"

#include <deque>
#include <memory>
#include <vector>

class CEditorHistory
{
public:
	CEditorHistory()
	{
		m_pEditor = nullptr;
		m_IsBulk = false;
	}

	~CEditorHistory()
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

	CEditor *m_pEditor;
	std::deque<std::shared_ptr<IEditorAction>> m_vpUndoActions;
	std::deque<std::shared_ptr<IEditorAction>> m_vpRedoActions;

private:
	std::vector<std::shared_ptr<IEditorAction>> m_vpBulkActions;
	bool m_IsBulk;
};

#endif
