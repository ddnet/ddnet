/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_DDRACE_GAMECONTEXT_H
#define GAME_SERVER_DDRACE_GAMECONTEXT_H

#include <game/server/gamecontext.h>

class CGameContextDDRace : public CGameContext
{
protected:
	CGameContextDDRace(int Resetting);

	void ResetContext() override;

public:
	CGameContextDDRace();

	IGameController *CreateGameController() override;

	void OnConsoleInit() override;
	void OnShutdown() override;

	void OnClientDrop(int ClientID, const char *pReason) override;

protected:
	class CGameControllerDDRace *m_pDDRaceController = nullptr;
};

#endif // GAME_SERVER_DDRACE_GAMECONTEXT_H
