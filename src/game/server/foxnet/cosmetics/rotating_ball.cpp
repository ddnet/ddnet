#include "game/server/entities/character.h"
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>
#include <game/server/teams.h>

#include <engine/shared/config.h>

#include <base/vmath.h>

#include "rotating_ball.h"

CRotatingBall::CRotatingBall(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_ROTATING_BALL, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;

	m_IsRotating = true;

	m_RotateDelay = Server()->TickSpeed() + 10;
	m_LaserDirAngle = 0;
	m_LaserInputDir = 0;

	m_TableDirV[0][0] = 5;
	m_TableDirV[0][1] = 12;
	m_TableDirV[1][0] = -12;
	m_TableDirV[1][1] = -5;

	m_Id1 = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

void CRotatingBall::Reset()
{
	Server()->SnapFreeId(m_Id1);

	Server()->SnapFreeId(GetId());
	GameWorld()->RemoveEntity(this);
}

void CRotatingBall::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;
	if(!pOwner->GetPlayer()->m_Cosmetics.m_RotatingBall)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();

	m_RotateDelay--;
	if(m_RotateDelay <= 0)
	{
		m_IsRotating ^= true;

		int DirSelect = rand() % 2;
		m_LaserInputDir = rand() % (m_TableDirV[DirSelect][1] - m_TableDirV[DirSelect][0] + 1) + m_TableDirV[DirSelect][0];
		m_RotateDelay = m_IsRotating ? Server()->TickSpeed() + (rand() % (7 - 3 + 1) + 3) : Server()->TickSpeed() + (rand() % (20 - 5 + 1) + 5);
	}

	if(m_IsRotating)
		m_LaserDirAngle += m_LaserInputDir;

	m_LaserPos.x = pOwner->GetPos().x + 65 * sin(m_LaserDirAngle * pi / 180.0f);
	m_LaserPos.y = pOwner->GetPos().y + 65 * cos(m_LaserDirAngle * pi / 180.0f);

	m_ProjPos.x = m_LaserPos.x + 22 * sin(Server()->Tick() * 13 * pi / 180.0f);
	m_ProjPos.y = m_LaserPos.y + 22 * cos(Server()->Tick() * 13 * pi / 180.0f);
}

void CRotatingBall::Snap(int SnappingClient)
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

	const int SnapVer = Server()->GetClientVersion(SnappingClient);
	const bool SixUp = Server()->IsSixup(SnappingClient);
	const double Pred = pOwnerChr->GetPlayer()->m_PredLatency;
	const float dist = distance(pOwnerChr->m_Pos, pOwnerChr->m_PrevPos);
	const vec2 nVel = normalize(pOwnerChr->GetVelocity()) * Pred * dist / 2.0f;

	vec2 Pos = m_ProjPos + pOwnerChr->GetVelocity();
	vec2 LaserPos = m_LaserPos + pOwnerChr->GetVelocity();
	if(g_Config.m_SvExperimentalPrediction && m_Owner == SnappingClient && !pOwnerChr->GetPlayer()->IsPaused())
	{
		Pos = m_ProjPos + nVel;
		LaserPos = m_LaserPos + nVel;
	}
	GameServer()->SnapLaserObject(CSnapContext(SnapVer, SixUp, SnappingClient), GetId(), LaserPos, LaserPos, Server()->Tick(), m_Owner, LASERTYPE_GUN, -1, -1, LASERFLAG_NO_PREDICT);

	CNetObj_DDNetProjectile *pProj = Server()->SnapNewItem<CNetObj_DDNetProjectile>(m_Id1);
	if(!pProj)
		return;

	pProj->m_X = round_to_int(Pos.x * 100.0f);
	pProj->m_Y = round_to_int(Pos.y * 100.0f);
	pProj->m_Type = WEAPON_HAMMER;
	pProj->m_Owner = m_Owner;
	pProj->m_StartTick = 0;
	pProj->m_VelX = 0;
	pProj->m_VelY = 0;
}
