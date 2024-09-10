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
	// this is a base gamemode
	// full implementations are "fng" and "boomfng"
	m_GameFlags = GAMEFLAG_TEAMS;
}

CGameControllerTeamFng::~CGameControllerTeamFng() = default;

void CGameControllerTeamFng::Tick()
{
	CGameControllerBaseFng::Tick();
}

void CGameControllerTeamFng::OnCharacterSpawn(class CCharacter *pChr)
{
	CGameControllerBaseFng::OnCharacterSpawn(pChr);

	// give default weapons
	pChr->GiveWeapon(m_DefaultWeapon, false, -1);
}

int CGameControllerTeamFng::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponId)
{
	CGameControllerBaseFng::OnCharacterDeath(pVictim, pKiller, WeaponId);
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

	if(Server()->IsSixup(SnappingClient))
		return;

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
	pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

	pGameDataObj->m_FlagCarrierRed = 0;
	pGameDataObj->m_FlagCarrierBlue = 0;
}
