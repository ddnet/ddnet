#include "layer_selector.h"

#include "editor.h"

#include <engine/shared/config.h>

void CLayerSelector::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);

	m_SelectionOffset = 0;
}

bool CLayerSelector::SelectByTile()
{
	// ctrl+right click a map index to select the layer that has a tile there
	if(Ui()->HotItem() != Editor()->MapView())
		return false;
	if(!Input()->ModifierIsPressed() || !Ui()->MouseButtonClicked(1))
		return false;
	if(!g_Config.m_EdLayerSelector)
		return false;

	int MatchedGroup = -1;
	int MatchedLayer = -1;
	int Matches = 0;
	bool IsFound = false;
	for(const auto &HoverTile : m_vHoverTiles)
	{
		if(!Map()->m_vpGroups[HoverTile.m_Group]->m_Visible ||
			!Map()->m_vpGroups[HoverTile.m_Group]->m_vpLayers[HoverTile.m_Layer]->m_Visible)
			continue;

		if(MatchedGroup == -1)
		{
			MatchedGroup = HoverTile.m_Group;
			MatchedLayer = HoverTile.m_Layer;
		}
		if(++Matches > m_SelectionOffset)
		{
			m_SelectionOffset++;
			MatchedGroup = HoverTile.m_Group;
			MatchedLayer = HoverTile.m_Layer;
			IsFound = true;
			break;
		}
	}
	if(MatchedGroup != -1 && MatchedLayer != -1)
	{
		if(!IsFound)
			m_SelectionOffset = 1;
		Map()->SelectLayer(MatchedLayer, MatchedGroup);
		return true;
	}
	return false;
}

void CLayerSelector::UpdateHoveredTiles()
{
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();

	m_vHoverTiles.clear();
	for(size_t g = 0; g < Map()->m_vpGroups.size(); g++)
	{
		const std::shared_ptr<CLayerGroup> pGroup = Map()->m_vpGroups[g];
		for(size_t l = 0; l < pGroup->m_vpLayers.size(); l++)
		{
			const std::shared_ptr<CLayer> pLayer = pGroup->m_vpLayers[l];
			int LayerType = pLayer->m_Type;
			if(LayerType != LAYERTYPE_TILES &&
				LayerType != LAYERTYPE_FRONT &&
				LayerType != LAYERTYPE_TELE &&
				LayerType != LAYERTYPE_SPEEDUP &&
				LayerType != LAYERTYPE_SWITCH &&
				LayerType != LAYERTYPE_TUNE)
				continue;

			std::shared_ptr<CLayerTiles> pTiles = std::static_pointer_cast<CLayerTiles>(pLayer);
			pGroup->MapScreen();
			float aPoints[4];
			pGroup->Mapping(aPoints);
			float WorldWidth = aPoints[2] - aPoints[0];
			float WorldHeight = aPoints[3] - aPoints[1];
			CUIRect Rect;
			Rect.x = aPoints[0] + WorldWidth * (UpdatedMousePos.x / Graphics()->WindowWidth());
			Rect.y = aPoints[1] + WorldHeight * (UpdatedMousePos.y / Graphics()->WindowHeight());
			Rect.w = 0;
			Rect.h = 0;
			CIntRect r;
			pTiles->Convert(Rect, &r);
			pTiles->Clamp(&r);
			int x = r.x;
			int y = r.y;

			if(x < 0 || x >= pTiles->m_Width)
				continue;
			if(y < 0 || y >= pTiles->m_Height)
				continue;

			if(pTiles->GetTile(x, y).m_Index > 0)
				m_vHoverTiles.emplace_back(g, l);
		}
	}
	Ui()->MapScreen();
}
