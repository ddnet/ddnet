#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>
#include <generated/protocol.h>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <base/math.h>
#include <base/vmath.h>

#include <algorithm>
#include <iterator>

#include "staff_ind.h"

CStaffInd::CStaffInd(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_STAFF_IND, Pos)
{
	m_Owner = Owner;

	m_Dist = 0.f;
	m_BallFirst = true;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChar ? pOwnerChar->TeamMask() : CClientMask();

	for(int i = 0; i < NUM_IDS; i++)
		m_aIds[i] = Server()->SnapNewId();
	std::sort(std::begin(m_aIds), std::end(m_aIds));
	GameWorld()->InsertEntity(this);
}

void CStaffInd::Reset()
{
	for(int i = 0; i < NUM_IDS; i++)
		Server()->SnapFreeId(m_aIds[i]);

	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CStaffInd::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(!pOwner)
		return;

	if(!pOwner->GetPlayer()->m_Cosmetics.m_StaffInd)
	{
		Reset();
		return;
	}

	m_TeamMask = pOwner->TeamMask();
	m_Pos = pOwner->GetPos();
	m_aPos[ARMOR] = vec2(m_Pos.x, m_Pos.y - 70.f);

	if(m_BallFirst)
	{
		m_Dist += 0.9f;
		if(m_Dist > 25.f)
			m_BallFirst = false;
	}
	else
	{
		m_Dist -= 0.9f;
		if(m_Dist < -25.f)
			m_BallFirst = true;
	}

	m_aPos[BALL] = vec2(m_Pos.x + m_Dist, m_aPos[ARMOR].y);
}

void CStaffInd::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	CPlayer *pSnapPlayer = GameServer()->m_apPlayers[SnappingClient];

	if(!pOwnerChr || !pSnapPlayer)
		return;

	if(pSnapPlayer->m_HideCosmetics)
		return;

	CGameTeams Teams = GameServer()->m_pController->Teams();
	int Team = pOwnerChr->Team();

	if(!Teams.SetMask(SnappingClient, Team))
		return;

	if(pSnapPlayer->GetCharacter() && pOwnerChr)
		if(!pOwnerChr->CanSnapCharacter(SnappingClient))
			return;

	if(pOwnerChr->GetPlayer()->m_Vanish && SnappingClient != pOwnerChr->GetPlayer()->GetCid() && SnappingClient != -1)
		if(!pSnapPlayer->m_Vanish && Server()->GetAuthedState(SnappingClient) < AUTHED_ADMIN)
			return;

	const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
	const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);

	CNetObj_DDNetPickup *pPickup = Server()->SnapNewItem<CNetObj_DDNetPickup>(m_aIds[ARMOR]);
	if(pPickup)
	{
		vec2 Pos = m_aPos[ARMOR];
		if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient)
		{
			vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
			Pos += nVel;
		}

		pPickup->m_X = round_to_int(Pos.x);
		pPickup->m_Y = round_to_int(Pos.y);
		pPickup->m_Type = POWERUP_ARMOR;
		pPickup->m_Flags = PICKUPFLAG_NO_PREDICT;
	}

	// m_ID is created before m_aID is created, means that id is lower and we can simply use it to make the ball behind
	CNetObj_DDNetLaser *pLaser = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, m_BallFirst ? m_aIds[BALL_FRONT] : m_aIds[BALL], sizeof(CNetObj_DDNetLaser)));
	if(pLaser)
	{
		vec2 Pos = m_aPos[BALL];
		if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient)
		{
			vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;
			Pos += nVel;
		}
		pLaser->m_ToX = round_to_int(Pos.x);
		pLaser->m_ToY = round_to_int(Pos.y);
		pLaser->m_FromX = round_to_int(Pos.x);
		pLaser->m_FromY = round_to_int(Pos.y);
		pLaser->m_StartTick = Server()->Tick();
		pLaser->m_Type = LASERTYPE_PLASMA;
		pLaser->m_Owner = m_Owner;
	}
}
