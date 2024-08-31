#include <base/system.h>
#include <game/generated/protocol.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerPvp(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	m_AllowSkinChange = true;
}

CGameControllerInstagib::~CGameControllerInstagib() = default;

bool CGameControllerInstagib::OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character)
{
	if(From == Character.GetPlayer()->GetCid())
	{
		// Give back ammo on grenade self push
		// Only if not infinite ammo and activated
		if(Weapon == WEAPON_GRENADE && g_Config.m_SvGrenadeAmmoRegen && g_Config.m_SvGrenadeAmmoRegenSpeedNade)
		{
			Character.SetWeaponAmmo(WEAPON_GRENADE, minimum(Character.GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + 1, g_Config.m_SvGrenadeAmmoRegenNum));
		}

		// no self damage
		return false;
	}
	if(Dmg < g_Config.m_SvDamageNeededForKill)
		return false;
	Dmg = 20;

	dbg_assert(Character.IsAlive(), "tried to apply damage to dead character");

	CGameControllerPvp::OnCharacterTakeDamage(Force, Dmg, From, Weapon, Character);

	if(!Character.IsAlive())
	{
		CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
		if(!pChr)
			return false;

		// refill nades
		int RefillNades = 0;
		if(g_Config.m_SvGrenadeAmmoRegenOnKill == 1)
			RefillNades = 1;
		else if(g_Config.m_SvGrenadeAmmoRegenOnKill == 2)
			RefillNades = g_Config.m_SvGrenadeAmmoRegenNum;
		if(RefillNades && g_Config.m_SvGrenadeAmmoRegen && Weapon == WEAPON_GRENADE)
		{
			pChr->SetWeaponAmmo(WEAPON_GRENADE, minimum(pChr->GetCore().m_aWeapons[WEAPON_GRENADE].m_Ammo + RefillNades, g_Config.m_SvGrenadeAmmoRegenNum));
		}
		return false;
	}
	return false;
}

bool CGameControllerInstagib::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	// ddnet-insta
	// do not spawn pickups
	if(Index == ENTITY_ARMOR_1 || Index == ENTITY_HEALTH_1 || Index == ENTITY_WEAPON_SHOTGUN || Index == ENTITY_WEAPON_GRENADE || Index == ENTITY_WEAPON_LASER || Index == ENTITY_POWERUP_NINJA)
		return false;

	return CGameControllerPvp::OnEntity(Index, x, y, Layer, Flags, Initial);
}
