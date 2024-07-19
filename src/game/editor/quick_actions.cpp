#include "editor.h"

#include "editor_actions.h"

void CEditor::AddGroup()
{
	m_Map.NewGroup();
	m_SelectedGroup = m_Map.m_vpGroups.size() - 1;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionGroup>(this, m_SelectedGroup, false));
}
void CEditor::AddTileLayer()
{
	std::shared_ptr<CLayer> pTileLayer = std::make_shared<CLayerTiles>(this, m_Map.m_pGameLayer->m_Width, m_Map.m_pGameLayer->m_Height);
	pTileLayer->m_pEditor = this;
	m_Map.m_vpGroups[m_SelectedGroup]->AddLayer(pTileLayer);
	int LayerIndex = m_Map.m_vpGroups[m_SelectedGroup]->m_vpLayers.size() - 1;
	SelectLayer(LayerIndex);
	m_Map.m_vpGroups[m_SelectedGroup]->m_Collapse = false;
	m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(this, m_SelectedGroup, LayerIndex));
}
