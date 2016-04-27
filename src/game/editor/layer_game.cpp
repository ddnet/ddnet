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

CTile CLayerGame::GetTile(int x, int y, bool force)
{
	if(!force && m_pEditor->m_Map.m_pFrontLayer && GetTile(x, y, true).m_Index == TILE_NOHOOK && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y, true).m_Index == TILE_THROUGH_CUT)
	{
		CTile throughcut = {TILE_THROUGH_CUT, 0, 0, 0};
		return throughcut;
	} else {
		return m_pTiles[y*m_Width+x];
	}
}

void CLayerGame::SetTile(int x, int y, CTile tile, bool force)
{
	if(!force && tile.m_Index == TILE_THROUGH_CUT) {
		if(!m_pEditor->m_Map.m_pFrontLayer) {
			CLayer *l = new CLayerFront(m_Width, m_Height);
			m_pEditor->m_Map.MakeFrontLayer(l);
			m_pEditor->m_Map.m_lGroups[m_pEditor->m_SelectedGroup]->AddLayer(l);
		}
		CTile nohook = {TILE_NOHOOK, 0, 0, 0};
		SetTile(x, y, nohook, true);
		CTile throughcut = {TILE_THROUGH_CUT, 0, 0, 0};
		m_pEditor->m_Map.m_pFrontLayer->SetTile(x, y, throughcut, true);
	} else {
		if(!force && m_pEditor->m_Map.m_pFrontLayer && m_pEditor->m_Map.m_pFrontLayer->GetTile(x, y, true).m_Index == TILE_THROUGH_CUT) {
			CTile air = {TILE_AIR, 0, 0, 0};
			m_pEditor->m_Map.m_pFrontLayer->SetTile(x, y, air, true);
		}
		m_pTiles[y*m_Width+x] = tile;
	}
}

int CLayerGame::RenderProperties(CUIRect *pToolbox)
{
	int r = CLayerTiles::RenderProperties(pToolbox);
	m_Image = -1;
	return r;
}
