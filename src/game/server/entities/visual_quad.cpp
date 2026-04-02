#include "visual_quad.h"

#include <game/server/gamecontext.h>

CVisualQuad::CVisualQuad(CGameWorld *pGameWorld, vec2 Pos, int W, int H,
	int Angle, int Color, int Width, int ImageIndex,
	int Flags, int RenderOrder, int Lifetime, CClientMask Mask) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos)
{
	m_W = W;
	m_H = H;
	m_Angle = Angle;
	m_Color = Color;
	m_VisualWidth = Width;
	m_ImageIndex = ImageIndex;
	m_VisualFlags = Flags;
	m_RenderOrder = RenderOrder;
	m_Lifetime = Lifetime;
	m_Mask = Mask;
	GameWorld()->InsertEntity(this);
}

void CVisualQuad::Tick()
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

void CVisualQuad::Snap(int SnappingClient)
{
	if(!(m_VisualFlags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)) && NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_Mask.test(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	GameServer()->SnapVisualQuad(
		CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient),
		GetId(), m_Pos, m_W, m_H, m_Angle, m_Color, m_VisualWidth, m_ImageIndex, m_VisualFlags, m_RenderOrder);
}
