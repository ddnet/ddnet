/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include "character.h"
#include <game/collision.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>

void CPickup::Tick()
{
	Move();
	// Check if a player intersected us
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pChr = apEnts[i];
		if(pChr)
		{
			if(GameWorld()->m_WorldConfig.m_IsVanilla && distance(m_Pos, pChr->m_Pos) >= 20.0f * 2) // pickup distance is shorter on vanilla due to using ClosestEntity
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
	if(GameWorld()->GameTick() % int(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = Collision()->IsMover(m_Pos.x, m_Pos.y, &Flags);
		if(index)
		{
			m_IsCoreActive = true;
			m_Core = Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}

CPickup::CPickup(CGameWorld *pGameWorld, int ID, CNetObj_Pickup *pPickup, const CNetObj_EntityEx *pEntEx) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Pos.x = pPickup->m_X;
	m_Pos.y = pPickup->m_Y;
	m_Type = pPickup->m_Type;
	m_Subtype = pPickup->m_Subtype;
	m_Core = vec2(0.f, 0.f);
	m_IsCoreActive = false;
	m_ID = ID;
	m_Layer = LAYER_GAME;
	m_Number = 0;

	if(pEntEx)
	{
		m_Layer = pEntEx->m_Layer;
		m_Number = pEntEx->m_SwitchNumber;
	}
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
