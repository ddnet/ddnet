/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include "projectile.h"

#include <engine/shared/config.h>

CProjectile::CProjectile
	(
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
		int Weapon,
		int Layer,
		int Number
	)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Type = Type;
	m_Pos = Pos;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	//m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = GameWorld()->GameTick();
	m_Explosive = Explosive;

	m_Layer = Layer;
	m_Number = Number;
	m_Freeze = Freeze;

	//m_TuneZone = Collision()->IsTune(Collision()->GetMapIndex(m_Pos));

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	if(m_LifeSpan > -2)
		GameWorld()->DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = Tuning()->m_GrenadeCurvature;
			Speed = Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = Tuning()->m_ShotgunCurvature;
			Speed = Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = Tuning()->m_GunCurvature;
			Speed = Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	float Pt = (GameWorld()->GameTick()-m_StartTick-1)/(float)GameWorld()->GameTickSpeed();
	float Ct = (GameWorld()->GameTick()-m_StartTick)/(float)GameWorld()->GameTickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = Collision()->IntersectLine(PrevPos, CurPos, &ColPos, &NewPos);
	CCharacter *pOwnerChar = GameWorld()->GetCharacterByID(m_Owner);

	CCharacter *pTargetChr = GameWorld()->IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar, m_Owner);

	if(m_LifeSpan > -1)
		m_LifeSpan--;

	int64_t TeamMask = -1LL;
	bool isWeaponCollide = false;
	if
	(
			pOwnerChar &&
			pTargetChr &&
			pOwnerChar->IsAlive() &&
			pTargetChr->IsAlive() &&
			!pTargetChr->CanCollide(m_Owner)
			)
	{
			isWeaponCollide = true;
			//TeamMask = OwnerChar->Teams()->TeamMask( OwnerChar->Team());
	}
	if (pOwnerChar && pOwnerChar->IsAlive())
	{
		//TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);
	}
	else if (m_Owner >= 0)
	{
		GameWorld()->DestroyEntity(this);
	}

	if( ((pTargetChr && (pOwnerChar ? !(pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_GRENADE) : g_Config.m_SvHit || m_Owner == -1 || pTargetChr == pOwnerChar)) || Collide || GameLayerClipped(CurPos)) && !isWeaponCollide)
	{
		if(m_Explosive/*??*/ && (!pTargetChr || (pTargetChr && (!m_Freeze || (m_Weapon == WEAPON_SHOTGUN && Collide)))))
		{
			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Weapon, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()),
			(m_Owner != -1)? TeamMask : -1LL);
		}
		else if(pTargetChr && m_Freeze && ((m_Layer == LAYER_SWITCH && Collision()->m_pSwitchers[m_Number].m_Status[pTargetChr->Team()]) || m_Layer != LAYER_SWITCH))
			pTargetChr->Freeze();
		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = GameWorld()->GameTick();
			m_Pos = NewPos+(-(m_Direction*4));
			if (m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y = -m_Direction.y;
			if (fabs(m_Direction.x) < 1e-6)
				m_Direction.x = 0;
			if (fabs(m_Direction.y) < 1e-6)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if (m_Weapon == WEAPON_GUN)
		{
			GameWorld()->DestroyEntity(this);
		}
		else
			if (!m_Freeze)
				GameWorld()->DestroyEntity(this);
	}
	if(m_LifeSpan == -1)
	{
		if(m_Explosive)
		{
			if(m_Owner >= 0)
				pOwnerChar = GameWorld()->GetCharacterByID(m_Owner);

			int64_t TeamMask = -1LL;
			if (pOwnerChar && pOwnerChar->IsAlive())
			{
					TeamMask = pOwnerChar->TeamMask();
			}

			GameWorld()->CreateExplosion(ColPos, m_Owner, m_Weapon, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()),
			(m_Owner != -1)? TeamMask : -1LL);
		}
		GameWorld()->DestroyEntity(this);
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}

CProjectile::CProjectile(CGameWorld *pGameWorld, int ID, CNetObj_Projectile *pProj)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	ExtractInfo(pProj, &m_Pos, &m_Direction, pGameWorld->m_IsDDRace);
	if(UseExtraInfo(pProj) && pGameWorld->m_IsDDRace)
		ExtractExtraInfo(pProj, &m_Owner, &m_Explosive, &m_Bouncing, &m_Freeze);
	else
	{
		m_Owner = -1;
		m_Bouncing = m_Freeze = 0;
		m_Explosive = (pProj->m_Type == WEAPON_GRENADE) && (abs(1.0f - length(m_Direction)) < 0.015f);
	}
	m_Type = m_Weapon = pProj->m_Type;
	m_StartTick = pProj->m_StartTick;

	int Lifetime = 20 * GameWorld()->GameTickSpeed();
	m_SoundImpact = -1;
	if(m_Weapon == WEAPON_GRENADE)
	{
		Lifetime = pGameWorld->Tuning()->m_GrenadeLifetime * GameWorld()->GameTickSpeed();
		m_SoundImpact = SOUND_GRENADE_EXPLODE;
	}
	else if(m_Weapon == WEAPON_GUN)
		Lifetime = pGameWorld->Tuning()->m_GunLifetime * GameWorld()->GameTickSpeed();
	else if(m_Weapon == WEAPON_SHOTGUN && !GameWorld()->m_IsDDRace)
		Lifetime = pGameWorld->Tuning()->m_ShotgunLifetime * GameWorld()->GameTickSpeed();
	m_LifeSpan = Lifetime - (pGameWorld->GameTick() - m_StartTick);
	m_ID = ID;
}
