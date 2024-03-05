/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/client/projectile_data.h>
#include <game/collision.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>

#include "character.h"
#include "projectile.h"

CProjectile::CProjectile(
	CGameWorld *pGameWorld,
	int Type,
	int Owner,
	vec2 Pos,
	vec2 Dir,
	int Span,
	bool Freeze,
	bool Explosive,
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
	m_SoundImpact = SoundImpact;
	m_StartTick = GameWorld()->GameTick();
	m_Explosive = Explosive;

	m_Layer = Layer;
	m_Number = Number;
	m_Freeze = Freeze;

	m_TuneZone = GameWorld()->m_WorldConfig.m_UseTuneZones ? Collision()->IsTune(Collision()->GetMapIndex(m_Pos)) : 0;

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
	CCharacter *pOwnerChar = GameWorld()->GetCharacterById(m_Owner);

	CCharacter *pTargetChr = GameWorld()->IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar, m_Owner);

	if(GameWorld()->m_WorldConfig.m_IsSolo && !(m_Type == WEAPON_SHOTGUN && GameWorld()->m_WorldConfig.m_IsDDRace))
		pTargetChr = 0;

	if(m_LifeSpan > -1)
		m_LifeSpan--;

	bool isWeaponCollide = false;
	if(
		pOwnerChar &&
		pTargetChr &&
		!pTargetChr->CanCollide(m_Owner))
	{
		isWeaponCollide = true;
	}

	if(((pTargetChr && (pOwnerChar ? !pOwnerChar->GrenadeHitDisabled() : g_Config.m_SvHit || m_Owner == -1 || pTargetChr == pOwnerChar)) || Collide || GameLayerClipped(CurPos)) && !isWeaponCollide)
	{
		if(m_Explosive && (!pTargetChr || (!m_Freeze || (m_Type == WEAPON_SHOTGUN && Collide))))
		{
			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()), CClientMask().set());
		}
		else if(m_Freeze)
		{
			CEntity *apEnts[MAX_CLIENTS];
			int Num = GameWorld()->FindEntities(CurPos, 1.0f, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; ++i)
			{
				auto *pChr = static_cast<CCharacter *>(apEnts[i]);
				if(pChr && (m_Layer != LAYER_SWITCH || (m_Layer == LAYER_SWITCH && m_Number > 0 && m_Number < (int)Switchers().size() && Switchers()[m_Number].m_aStatus[pChr->Team()])))
					pChr->Freeze();
			}
		}
		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = GameWorld()->GameTick();
			m_Pos = NewPos + (-(m_Direction * 4));
			if(m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if(absolute(m_Direction.x) < 1e-6f)
				m_Direction.x = 0;
			if(absolute(m_Direction.y) < 1e-6f)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if(m_Type == WEAPON_GUN)
		{
			m_MarkedForDestroy = true;
		}
		else if(!m_Freeze)
			m_MarkedForDestroy = true;
	}
	if(m_LifeSpan == -1)
	{
		if(m_Explosive)
		{
			if(m_Owner >= 0)
				pOwnerChar = GameWorld()->GetCharacterById(m_Owner);

			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Type, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()), CClientMask().set());
		}
		m_MarkedForDestroy = true;
	}
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

CProjectile::CProjectile(CGameWorld *pGameWorld, int Id, const CProjectileData *pProj) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Pos = pProj->m_StartPos;
	m_Direction = pProj->m_StartVel;
	if(pProj->m_ExtraInfo)
	{
		m_Owner = pProj->m_Owner;
		m_Explosive = pProj->m_Explosive;
		m_Bouncing = pProj->m_Bouncing;
		m_Freeze = pProj->m_Freeze;
	}
	else
	{
		m_Owner = -1;
		m_Bouncing = 0;
		m_Freeze = false;
		m_Explosive = (pProj->m_Type == WEAPON_GRENADE) && (absolute(1.0f - length(m_Direction)) < 0.015f);
	}
	m_Type = pProj->m_Type;
	m_StartTick = pProj->m_StartTick;
	m_TuneZone = pProj->m_TuneZone;

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
	m_Id = Id;
	m_Number = pProj->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;
}

CProjectileData CProjectile::GetData() const
{
	CProjectileData Result;
	Result.m_StartPos = m_Pos;
	Result.m_StartVel = m_Direction;
	Result.m_Type = m_Type;
	Result.m_StartTick = m_StartTick;
	Result.m_ExtraInfo = true;
	Result.m_Owner = m_Owner;
	Result.m_Explosive = m_Explosive;
	Result.m_Bouncing = m_Bouncing;
	Result.m_Freeze = m_Freeze;
	Result.m_TuneZone = m_TuneZone;
	Result.m_SwitchNumber = m_Number;
	return Result;
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
