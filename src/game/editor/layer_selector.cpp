#include <engine/shared/config.h>

#include "editor.h"

#include "layer_selector.h"

void CLayerSelector::OnInit(CEditor *pEditor)
{
	CEditorComponent::OnInit(pEditor);

	m_SelectionOffset = 0;
}

bool CLayerSelector::SelectByTile()
{
	// ctrl+right click a map index to select the layer that has a tile there
	if(Ui()->HotItem() != &Editor()->m_MapEditorId)
		return false;
	if(!Input()->ModifierIsPressed() || !Ui()->MouseButtonClicked(1))
		return false;
	if(!g_Config.m_EdLayerSelector)
		return false;

	int MatchedGroup = -1;
	int MatchedLayer = -1;
	int Matches = 0;
	bool IsFound = false;
	for(auto HoverTile : Editor()->HoverTiles())
	{
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
		Editor()->SelectLayer(MatchedLayer, MatchedGroup);
		return true;
	}
	return false;
}
