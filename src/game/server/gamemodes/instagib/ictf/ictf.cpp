#include <game/server/entities/character.h>

#include "ictf.h"

CGameControllerICTF::CGameControllerICTF(class CGameContext *pGameServer) :
	CGameControllerInstaBaseCTF(pGameServer)
{
	m_pGameType = "iCTF";
	m_DefaultWeapon = WEAPON_LASER;

	m_pStatsTable = "ictf";
	m_pExtraColumns = new CICTFColumns();
	m_pSqlStats->SetExtraColumns(m_pExtraColumns);
	m_pSqlStats->CreateTable(m_pStatsTable);
}

CGameControllerICTF::~CGameControllerICTF() = default;

void CGameControllerICTF::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	CGameControllerInstaBaseCTF::Tick();
}

void CGameControllerICTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstaBaseCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, -1);
}
