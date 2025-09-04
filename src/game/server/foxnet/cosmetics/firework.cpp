// Made by qxdFox
#include "firework.h"
#include "game/server/entities/character.h"
#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <generated/protocol.h>
#include <random>

constexpr int LaunchSpeed = -25;
constexpr float LaunchTime = 1.5f;
constexpr float FireworkTime = 3.5f;
constexpr float MaxSpeed = 20.0f;

CFirework::CFirework(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FIREWORK, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	m_StartTick = Server()->Tick();

	std::random_device rd;
	for(int i = 0; i < MAX_FIREWORKS; i++)
	{
		m_Ids[i] = Server()->SnapNewId();

		std::uniform_int_distribution<long> X(-MaxSpeed, MaxSpeed);
		std::uniform_int_distribution<long> Y(-MaxSpeed, MaxSpeed);
		std::uniform_int_distribution<long> Lt(Server()->TickSpeed() * 1.5f, FireworkTime * Server()->TickSpeed() - MAX_FIREWORKS);

		m_aVel[i] = vec2(X(rd), Y(rd));
		m_aLifetime[i] = Lt(rd) + i;
	}
	m_State = STATE_START;
	GameWorld()->InsertEntity(this);
}

void CFirework::Reset()
{
	for(int i = 0; i < MAX_FIREWORKS; i++)
		Server()->SnapFreeId(m_Ids[i]);

	// Always alocated when entity is created
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CFirework::Tick()
{
	if(m_State == STATE_START)
	{
		if(m_StartTick + Server()->TickSpeed() * LaunchTime < Server()->Tick())
		{
			for(int i = 0; i < MAX_FIREWORKS; i++)
			{
				m_aPos[i].y = m_Pos.y + LaunchSpeed * LaunchTime * 5;
				m_aPos[i].x = m_Pos.x;
			}

			m_State = STATE_EXPLOSION;
			m_StartTick = Server()->Tick();
		}
	}
	else if(m_State == STATE_EXPLOSION)
	{
		for(int i = 0; i < MAX_FIREWORKS; i++)
			m_aLifetime[i]--;

		if(m_StartTick + Server()->TickSpeed() * FireworkTime < Server()->Tick())
			m_State = STATE_NONE;
	}
	else
	{
		Reset();
		return;
	}

	for(int i = 0; i < MAX_FIREWORKS; i++)
	{
		m_aPos[i] += m_aVel[i] / 10;
	}
}

void CFirework::Snap(int SnappingClient)
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	int Team = pOwnerChar->Team();

	if(!Teams.SetMask(SnappingClient, Team))
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChar)
		if(!pOwnerChar->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	if(m_State == STATE_START)
	{
		CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(GetId());
		if(!pProj)
			return;

		pProj->m_X = round_to_int(m_Pos.x * 100.0f);
		pProj->m_Y = round_to_int(m_Pos.y * 100.0f);
		pProj->m_VelX = 0;
		pProj->m_VelY = round_to_int(LaunchSpeed * 10000.0f);
		pProj->m_Type = WEAPON_SHOTGUN;
		pProj->m_StartTick = m_StartTick;
	}
	else if(m_State == STATE_EXPLOSION)
	{
		for(int i = 0; i < MAX_FIREWORKS; i++)
		{
			CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(m_Ids[i]);
			if(!pProj || m_aLifetime[i] <= 0)
				continue;

			pProj->m_StartTick = Server()->Tick();

			pProj->m_X = round_to_int(m_aPos[i].x * 100.0f);
			pProj->m_Y = round_to_int(m_aPos[i].y * 100.0f);
			pProj->m_VelX = m_aVel[i].x * 32;
			pProj->m_VelY = m_aVel[i].x * 32;
			pProj->m_Type = WEAPON_LASER;
		}
	}
}