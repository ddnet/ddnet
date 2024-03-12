#ifndef GAME_SERVER_GAMEMODES_GDM_H
#define GAME_SERVER_GAMEMODES_GDM_H

#include "dm.h"

class CGameControllerGDM : public CGameControllerDM
{
public:
	CGameControllerGDM(class CGameContext *pGameServer);
	~CGameControllerGDM();

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_GDM_H
