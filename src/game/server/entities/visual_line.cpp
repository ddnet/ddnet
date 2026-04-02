#include "visual_line.h"

#include <game/server/gamecontext.h>

CVisualLine::CVisualLine(CGameWorld *pGameWorld, vec2 From, vec2 To,
	int Color, int Width, int Flags,
	int RenderOrder, int Lifetime, CClientMask Mask) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, From)
{
	m_From = From;
	m_To = To;
	m_Color = Color;
	m_Width = Width;
	m_VisualFlags = Flags;
	m_RenderOrder = RenderOrder;
	m_Lifetime = Lifetime;
	m_Mask = Mask;
	GameWorld()->InsertEntity(this);
}

void CVisualLine::Tick()
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

void CVisualLine::Snap(int SnappingClient)
{
	if(!(m_VisualFlags & (VISUALFLAG_SCREEN_SPACE | VISUALFLAG_CAMERA_RELATIVE)) && NetworkClippedLine(SnappingClient, m_From, m_To))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_Mask.test(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	GameServer()->SnapVisualLine(
		CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient),
		GetId(), m_From, m_To, m_Color, m_Width, m_VisualFlags, m_RenderOrder);
}
