#include <game/server/entities/character.h>
#include <game/server/player.h>

#include "ictf.h"

CGameControllerICTF::CGameControllerICTF(class CGameContext *pGameServer) :
	CGameControllerCTF(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? "iCTF-test" : "iCTF";
}

CGameControllerICTF::~CGameControllerICTF() = default;

void CGameControllerICTF::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	CGameControllerCTF::Tick();
}

void CGameControllerICTF::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerCTF::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(WEAPON_LASER, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}

void CGameControllerICTF::OnPlayerConnect(CPlayer *pPlayer)
{
	// this is the main part of the gamemode, this function is run every tick
	pPlayer->SetTeam(TEAM_RED, false);
	CGameControllerInstagib::OnPlayerConnect(pPlayer);
}