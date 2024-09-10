#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "boomfng.h"

CGameControllerBoomFng::CGameControllerBoomFng(class CGameContext *pGameServer) :
	CGameControllerTeamFng(pGameServer)
{
	m_pGameType = "boomfng";
	m_GameFlags = GAMEFLAG_TEAMS;

	m_SpawnWeapons = ESpawnWeapons::SPAWN_WEAPON_GRENADE;
	m_DefaultWeapon = WEAPON_GRENADE;
}

CGameControllerBoomFng::~CGameControllerBoomFng() = default;

void CGameControllerBoomFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseFng::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, -1);
}
