#ifndef GAME_SERVER_GAMEMODES_GCTF_H
#define GAME_SERVER_GAMEMODES_GCTF_H

#include "ctf.h"

class CGameControllerGCTF : public CGameControllerCTF
{
public:
	CGameControllerGCTF(class CGameContext *pGameServer);
	~CGameControllerGCTF();

	void Tick() override;
};
#endif // GAME_SERVER_GAMEMODES_GCTF_H
