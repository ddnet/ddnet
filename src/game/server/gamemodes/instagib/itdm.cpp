#include <game/server/entities/character.h>

#include "itdm.h"

CGameControllerITDM::CGameControllerITDM(class CGameContext *pGameServer) :
	CGameControllerTDM(pGameServer)
{
	m_pGameType = "iTDM";
}

CGameControllerITDM::~CGameControllerITDM() = default;

void CGameControllerITDM::Tick()
{
	CGameControllerTDM::Tick();
}

void CGameControllerITDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerTDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_LASER, false, -1);
}
