/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/gamemodes/DDRace.h>
#include "projectile.h"

#include <engine/shared/config.h>
#include <game/server/teams.h>

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
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	m_Layer = Layer;
	m_Number = Number;
	m_Freeze = Freeze;
	
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	if(m_LifeSpan > -2)
		GameServer()->m_World.DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			if (!m_TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
				Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[m_TuneZone].m_GrenadeCurvature;
				Speed = GameServer()->TuningList()[m_TuneZone].m_GrenadeSpeed;
			}
				
			break;

		case WEAPON_SHOTGUN:
			if (!m_TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
				Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[m_TuneZone].m_ShotgunCurvature;
				Speed = GameServer()->TuningList()[m_TuneZone].m_ShotgunSpeed;
			}
			
			break;

		case WEAPON_GUN:
			if (!m_TuneZone)
			{
				Curvature = GameServer()->Tuning()->m_GunCurvature;
				Speed = GameServer()->Tuning()->m_GunSpeed;
			}
			else
			{
				Curvature = GameServer()->TuningList()[m_TuneZone].m_GunCurvature;
				Speed = GameServer()->TuningList()[m_TuneZone].m_GunSpeed;
			}
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 ColPos;
	vec2 NewPos;
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &ColPos, &NewPos, false);
	CCharacter *pOwnerChar = 0;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	CCharacter *pTargetChr = GameServer()->m_World.IntersectCharacter(PrevPos, ColPos, m_Freeze ? 1.0f : 6.0f, ColPos, pOwnerChar);

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
			TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);
	}
	else if (m_Owner >= 0)
	{
		GameServer()->m_World.DestroyEntity(this);
	}

	if( ((pTargetChr && (pOwnerChar ? !(pOwnerChar->m_Hit&CCharacter::DISABLE_HIT_GRENADE) : g_Config.m_SvHit || m_Owner == -1 || pTargetChr == pOwnerChar)) || Collide || GameLayerClipped(CurPos)) && !isWeaponCollide)
	{
		if(m_Explosive/*??*/ && (!pTargetChr || (pTargetChr && !m_Freeze)))
		{
			GameServer()->CreateExplosion(ColPos, m_Owner, m_Weapon, m_Owner == -1, (!pTargetChr ? -1 : pTargetChr->Team()),
			(m_Owner != -1)? TeamMask : -1LL);
			GameServer()->CreateSound(ColPos, m_SoundImpact,
			(m_Owner != -1)? TeamMask : -1LL);
		}
		else if(pTargetChr && m_Freeze && ((m_Layer == LAYER_SWITCH && GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pTargetChr->Team()]) || m_Layer != LAYER_SWITCH))
			pTargetChr->Freeze();
		if(Collide && m_Bouncing != 0)
		{
			m_StartTick = Server()->Tick();
			m_Pos = NewPos+(-(m_Direction*4));
			if (m_Bouncing == 1)
				m_Direction.x = -m_Direction.x;
			else if(m_Bouncing == 2)
				m_Direction.y =- m_Direction.y;
			if (fabs(m_Direction.x) < 1e-6)
				m_Direction.x = 0;
			if (fabs(m_Direction.y) < 1e-6)
				m_Direction.y = 0;
			m_Pos += m_Direction;
		}
		else if (m_Weapon == WEAPON_GUN)
		{
			GameServer()->CreateDamageInd(CurPos, -atan2(m_Direction.x, m_Direction.y), 10, (m_Owner != -1)? TeamMask : -1LL);
			GameServer()->m_World.DestroyEntity(this);
		}
		else
			if (!m_Freeze)
				GameServer()->m_World.DestroyEntity(this);
	}
	if(m_LifeSpan == -1)
	{
		if(m_Explosive)
		{
			if(m_Owner >= 0)
				pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

			int64_t TeamMask = -1LL;
			if (pOwnerChar && pOwnerChar->IsAlive())
			{
					TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);
			}

			GameServer()->CreateExplosion(ColPos, m_Owner, m_Weapon, m_Owner == -1, (!pOwnerChar ? -1 : pOwnerChar->Team()),
			(m_Owner != -1)? TeamMask : -1LL);
			GameServer()->CreateSound(ColPos, m_SoundImpact,
			(m_Owner != -1)? TeamMask : -1LL);
		}
		GameServer()->m_World.DestroyEntity(this);
	}

	int x = GameServer()->Collision()->GetIndex(PrevPos, CurPos);
	int z;
	if (g_Config.m_SvOldTeleportWeapons)
		z = GameServer()->Collision()->IsTeleport(x);
	else
		z = GameServer()->Collision()->IsTeleportWeapon(x);
	if (z && ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1].size())
	{
		int Num = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1].size();
		m_Pos = ((CGameControllerDDRace*)GameServer()->m_pController)->m_TeleOuts[z-1][(!Num)?Num:rand() % Num];
		m_StartTick = Server()->Tick();
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CCharacter* pSnapChar = GameServer()->GetPlayerChar(SnappingClient);
	int Tick = (Server()->Tick()%Server()->TickSpeed())%((m_Explosive)?6:20);
	if (pSnapChar && pSnapChar->IsAlive() && (m_Layer == LAYER_SWITCH && !GameServer()->Collision()->m_pSwitchers[m_Number].m_Status[pSnapChar->Team()] && (!Tick)))
		return;

	CCharacter *pOwnerChar = 0;
	int64_t TeamMask = -1LL;

	if(m_Owner >= 0)
		pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

	if (pOwnerChar && pOwnerChar->IsAlive())
			TeamMask = pOwnerChar->Teams()->TeamMask(pOwnerChar->Team(), -1, m_Owner);

	if(m_Owner != -1 && !CmaskIsSet(TeamMask, SnappingClient))
		return;

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}

// DDRace

void CProjectile::SetBouncing(int Value)
{
	m_Bouncing = Value;
}
