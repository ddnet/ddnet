#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_H

#include "DDRace.h"

class CGameControllerInstagib : public CGameControllerDDRace
{
public:
	CGameControllerInstagib(class CGameContext *pGameServer);
	~CGameControllerInstagib();

	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_INSTAGIB_H
