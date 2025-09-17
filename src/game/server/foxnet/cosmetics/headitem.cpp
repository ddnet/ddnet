// Made by qxdFox
#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

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
		const CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];
		if(!pSnapPlayer)
			return;

		CGameTeams Teams = GameServer()->m_pController->Teams();
		const int Team = pOwnerChr->Team();

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
	const int SubType = Type - 7;
	if(m_Type == 8)
		Type = POWERUP_WEAPON;
	else if(m_Type == 9)
		Type = POWERUP_WEAPON;
	else if(m_Type == 10)
		Type = POWERUP_WEAPON;
	else if(m_Type == 11)
		Type = POWERUP_WEAPON;

	vec2 Pos = m_Pos + pOwnerChr->GetVelocity();
	if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient && !pOwnerChr->GetPlayer()->IsPaused())
	{
		const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
		const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
		const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
		Pos = m_Pos + nVel;
	}

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	GameServer()->SnapPickup(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), Pos, Type, SubType, -1, PICKUPFLAG_NO_PREDICT);
}
