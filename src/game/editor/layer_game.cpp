/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "editor.h"

CLayerGame::CLayerGame(int w, int h) :
	CLayerTiles(w, h)
{
	str_copy(m_aName, "Game");
	m_Game = 1;
}

CLayerGame::~CLayerGame() = default;

CTile CLayerGame::GetTile(int x, int y)
{
	if(m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
	{
		CTile through_cut = {TILE_THROUGH_CUT};
		return through_cut;
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
			std::shared_ptr<CLayer> pLayerFront = std::make_shared<CLayerFront>(m_Width, m_Height);
			m_pEditor->m_Map.MakeFrontLayer(pLayerFront);
			m_pEditor->m_Map.m_pGameGroup->AddLayer(pLayerFront);
		}
		CTile nohook = {TILE_NOHOOK};
		CLayerTiles::SetTile(x, y, nohook);
		CTile through_cut = {TILE_THROUGH_CUT};
		m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, through_cut); // NOLINT(bugprone-parent-virtual-call)
	}
	else
	{
		if(m_pEditor->m_SelectEntitiesImage == "DDNet" && m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
		{
			CTile air = {TILE_AIR};
			m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, air); // NOLINT(bugprone-parent-virtual-call)
		}
		if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidGameTile(Tile.m_Index))
		{
			CLayerTiles::SetTile(x, y, Tile);
		}
		else
		{
			CTile air = {TILE_AIR};
			CLayerTiles::SetTile(x, y, air);
			if(!m_pEditor->m_PreventUnusedTilesWasWarned)
			{
				m_pEditor->m_PopupEventType = CEditor::POPEVENT_PREVENTUNUSEDTILES;
				m_pEditor->m_PopupEventActivated = true;
				m_pEditor->m_PreventUnusedTilesWasWarned = true;
			}
		}
	}
}

CUI::EPopupMenuFunctionResult CLayerGame::RenderProperties(CUIRect *pToolbox)
{
	const CUI::EPopupMenuFunctionResult Result = CLayerTiles::RenderProperties(pToolbox);
	m_Image = -1;
	return Result;
}
