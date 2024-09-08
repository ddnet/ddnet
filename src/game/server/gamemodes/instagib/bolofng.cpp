#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "bolofng.h"

CGameControllerBoloFng::CGameControllerBoloFng(class CGameContext *pGameServer) :
	CGameControllerBaseFng(pGameServer)
{
	m_pGameType = "bolofng";
	m_GameFlags = 0;

	m_SpawnWeapons = ESpawnWeapons::SPAWN_WEAPON_GRENADE;
	m_DefaultWeapon = WEAPON_GRENADE;
}

CGameControllerBoloFng::~CGameControllerBoloFng() = default;

void CGameControllerBoloFng::Tick()
{
	CGameControllerBaseFng::Tick();
}

void CGameControllerBoloFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseFng::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, g_Config.m_SvGrenadeAmmoRegen ? g_Config.m_SvGrenadeAmmoRegenNum : -1);
}

int CGameControllerBoloFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerBaseFng::OnCharacterDeath(pVictim, pKiller, WeaponId);
	return 0;
}

bool CGameControllerBoloFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerBaseFng::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerBoloFng::Snap(int SnappingClient)
{
	CGameControllerBaseFng::Snap(SnappingClient);
}
