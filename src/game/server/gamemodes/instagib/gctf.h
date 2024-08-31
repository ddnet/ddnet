#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_GCTF_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_GCTF_H

#include <game/server/gamemodes/instagib/ctf.h>

class CGameControllerGCTF : public CGameControllerInstaBaseCTF
{
public:
	CGameControllerGCTF(class CGameContext *pGameServer);
	~CGameControllerGCTF() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif
