#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_BOOMFNG_BOOMFNG_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_BOOMFNG_BOOMFNG_H

#include "../team_fng.h"

class CGameControllerBoomFng : public CGameControllerTeamFng
{
public:
	CGameControllerBoomFng(class CGameContext *pGameServer);
	~CGameControllerBoomFng() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
};
#endif
