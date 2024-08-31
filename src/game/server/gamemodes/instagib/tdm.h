#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_TDM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_TDM_H

#include <game/server/gamemodes/instagib/dm.h>

class CGameControllerInstaTDM : public CGameControllerInstaBaseDM
{
public:
	CGameControllerInstaTDM(class CGameContext *pGameServer);
	~CGameControllerInstaTDM() override;

	void Snap(int SnappingClient) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;
};
#endif
