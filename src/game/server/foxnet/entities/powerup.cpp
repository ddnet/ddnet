// Made by qxdFox
#include <game/server/entities/character.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <game/gamecore.h>
#include <game/teamscore.h>

#include <generated/protocol.h>

#include <engine/shared/protocol.h>

#include <base/vmath.h>
#include <iterator>

#include "powerup.h"
#include <algorithm>

CPowerUp::CPowerUp(CGameWorld *pGameWorld, vec2 Pos, int XP) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTALS, Pos, 54)
{
	m_Pos = Pos;
	m_XP = XP;

	for(int i = 0; i < NUM_LASERS; i++)
		m_Snap.m_aLaserIds[i] = Server()->SnapNewId();
	std::sort(std::begin(m_Snap.m_aLaserIds), std::end(m_Snap.m_aLaserIds));

	GameWorld()->InsertEntity(this);
}

void CPowerUp::Reset()
{
	Server()->SnapFreeId(GetId());
	for(int i = 0; i < NUM_LASERS; i++)

		Server()->SnapFreeId(m_Snap.m_aLaserIds[i]);

	for(size_t i = 0; i < GameServer()->m_vPowerups.size(); i++)
	{
		if(GameServer()->m_vPowerups[i] == this)
			GameServer()->m_vPowerups.erase(GameServer()->m_vPowerups.begin() + i);
	}

	GameWorld()->RemoveEntity(this);
}

inline static bool PointInSquare(vec2 Point, vec2 Center, float Size)
{
	return (Point.x > Center.x - Size && Point.x < Center.x + Size && Point.y > Center.y - Size && Point.y < Center.y + Size);
}

void CPowerUp::Tick()
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(ClientId);
		if(!pChar || !pChar->IsAlive())
			continue;

		if(PointInSquare(m_Pos, pChar->GetPos(), 54.0f))
		{
			GameServer()->CollectedPowerup(ClientId, m_XP);
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, CClientMask().set(ClientId));
			Reset();
			return;
		}
	}
	if(m_Delay > 0)
		m_Delay--;
	else
		m_Delay = Server()->TickSpeed();
	SetPowerupVisual();
}

void CPowerUp::SetPowerupVisual()
{
	for(int i = 0; i < NUM_LASERS; i++)
		m_Snap.m_aTo[i] = m_Snap.m_aFrom[i] = vec2(0, 0);

	float Len = 28.0f;

	m_Snap.m_aTo[0] = m_Pos + vec2(-Len, -Len);
	m_Snap.m_aFrom[0] = m_Pos + vec2(Len, -Len);

	m_Snap.m_aTo[1] = m_Pos + vec2(Len, -Len);
	m_Snap.m_aFrom[1] = m_Pos + vec2(Len, Len);

	m_Snap.m_aTo[2] = m_Pos + vec2(Len, Len);
	m_Snap.m_aFrom[2] = m_Pos + vec2(-Len, Len);

	m_Snap.m_aTo[3] = m_Pos + vec2(-Len, Len);
	m_Snap.m_aFrom[3] = m_Pos + vec2(-Len, -Len);

	m_Snap.m_aTo[4] = m_Pos + vec2(-Len, -Len);
	m_Snap.m_aFrom[4] = m_Pos + vec2(-Len, -Len);
}


void CPowerUp::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
	if(!pSnapPlayer)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	if(!Teams.SetMask(SnappingClient, TEAM_FLOCK))
		return;

	int SnappingClientVersion = pSnapPlayer->GetClientVersion();
	bool SixUp = Server()->IsSixup(SnappingClient);

	if(m_Delay == 0)
		m_Switch = !m_Switch;

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), GetId(), m_Pos, m_Switch, 0, -1, PICKUPFLAG_NO_PREDICT);

	for(int i = 0; i < NUM_LASERS; i++)
	{
		vec2 To = m_Snap.m_aTo[i];
		vec2 From = m_Snap.m_aFrom[i];
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), m_Snap.m_aLaserIds[i], To, From, Server()->Tick(),-1, 0);
	}
}