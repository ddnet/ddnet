#ifndef GAME_SERVER_GAMEMODES_BASE_PVP_CTF_H
#define GAME_SERVER_GAMEMODES_BASE_PVP_CTF_H

#include "base_pvp.h"

class CGameControllerBaseCTF : public CGameControllerPvp
{
	class CFlag *m_apFlags[2];
	bool DoWincheckRound() override;

public:
	CGameControllerBaseCTF(class CGameContext *pGameServer);
	~CGameControllerBaseCTF() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnFlagReturn(class CFlag *pFlag) override;
	void OnFlagGrab(class CFlag *pFlag) override;
	void OnFlagCapture(class CFlag *pFlag, float Time) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;

	void FlagTick();
};
#endif
