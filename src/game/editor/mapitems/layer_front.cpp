#include <game/editor/editor.h>

CLayerFront::CLayerFront(CEditor *pEditor, int w, int h) :
	CLayerTiles(pEditor, w, h)
{
	str_copy(m_aName, "Front");
	m_HasFront = true;
}

void CLayerFront::SetTile(int x, int y, CTile Tile)
{
	if(Tile.m_Index == TILE_THROUGH_CUT)
	{
		m_pEditor->m_Map.m_pGameLayer->CLayerTiles::SetTile(x, y, CTile{TILE_NOHOOK}); // NOLINT(bugprone-parent-virtual-call)
	}
	else if(Tile.m_Index == TILE_AIR && CLayerTiles::GetTile(x, y).m_Index == TILE_THROUGH_CUT)
	{
		m_pEditor->m_Map.m_pGameLayer->CLayerTiles::SetTile(x, y, CTile{TILE_AIR}); // NOLINT(bugprone-parent-virtual-call)
	}
	if(m_pEditor->IsAllowPlaceUnusedTiles() || IsValidFrontTile(Tile.m_Index))
	{
		CLayerTiles::SetTile(x, y, Tile);
	}
	else
	{
		CLayerTiles::SetTile(x, y, CTile{TILE_AIR});
		ShowPreventUnusedTilesWarning();
	}
}

void CLayerFront::Resize(int NewW, int NewH)
{
	// resize tile data
	CLayerTiles::Resize(NewW, NewH);

	// resize gamelayer too
	if(m_pEditor->m_Map.m_pGameLayer->m_Width != NewW || m_pEditor->m_Map.m_pGameLayer->m_Height != NewH)
		m_pEditor->m_Map.m_pGameLayer->Resize(NewW, NewH);
}

const char *CLayerFront::TypeName() const
{
	return "front";
}
