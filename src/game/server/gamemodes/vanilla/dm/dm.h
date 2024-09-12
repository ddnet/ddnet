#ifndef GAME_SERVER_GAMEMODES_VANILLA_DM_DM_H
#define GAME_SERVER_GAMEMODES_VANILLA_DM_DM_H

#include "../base_vanilla.h"

class CGameControllerDM : public CGameControllerVanilla
{
public:
	CGameControllerDM(class CGameContext *pGameServer);
	~CGameControllerDM() override;

	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
