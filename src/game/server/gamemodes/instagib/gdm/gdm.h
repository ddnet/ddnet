#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_GDM_GDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_GDM_GDM_H

#include <game/server/gamemodes/instagib/dm.h>

class CGameControllerGDM : public CGameControllerInstaBaseDM
{
public:
	CGameControllerGDM(class CGameContext *pGameServer);
	~CGameControllerGDM() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
