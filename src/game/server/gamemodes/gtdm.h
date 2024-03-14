#ifndef GAME_SERVER_GAMEMODES_GTDM_H
#define GAME_SERVER_GAMEMODES_GTDM_H

#include "tdm.h"

class CGameControllerGTDM : public CGameControllerTDM
{
public:
	CGameControllerGTDM(class CGameContext *pGameServer);
	~CGameControllerGTDM();

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_GTDM_H
