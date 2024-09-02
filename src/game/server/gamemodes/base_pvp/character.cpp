#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_pvp.h"

void CGameControllerPvp::OnCharacterConstruct(class CCharacter *pChr)
{
	pChr->m_IsGodmode = false;
}

void CCharacter::ResetInstaSettings()
{
	int Ammo = -1;
	// TODO: this should not check the default weapon
	//       but if any of the spawn weapons is a grenade
	if(GameServer()->m_pController->GetDefaultWeapon(GetPlayer()) == WEAPON_GRENADE)
	{
		Ammo = g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1;
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
	}
	GiveWeapon(GameServer()->m_pController->GetDefaultWeapon(GetPlayer()), false, Ammo);
}
