#ifndef GAME_SERVER_GAMEMODES_IDM_H
#define GAME_SERVER_GAMEMODES_IDM_H

#include "dm.h"

class CGameControllerIDM : public CGameControllerDM
{
public:
	CGameControllerIDM(class CGameContext *pGameServer);
	~CGameControllerIDM();

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_IDM_H
