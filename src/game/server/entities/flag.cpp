/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "character.h"
#include <game/server/gamecontext.h>

#include "flag.h"
CFlag::CFlag(CGameWorld *pGameWorld, int Team) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG)
{
	m_Team = Team;
	m_pCarrier = NULL;
	m_GrabTick = 0;

	Reset();
}

void CFlag::Reset()
{
	m_pCarrier = NULL;
	m_AtStand = 1;
	m_Pos = m_StandPos;
	m_Vel = vec2(0, 0);
	m_GrabTick = 0;
}

void CFlag::Grab(CCharacter *pChar)
{
	m_pCarrier = pChar;
	if(m_AtStand)
		m_GrabTick = Server()->Tick();
	m_AtStand = false;
}

void CFlag::Drop()
{
	m_pCarrier = 0;
	m_Vel = vec2(0, 0);
	m_DropTick = Server()->Tick();
}

void CFlag::TickDefered()
{
	if(m_pCarrier)
	{
		// update flag position
		m_Pos = m_pCarrier->GetPos();
	}
}

void CFlag::TickPaused()
{
	++m_DropTick;
	if(m_GrabTick)
		++m_GrabTick;
}

void CFlag::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if(!pFlag)
		return;

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;
}
