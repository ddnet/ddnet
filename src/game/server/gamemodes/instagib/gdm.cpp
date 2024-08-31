#include <game/server/entities/character.h>

#include "gdm.h"

CGameControllerGDM::CGameControllerGDM(class CGameContext *pGameServer) :
	CGameControllerDM(pGameServer)
{
	m_pGameType = "gDM";
}

CGameControllerGDM::~CGameControllerGDM() = default;

void CGameControllerGDM::Tick()
{
	CGameControllerDM::Tick();
}

void CGameControllerGDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
