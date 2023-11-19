/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_GAMEMODES_DDRACE_H
#define GAME_SERVER_GAMEMODES_DDRACE_H

#include <game/server/gamecontroller.h>

struct CScoreLoadBestTimeResult;
class CGameControllerDDRace : public IGameController
{
	// gctf
	class CFlag *m_apFlags[2];
	virtual bool DoWincheckMatch() override;

public:
	CGameControllerDDRace(class CGameContext *pGameServer);
	~CGameControllerDDRace();

	CScore *Score();

	void HandleCharacterTiles(class CCharacter *pChr, int MapIndex) override;

	void OnPlayerConnect(class CPlayer *pPlayer) override;
	void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason) override;

	void OnReset() override;

	void Tick() override;

	void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true) override;

	std::shared_ptr<CScoreLoadBestTimeResult> m_pLoadBestTimeResult;

	// gctf

	virtual void Snap(int SnappingClient) override;
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnFlagReturn(class CFlag *pFlag) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;

	void FlagTick();
};
#endif // GAME_SERVER_GAMEMODES_DDRACE_H
