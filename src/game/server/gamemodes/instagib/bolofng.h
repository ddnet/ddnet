#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_BOLOFNG_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_BOLOFNG_H

#include "base_fng.h"

class CGameControllerBoloFng : public CGameControllerBaseFng
{
public:
	CGameControllerBoloFng(class CGameContext *pGameServer);
	~CGameControllerBoloFng() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;
};
#endif // GAME_SERVER_GAMEMODES_BOLOFNG_H
