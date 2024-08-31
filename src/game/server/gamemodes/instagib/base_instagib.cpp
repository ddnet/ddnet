#include <base/system.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_instagib.h"

CGameControllerInstagib::CGameControllerInstagib(class CGameContext *pGameServer) :
	CGameControllerPvp(pGameServer)
{
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;

	m_SpawnWeapons = SPAWN_WEAPON_GRENADE;
	m_AllowSkinChange = true;
}

CGameControllerInstagib::~CGameControllerInstagib() = default;
