#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_ICTF_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_ICTF_H

#include <game/server/gamemodes/instagib/ctf.h>

class CGameControllerICTF : public CGameControllerInstaBaseCTF
{
public:
	CGameControllerICTF(class CGameContext *pGameServer);
	~CGameControllerICTF() override;

	void OnCharacterSpawn(class CCharacter *pChr) override;
	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_ICTF_H
