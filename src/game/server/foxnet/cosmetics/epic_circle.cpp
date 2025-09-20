#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <base/math.h>
#include <base/vmath.h>

#include "epic_circle.h"

CEpicCircle::CEpicCircle(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;

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

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	const CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChr || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	if(pOwnerChr->IsPaused())
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	const int Team = pOwnerChr->Team();

	if(!Teams.SetMask(SnappingClient, Team))
		return;

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(m_aIds[i]);
		if(!pProj)
			return;
		vec2 Pos = m_Pos + m_RotatePos[i] + pOwnerChr->GetVelocity();
		if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient && !pOwnerChr->GetPlayer()->IsPaused())
		{
			const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
			const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
			const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
			Pos = m_Pos + m_RotatePos[i] + nVel;
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
