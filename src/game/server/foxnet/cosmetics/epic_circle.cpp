#include "epic_circle.h"
#include "game/server/entities/character.h"
#include <algorithm>
#include <base/vmath.h>
#include <cmath>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

CEpicCircle::CEpicCircle(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Owner = Owner;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();

	for(int i = 0; i < MAX_PARTICLES; i++)
		m_aIds[i] = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

void CEpicCircle::Reset()
{
	for(int i = 0; i < MAX_PARTICLES; i++)
		Server()->SnapFreeId(m_aIds[i]);

	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CEpicCircle::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwner)
		return;

	if(!pOwner->GetPlayer()->m_Cosmetics.m_EpicCircle)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		float rad = 16.0f * powf(sinf(Server()->Tick() / 30.0f), 3) * 1 + 75;
		float TurnFac = 0.025f;
		m_RotatePos[i].x = cosf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick() * TurnFac) * rad;
		m_RotatePos[i].y = sinf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick() * TurnFac) * rad;
	}
}

void CEpicCircle::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChar || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	if(pOwnerChar->IsPaused())
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

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(m_aIds[i]);
		if(!pProj)
			return;
		vec2 Pos = m_Pos + m_RotatePos[i];

		if(m_Owner == SnappingClient)
		{
			float Lat = std::clamp((float)pOwnerChar->GetPlayer()->m_Latency.m_Avg, 0.0f, 125.0f) / 27.77f;
			Pos.x = m_Pos.x + m_RotatePos[i].x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos.y = m_Pos.y + m_RotatePos[i].y + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos = m_Pos + vec2(0.8f, 0.8f) * (Pos - m_Pos);
		}

		pProj->m_X = round_to_int(Pos.x * 100.0f);
		pProj->m_Y = round_to_int(Pos.y * 100.0f);
		pProj->m_Type = WEAPON_HAMMER;
		pProj->m_Owner = m_Owner;
		pProj->m_StartTick = 0;
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
	}
}
