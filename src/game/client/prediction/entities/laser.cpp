/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser.h"
#include "character.h"
#include <game/generated/protocol.h>

#include <engine/shared/config.h>

CLaser::CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Type) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos = Pos;
	m_Owner = Owner;
	m_Energy = StartEnergy;
	if(pGameWorld->m_WorldConfig.m_IsFNG && m_Energy < 10.f)
		m_Energy = 800.0f;
	m_Dir = Direction;
	m_Bounces = 0;
	m_EvalTick = 0;
	m_TelePos = vec2(0, 0);
	m_WasTele = false;
	m_Type = Type;
	m_TuneZone = GameWorld()->m_WorldConfig.m_PredictTiles ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0;
	GameWorld()->InsertEntity(this);
	DoBounce();
}

bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	CCharacter *pOwnerChar = GameWorld()->GetCharacterByID(m_Owner);
	CCharacter *pHit;
	bool pDontHitSelf = g_Config.m_SvOldLaser || (m_Bounces == 0 && !m_WasTele);

	if(pOwnerChar ? (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_LASER) && m_Type == WEAPON_LASER) || (!(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN) && m_Type == WEAPON_SHOTGUN) : g_Config.m_SvHit)
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner);
	else
		pHit = GameWorld()->IntersectCharacter(m_Pos, To, 0.f, At, pDontHitSelf ? pOwnerChar : 0, m_Owner, pOwnerChar);

	if(!pHit || (pHit == pOwnerChar && g_Config.m_SvOldLaser) || (pHit != pOwnerChar && pOwnerChar ? (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_LASER && m_Type == WEAPON_LASER) || (pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_SHOTGUN && m_Type == WEAPON_SHOTGUN) : !g_Config.m_SvHit))
		return false;
	m_From = From;
	m_Pos = At;
	m_Energy = -1;
	if(m_Type == WEAPON_SHOTGUN)
	{
		vec2 Temp;

		float Strength = GetTuning(m_TuneZone)->m_ShotgunStrength;

		if(!g_Config.m_SvOldLaser)
			Temp = pHit->Core()->m_Vel + normalize(m_PrevPos - pHit->Core()->m_Pos) * Strength;
		else
			Temp = pHit->Core()->m_Vel + normalize(pOwnerChar->Core()->m_Pos - pHit->Core()->m_Pos) * Strength;
		pHit->Core()->m_Vel = ClampVel(pHit->m_MoveRestrictions, Temp);
	}
	else if(m_Type == WEAPON_LASER)
	{
		pHit->UnFreeze();
	}
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = GameWorld()->GameTick();

	if(m_Energy < 0)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}
	m_PrevPos = m_Pos;
	vec2 Coltile;

	int Res;
	int z;

	if(m_WasTele)
	{
		m_PrevPos = m_TelePos;
		m_Pos = m_TelePos;
		m_TelePos = vec2(0, 0);
	}

	vec2 To = m_Pos + m_Dir * m_Energy;

	Res = Collision()->IntersectLineTeleWeapon(m_Pos, To, &Coltile, &To, &z);

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
				f = Collision()->GetTile(round_to_int(Coltile.x), round_to_int(Coltile.y));
				Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), TILE_SOLID);
			}
			Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			if(Res == -1)
			{
				Collision()->SetCollisionAt(round_to_int(Coltile.x), round_to_int(Coltile.y), f);
			}
			m_Pos = TempPos;
			m_Dir = normalize(TempDir);

			m_Energy -= distance(m_From, m_Pos) + GetTuning(m_TuneZone)->m_LaserBounceCost;

			m_Bounces++;
			m_WasTele = false;

			int BounceNum = GetTuning(m_TuneZone)->m_LaserBounceNum;

			if(m_Bounces > BounceNum)
				m_Energy = -1;
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
}

void CLaser::Tick()
{
	float Delay = GetTuning(m_TuneZone)->m_LaserBounceDelay;

	if(GameWorld()->m_WorldConfig.m_IsVanilla) // predict old physics on vanilla 0.6 servers
	{
		if(GameWorld()->GameTick() > m_EvalTick + (GameWorld()->GameTickSpeed() * Delay / 1000.0f))
			DoBounce();
	}
	else
	{
		if((GameWorld()->GameTick() - m_EvalTick) > (GameWorld()->GameTickSpeed() * Delay / 1000.0f))
			DoBounce();
	}
}

CLaser::CLaser(CGameWorld *pGameWorld, int ID, CNetObj_Laser *pLaser) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{
	m_Pos.x = pLaser->m_X;
	m_Pos.y = pLaser->m_Y;
	m_From.x = pLaser->m_FromX;
	m_From.y = pLaser->m_FromY;
	m_EvalTick = pLaser->m_StartTick;
	m_TuneZone = GameWorld()->m_WorldConfig.m_PredictTiles ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0;
	m_Owner = -2;
	m_Energy = GetTuning(m_TuneZone)->m_LaserReach;
	if(pGameWorld->m_WorldConfig.m_IsFNG && m_Energy < 10.f)
		m_Energy = 800.0f;

	m_Dir = m_Pos - m_From;
	if(length(m_Pos - m_From) > 0.001f)
		m_Dir = normalize(m_Dir);
	else
		m_Energy = 0;
	m_Type = WEAPON_LASER;
	m_PrevPos = m_From;
	m_ID = ID;
}

void CLaser::FillInfo(CNetObj_Laser *pLaser)
{
	pLaser->m_X = (int)m_Pos.x;
	pLaser->m_Y = (int)m_Pos.y;
	pLaser->m_FromX = (int)m_From.x;
	pLaser->m_FromY = (int)m_From.y;
	pLaser->m_StartTick = m_EvalTick;
}

bool CLaser::Match(CLaser *pLaser)
{
	if(pLaser->m_EvalTick != m_EvalTick)
		return false;
	if(distance(pLaser->m_From, m_From) > 2.f)
		return false;
	const vec2 ThisDiff = m_Pos - m_From;
	const vec2 OtherDiff = pLaser->m_Pos - pLaser->m_From;
	const float DirError = distance(normalize(OtherDiff) * length(ThisDiff), ThisDiff);
	if(DirError > 2.f)
		return false;
	return true;
}
