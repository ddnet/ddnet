#include <game/server/entities/character.h>

#include "ictf.h"

CGameControllerICTF::CGameControllerICTF(class CGameContext *pGameServer) :
	CGameControllerBaseCTF(pGameServer)
{
	m_pGameType = "iCTF";
}

CGameControllerICTF::~CGameControllerICTF() = default;

void CGameControllerICTF::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	CGameControllerBaseCTF::Tick();
}

void CGameControllerICTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_LASER, false, -1);
}
