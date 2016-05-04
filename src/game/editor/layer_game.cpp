/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "editor.h"


CLayerGame::CLayerGame(int w, int h)
: CLayerTiles(w, h)
{
	str_copy(m_aName, "Game", sizeof(m_aName));
	m_Game = 1;
}

CLayerGame::~CLayerGame()
{
}

CTile CLayerGame::GetTile(int x, int y)
{
	if(m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT) {
		CTile through_cut = {TILE_THROUGH_CUT};
		return through_cut;
	} else {
		return CLayerTiles::GetTile(x, y);
	}
}

void CLayerGame::SetTile(int x, int y, CTile tile)
{
	if(tile.m_Index == TILE_THROUGH_CUT) {
		if(!m_pEditor->m_Map.m_pFrontLayer) {
			CLayer *l = new CLayerFront(m_Width, m_Height);
			m_pEditor->m_Map.MakeFrontLayer(l);
			m_pEditor->m_Map.m_lGroups[m_pEditor->m_SelectedGroup]->AddLayer(l);
		}
		CTile nohook = {TILE_NOHOOK};
		CLayerTiles::SetTile(x, y, nohook);
		CTile through_cut = {TILE_THROUGH_CUT};
		m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, through_cut);
	} else {
		if(m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y).m_Index == TILE_THROUGH_CUT) {
			CTile air = {TILE_AIR};
			m_pEditor->m_Map.m_pFrontLayer->CLayerTiles::SetTile(x, y, air);
		}
		if(m_pEditor->m_AllowPlaceUnusedTiles || IsValidGameTile(tile.m_Index)) {
			CLayerTiles::SetTile(x, y, tile);
		} else {
			CTile air = {TILE_AIR};
			CLayerTiles::SetTile(x, y, air);
			if(!m_pEditor->m_PreventUnusedTilesWasWarned) {
				m_pEditor->m_PopupEventType = m_pEditor->POPEVENT_PREVENTUNUSEDTILES;
				m_pEditor->m_PopupEventActivated = true;
				m_pEditor->m_PreventUnusedTilesWasWarned = true;
			}
		}
	}
}

int CLayerGame::RenderProperties(CUIRect *pToolbox)
{
	int r = CLayerTiles::RenderProperties(pToolbox);
	m_Image = -1;
	return r;
}
