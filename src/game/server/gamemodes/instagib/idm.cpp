#include <game/server/entities/character.h>

#include "idm.h"

CGameControllerIDM::CGameControllerIDM(class CGameContext *pGameServer) :
	CGameControllerBaseDM(pGameServer)
{
	m_pGameType = "iDM";
}

CGameControllerIDM::~CGameControllerIDM() = default;

void CGameControllerIDM::Tick()
{
	CGameControllerBaseDM::Tick();
}

void CGameControllerIDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_LASER, false, -1);
}
