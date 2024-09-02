#include <game/server/entities/character.h>

#include "ctf.h"

CGameControllerCTF::CGameControllerCTF(class CGameContext *pGameServer) :
	CGameControllerBaseCTF(pGameServer)
{
	m_pGameType = "CTF*";
	m_DefaultWeapon = WEAPON_GUN;
}

CGameControllerCTF::~CGameControllerCTF() = default;

void CGameControllerCTF::Tick()
{
	CGameControllerBaseCTF::Tick();
}

void CGameControllerCTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_HAMMER, false, -1);
	pChr->GiveWeapon(WEAPON_GUN, false, 10);
}
