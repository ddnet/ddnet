// Made by qxdFox
#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <engine/shared/protocol.h>
#include <engine/server.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <base/vmath.h>

#include "headitem.h"

CHeadItem::CHeadItem(CGameWorld *pGameWorld, int Owner, vec2 Pos, int Type, float Offset) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_HEAD_ITEM, Pos)
{
	m_Owner = Owner;

	// Type of Entity
	m_Type = Type;
	m_Offset = Offset;

	GameWorld()->InsertEntity(this);
}

void CHeadItem::Reset()
{
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CHeadItem::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwner)
		return;
	if(!pOwner->m_pHeadItem)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_Pos.y -= m_Offset;
}

void CHeadItem::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwnerChr)
		return;

	if(pOwnerChr->IsPaused())
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT)
	{
		CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;

		CGameTeams Teams = GameServer()->m_pController->Teams();
		int Team = pOwnerChr->Team();

		if(!Teams.SetMaskWithFlags(SnappingClient, Team))
			return;

		if(pSnapPlayer->GetCharacter() && pOwnerChr)
			if(!pOwnerChr->CanSnapCharacter(SnappingClient))
				return;

		if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
			if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
				return;
	}

	int Type = m_Type;
	int SubType = Type - 7;
	if(m_Type == 8)
		Type = POWERUP_WEAPON;
	else if(m_Type == 9)
		Type = POWERUP_WEAPON;
	else if(m_Type == 10)
		Type = POWERUP_WEAPON;
	else if(m_Type == 11)
		Type = POWERUP_WEAPON;

	vec2 Pos = m_Pos;
	if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient)
	{
		const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
		const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
		vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
		Pos += nVel;
	}

	if(GameServer()->GetClientVersion(SnappingClient) >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(GetId());
		if(!pPickup)
			return;

		pPickup->m_X = (int)(Pos.x);
		pPickup->m_Y = (int)(Pos.y);

		pPickup->m_Type = Type;
		pPickup->m_Subtype = SubType;
		pPickup->m_Flags = PICKUPFLAG_NO_PREDICT;
	}
	else
	{
		CNetObj_Pickup *pPickup = Server()->SnapNewItem<CNetObj_Pickup>(GetId());
		if(!pPickup)
			return;

		pPickup->m_X = (int)(Pos.x);
		pPickup->m_Y = (int)(Pos.y);

		pPickup->m_Type = Type;
		if(GameServer()->GetClientVersion(SnappingClient) < VERSION_DDNET_WEAPON_SHIELDS)
		{
			if(Type >= POWERUP_ARMOR_SHOTGUN && Type <= POWERUP_ARMOR_LASER)
			{
				pPickup->m_Type = POWERUP_ARMOR;
			}
		}
		pPickup->m_Subtype = SubType;
	}
}
