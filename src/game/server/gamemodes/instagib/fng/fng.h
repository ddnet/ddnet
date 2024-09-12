#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_FNG_FNG_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_FNG_FNG_H

#include "../team_fng.h"

class CGameControllerFng : public CGameControllerTeamFng
{
public:
	CGameControllerFng(class CGameContext *pGameServer);
	~CGameControllerFng() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
};
#endif
