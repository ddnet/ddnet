/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
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
	m_TelePos = vec2(0,0);
	m_WasTele = false;
	m_Type = Type;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	m_TeamMask = GameServer()->GetPlayerChar(Owner) ? GameServer()->GetPlayerChar(Owner)->Teams()->TeamMask(GameServer()->GetPlayerChar(Owner)->Team(), -1, m_Owner) : 0;
	GameWorld()->InsertEntity(this);
	DoBounce();
}


bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *pHit;
	bool pDontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);

	if(pOwnerChar ? (!(pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_RIFLE) && m_Type == WEAPON_RIFLE) || (!(pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_SHOTGUN) && m_Type == WEAPON_SHOTGUN) : g_Config.m_SvHit)
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner);
	else
		pHit = GameServer()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner, pOwnerChar);

	if(!pHit || (pHit == pOwnerChar && g_Config.m_SvOldLaser) || (pHit != pOwnerChar && pOwnerChar ? (pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_RIFLE  && m_Type == WEAPON_RIFLE) || (pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_SHOTGUN && m_Type == WEAPON_SHOTGUN) : !g_Config.m_SvHit))
		return false;
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if (m_Type == WEAPON_SHOTGUN)
	{
		vec2 Temp;

		float Strength;
		if (!m_TuneZone)
			Strength = GameServer()->Tuning()->m_ShotgunStrength;
		else
			Strength = GameServer()->TuningList()[m_TuneZone].m_ShotgunStrength;

		if(!g_Config.m_SvOldLaser)
			Temp = pHit->Core()->m_Vel + normalize(m_PrevPos - pHit->Core()->m_Pos) * Strength;
		else
			Temp = pHit->Core()->m_Vel + normalize(pOwnerChar->Core()->m_Pos - pHit->Core()->m_Pos) * Strength;
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
	else if (m_Type == WEAPON_RIFLE)
	{
		pHit->UnFreeze();
	}
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0)
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}
	m_PrevPos = m_Pos;
	vec2 Coltile;

	int Res;
	int z;

	if (m_WasTele)
	{
		m_PrevPos = m_TelePos;
		m_Pos = m_TelePos;
		m_TelePos = vec2(0,0);
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	Res = GameServer()->Collision()->IntersectLineTeleWeapon(m_Pos, To, &Coltile, &To, &z, false);

	if(Res)
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_From = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Dir * 4.0f;

			int f = 0;
			if(Res == -1)
			{
				f = GameServer()->Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), CCollision::COLFLAG_SOLID);
			}
			GameServer()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			if(Res == -1)
			{
				GameServer()->Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
			}
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			if (!m_TuneZone)
				m_Energy -= distance(m_From, m_Pos) + GameServer()->Tuning()->m_LaserBounceCost;
			else
				m_Energy -= distance(m_From, m_Pos) + GameServer()->TuningList()[m_TuneZone].m_LaserBounceCost;

			if (Res&CCollision::COLFLAG_TELE && ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1].size())
			{
				int Num = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1].size();
				m_TelePos = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1][(!Num)?Num:rand() % Num];
				m_WasTele = true;
			}
			else
			{
				m_Bounces++;
				m_WasTele = false;
			}
			
			int BounceNum = GameServer()->Tuning()->m_LaserBounceNum;
			if (m_TuneZone)
				BounceNum = GameServer()->TuningList()[m_TuneZone].m_LaserBounceNum;
			
			if(m_Bounces > BounceNum)
				m_Energy = -1;

			GameServer()->CreateSound(m_Pos, SOUND_RIFLE_BOUNCE, m_TeamMask);
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
	float Delay;
	if (m_TuneZone)
		Delay = GameServer()->TuningList()[m_TuneZone].m_LaserBounceDelay;
	else
		Delay = GameServer()->Tuning()->m_LaserBounceDelay;
		
	if(Server()->Tick() > m_EvalTick+(Server()->TickSpeed()*Delay/1000.0f))
		DoBounce();	
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CCharacter * OwnerChar = 0;
	if(m_Owner >= 0)
		OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(!OwnerChar)
		return;

	CCharacter *pOwnerChar = 0;
	int64_t TeamMask = -1LL;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if (pOwnerChar && pOwnerChar->IsAlive())
			TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);

	if(!CmaskIsSet(TeamMask, SnappingClient))
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
