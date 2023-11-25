#include <game/server/entities/character.h>

#include "gctf.h"

CGameControllerGCTF::CGameControllerGCTF(class CGameContext *pGameServer) :
	CGameControllerCTF(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? "gCTF-test" : "gCTF";
}

CGameControllerGCTF::~CGameControllerGCTF() = default;

void CGameControllerGCTF::Tick()
{
	CGameControllerCTF::Tick();
}

void CGameControllerGCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
