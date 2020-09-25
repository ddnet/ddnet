/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "projectile.h"
#include <game/generated/protocol.h>

#include <engine/shared/config.h>

CProjectile::CProjectile(
	CGameWorld *pGameWorld,
	int Type,
	int Owner,
	vec2 Pos,
	vec2 Dir,
	int Span,
	bool Freeze,
	bool Explosive,
	float Force,
	int SoundImpact,
	int Layer,
	int Number) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Type = Type;
	m_Pos = Pos;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	m_SoundImpact = SoundImpact;
	m_StartTick = GameWorld()->GameTick();
	m_Explosive = Explosive;

	m_Layer = Layer;
	m_Number = Number;
	m_Freeze = Freeze;

	m_TuneZone = GameWorld()->m_WorldConfig.m_PredictTiles ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0;

	GameWorld()->InsertEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;
	CTuningParams *pTuning = GetTuning(m_TuneZone);

	switch(m_Type)
	{
	case WEAPON_GRENADE:
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
		break;

	case WEAPON_SHOTGUN:
		Curvature = pTuning->m_ShotgunCurvature;
		Speed = pTuning->m_ShotgunSpeed;
		break;

	case WEAPON_GUN:
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
		break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}

void CProjectile::Tick()
{
	float Pt = (GameWorld()->GameTick() - m_StartTick - 1) / (float)GameWorld()->GameTickSpeed();
	float Ct = (GameWorld()->GameTick() - m_StartTick) / (float)GameWorld()->GameTickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = Collision()->IntersectLine(PrevPos, CurPos, &ColPos, &NewPos);
	CCharacter *pOwnerChar = GameWorld()->GetCharacterByID(m_Owner);

	CCharacter *pTargetChr = GameWorld()->IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar, m_Owner);

	if(GameWorld()->m_WorldConfig.m_IsSolo && !(m_Type == WEAPON_SHOTGUN && GameWorld()->m_WorldConfig.m_IsDDRace))
		pTargetChr = 0;

	if(m_LifeSpan > -1)
		m_LifeSpan--;

	int64 TeamMask = -1LL;
	bool isWeaponCollide = false;
	if(
		pOwnerChar &&
		pTargetChr &&
		pOwnerChar->IsAlive() &&
		pTargetChr->IsAlive() &&
		!pTargetChr->CanCollide(m_Owner))
	{
		isWeaponCollide = true;
	}

	if(((pTargetChr && (pOwnerChar ? !(pOwnerChar->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : g_Config.m_SvHit || m_Owner == -1 || pTargetChr == pOwnerChar)) || Collide || GameLayerClipped(CurPos)) && !isWeaponCollide)
	{
		if(m_Explosive && (!pTargetChr || (pTargetChr && (!m_Freeze || (m_Type == WEAPON_SHOTGUN && Collide)))))
		{
			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()),
				(m_Owner != -1) ? TeamMask : -1LL);
		}
		else if(pTargetChr && m_Freeze && ((m_Layer == LAYER_SWITCH && Collision()->m_pSwitchers[m_Number].m_Status[pTargetChr->Team()]) || m_Layer != LAYER_SWITCH))
			pTargetChr->Freeze();
		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = GameWorld()->GameTick();
			m_Pos = NewPos + (-(m_Direction * 4));
			if(m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if(fabs(m_Direction.x) < 1e-6)
				m_Direction.x = 0;
			if(fabs(m_Direction.y) < 1e-6)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if(m_Type == WEAPON_GUN)
		{
			GameWorld()->DestroyEntity(this);
		}
		else if(!m_Freeze)
			GameWorld()->DestroyEntity(this);
	}
	if(m_LifeSpan == -1)
	{
		if(m_Explosive)
		{
			if(m_Owner >= 0)
				pOwnerChar = GameWorld()->GetCharacterByID(m_Owner);

			int64 TeamMask = -1LL;

			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()),
				(m_Owner != -1) ? TeamMask : -1LL);
		}
		GameWorld()->DestroyEntity(this);
	}
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

CProjectile::CProjectile(CGameWorld *pGameWorld, int ID, CNetObj_Projectile *pProj) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	ExtractInfo(pProj, &m_Pos, &m_Direction);
	if(UseExtraInfo(pProj))
		ExtractExtraInfo(pProj, &m_Owner, &m_Explosive, &m_Bouncing, &m_Freeze);
	else
	{
		m_Owner = -1;
		m_Bouncing = m_Freeze = 0;
		m_Explosive = (pProj->m_Type == WEAPON_GRENADE) && (fabs(1.0f - length(m_Direction)) < 0.015f);
	}
	m_Type = pProj->m_Type;
	m_StartTick = pProj->m_StartTick;
	m_TuneZone = GameWorld()->m_WorldConfig.m_PredictTiles ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0;

	int Lifetime = 20 * GameWorld()->GameTickSpeed();
	m_SoundImpact = -1;
	if(m_Type == WEAPON_GRENADE)
	{
		Lifetime = GetTuning(m_TuneZone)->m_GrenadeLifetime * GameWorld()->GameTickSpeed();
		m_SoundImpact = SOUND_GRENADE_EXPLODE;
	}
	else if(m_Type == WEAPON_GUN)
		Lifetime = GetTuning(m_TuneZone)->m_GunLifetime * GameWorld()->GameTickSpeed();
	else if(m_Type == WEAPON_SHOTGUN && !GameWorld()->m_WorldConfig.m_IsDDRace)
		Lifetime = GetTuning(m_TuneZone)->m_ShotgunLifetime * GameWorld()->GameTickSpeed();
	m_LifeSpan = Lifetime - (pGameWorld->GameTick() - m_StartTick);
	m_ID = ID;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x * 100.0f);
	pProj->m_VelY = (int)(m_Direction.y * 100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::FillExtraInfo(CNetObj_Projectile *pProj)
{
	const int MaxPos = 0x7fffffff / 100;
	if(abs((int)m_Pos.y) + 1 >= MaxPos || abs((int)m_Pos.x) + 1 >= MaxPos)
	{
		//If the modified data would be too large to fit in an integer, send normal data instead
		FillInfo(pProj);
		return;
	}
	//Send additional/modified info, by modifiying the fields of the netobj
	float Angle = -atan2f(m_Direction.x, m_Direction.y);

	int Data = 0;
	Data |= (abs(m_Owner) & 255) << 0;
	if(m_Owner < 0)
		Data |= 1 << 8;
	Data |= 1 << 9; //This bit tells the client to use the extra info
	Data |= (m_Bouncing & 3) << 10;
	if(m_Explosive)
		Data |= 1 << 12;
	if(m_Freeze)
		Data |= 1 << 13;

	pProj->m_X = (int)(m_Pos.x * 100.0f);
	pProj->m_Y = (int)(m_Pos.y * 100.0f);
	pProj->m_VelX = (int)(Angle * 1000000.0f);
	pProj->m_VelY = Data;
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

bool CProjectile::Match(CProjectile *pProj)
{
	if(pProj->m_Type != m_Type)
		return false;
	if(pProj->m_StartTick != m_StartTick)
		return false;
	if(distance(pProj->m_Pos, m_Pos) > 2.f)
		return false;
	if(distance(pProj->m_Direction, m_Direction) > 2.f)
		return false;
	return true;
}

int CProjectile::NetworkClipped(vec2 ViewPos)
{
	float Ct = (GameWorld()->GameTick() - m_StartTick) / (float)GameWorld()->GameTickSpeed();
	return ((CEntity *)this)->NetworkClipped(GetPos(Ct), ViewPos);
}
