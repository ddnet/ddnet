#include "lovely.h"
#include "game/server/entities/character.h"
#include <base/math.h>
#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <generated/protocol.h>

CLovely::CLovely(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LOVELY, Pos)
{
	m_Owner = Owner;
	m_SpawnDelay = 0;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();
	for(int i = 0; i < MAX_HEARTS; i++)
		m_aLovelyData[i].m_ID = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

void CLovely::Reset()
{
	for(int i = 0; i < MAX_HEARTS; i++)
		Server()->SnapFreeId(m_aLovelyData[i].m_ID);

	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CLovely::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	if(!pOwner->GetPlayer()->m_Cosmetics.m_Lovely)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	m_SpawnDelay--;
	if(m_SpawnDelay <= 0)
	{
		SpawnNewHeart();
		int SpawnTime = 45;
		m_SpawnDelay = Server()->TickSpeed() - (rand() % (SpawnTime - (SpawnTime - 10) + 1) + (SpawnTime - 10));
	}

	for(int i = 0; i < MAX_HEARTS; i++)
	{
		if(m_aLovelyData[i].m_Lifespan == -1)
			continue;

		m_aLovelyData[i].m_Lifespan--;
		m_aLovelyData[i].m_Pos.y -= 5.f;

		if(m_aLovelyData[i].m_Lifespan == 0 || GameServer()->Collision()->TestBox(m_aLovelyData[i].m_Pos, vec2(14.f, 14.f)))
			m_aLovelyData[i].m_Lifespan = -1;
	}
}

void CLovely::SpawnNewHeart()
{
	for(int i = 0; i < MAX_HEARTS; i++)
	{
		if(m_aLovelyData[i].m_Lifespan > 0)
			continue;

		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		m_aLovelyData[i].m_Lifespan = Server()->TickSpeed() / 2;
		m_aLovelyData[i].m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
		pOwner->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		break;
	}
}

void CLovely::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

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

	for(int i = 0; i < MAX_HEARTS; i++)
	{
		if(m_aLovelyData[i].m_Lifespan == -1)
			continue;

		if(GameServer()->GetClientVersion(SnappingClient) >= VERSION_DDNET_ENTITY_NETOBJS)
		{
			CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(m_aLovelyData[i].m_ID);
			if(!pPickup)
				return;

			pPickup->m_X = round_to_int(m_aLovelyData[i].m_Pos.x);
			pPickup->m_Y = round_to_int(m_aLovelyData[i].m_Pos.y);
			pPickup->m_Type = POWERUP_HEALTH;
			pPickup->m_Flags = PICKUPFLAG_NO_PREDICT;
		}
		else
		{
			CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(m_aLovelyData[i].m_ID);
			if(!pPickup)
				return;

			pPickup->m_X = round_to_int(m_aLovelyData[i].m_Pos.x);
			pPickup->m_Y = round_to_int(m_aLovelyData[i].m_Pos.y);
			pPickup->m_Type = POWERUP_HEALTH;
		}
	}
}
