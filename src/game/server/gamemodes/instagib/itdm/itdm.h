#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_ITDM_ITDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_ITDM_ITDM_H

#include "../tdm.h"

class CGameControllerITDM : public CGameControllerInstaTDM
{
public:
	CGameControllerITDM(class CGameContext *pGameServer);
	~CGameControllerITDM() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
