#include "mod.h"

// Exchange this to a string that identifies your game mode.
// DM, TDM and CTF are reserved for teeworlds original modes.
// DDraceNetwork and TestDDraceNetwork are used by DDNet.
#define GAME_TYPE_NAME "Mod"
#define TEST_TYPE_NAME "TestMod"

#include <game/server/player.h>

class CPlayerMod : public CPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	CPlayerMod(CGameControllerMod *pGameController, uint32_t UniqueClientID, int ClientID, int Team);

private:
	CGameControllerMod *m_pGameController = nullptr;
};

MACRO_ALLOC_POOL_ID_IMPL(CPlayerMod, MAX_CLIENTS)

CPlayerMod::CPlayerMod(CGameControllerMod *pGameController, uint32_t UniqueClientID, int ClientID, int Team) :
	CPlayer(pGameController->GameServer(), UniqueClientID, ClientID, Team),
	m_pGameController(pGameController)
{
}

CGameControllerMod::CGameControllerMod(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

CGameControllerMod::~CGameControllerMod() = default;

CPlayer *CGameControllerMod::CreatePlayer(int ClientID, int StartTeam)
{
	return new(ClientID) CPlayerMod(this, GameServer()->GetNextUniqueClientID(), ClientID, StartTeam);
}

void CGameControllerMod::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	IGameController::Tick();
}
