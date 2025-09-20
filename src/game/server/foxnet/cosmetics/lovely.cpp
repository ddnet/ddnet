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
	for(int i = 0; i < MAX_HEARTS; i++)
		m_aData[i].m_Id = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

void CLovely::Reset()
{
	for(int i = 0; i < MAX_HEARTS; i++)
		Server()->SnapFreeId(m_aData[i].m_Id);

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
		if(m_aData[i].m_Lifespan == -1)
			continue;

		m_aData[i].m_Lifespan--;
		m_aData[i].m_Pos.y -= 5.f;

		if(m_aData[i].m_Lifespan == 0 || GameServer()->Collision()->TestBox(m_aData[i].m_Pos, vec2(14.f, 14.f)))
			m_aData[i].m_Lifespan = -1;
	}
}

void CLovely::SpawnNewHeart()
{
	for(int i = 0; i < MAX_HEARTS; i++)
	{
		if(m_aData[i].m_Lifespan > 0)
			continue;

		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		m_aData[i].m_Lifespan = Server()->TickSpeed() / 2;
		m_aData[i].m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
		pOwner->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		break;
	}
}

void CLovely::Snap(int SnappingClient)
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

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	for(int i = 0; i < MAX_HEARTS; i++)
	{
		if(m_aData[i].m_Lifespan == -1)
			continue;
		GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), m_aData[i].m_Id, m_aData[i].m_Pos, POWERUP_HEALTH, -1, -1, PICKUPFLAG_NO_PREDICT);
	}
}
