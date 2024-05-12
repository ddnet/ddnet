#include "base/system.h"
#include "editor.h"

#include "editor_actions.h"

void CEditor::ButtonAddGroup(void *pEditor)
{
	CEditor *pSelf = (CEditor*)pEditor;

	pSelf->m_Map.NewGroup();
	pSelf->m_SelectedGroup = pSelf->m_Map.m_vpGroups.size() - 1;
	pSelf->m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(pSelf, pSelf->m_SelectedGroup, false));

	dbg_msg("editor", "callback baby");
}
