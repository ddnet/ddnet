#include <game/server/entities/character.h>

#include "gtdm.h"

CGameControllerGTDM::CGameControllerGTDM(class CGameContext *pGameServer) :
	CGameControllerTDM(pGameServer)
{
	m_pGameType = "gTDM";
}

CGameControllerGTDM::~CGameControllerGTDM() = default;

void CGameControllerGTDM::Tick()
{
	CGameControllerTDM::Tick();
}

void CGameControllerGTDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerTDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
