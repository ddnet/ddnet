/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "targetswitch.h"
#include "character.h"

#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/teamscore.h>
#include <base/logger.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

// fills up an entire block to allow hammering thru a wall from any side
static constexpr int gs_TargetSwitchSize = 32;

CTargetSwitch::CTargetSwitch(CGameWorld *pGameWorld, vec2 Pos, int Type, int Layer, int Number, int Flags, int Delay) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_HITTABLE, Pos, gs_TargetSwitchSize)
{
	m_Core = vec2(0.0f, 0.0f);
	m_Type = Type;

	m_Layer = Layer;
	m_Number = Number;
	m_Flags = Flags;
	m_Delay = Delay;

	GameWorld()->InsertEntity(this);
}

void CTargetSwitch::Reset()
{
	m_MarkedForDestroy = true;
}

void CTargetSwitch::Tick()
{
}

void CTargetSwitch::TickPaused()
{
}

void CTargetSwitch::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	bool Sixup = Server()->IsSixup(SnappingClient);

	// Switch state sent as flag so clients can predict alternating target switches
	int SnappingClientTeam = GameServer()->GetDDRaceTeam(SnappingClient);
	int SwitchState = Switchers()[m_Number].m_aStatus[SnappingClientTeam];

	GameServer()->SnapTargetSwitch(CSnapContext(SnappingClientVersion, Sixup), GetId(), m_Pos, m_Type, m_Number, m_Delay, SwitchState);
}

void CTargetSwitch::Move()
{
}

void CTargetSwitch::GetHit(int TeamHitFrom)
{
	std::bitset<MAX_CLIENTS> TeamHitBitset;
	TeamHitBitset.set(TeamHitFrom);
	GameServer()->CreateSound(m_Pos, SOUND_HIT, TeamHitBitset);
	if(m_Type == TARGETSWITCHTYPE_OPEN)
	{
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = true;
		if(m_Delay)
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = Server()->Tick() + 1 + m_Delay * Server()->TickSpeed();
		}
		else
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = 0;
		}
		Switchers()[m_Number].m_aType[TeamHitFrom] = TILE_SWITCHOPEN;
		Switchers()[m_Number].m_aLastUpdateTick[TeamHitFrom] = Server()->Tick();
	}
	if(m_Type == TARGETSWITCHTYPE_CLOSE)
	{
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = false;
		if(m_Delay)
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = Server()->Tick() + 1 + m_Delay * Server()->TickSpeed();
		}
		else
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = 0;
		}
		Switchers()[m_Number].m_aType[TeamHitFrom] = TILE_SWITCHCLOSE;
		Switchers()[m_Number].m_aLastUpdateTick[TeamHitFrom] = Server()->Tick();
	}
	if(m_Type == TARGETSWITCHTYPE_ALTERNATE)
	{
		// alternate all behavior
		Switchers()[m_Number].m_aStatus[TeamHitFrom] = !Switchers()[m_Number].m_aStatus[TeamHitFrom];
		if(m_Delay)
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = Server()->Tick() + 1 + m_Delay * Server()->TickSpeed();
		}
		else
		{
			Switchers()[m_Number].m_aEndTick[TeamHitFrom] = 0;
		}

		if(Switchers()[m_Number].m_aType[TeamHitFrom] == TILE_SWITCHCLOSE)
		{
			Switchers()[m_Number].m_aType[TeamHitFrom] = TILE_SWITCHCLOSE;
		}
		else
		{
			Switchers()[m_Number].m_aType[TeamHitFrom] = TILE_SWITCHOPEN;
		}
		Switchers()[m_Number].m_aLastUpdateTick[TeamHitFrom] = Server()->Tick();
	}
}
