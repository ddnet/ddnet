/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"
#include "character.h"
#include <game/generated/protocol.h>

void CPickup::Tick()
{
	Move();
	// Check if a player intersected us
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity **)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i)
	{
		CCharacter *pChr = apEnts[i];
		if(pChr && pChr->IsAlive())
		{
			if(GameWorld()->m_WorldConfig.m_IsVanilla && distance(m_Pos, pChr->m_Pos) >= 20.0f * 2) // pickup distance is shorter on vanilla due to using ClosestEntity
				continue;
			if(m_Layer == LAYER_SWITCH && !Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()])
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
				if(pChr->m_Super)
					continue;
				for(int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
				{
					if(pChr->GetWeaponGot(i))
					{
						pChr->SetWeaponGot(i, false);
						pChr->SetWeaponAmmo(i, 0);
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
			m_Core = Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}

CPickup::CPickup(CGameWorld *pGameWorld, int ID, CNetObj_Pickup *pPickup) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Pos.x = pPickup->m_X;
	m_Pos.y = pPickup->m_Y;
	m_Type = pPickup->m_Type;
	m_Subtype = pPickup->m_Subtype;
	m_Core = vec2(0.f, 0.f);
	m_ID = ID;
	m_Layer = LAYER_GAME;
	m_Number = 0;
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
