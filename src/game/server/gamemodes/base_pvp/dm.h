#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_DM_H
#define GAME_SERVER_GAMEMODES_BASE_PVP_DM_H

#include "base_pvp.h"

class CGameControllerBaseDM : public CGameControllerPvp
{
public:
	CGameControllerBaseDM(class CGameContext *pGameServer);
	~CGameControllerBaseDM() override;

	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
