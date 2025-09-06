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
#include <engine/server.h>

#include <base/vmath.h>
#include <iterator>

#include "powerup.h"
#include <algorithm>

// Its called powerup because i want to add more functionality later to it like giving custom weapons or abilities
// For now it just acts like the 0xf one
CPowerUp::CPowerUp(CGameWorld *pGameWorld, vec2 Pos, int Lifetime, int XP) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTALS, Pos, 54)
{
	m_Pos = Pos;
	m_XP = XP;
	m_Lifetime = Lifetime * Server()->TickSpeed();

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
	m_Lifetime--;
	if(m_Lifetime <= 0)
	{
		Reset();
		return;
	}

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
		if(!pChr || !pChr->IsAlive() || pChr->Team() != TEAM_FLOCK)
			continue;

		if(PointInSquare(m_Pos, pChr->GetPos(), 54.0f))
		{
			GameServer()->CollectedPowerup(ClientId, m_XP);
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
			Reset();
			return;
		}
	}
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

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;

		if(pSnapPlayer->m_HidePowerUps)
			return;
	}

	// Make the powerup blink when about to disappear
	if(m_Lifetime < Server()->TickSpeed() * 10 && (Server()->Tick() / (Server()->TickSpeed() / 4)) % 2 == 0)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	if(!Teams.SetMaskWithFlags(SnappingClient, TEAM_FLOCK, CGameTeams::EXTRAFLAG_IGNORE_SOLO))
		return;

	int SnappingClientVersion = Server()->GetClientVersion(SnappingClient);
	bool SixUp = Server()->IsSixup(SnappingClient);

	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Switch = !m_Switch;

	GameServer()->SnapPickup(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), GetId(), m_Pos, m_Switch, 0, -1, PICKUPFLAG_NO_PREDICT);

	for(int i = 0; i < NUM_LASERS; i++)
	{
		vec2 To = m_Snap.m_aTo[i];
		vec2 From = m_Snap.m_aFrom[i];
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, SixUp, SnappingClient), m_Snap.m_aLaserIds[i], To, From, Server()->Tick(), -1, 0);
	}
}