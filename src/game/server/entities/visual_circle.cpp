#include "visual_circle.h"

#include <game/server/gamecontext.h>

CVisualCircle::CVisualCircle(CGameWorld *pGameWorld, vec2 Pos, int Radius,
	int Color, int Width, int Flags,
	int RenderOrder, int Lifetime, CClientMask Mask) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos)
{
	m_Radius = Radius;
	m_Color = Color;
	m_Width = Width;
	m_VisualFlags = Flags;
	m_RenderOrder = RenderOrder;
	m_Lifetime = Lifetime;
	m_Mask = Mask;
	GameWorld()->InsertEntity(this);
}

void CVisualCircle::Tick()
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

void CVisualCircle::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_Mask.test(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	GameServer()->SnapVisualCircle(
		CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient),
		GetId(), m_Pos, m_Radius, m_Color, m_Width, m_VisualFlags, m_RenderOrder);
}
