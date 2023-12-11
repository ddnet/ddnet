#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/score.h>
#include <game/version.h>

#include "team_fng.h"

CGameControllerTeamFng::CGameControllerTeamFng(class CGameContext *pGameServer) :
	CGameControllerBaseFng(pGameServer)
{
	m_pGameType = "fng";
	m_GameFlags = GAMEFLAG_TEAMS | GAMEFLAG_FLAGS;
}

CGameControllerTeamFng::~CGameControllerTeamFng() = default;

void CGameControllerTeamFng::Tick()
{
	CGameControllerBaseFng::Tick();
}

void CGameControllerTeamFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseFng::OnCharacterSpawn(pChr);
}

int CGameControllerTeamFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	CGameControllerBaseFng::OnCharacterDeath(pVictim, pKiller, WeaponID);
	return 0;
}

bool CGameControllerTeamFng::OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number)
{
	CGameControllerBaseFng::OnEntity(Index, x, y, Layer, Flags, Initial, Number);
	return false;
}

void CGameControllerTeamFng::Snap(int SnappingClient)
{
	CGameControllerBaseFng::Snap(SnappingClient);
}
