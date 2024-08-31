#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_ITDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_ITDM_H

#include "tdm.h"

class CGameControllerITDM : public CGameControllerTDM
{
public:
	CGameControllerITDM(class CGameContext *pGameServer);
	~CGameControllerITDM();

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_ITDM_H
