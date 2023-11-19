#ifndef GAME_SERVER_GAMEMODES_CTF_H
#define GAME_SERVER_GAMEMODES_CTF_H

#include "instagib.h"

class CGameControllerCTF : public CGameControllerInstagib
{
	class CFlag *m_apFlags[2];
	virtual bool DoWincheckMatch() override;

public:
	CGameControllerCTF(class CGameContext *pGameServer);
	~CGameControllerCTF();

	void Tick() override;
	virtual void Snap(int SnappingClient) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnFlagReturn(class CFlag *pFlag) override;
	virtual void OnFlagGrab(class CFlag *pFlag) override;
	virtual void OnFlagCapture(class CFlag *pFlag, float Time) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;

	void FlagTick();
};
#endif // GAME_SERVER_GAMEMODES_CTF_H
