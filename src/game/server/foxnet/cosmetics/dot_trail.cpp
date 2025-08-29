// Made by qxdFox
#include "dot_trail.h"
#include "game/server/entities/character.h"
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

CDotTrail::CDotTrail(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DOT_TRAIL, Pos)
{
	m_Owner = Owner;

	GameWorld()->InsertEntity(this);
}

void CDotTrail::Reset()
{
	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CDotTrail::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	if(pOwner->GetPlayer()->m_Cosmetics.m_Trail != TRAIL_DOT)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
}

void CDotTrail::Snap(int SnappingClient)
{
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	if(pOwnerChar->IsPaused())
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChar)
		if(!pOwnerChar->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChar->GetPlayer()->m_Vanish && SnappingClient != pOwnerChar->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	CNetObj_Projectile *pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetId());
	if(!pProj)
		return;

	vec2 Pos = m_Pos;
	if(m_Owner == SnappingClient)
	{
		float Lat = std::clamp((float)pOwnerChar->GetPlayer()->m_Latency.m_Avg, 0.0f, 125.0f) / 27.77f;
		Pos.x = m_Pos.x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
		Pos.y = m_Pos.y + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
		Pos = m_Pos + vec2(0.8f, 0.8f) * (Pos - m_Pos);
	}

	pProj->m_X = (int)(Pos.x);
	pProj->m_Y = (int)(Pos.y);
	pProj->m_VelX = 0;
	pProj->m_VelY = 0;
	pProj->m_StartTick = 0;
	pProj->m_Type = WEAPON_HAMMER;
}
