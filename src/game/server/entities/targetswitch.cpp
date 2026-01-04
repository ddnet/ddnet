#include "targetswitch.h"

#include "character.h"

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/teamscore.h>

// fills up an entire block to allow hammering thru a wall from any side
static constexpr int TARGET_SWITCH_SIZE = 30;

CTargetSwitch::CTargetSwitch(CGameWorld *pGameWorld, vec2 Pos, int Type, int Layer, int Number, int Flags, int Delay) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TARGETSWITCH, Pos, TARGET_SWITCH_SIZE)
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
	Move();
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

	GameServer()->SnapTargetSwitch(CSnapContext(SnappingClientVersion, Sixup, SnappingClient), GetId(), m_Pos, m_Type, m_Number, m_Delay, 0);
}

void CTargetSwitch::GetHit(int ClientId, int ForcedTeam)
{
	CClientMask Mask;
	int TeamHitFrom;
	if(ForcedTeam == -1 && ClientId != -1)
	{
		Mask = GameServer()->GetPlayerChar(ClientId)->TeamMask();
		TeamHitFrom = GameServer()->GetPlayerChar(ClientId)->Team();
	}
	else
	{
		Mask.reset();
		Mask.set(ForcedTeam);
		TeamHitFrom = ForcedTeam;
	}

	bool PreviousSwitchStatus = Switchers()[m_Number].m_aStatus[TeamHitFrom];
	const int EndTick = m_Delay ? Server()->Tick() + 1 + m_Delay * Server()->TickSpeed() : 0;
	Switchers()[m_Number].m_aLastUpdateTick[TeamHitFrom] = Server()->Tick();
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
		// delay not supported on alternating switches due to confusing behavior
		Switchers()[m_Number].m_aEndTick[TeamHitFrom] = 0;
		Switchers()[m_Number].m_aType[TeamHitFrom] = (Switchers()[m_Number].m_aType[TeamHitFrom] == TILE_SWITCHCLOSE) ? TILE_SWITCHCLOSE : TILE_SWITCHOPEN;
	}

	// Hitting this switch changed something, provide feedback
	if(PreviousSwitchStatus != Switchers()[m_Number].m_aStatus[TeamHitFrom] || m_Delay != 0)
	{
		GameServer()->CreateTargetHit(m_Pos, ClientId, Mask);
	}
}

void CTargetSwitch::Move()
{
	if(Server()->Tick() % 2 == 0)
	{
		GameServer()->Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core);
		m_Pos += m_Core;
	}
}
