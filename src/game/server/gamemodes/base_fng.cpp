#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "base_fng.h"

CGameControllerBaseFng::CGameControllerBaseFng(class CGameContext *pGameServer) :
	CGameControllerInstagib(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? "fng-test" : "fng";
}

CGameControllerBaseFng::~CGameControllerBaseFng() = default;

void CGameControllerBaseFng::Tick()
{
	CGameControllerInstagib::Tick();
}

void CGameControllerBaseFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerInstagib::OnCharacterSpawn(pChr);
}

int CGameControllerBaseFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	CGameControllerInstagib::OnCharacterDeath(pVictim, pKiller, WeaponID);
	return 0;
}

bool CGameControllerBaseFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerInstagib::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerBaseFng::Snap(int SnappingClient)
{
	CGameControllerInstagib::Snap(SnappingClient);
}
