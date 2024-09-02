#ifndef GAME_SERVER_GAMEMODES_VANILLA_CTF_H
#define GAME_SERVER_GAMEMODES_VANILLA_CTF_H

#include <game/server/gamemodes/base_pvp/ctf.h>

class CGameControllerCTF : public CGameControllerBaseCTF
{
public:
	CGameControllerCTF(class CGameContext *pGameServer);
	~CGameControllerCTF() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
	int GameInfoExFlags(int SnappingClient) override;
	bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;
};
#endif
