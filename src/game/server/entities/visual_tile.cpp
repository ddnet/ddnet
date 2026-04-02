#include "visual_tile.h"

#include <game/server/gamecontext.h>

CVisualTile::CVisualTile(CGameWorld *pGameWorld, vec2 Pos, int W, int H,
	int Angle, int Color, int ImageIndex, int TileIndex,
	int Flags, int RenderOrder, int Lifetime, CClientMask Mask) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos)
{
	m_W = W;
	m_H = H;
	m_Angle = Angle;
	m_Color = Color;
	m_ImageIndex = ImageIndex;
	m_TileIndex = TileIndex;
	m_VisualFlags = Flags;
	m_RenderOrder = RenderOrder;
	m_Lifetime = Lifetime;
	m_Mask = Mask;
	GameWorld()->InsertEntity(this);
}

void CVisualTile::Tick()
{
	if(m_Lifetime > 0)
	{
		m_Lifetime--;
		if(m_Lifetime == 0)
		{
			m_MarkedForDestroy = true;
			return;
		}
	}
}

void CVisualTile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_Mask.test(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	GameServer()->SnapVisualTile(
		CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient),
		GetId(), m_Pos, m_W, m_H, m_Angle, m_Color, m_ImageIndex, m_TileIndex, m_VisualFlags, m_RenderOrder);
}
