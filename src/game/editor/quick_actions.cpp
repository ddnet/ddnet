#include "editor.h"

#include "editor_actions.h"

void CEditor::QuickActionAddGroup()
{
	m_Map.NewGroup();
	m_SelectedGroup = m_Map.m_vpGroups.size() - 1;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(this, m_SelectedGroup, false));
}

void CEditor::QuickActionRefocus()
{
	MapView()->Focus();
}

void CEditor::QuickActionProof()
{
	MapView()->ProofMode()->Toggle();
}

