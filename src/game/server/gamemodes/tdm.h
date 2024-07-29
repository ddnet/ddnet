#ifndef GAME_SERVER_GAMEMODES_TDM_H
#define GAME_SERVER_GAMEMODES_TDM_H

#include "dm.h"

class CGameControllerTDM : public CGameControllerDM
{
public:
	CGameControllerTDM(class CGameContext *pGameServer);
	~CGameControllerTDM();

	virtual void Snap(int SnappingClient) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_TDM_H
