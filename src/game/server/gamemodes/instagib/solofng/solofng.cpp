#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "solofng.h"

CGameControllerSoloFng::CGameControllerSoloFng(class CGameContext *pGameServer) :
	CGameControllerBaseFng(pGameServer)
{
	m_pGameType = "solofng";
	m_GameFlags = 0;

	m_SpawnWeapons = ESpawnWeapons::SPAWN_WEAPON_LASER;
	m_DefaultWeapon = WEAPON_LASER;

	m_pStatsTable = "solofng";
	m_pExtraColumns = new CSolofngColumns();
	m_pSqlStats->SetExtraColumns(m_pExtraColumns);
	m_pSqlStats->CreateTable(m_pStatsTable);
}

CGameControllerSoloFng::~CGameControllerSoloFng() = default;

void CGameControllerSoloFng::Tick()
{
	CGameControllerBaseFng::Tick();
}

void CGameControllerSoloFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseFng::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, -1);
}

int CGameControllerSoloFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerBaseFng::OnCharacterDeath(pVictim, pKiller, WeaponId);
	return 0;
}

bool CGameControllerSoloFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerBaseFng::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerSoloFng::Snap(int SnappingClient)
{
	CGameControllerBaseFng::Snap(SnappingClient);
}
