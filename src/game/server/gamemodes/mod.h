#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H

#include <game/server/gamecontroller.h>

class CGameControllerMod : public IGameController
{
public:
	CGameControllerMod(class CGameContext *pGameServer);
	~CGameControllerMod();

	void Tick() override;
	void OnSayNetMessage(const CNetMsg_Cl_Say *pMsg, int ClientID, const CUnpacker *pUnpacker) override;
};
#endif // GAME_SERVER_GAMEMODES_MOD_H
