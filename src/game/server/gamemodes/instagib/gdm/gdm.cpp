#include <game/server/entities/character.h>

#include "gdm.h"

CGameControllerGDM::CGameControllerGDM(class CGameContext *pGameServer) :
	CGameControllerInstaBaseDM(pGameServer)
{
	m_pGameType = "gDM";
	m_DefaultWeapon = WEAPON_GRENADE;
}

CGameControllerGDM::~CGameControllerGDM() = default;

void CGameControllerGDM::Tick()
{
	CGameControllerInstaBaseDM::Tick();
}

void CGameControllerGDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstaBaseDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
