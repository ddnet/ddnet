#include <game/server/entities/character.h>

#include "gdm.h"

CGameControllerGDM::CGameControllerGDM(class CGameContext *pGameServer) :
	CGameControllerInstaBaseDM(pGameServer)
{
	m_pGameType = "gDM";
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
	pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
