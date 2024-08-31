#ifndef GAME_SERVER_GAMEMODES_VANILLA_BASE_VANILLA_H
#define GAME_SERVER_GAMEMODES_VANILLA_BASE_VANILLA_H

#include "../base_pvp/base_pvp.h"

class CGameControllerVanilla : public CGameControllerPvp
{
public:
	CGameControllerVanilla(class CGameContext *pGameServer);
	~CGameControllerVanilla() override;
};
#endif
