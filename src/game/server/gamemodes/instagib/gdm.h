#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_GDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_GDM_H

#include <game/server/gamemodes/base_pvp/dm.h>

class CGameControllerGDM : public CGameControllerBaseDM
{
public:
	CGameControllerGDM(class CGameContext *pGameServer);
	~CGameControllerGDM() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
