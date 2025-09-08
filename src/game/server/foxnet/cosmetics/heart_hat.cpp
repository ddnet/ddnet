// Made by qxdFox
#include "heart_hat.h"
#include "game/server/entities/character.h"
#include <algorithm>
#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <generated/protocol.h>

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

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	if(pOwnerChar->IsPaused())
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

	vec2 Pos;

	for(int i = 0; i < NUM_HEARTS; i++)
	{
		int Id = m_Ids[i];
		if(m_switch && i == HEART_FRONT)
			continue;

		Pos = m_aPos[i] + vec2(0, -42);

		if(m_Owner == SnappingClient)
		{
			float Lat = std::clamp((float)pOwnerChar->GetPlayer()->m_Latency.m_Avg, 0.0f, 125.0f) / 27.77f;
			Pos.x = m_aPos[i].x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos.y = m_aPos[i].y - 52 + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos = m_Pos + vec2(0.8f, 0.8f) * (Pos - m_Pos);
		}

		if(GameServer()->GetClientVersion(SnappingClient) >= VERSION_DDNET_ENTITY_NETOBJS)
		{
			CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(Id);
			if(!pPickup)
				return;

			pPickup->m_X = (int)(Pos.x);
			pPickup->m_Y = (int)(Pos.y);
			pPickup->m_Type = POWERUP_HEALTH;
			pPickup->m_Flags = PICKUPFLAG_NO_PREDICT;
		}
		else
		{
			CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(Id);
			if(!pPickup)
				return;

			pPickup->m_X = (int)(Pos.x);
			pPickup->m_Y = (int)(Pos.y);
			pPickup->m_Type = POWERUP_HEALTH;
		}
	}
}
