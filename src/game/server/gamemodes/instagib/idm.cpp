#include <game/server/entities/character.h>

#include "idm.h"

CGameControllerIDM::CGameControllerIDM(class CGameContext *pGameServer) :
	CGameControllerDM(pGameServer)
{
	m_pGameType = "iDM";
}

CGameControllerIDM::~CGameControllerIDM() = default;

void CGameControllerIDM::Tick()
{
	CGameControllerDM::Tick();
}

void CGameControllerIDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_LASER, false, -1);
}
