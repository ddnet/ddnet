#include <engine/shared/config.h>

#include "editor.h"
#include "editor_actions.h"
#include "editor_history.h"

void CEditorHistory::RecordAction(const std::shared_ptr<IEditorAction> &pAction)
{
	RecordAction(pAction, nullptr);
}

void CEditorHistory::Execute(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay)
{
	pAction->Redo();
	RecordAction(pAction, pDisplay);
}

void CEditorHistory::RecordAction(const std::shared_ptr<IEditorAction> &pAction, const char *pDisplay)
{
	if(m_IsBulk)
	{
		m_vpBulkActions.push_back(pAction);
		return;
	}

	m_vpRedoActions.clear();

	if((int)m_vpUndoActions.size() >= g_Config.m_ClEditorMaxHistory)
	{
		m_vpUndoActions.pop_front();
	}

	if(pDisplay == nullptr)
		m_vpUndoActions.emplace_back(pAction);
	else
		m_vpUndoActions.emplace_back(std::make_shared<CEditorActionBulk>(m_pEditor, std::vector<std::shared_ptr<IEditorAction>>{pAction}, pDisplay));
}

bool CEditorHistory::Undo()
{
	if(m_vpUndoActions.empty())
		return false;

	auto pLastAction = m_vpUndoActions.back();
	m_vpUndoActions.pop_back();

	pLastAction->Undo();

	m_vpRedoActions.emplace_back(pLastAction);
	return true;
}

bool CEditorHistory::Redo()
{
	if(m_vpRedoActions.empty())
		return false;

	auto pLastAction = m_vpRedoActions.back();
	m_vpRedoActions.pop_back();

	pLastAction->Redo();

	m_vpUndoActions.emplace_back(pLastAction);
	return true;
}

void CEditorHistory::Clear()
{
	m_vpUndoActions.clear();
	m_vpRedoActions.clear();
}

void CEditorHistory::BeginBulk()
{
	m_IsBulk = true;
	m_vpBulkActions.clear();
}

void CEditorHistory::EndBulk(const char *pDisplay)
{
	if(!m_IsBulk)
		return;
	m_IsBulk = false;

	// Record bulk action
	if(!m_vpBulkActions.empty())
		RecordAction(std::make_shared<CEditorActionBulk>(m_pEditor, m_vpBulkActions, pDisplay, true));

	m_vpBulkActions.clear();
}

void CEditorHistory::EndBulk(int DisplayToUse)
{
	EndBulk((DisplayToUse < 0 || DisplayToUse >= (int)m_vpBulkActions.size()) ? nullptr : m_vpBulkActions[DisplayToUse]->DisplayText());
}
