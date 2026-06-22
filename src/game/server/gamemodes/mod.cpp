#include "mod.h"

#include <engine/shared/config.h>

// Exchange this to a string that identifies your game mode.
// DM, TDM and CTF are reserved for teeworlds original modes.
// DDraceNetwork and TestDDraceNetwork are used by DDNet.
#define GAME_TYPE_NAME "Mod"
#define TEST_TYPE_NAME "TestMod"

CGameControllerMod::CGameControllerMod(class CGameContext *pGameServer) :
	IGameController(pGameServer)
{
	m_pGameType = g_Config.m_SvTestingCommands ? TEST_TYPE_NAME : GAME_TYPE_NAME;
	
	//yirou
	// 初始化接力时间 默认设置为 5 秒
	int DefaultTicks = 5 * Server()->TickSpeed(); // 5 * 50 = 250
	for(int i = 0; i < NUM_DDRACE_TEAMS; i++)
	{
		m_aTeamRelayDurationTicks[i] = DefaultTicks;
	}

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

CGameControllerMod::~CGameControllerMod() = default;

void CGameControllerMod::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	IGameController::Tick();
}
