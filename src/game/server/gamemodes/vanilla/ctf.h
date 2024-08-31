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
};
#endif
