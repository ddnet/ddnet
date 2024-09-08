#ifndef GAME_SERVER_GAMEMODES_INSTAGIB_CTF_H
#define GAME_SERVER_GAMEMODES_INSTAGIB_CTF_H

#include <game/server/gamemodes/instagib/base_instagib.h>

class CGameControllerInstaBaseCTF : public CGameControllerInstagib
{
	class CFlag *m_apFlags[2];
	bool DoWincheckRound() override;

public:
	CGameControllerInstaBaseCTF(class CGameContext *pGameServer);
	~CGameControllerInstaBaseCTF() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnFlagReturn(class CFlag *pFlag) override;
	void OnFlagGrab(class CFlag *pFlag) override;
	void OnFlagCapture(class CFlag *pFlag, float Time) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;

	// return true to consume the event
	// and supress default ddnet selfkill behavior
	bool OnSelfkill(int ClientId) override;
	bool DropFlag(class CCharacter *pChr) override;
	bool OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId) override;

	void FlagTick();
};
#endif
