#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_BASE_FNG_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_BASE_FNG_H

#include "base_instagib.h"

class CGameControllerBaseFng : public CGameControllerInstagib
{
public:
	CGameControllerBaseFng(class CGameContext *pGameServer);
	~CGameControllerBaseFng() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
};
#endif // GAME_SERVER_GAMEMODES_BASE_FNG_H
