#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CGameControllerMod : public IGameController
{
public:
	CGameControllerMod(class CGameContext *pGameServer);
	~CGameControllerMod();

	void Tick() override;
	CServerInfo::EClientScoreKind ScoreKind() const override { return CServerInfo::CLIENT_SCORE_KIND_POINTS; };
};
#endif // GAME_SERVER_GAMEMODES_MOD_H
