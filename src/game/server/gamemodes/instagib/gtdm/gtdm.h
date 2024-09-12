#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_GTDM_GTDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_GTDM_GTDM_H

#include "../tdm.h"

class CGameControllerGTDM : public CGameControllerInstaTDM
{
public:
	CGameControllerGTDM(class CGameContext *pGameServer);
	~CGameControllerGTDM() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
