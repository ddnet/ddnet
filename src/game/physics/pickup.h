/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_PHYSICS_PICKUP_H
#define GAME_PHYSICS_PICKUP_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/mapitems.h>

#include <type_traits>

// Shared pickup physics, used by the server entity and the client prediction
// entity. TPickup provides the state and the side specific sound, weapon
// pickup announcement and mover behavior, TCharacter the character
// interaction. The world config makes the prediction only checks vanish on
// the server, which simulates everything.
template<typename TPickup, typename TCharacter>
class CPickupPhysics
{
public:
	static void Tick(TPickup *pThis)
	{
		using TWorld = std::remove_pointer_t<decltype(pThis->GameWorld())>;
		using TEntity = std::remove_pointer_t<decltype(pThis->GameWorld()->FindFirst(0))>;

		Move(pThis);

		// Check if a player intersected us
		TEntity *apEnts[MAX_CLIENTS];
		int Num = pThis->GameWorld()->FindEntities(pThis->m_Pos, pThis->GetProximityRadius() + TPickup::ms_CollisionExtraSize, apEnts, MAX_CLIENTS, TWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; ++i)
		{
			auto *pChr = static_cast<TCharacter *>(apEnts[i]);

			if(pChr && pChr->IsAlive())
			{
				if(pThis->GameWorld()->m_WorldConfig.m_IsVanilla && distance(pThis->m_Pos, pChr->m_Pos) >= (pThis->GetProximityRadius() + TPickup::ms_CollisionExtraSize) * 2) // pickup distance is shorter on vanilla due to using ClosestEntity
					continue;
				// The switcher bounds check only matters on the client, where entity data
				// from the snapshot is not validated against the switch layer of the map
				if(pThis->m_Layer == LAYER_SWITCH && pThis->m_Number > 0 && pThis->m_Number < (int)pThis->Switchers().size() && !pThis->Switchers()[pThis->m_Number].m_aStatus[pChr->Team()])
					continue;
				bool Sound = false;
				// player picked us up, is someone was hooking us, let them go
				switch(pThis->m_Type)
				{
				case POWERUP_HEALTH:
					if(!pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Freeze())
						pThis->CreatePickupSound(SOUND_PICKUP_HEALTH, pChr);
					break;

				case POWERUP_ARMOR:
					if(!pThis->GameWorld()->m_WorldConfig.m_IsDDRace || !pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Team() == pChr->TeamsCore()->TeamSuper())
						continue;
					for(int j = WEAPON_SHOTGUN; j < NUM_WEAPONS; j++)
					{
						if(pChr->GetWeaponGot(j))
						{
							pChr->SetWeaponGot(j, false);
							pChr->SetWeaponAmmo(j, 0);
							Sound = true;
						}
					}
					pChr->SetNinjaActivationDir(vec2(0, 0));
					pChr->SetNinjaActivationTick(-500);
					pChr->SetNinjaCurrentMoveTime(0);
					if(Sound)
					{
						pChr->SetLastWeapon(WEAPON_GUN);
						pThis->CreatePickupSound(SOUND_PICKUP_ARMOR, pChr);
					}
					if(pChr->GetActiveWeapon() >= WEAPON_SHOTGUN)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
					break;

				case POWERUP_ARMOR_SHOTGUN:
					if(!pThis->GameWorld()->m_WorldConfig.m_IsDDRace || !pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Team() == pChr->TeamsCore()->TeamSuper())
						continue;
					if(pChr->GetWeaponGot(WEAPON_SHOTGUN))
					{
						pChr->SetWeaponGot(WEAPON_SHOTGUN, false);
						pChr->SetWeaponAmmo(WEAPON_SHOTGUN, 0);
						pChr->SetLastWeapon(WEAPON_GUN);
						pThis->CreatePickupSound(SOUND_PICKUP_ARMOR, pChr);
					}
					if(pChr->GetActiveWeapon() == WEAPON_SHOTGUN)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
					break;

				case POWERUP_ARMOR_GRENADE:
					if(!pThis->GameWorld()->m_WorldConfig.m_IsDDRace || !pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Team() == pChr->TeamsCore()->TeamSuper())
						continue;
					if(pChr->GetWeaponGot(WEAPON_GRENADE))
					{
						pChr->SetWeaponGot(WEAPON_GRENADE, false);
						pChr->SetWeaponAmmo(WEAPON_GRENADE, 0);
						pChr->SetLastWeapon(WEAPON_GUN);
						pThis->CreatePickupSound(SOUND_PICKUP_ARMOR, pChr);
					}
					if(pChr->GetActiveWeapon() == WEAPON_GRENADE)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
					break;

				case POWERUP_ARMOR_NINJA:
					if(!pThis->GameWorld()->m_WorldConfig.m_IsDDRace || !pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Team() == pChr->TeamsCore()->TeamSuper())
						continue;
					pChr->SetNinjaActivationDir(vec2(0, 0));
					pChr->SetNinjaActivationTick(-500);
					pChr->SetNinjaCurrentMoveTime(0);
					break;

				case POWERUP_ARMOR_LASER:
					if(!pThis->GameWorld()->m_WorldConfig.m_IsDDRace || !pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						continue;
					if(pChr->Team() == pChr->TeamsCore()->TeamSuper())
						continue;
					if(pChr->GetWeaponGot(WEAPON_LASER))
					{
						pChr->SetWeaponGot(WEAPON_LASER, false);
						pChr->SetWeaponAmmo(WEAPON_LASER, 0);
						pChr->SetLastWeapon(WEAPON_GUN);
						pThis->CreatePickupSound(SOUND_PICKUP_ARMOR, pChr);
					}
					if(pChr->GetActiveWeapon() == WEAPON_LASER)
						pChr->SetActiveWeapon(WEAPON_HAMMER);
					break;

				case POWERUP_WEAPON:
					if(pThis->m_Subtype >= 0 && pThis->m_Subtype < NUM_WEAPONS && (!pChr->GetWeaponGot(pThis->m_Subtype) || pChr->GetWeaponAmmo(pThis->m_Subtype) != -1))
					{
						pChr->GiveWeapon(pThis->m_Subtype);

						if(pThis->GameWorld()->m_WorldConfig.m_IsDDRace && pThis->GameWorld()->m_WorldConfig.m_PredictDDRace)
						{
							if(pThis->m_Subtype == WEAPON_GRENADE)
								pThis->CreatePickupSound(SOUND_PICKUP_GRENADE, pChr);
							else if(pThis->m_Subtype == WEAPON_SHOTGUN)
								pThis->CreatePickupSound(SOUND_PICKUP_SHOTGUN, pChr);
							else if(pThis->m_Subtype == WEAPON_LASER)
								pThis->CreatePickupSound(SOUND_PICKUP_SHOTGUN, pChr);
						}

						pThis->AnnounceWeaponPickup(pChr);
					}
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

private:
	static void Move(TPickup *pThis)
	{
		if(pThis->GameWorld()->GameTick() % (int)(pThis->GameWorld()->GameTickSpeed() * 0.15f) == 0)
		{
			if(pThis->Collision()->MoverSpeed(pThis->m_Pos.x, pThis->m_Pos.y, &pThis->m_Core))
			{
				pThis->OnMoverActivated();
			}
			pThis->m_Pos += pThis->m_Core;
		}
	}
};

#endif // GAME_PHYSICS_PICKUP_H
