/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include "character.h"
#include <game/client/pickup_data.h>
#include <game/collision.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>

static constexpr int gs_PickupPhysSize = 14;

void CPickup::Tick()
{
	Move();
	// Check if a player intersected us
	CEntity *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius() + ms_CollisionExtraSize, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);
		if(pChr)
		{
			if(GameWorld()->m_WorldConfig.m_IsVanilla && distance(m_Pos, pChr->m_Pos) >= (GetProximityRadius() + ms_CollisionExtraSize) * 2) // pickup distance is shorter on vanilla due to using ClosestEntity
				continue;
			if(m_Layer == LAYER_SWITCH && m_Number > 0 && m_Number < (int)Switchers().size() && !Switchers()[m_Number].m_aStatus[pChr->Team()])
				continue;
			bool sound = false;
			// player picked us up, is someone was hooking us, let them go
			switch(m_Type)
			{
			case POWERUP_HEALTH:
				//pChr->Freeze();
				break;

			case POWERUP_ARMOR:
				if(!GameWorld()->m_WorldConfig.m_IsDDRace || !GameWorld()->m_WorldConfig.m_PredictDDRace)
					continue;
				if(pChr->IsSuper())
					continue;
				for(int j = WEAPON_SHOTGUN; j < NUM_WEAPONS; j++)
				{
					if(pChr->GetWeaponGot(j))
					{
						pChr->SetWeaponGot(j, false);
						pChr->SetWeaponAmmo(j, 0);
						sound = true;
					}
				}
				pChr->SetNinjaActivationDir(vec2(0, 0));
				pChr->SetNinjaActivationTick(-500);
				pChr->SetNinjaCurrentMoveTime(0);
				if(sound)
					pChr->SetLastWeapon(WEAPON_GUN);
				if(pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_SHOTGUN:
				if(!GameWorld()->m_WorldConfig.m_IsDDRace || !GameWorld()->m_WorldConfig.m_PredictDDRace)
					continue;
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_SHOTGUN))
				{
					pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
					pChr->SetWeaponAmmo(WEAPON_SHOTGUN, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
				}
				if(pChr->GetActiveWeapon() == WEAPON_SHOTGUN)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_GRENADE:
				if(!GameWorld()->m_WorldConfig.m_IsDDRace || !GameWorld()->m_WorldConfig.m_PredictDDRace)
					continue;
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_GRENADE))
				{
					pChr->SetWeaponGot(WEAPON_GRENADE, false);
					pChr->SetWeaponAmmo(WEAPON_GRENADE, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
				}
				if(pChr->GetActiveWeapon() == WEAPON_GRENADE)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_ARMOR_NINJA:
				if(!GameWorld()->m_WorldConfig.m_IsDDRace || !GameWorld()->m_WorldConfig.m_PredictDDRace)
					continue;
				if(pChr->Team() == TEAM_SUPER)
					continue;
				pChr->SetNinjaActivationDir(vec2(0, 0));
				pChr->SetNinjaActivationTick(-500);
				pChr->SetNinjaCurrentMoveTime(0);
				break;

			case POWERUP_ARMOR_LASER:
				if(!GameWorld()->m_WorldConfig.m_IsDDRace || !GameWorld()->m_WorldConfig.m_PredictDDRace)
					continue;
				if(pChr->Team() == TEAM_SUPER)
					continue;
				if(pChr->GetWeaponGot(WEAPON_LASER))
				{
					pChr->SetWeaponGot(WEAPON_LASER, false);
					pChr->SetWeaponAmmo(WEAPON_LASER, 0);
					pChr->SetLastWeapon(WEAPON_GUN);
				}
				if(pChr->GetActiveWeapon() == WEAPON_LASER)
					pChr->SetActiveWeapon(WEAPON_HAMMER);
				break;

			case POWERUP_WEAPON:
				if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || pChr->GetWeaponAmmo(m_Subtype) != -1))
					pChr->GiveWeapon(m_Subtype);
				break;

			case POWERUP_NINJA:
			{
				// activate ninja on target player
				pChr->GiveNinja();
				break;
			}

			default:
				break;
			};
		}
	}
}

void CPickup::Move()
{
	if(GameWorld()->GameTick() % (int)(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		if(Collision()->MoverSpeed(m_Pos.x, m_Pos.y, &m_Core))
		{
			m_IsCoreActive = true;
		}
		m_Pos += m_Core;
	}
}

CPickup::CPickup(CGameWorld *pGameWorld, int Id, const CPickupData *pPickup) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), gs_PickupPhysSize)
{
	m_Pos = pPickup->m_Pos;
	m_Type = pPickup->m_Type;
	m_Subtype = pPickup->m_Subtype;
	m_Core = vec2(0.f, 0.f);
	m_IsCoreActive = false;
	m_Id = Id;
	m_Number = pPickup->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;
}

void CPickup::FillInfo(CNetObj_Pickup *pPickup)
{
	pPickup->m_X = (int)m_Pos.x;
	pPickup->m_Y = (int)m_Pos.y;
	pPickup->m_Type = m_Type;
	pPickup->m_Subtype = m_Subtype;
}

bool CPickup::Match(CPickup *pPickup)
{
	if(pPickup->m_Type != m_Type || pPickup->m_Subtype != m_Subtype)
		return false;
	if(distance(pPickup->m_Pos, m_Pos) > 2.0f)
		return false;
	return true;
}
