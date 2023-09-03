/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "plasma.h"
#include "character.h"

#include <engine/server.h>

#include <game/generated/protocol.h>
#include <game/teamscore.h>

#include <game/server/gamecontext.h>

const float PLASMA_ACCEL = 1.1f;

CPlasma::CPlasma(CGameWorld *pGameWorld, vec2 Pos, vec2 Dir, bool Freeze,
	bool Explosive, int ForClientID) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Core = Dir;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_ForClientID = ForClientID;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * 1.5f;

	GameWorld()->InsertEntity(this);
}

void CPlasma::Tick()
{
	// A plasma bullet has only a limited lifetime
	if(m_LifeTime == 0)
	{
		Reset();
		return;
	}
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_ForClientID);
	// Without a target, a plasma bullet has no reason to live
	if(!pTarget)
	{
		Reset();
		return;
	}
	m_LifeTime--;
	Move();
	HitCharacter(pTarget);
	// Plasma bullets may explode twice if they would hit both a player and an obstacle in the next move step
	HitObstacle(pTarget);
}

void CPlasma::Move()
{
	m_Pos += m_Core;
	m_Core *= PLASMA_ACCEL;
}

bool CPlasma::HitCharacter(CCharacter *pTarget)
{
	vec2 IntersectPos;
	CCharacter *pHitPlayer = GameServer()->m_World.IntersectCharacter(
		m_Pos, m_Pos + m_Core, 0.0f, IntersectPos, 0, m_ForClientID);
	if(!pHitPlayer)
	{
		return false;
	}

	// Super player should not be able to stop the plasma bullets
	if(pHitPlayer->Team() == TEAM_SUPER)
	{
		return false;
	}

	m_Freeze ? pHitPlayer->Freeze() : pHitPlayer->UnFreeze();
	if(m_Explosive)
	{
		// Plasma Turrets are very precise weapons only one tee gets speed from it,
		// other tees near the explosion remain unaffected
		GameServer()->CreateExplosion(
			m_Pos, m_ForClientID, WEAPON_GRENADE, true, pTarget->Team(), pTarget->TeamMask());
	}
	Reset();
	return true;
}

bool CPlasma::HitObstacle(CCharacter *pTarget)
{
	// Check if the plasma bullet is stopped by a solid block or a laser stopper
	int HasIntersection = GameServer()->Collision()->IntersectNoLaser(m_Pos, m_Pos + m_Core, 0, 0);
	if(HasIntersection)
	{
		if(m_Explosive)
		{
			// Even in the case of an explosion due to a collision with obstacles, only one player is affected
			GameServer()->CreateExplosion(
				m_Pos, m_ForClientID, WEAPON_GRENADE, true, pTarget->Team(), pTarget->TeamMask());
		}
		Reset();
		return true;
	}
	return false;
}

void CPlasma::Reset()
{
	m_MarkedForDestroy = true;
}

void CPlasma::Snap(int SnappingClient)
{
	// Only players who can see the targeted player can see the plasma bullet
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_ForClientID);
	if(!pTarget || !pTarget->CanSnapCharacter(SnappingClient))
	{
		return;
	}

	// Only players with the plasma bullet in their field of view or who want to see everything will receive the snap
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(
		NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_Pos.x;
	pObj->m_FromY = (int)m_Pos.y;
	pObj->m_StartTick = m_EvalTick;
}
