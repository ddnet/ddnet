#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H

#include "base_instagib.h"

class CGameControllerZcatch : public CGameControllerInstagib
{
public:
	CGameControllerZcatch(class CGameContext *pGameServer);
	~CGameControllerZcatch() override;

	int m_aBodyColors[MAX_CLIENTS] = {0};

	void OnCaught(class CPlayer *pVictim, class CPlayer *pKiller);

	void Tick() override;
	void Snap(int SnappingClient) override;
	void OnPlayerConnect(CPlayer *pPlayer) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	bool CanJoinTeam(int Team, int NotThisId, char *pErrorReason, int ErrorReasonSize) override;
	void DoTeamChange(CPlayer *pPlayer, int Team, bool DoChatMsg) override;
	bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number) override;
	bool DoWincheckRound() override;

	// gets the tee's body color based on the amount of its kills
	// the value is the integer that will be sent over the network
	int GetBodyColor(int Kills);

	void SendSkinBodyColor7(int ClientId, int Color);

	enum class EZcatchGameState
	{
		WAITING_FOR_PLAYERS,
		RELEASE_GAME,
		RUNNING,
	};
	EZcatchGameState m_GameState = EZcatchGameState::RUNNING; // TODO: set to waiting for players
	EZcatchGameState GameState() const { return m_GameState; }
	void SetGameState(EZcatchGameState State) { m_GameState = State; }

	void CheckGameState();
};
#endif // GAME_SERVER_GAMEMODES_ZCATCH_H
