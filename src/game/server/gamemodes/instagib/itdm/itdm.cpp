#include <game/server/entities/character.h>

#include "itdm.h"

CGameControllerITDM::CGameControllerITDM(class CGameContext *pGameServer) :
	CGameControllerInstaTDM(pGameServer)
{
	m_pGameType = "iTDM";
	m_DefaultWeapon = WEAPON_LASER;
}

CGameControllerITDM::~CGameControllerITDM() = default;

void CGameControllerITDM::Tick()
{
	CGameControllerInstaTDM::Tick();
}

void CGameControllerITDM::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstaTDM::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, -1);
}
