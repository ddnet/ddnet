// Made by qxdFox
#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <generated/protocol.h>

#include <engine/shared/config.h>

#include <base/vmath.h>

#include "heart_hat.h"
CHeartHat::CHeartHat(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_HEART_HAT, vec2(0, 0))
{
	m_aPos[0] = vec2(0, 0);

	m_Owner = Owner;
	m_Ids[0] = GetId();
	for(int i = 0; i < NUM_HEARTS - 1; i++)
		m_Ids[i + 1] = Server()->SnapNewId();

	GameWorld()->InsertEntity(this);
}

void CHeartHat::Reset()
{
	for(int i = 0; i < NUM_HEARTS - 1; i++)
		Server()->SnapFreeId(m_Ids[i + 1]);

	Server()->SnapFreeId(GetId());

	GameWorld()->RemoveEntity(this);
}

void CHeartHat::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	if(!pOwner->GetPlayer()->m_Cosmetics.m_HeartHat)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();

	if(!m_switch)
	{
		m_Dist[HEART_BACK] += 1.80f;
		m_Dist[HEART_FRONT] += 1.80f;

		m_Dist[HEART_MIDDLE] -= 1.80f;
		if(m_Dist[HEART_BACK] > 24.0f)
			m_switch = true;
	}
	else
	{
		m_Dist[HEART_BACK] -= 1.68f;
		m_Dist[HEART_FRONT] -= 1.68f;

		m_Dist[HEART_MIDDLE] += 1.68f;
		if(m_Dist[HEART_BACK] < -24.0f)
			m_switch = false;
	}

	for(int i = 0; i < NUM_HEARTS; i++)
	{
		m_aPos[i] = pOwner->GetPos();
		m_aPos[i].x += (int)m_Dist[i];
	}
}

void CHeartHat::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	const CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChr || !pSnapPlayer)
		return;

	if(pOwnerChr->IsPaused())
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	const int Team = pOwnerChr->Team();

	if(!Teams.SetMask(SnappingClient, Team))
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChr)
		if(!pOwnerChr->CanSnapCharacter(SnappingClient))
			return;

	for(int i = 0; i < NUM_HEARTS; i++)
	{
		const int Id = m_Ids[i];
		if(m_switch && i == HEART_FRONT)
			continue;

		vec2 Pos = m_aPos[i] + pOwnerChr->GetVelocity() + vec2(0, -42);

		if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient && !pOwnerChr->GetPlayer()->IsPaused())
		{
			const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
			const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
			const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
			Pos = m_aPos[i] + vec2(0, -42) + nVel;
		}

		const int SnapVer = Server()->GetClientVersion(SnappingClient);
		const bool SixUp = Server()->IsSixup(SnappingClient);
		GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), Id, Pos, POWERUP_HEALTH, -1, -1, PICKUPFLAG_NO_PREDICT);
	}
}
