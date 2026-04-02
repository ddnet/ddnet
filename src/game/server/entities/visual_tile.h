#ifndef GAME_SERVER_ENTITIES_VISUAL_TILE_H
#define GAME_SERVER_ENTITIES_VISUAL_TILE_H

#include <game/server/entity.h>
#include <game/visual_primitives.h>

class CVisualTile : public CEntity
{
public:
	CVisualTile(CGameWorld *pGameWorld, vec2 Pos, int W, int H,
		int Angle, int Color, int ImageIndex, int TileIndex,
		int Flags = 0,
		int RenderOrder = RENDERORDER_DEFAULT,
		int Lifetime = -1, CClientMask Mask = CClientMask().set());

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetColor(int Color) { m_Color = Color; }
	void SetTileIndex(int TileIndex) { m_TileIndex = TileIndex; }

private:
	int m_W;
	int m_H;
	int m_Angle;
	int m_Color;
	int m_ImageIndex;
	int m_TileIndex;
	int m_VisualFlags;
	int m_RenderOrder;
	int m_Lifetime;
	CClientMask m_Mask;
};

#endif
