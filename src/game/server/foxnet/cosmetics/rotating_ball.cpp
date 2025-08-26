#include "rotating_ball.h"
#include "game/server/entities/character.h"
#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/player.h>

CRotatingBall::CRotatingBall(CGameWorld *pGameWorld, int Owner, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_ROTATING_BALL, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;

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

	CNetObj_DDNetLaser *pLaser = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetId(), sizeof(CNetObj_DDNetLaser)));
	if(!pLaser)
		return;

	{
		vec2 Pos = m_LaserPos;
		if(m_Owner == SnappingClient)
		{
			float Lat = std::clamp((float)pOwnerChar->GetPlayer()->m_Latency.m_Avg, 0.0f, 125.0f) / 27.77f;
			Pos.x = m_LaserPos.x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos.y = m_LaserPos.y + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
		}

		pLaser->m_ToX = round_to_int(Pos.x);
		pLaser->m_ToY = round_to_int(Pos.y);
		pLaser->m_FromX = round_to_int(Pos.x);
		pLaser->m_FromY = round_to_int(Pos.y);
		pLaser->m_StartTick = Server()->Tick();
		pLaser->m_Owner = m_Owner;
		pLaser->m_Flags = LASERFLAG_NO_PREDICT;
	}

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_Id1, sizeof(CNetObj_Projectile)));
	if(!pProj)
		return;

	{
		vec2 Pos = m_ProjPos;
		if(m_Owner == SnappingClient)
		{
			float Lat = std::clamp((float)(pOwnerChar->GetPlayer()->m_Latency.m_Avg + pOwnerChar->GetPlayer()->m_ExtraPing), 0.0f, 125.0f) / 27.77f;
			Pos.x = m_ProjPos.x + std::clamp(pOwnerChar->Core()->m_Vel.x, -100.0f, 100.0f) * std::clamp(Lat, 0.0f, 150.0f);
			Pos.y = m_ProjPos.y + std::clamp(pOwnerChar->Core()->m_Vel.y, -15.0f, 15.0f) * std::clamp(Lat, 0.0f, 150.0f);
		}

		pProj->m_X = round_to_int(Pos.x);
		pProj->m_Y = round_to_int(Pos.y);
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = 0;
		pProj->m_Type = WEAPON_HAMMER;
	}
}
