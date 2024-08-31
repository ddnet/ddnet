#include <game/server/entities/character.h>

#include "gctf.h"

CGameControllerGCTF::CGameControllerGCTF(class CGameContext *pGameServer) :
	CGameControllerBaseCTF(pGameServer)
{
	m_pGameType = "gCTF";
}

CGameControllerGCTF::~CGameControllerGCTF() = default;

void CGameControllerGCTF::Tick()
{
	CGameControllerBaseCTF::Tick();
}

void CGameControllerGCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_GRENADE, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}
