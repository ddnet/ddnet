#include <game/mapitems.h>

#include "editor.h"

#include "editor_actions.h"

void CEditor::FillGameTiles(EGameTileOp FillTile) const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		pTileLayer->FillGameTiles(FillTile);
}

bool CEditor::CanFillGameTiles() const
{
	std::shared_ptr<CLayerTiles> pTileLayer = std::static_pointer_cast<CLayerTiles>(GetSelectedLayerType(0, LAYERTYPE_TILES));
	if(pTileLayer)
		return pTileLayer->CanFillGameTiles();
	return false;
}

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

bool CEditor::IsNonGameTileLayerSelected() const
{
	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
	if(!pLayer)
		return false;
	if(pLayer->m_Type != LAYERTYPE_TILES)
		return false;
	if(
		pLayer == m_Map.m_pGameLayer ||
		pLayer == m_Map.m_pFrontLayer ||
		pLayer == m_Map.m_pSwitchLayer ||
		pLayer == m_Map.m_pTeleLayer ||
		pLayer == m_Map.m_pSpeedupLayer ||
		pLayer == m_Map.m_pTuneLayer)
		return false;

	return true;
}

void CEditor::LayerSelectImage()
{
	if(!IsNonGameTileLayerSelected())
		return;

	std::shared_ptr<CLayer> pLayer = GetSelectedLayer(0);
	std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);

	static SLayerPopupContext s_LayerPopupContext = {};
	s_LayerPopupContext.m_pEditor = this;
	Ui()->DoPopupMenu(&s_LayerPopupContext, Ui()->MouseX(), Ui()->MouseY(), 120, 270, &s_LayerPopupContext, PopupLayer);
	PopupSelectImageInvoke(pTiles->m_Image, Ui()->MouseX(), Ui()->MouseY());
}
