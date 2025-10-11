/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_game.h"

#include <game/editor/editor.h>
#include <game/editor/editor_actions.h>

CLayerGame::CLayerGame(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aLayerName, "Game");
	m_HasGame = true;
}

CLayerGame::~CLayerGame() = default;

CTile CLayerGame::GetTile(int x, int y) const
{
	if(m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
	{
		return CTile{TILE_THROUGH_CUT};
	}
	else
	{
		return CLayerTiles::GetTile(x, y);
	}
}

void CLayerGame::SetTile(int x, int y, CTile Tile)
{
	if(Tile.m_Index == TILE_THROUGH_CUT && m_pEditor->m_SelectEntitiesImage == "DDNet")
	{
		if(!m_pEditor->m_Map.m_pFrontLayer)
		{
			std::shared_ptr<CLayer> pLayerFront = std::make_shared<CLayerFront>(m_pEditor, m_Width, m_Height);
			m_pEditor->m_Map.MakeFrontLayer(pLayerFront);
			m_pEditor->m_Map.m_pGameGroup->AddLayer(pLayerFront);
			int GameGroupIndex = std::find(m_pEditor->m_Map.m_vpGroups.begin(), m_pEditor->m_Map.m_vpGroups.end(), m_pEditor->m_Map.m_pGameGroup) - m_pEditor->m_Map.m_vpGroups.begin();
			int LayerIndex = m_pEditor->m_Map.m_vpGroups[GameGroupIndex]->m_vpLayers.size() - 1;
			m_pEditor->m_EditorHistory.RecordAction(std::make_shared<CEditorActionAddLayer>(m_pEditor, GameGroupIndex, LayerIndex));
		}
		CLayerTiles::SetTile(x, y, CTile{TILE_NOHOOK});
		m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, CTile{TILE_THROUGH_CUT}); // NOLINT(bugprone-parent-virtual-call)
	}
	else
	{
		if(m_pEditor->m_SelectEntitiesImage == "DDNet" && m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
		{
			m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, CTile{TILE_AIR}); // NOLINT(bugprone-parent-virtual-call)
		}
		if(m_pEditor->IsAllowPlaceUnusedTiles() || IsValidGameTile(Tile.m_Index))
		{
			CLayerTiles::SetTile(x, y, Tile);
		}
		else
		{
			CLayerTiles::SetTile(x, y, CTile{TILE_AIR});
			ShowPreventUnusedTilesWarning();
		}
	}
}

bool CLayerGame::IsEmpty() const
{
	for(int y = 0; y < m_Height; y++)
	{
		for(int x = 0; x < m_Width; x++)
		{
			const int Index = GetTile(x, y).m_Index;
			if(Index == 0)
			{
				continue;
			}
			if(m_pEditor->IsAllowPlaceUnusedTiles() || IsValidGameTile(Index))
			{
				return false;
			}
		}
	}
	return true;
}

CUi::EPopupMenuFunctionResult CLayerGame::RenderProperties(CUIRect *pToolbox)
{
	const CUi::EPopupMenuFunctionResult Result = CLayerTiles::RenderProperties(pToolbox);
	m_Image = -1;
	return Result;
}

const char *CLayerGame::TypeName() const
{
	return "game";
}
