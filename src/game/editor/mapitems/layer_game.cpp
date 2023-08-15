/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layer_game.h"

#include <game/editor/editor.h>

#include "layer_front.h"
#include "layer_group.h"

CLayerGame::CLayerGame(int w, int h) :
	CLayerTiles(w, h)
{
	str_copy(m_aName, "Game");
	m_Game = 1;
}

CLayerGame::~CLayerGame() = default;

CTile CLayerGame::GetTile(int x, int y)
{
	if(Editor()->m_Map.m_pFrontLayer && Editor()->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
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
	if(Tile.m_Index == TILE_THROUGH_CUT && Editor()->m_SelectEntitiesImage == "DDNet")
	{
		if(!Editor()->m_Map.m_pFrontLayer)
		{
			std::shared_ptr<CLayer> pLayerFront = std::make_shared<CLayerFront>(m_Width, m_Height);
			Editor()->m_Map.MakeFrontLayer(pLayerFront);
			Editor()->m_Map.m_pGameGroup->AddLayer(pLayerFront);
		}
		CTile nohook = {TILE_NOHOOK};
		CLayerTiles::SetTile(x, y, nohook);
		CTile through_cut = {TILE_THROUGH_CUT};
		Editor()->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, through_cut); // NOLINT(bugprone-parent-virtual-call)
	}
	else
	{
		if(Editor()->m_SelectEntitiesImage == "DDNet" && Editor()->m_Map.m_pFrontLayer && Editor()->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT)
		{
			CTile air = {TILE_AIR};
			Editor()->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, air); // NOLINT(bugprone-parent-virtual-call)
		}
		if(Editor()->m_AllowPlaceUnusedTiles || IsValidGameTile(Tile.m_Index))
		{
			CLayerTiles::SetTile(x, y, Tile);
		}
		else
		{
			CTile air = {TILE_AIR};
			CLayerTiles::SetTile(x, y, air);
			if(!Editor()->m_PreventUnusedTilesWasWarned)
			{
				Editor()->m_PopupEventType = CEditor::POPEVENT_PREVENTUNUSEDTILES;
				Editor()->m_PopupEventActivated = true;
				Editor()->m_PreventUnusedTilesWasWarned = true;
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
