/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
/* Based on Race mod stuff and tweaked by GreYFoX@GTi and others to fit our DDRace needs. */
#include "gamecontext.h"

#include <game/server/player.h>

#include "gamecontroller.h"

CGameContextDDRace::CGameContextDDRace(int Resetting) :
	CGameContext(Resetting)
{
}

void CGameContextDDRace::ResetContext()
{
	this->~CGameContextDDRace();
	mem_zero(this, sizeof(*this));
	new(this) CGameContextDDRace(RESET);
}

CGameContextDDRace::CGameContextDDRace() :
	CGameContext()
{
}

IGameController *CGameContextDDRace::CreateGameController()
{
	m_pDDRaceController = new CGameControllerDDRace(this);
	return m_pDDRaceController;
}

void CGameContextDDRace::OnShutdown()
{
	CGameContext::OnShutdown();
	m_pDDRaceController = nullptr;
}

void CGameContextDDRace::OnClientDrop(int ClientID, const char *pReason)
{
	bool GameHadModerator = PlayerModerating();

	if(Server()->ClientIngame(ClientID))
	{
		char aBuf[512];
		if(pReason && *pReason)
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game (%s)", Server()->ClientName(ClientID), pReason);
		else
			str_format(aBuf, sizeof(aBuf), "'%s' has left the game", Server()->ClientName(ClientID));
		SendChat(-1, CGameContext::CHAT_ALL, aBuf, -1, CGameContext::CHAT_SIX);

		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}

	if(g_Config.m_SvTeam != 3)
		m_pDDRaceController->m_Teams.SetForceCharacterTeam(ClientID, TEAM_FLOCK);

	CGameContext::OnClientDrop(ClientID, pReason);

	if(GameHadModerator && !PlayerModerating())
		SendChat(-1, CGameContext::CHAT_ALL, "Server kick/spec votes are no longer actively moderated.");
}

IGameServer *CreateGameServer() { return new CGameContextDDRace; }
