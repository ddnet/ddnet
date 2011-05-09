/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "laser.h"

#include <engine/shared/config.h>
#include <game/server/teams.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_Type = Type;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit;

	if(g_Config.m_SvHit)
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, g_Config.m_SvOldLaser || m_Bounces == 0 ? pOwnerChar : 0, m_Owner);
	else
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, g_Config.m_SvOldLaser || m_Bounces == 0 ? pOwnerChar : 0, m_Owner, pOwnerChar);

	if(!pHit || (pHit == pOwnerChar && g_Config.m_SvOldLaser) || (pHit != pOwnerChar && !g_Config.m_SvHit))
		return false;
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if (m_Type == 1)
	{
		vec2 Temp;
		if(!g_Config.m_SvOldLaser)
			Temp = pHit->Core()->m_Vel + normalize(m_PrevPos - pHit->Core()->m_Pos) * 10;
		else
			Temp = pHit->Core()->m_Vel + normalize(pOwnerChar->Core()->m_Pos - pHit->Core()->m_Pos) * 10;
		if(Temp.x > 0 && ((pHit->m_TileIndex == TILE_STOP && pHit->m_TileFlags == ROTATION_270) || (pHit->m_TileIndexL == TILE_STOP && pHit->m_TileFlagsL == ROTATION_270) || (pHit->m_TileIndexL == TILE_STOPS && (pHit->m_TileFlagsL == ROTATION_90 || pHit->m_TileFlagsL ==ROTATION_270)) || (pHit->m_TileIndexL == TILE_STOPA) || (pHit->m_TileFIndex == TILE_STOP && pHit->m_TileFFlags == ROTATION_270) || (pHit->m_TileFIndexL == TILE_STOP && pHit->m_TileFFlagsL == ROTATION_270) || (pHit->m_TileFIndexL == TILE_STOPS && (pHit->m_TileFFlagsL == ROTATION_90 || pHit->m_TileFFlagsL == ROTATION_270)) || (pHit->m_TileFIndexL == TILE_STOPA) || (pHit->m_TileSIndex == TILE_STOP && pHit->m_TileSFlags == ROTATION_270) || (pHit->m_TileSIndexL == TILE_STOP && pHit->m_TileSFlagsL == ROTATION_270) || (pHit->m_TileSIndexL == TILE_STOPS && (pHit->m_TileSFlagsL == ROTATION_90 || pHit->m_TileSFlagsL == ROTATION_270)) || (pHit->m_TileSIndexL == TILE_STOPA)))
			Temp.x = 0;
		if(Temp.x < 0 && ((pHit->m_TileIndex == TILE_STOP && pHit->m_TileFlags == ROTATION_90) || (pHit->m_TileIndexR == TILE_STOP && pHit->m_TileFlagsR == ROTATION_90) || (pHit->m_TileIndexR == TILE_STOPS && (pHit->m_TileFlagsR == ROTATION_90 || pHit->m_TileFlagsR == ROTATION_270)) || (pHit->m_TileIndexR == TILE_STOPA) || (pHit->m_TileFIndex == TILE_STOP && pHit->m_TileFFlags == ROTATION_90) || (pHit->m_TileFIndexR == TILE_STOP && pHit->m_TileFFlagsR == ROTATION_90) || (pHit->m_TileFIndexR == TILE_STOPS && (pHit->m_TileFFlagsR == ROTATION_90 || pHit->m_TileFFlagsR == ROTATION_270)) || (pHit->m_TileFIndexR == TILE_STOPA) || (pHit->m_TileSIndex == TILE_STOP && pHit->m_TileSFlags == ROTATION_90) || (pHit->m_TileSIndexR == TILE_STOP && pHit->m_TileSFlagsR == ROTATION_90) || (pHit->m_TileSIndexR == TILE_STOPS && (pHit->m_TileSFlagsR == ROTATION_90 || pHit->m_TileSFlagsR == ROTATION_270)) || (pHit->m_TileSIndexR == TILE_STOPA)))
			Temp.x = 0;
		if(Temp.y < 0 && ((pHit->m_TileIndex == TILE_STOP && pHit->m_TileFlags == ROTATION_180) || (pHit->m_TileIndexB == TILE_STOP && pHit->m_TileFlagsB == ROTATION_180) || (pHit->m_TileIndexB == TILE_STOPS && (pHit->m_TileFlagsB == ROTATION_0 || pHit->m_TileFlagsB == ROTATION_180)) || (pHit->m_TileIndexB == TILE_STOPA) || (pHit->m_TileFIndex == TILE_STOP && pHit->m_TileFFlags == ROTATION_180) || (pHit->m_TileFIndexB == TILE_STOP && pHit->m_TileFFlagsB == ROTATION_180) || (pHit->m_TileFIndexB == TILE_STOPS && (pHit->m_TileFFlagsB == ROTATION_0 || pHit->m_TileFFlagsB == ROTATION_180)) || (pHit->m_TileFIndexB == TILE_STOPA) || (pHit->m_TileSIndex == TILE_STOP && pHit->m_TileSFlags == ROTATION_180) || (pHit->m_TileSIndexB == TILE_STOP && pHit->m_TileSFlagsB == ROTATION_180) || (pHit->m_TileSIndexB == TILE_STOPS && (pHit->m_TileSFlagsB == ROTATION_0 || pHit->m_TileSFlagsB == ROTATION_180)) || (pHit->m_TileSIndexB == TILE_STOPA)))
			Temp.y = 0;
		if(Temp.y > 0 && ((pHit->m_TileIndex == TILE_STOP && pHit->m_TileFlags == ROTATION_0) || (pHit->m_TileIndexT == TILE_STOP && pHit->m_TileFlagsT == ROTATION_0) || (pHit->m_TileIndexT == TILE_STOPS && (pHit->m_TileFlagsT == ROTATION_0 || pHit->m_TileFlagsT == ROTATION_180)) || (pHit->m_TileIndexT == TILE_STOPA) || (pHit->m_TileFIndex == TILE_STOP && pHit->m_TileFFlags == ROTATION_0) || (pHit->m_TileFIndexT == TILE_STOP && pHit->m_TileFFlagsT == ROTATION_0) || (pHit->m_TileFIndexT == TILE_STOPS && (pHit->m_TileFFlagsT == ROTATION_0 || pHit->m_TileFFlagsT == ROTATION_180)) || (pHit->m_TileFIndexT == TILE_STOPA) || (pHit->m_TileSIndex == TILE_STOP && pHit->m_TileSFlags == ROTATION_0) || (pHit->m_TileSIndexT == TILE_STOP && pHit->m_TileSFlagsT == ROTATION_0) || (pHit->m_TileSIndexT == TILE_STOPS && (pHit->m_TileSFlagsT == ROTATION_0 || pHit->m_TileSFlagsT == ROTATION_180)) || (pHit->m_TileSIndexT == TILE_STOPA)))
			Temp.y = 0;
		pHit->Core()->m_Vel = Temp;
	}
	else if (m_Type == 0)
	{
		pHit->UnFreeze();
	}
	return true;
}

void CLaser::DoBounce()
{
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}
	m_PrevPos = m_Pos;
	vec2 To = m_Pos + m_Dir * m_Energy;
	vec2 Coltile;

	int Res;
	Res = GameServer()->Collision()->IntersectLine(m_Pos, To, &Coltile, &To,false);

	if(Res)
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			int f;
			if(Res == -1)
			{
				f = GameServer()->Collision()->GetTile(round(Coltile.x), round(Coltile.y));
				GameServer()->Collision()->SetCollisionAt(round(Coltile.x), round(Coltile.y), CCollision::COLFLAG_SOLID);
			}
			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			if(Res == -1)
			{
				GameServer()->Collision()->SetCollisionAt(round(Coltile.x), round(Coltile.y), f);
			}
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			m_Bounces++;

			if(m_Bounces > GameServer()->Tuning()->m_LaserBounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_RIFLE_BOUNCE, OwnerChar->Teams()->TeamMask(OwnerChar->Team()));
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_From = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
	//m_Owner = -1;
}

void CLaser::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CLaser::Tick()
{
	if(Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*GameServer()->Tuning()->m_LaserBounceDelay)/1000.0f)
		DoBounce();
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CCharacter * SnappingChar = GameServer()->GetPlayerChar(SnappingClient);
	CCharacter * OwnerChar = 0;
	if(m_Owner >= 0)
		OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(!OwnerChar)
		return;
	if(SnappingChar && !SnappingChar->CanCollide(m_Owner))
		return;
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)m_From.x;
	pObj->m_FromY = (int)m_From.y;
	pObj->m_StartTick = m_EvalTick;
}
