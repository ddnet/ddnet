// Made by qxdFox
#include "game/server/entities/character.h"
#include "head_powerup.h"
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/generated/protocol.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <base/vmath.h>

CHeadItem::CHeadItem(CGameWorld *pGameWorld, int Owner, vec2 Pos, int Type, float Offset) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_HEAD_ITEM, Pos)
{
	m_Owner = Owner;

	// Type of Entity
	m_Type = Type;
	m_Offset = Offset;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();

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

	if(!pOwner || !pOwner->m_HeadItem)
	{
		Reset();
		return;
	}

	m_TeamMask = pOwner->TeamMask();
	m_Pos = pOwner->GetPos();
	m_Pos.y -= m_Offset;
}

void CHeadItem::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	//if(pSnapPlayer->m_HideCosmetics)
	//	return;

	if(pOwnerChar->IsPaused())
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChar)
		if(!pOwnerChar->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

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
	if(m_Owner == SnappingClient)
	{
		float Lat = std::clamp((float)pOwnerChar->GetPlayer()->m_Latency.m_Avg, 0.0f, 125.0f) / 27.77f;
		Pos.x = m_Pos.x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
		Pos.y = m_Pos.y + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
		Pos = m_Pos + vec2(0.8f, 0.8f) * (Pos - m_Pos);
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
