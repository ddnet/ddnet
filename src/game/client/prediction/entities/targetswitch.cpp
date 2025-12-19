#include "targetswitch.h"

#include "character.h"

#include <game/collision.h>
#include <game/mapitems.h>

// fills up an entire block to allow hammering thru a wall from any side
static constexpr int TARGET_SWITCH_SIZE = 30;

CTargetSwitch::CTargetSwitch(CGameWorld *pGameWorld, int Id, const CNetObj_DDNetTargetSwitch *pData) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TARGETSWITCH)
{
	m_Id = Id;
	m_ProximityRadius = TARGET_SWITCH_SIZE;
	m_Layer = LAYER_SWITCH;
	m_Core = vec2(0.0f, 0.0f);

	Read(pData);
}

bool CTargetSwitch::Match(const CTargetSwitch *pTarget) const
{
	return pTarget->m_Type == m_Type && pTarget->m_Number == m_Number && pTarget->m_Delay == m_Delay && pTarget->m_Flags == m_Flags;
}

void CTargetSwitch::Read(const CNetObj_DDNetTargetSwitch *pData)
{
	m_Pos = vec2(pData->m_X, pData->m_Y);
	m_Type = pData->m_Type;
	m_Number = pData->m_SwitchNumber;
	m_Delay = pData->m_SwitchDelay;
	m_Flags = pData->m_Flags;
}

void CTargetSwitch::Reset()
{
	m_MarkedForDestroy = true;
}

void CTargetSwitch::Tick()
{
	Move();
}

void CTargetSwitch::GetHit(int ClientId, bool Weakly, int ForcedTeam)
{
	int TeamHitFrom;
	if(ForcedTeam == -1 && ClientId != -1)
	{
		TeamHitFrom = GameWorld()->GetCharacterById(ClientId)->Team();
	}
	else
	{
		TeamHitFrom = ForcedTeam;
	}

	if(Weakly)
	{
		return;
	}

	const int EndTick = m_Delay ? GameWorld()->GameTick() + 1 + m_Delay * GameWorld()->GameTickSpeed() : 0;
	Switchers()[m_Number].m_aLastUpdateTick[TeamHitFrom] = GameWorld()->GameTick();
	if(m_Type == TARGETSWITCHTYPE_CLOSE)
	{
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = true;
		Switchers()[m_Number].m_aEndTick[TeamHitFrom] = EndTick;
		Switchers()[m_Number].m_aType[TeamHitFrom] = m_Delay ? TILE_SWITCHTIMEDOPEN : TILE_SWITCHCLOSE;
	}
	else if(m_Type == TARGETSWITCHTYPE_OPEN)
	{
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = false;
		Switchers()[m_Number].m_aEndTick[TeamHitFrom] = EndTick;
		Switchers()[m_Number].m_aType[TeamHitFrom] = m_Delay ? TILE_SWITCHTIMEDCLOSE : TILE_SWITCHOPEN;
	}
	else if(m_Type == TARGETSWITCHTYPE_ALTERNATE)
	{
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = !Switchers()[m_Number].m_aStatus[TeamHitFrom];
		Switchers()[m_Number].m_aEndTick[TeamHitFrom] = 0; // no delay on alternating targets
		Switchers()[m_Number].m_aType[TeamHitFrom] = (Switchers()[m_Number].m_aType[TeamHitFrom] == TILE_SWITCHCLOSE) ? TILE_SWITCHCLOSE : TILE_SWITCHOPEN;
	}
}
void CTargetSwitch::Move()
{
	if(GameWorld()->GameTick() % 2 == 0)
	{
		Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
	}
}
