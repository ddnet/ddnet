#ifndef GAME_SERVER_GAMEMODES_DM_H
#define GAME_SERVER_GAMEMODES_DM_H

#include "base_instagib.h"

class CGameControllerDM : public CGameControllerInstagib
{
public:
	CGameControllerDM(class CGameContext *pGameServer);
	~CGameControllerDM();

	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_DM_H
