#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_DM_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_DM_H

#include <game/server/gamemodes/instagib/base_instagib.h>

class CGameControllerInstaBaseDM : public CGameControllerInstagib
{
public:
	CGameControllerInstaBaseDM(class CGameContext *pGameServer);
	~CGameControllerInstaBaseDM() override;

	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
