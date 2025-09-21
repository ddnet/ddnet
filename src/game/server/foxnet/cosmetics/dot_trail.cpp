// Made by qxdFox
#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <generated/protocol.h>

#include <base/vmath.h>

#include "dot_trail.h"

CDotTrail::CDotTrail(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DOT_TRAIL, Pos)
{
	m_Pos = Pos;
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
	const CPlayer *pOwnerPl = GameServer()->m_apPlayers[m_Owner];
	if(!pOwnerPl || pOwnerPl->m_Cosmetics.m_Trail != TRAIL_DOT)
	{
		Reset();
		return;
	}
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	m_Pos = pOwner->GetPos();
}

void CDotTrail::Snap(int SnappingClient)
{
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

	if(pSnapPlayer->GetCharacter() && pOwnerChr)
		if(!pOwnerChr->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(GetId());
	if(!pProj)
		return;

	vec2 Pos = m_Pos + pOwnerChr->GetVelocity();
	if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient && !pOwnerChr->GetPlayer()->IsPaused())
	{
		const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
		const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
		const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
		Pos = m_Pos + nVel;
	}

	pProj->m_X = round_to_int(Pos.x * 100.0f);
	pProj->m_Y = round_to_int(Pos.y * 100.0f);
	pProj->m_Type = WEAPON_HAMMER;
	pProj->m_Owner = m_Owner;
	pProj->m_StartTick = 0;
	pProj->m_VelX = 0;
	pProj->m_VelY = 0;
}
