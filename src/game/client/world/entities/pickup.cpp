/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include "character.h"
#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, int Layer, int Number)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Type = Type;
	m_Subtype = SubType;
	m_ProximityRadius = PickupPhysSize;

	m_Layer = Layer;
	m_Number = Number;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
}

void CPickup::Tick()
{
	Move();
	// Check if a player intersected us
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; ++i) {
		CCharacter * pChr = apEnts[i];
		if(pChr && pChr->IsAlive())
		{
			if(m_Layer == LAYER_SWITCH && !Collision()->m_pSwitchers[m_Number].m_Status[pChr->Team()]) continue;
			bool sound = false;
			// player picked us up, is someone was hooking us, let them go
			switch (m_Type)
			{
				case POWERUP_HEALTH:
					//if(pChr->Freeze()) GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->Teams()->TeamMask(pChr->Team()));
					break;

				case POWERUP_ARMOR:
					if(pChr->Team() == TEAM_SUPER) continue;
					for(int i = WEAPON_SHOTGUN; i < NUM_WEAPONS; i++)
					{
						if(pChr->GetWeaponGot(i))
						{
							if(!(pChr->m_FreezeTime && i == WEAPON_NINJA))
							{
								pChr->SetWeaponGot(i, false);
								pChr->SetWeaponAmmo(i, 0);
								sound = true;
							}
						}
					}
					pChr->SetNinjaActivationDir(vec2(0,0));
					pChr->SetNinjaActivationTick(-500);
					pChr->SetNinjaCurrentMoveTime(0);
					if (sound)
					{
						pChr->SetLastWeapon(WEAPON_GUN);
						//GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->Teams()->TeamMask(pChr->Team()));
					}
					if(!pChr->m_FreezeTime && pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
					break;

				case POWERUP_WEAPON:

					if(m_Subtype >= 0 && m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(m_Subtype) || (pChr->GetWeaponAmmo(m_Subtype) != -1 && !pChr->m_FreezeTime)))
					{
						if(pChr->GiveWeapon(m_Subtype, -1))
						{
							//RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;

							//if(m_Subtype == WEAPON_GRENADE)
								//GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->Teams()->TeamMask(pChr->Team()));
							//else if(m_Subtype == WEAPON_SHOTGUN)
								//GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team()));
							//else if(m_Subtype == WEAPON_RIFLE)
								//GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->Teams()->TeamMask(pChr->Team()));

							//if(pChr->GetPlayer())
								//GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), m_Subtype);
						}
					}
					break;

			case POWERUP_NINJA:
				{
					// activate ninja on target player
					pChr->GiveNinja();
					//RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
					break;
				}
				default:
					break;
			};
		}
	}
}

void CPickup::TickPaused()
{
}

void CPickup::Move()
{
	if (GameWorld()->GameTick()%int(GameWorld()->GameTickSpeed() * 0.15f) == 0)
	{
		int Flags;
		int index = Collision()->IsMover(m_Pos.x,m_Pos.y, &Flags);
		if (index)
		{
			m_Core=Collision()->CpSpeed(index, Flags);
		}
		m_Pos += m_Core;
	}
}

CPickup::CPickup(CGameWorld *pGameWorld, int ID, CNetObj_Pickup *pPickup)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP)
{
	m_Pos.x = pPickup->m_X;
	m_Pos.y = pPickup->m_Y;
	m_Type = pPickup->m_Type;
	m_Subtype = pPickup->m_Subtype;
	m_Core = vec2(0.f, 0.f);
}
