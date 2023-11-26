/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

#include <base/vmath.h>
#include <engine/map.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>

#include <map>
#include <vector>

#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

struct CScoreLoadBestTimeResult;

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	friend class CSaveTeam; // need access to GameServer() and Server()

	std::vector<vec2> m_avSpawnPoints[3];

	class CGameContext *m_pGameServer;
	class CConfig *m_pConfig;
	class IServer *m_pServer;

	CGameTeams m_Teams;

protected:
	CGameContext *GameServer() const { return m_pGameServer; }
	CConfig *Config() { return m_pConfig; }
	IServer *Server() const { return m_pServer; }

	void DoActivityCheck();

	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100, 100);
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int DDTeam);
	void EvaluateSpawnType(CSpawnEval *pEval, int Type, int DDTeam);

	void ResetGame();

	char m_aMapWish[MAX_MAP_LENGTH];

	int m_RoundStartTick;
	int m_GameOverTick;
	int m_SuddenDeath;

	int m_Warmup;
	int m_RoundCount;

	int m_GameFlags;
	int m_UnbalancedTick;
	bool m_ForceBalanced;

public:
	const char *m_pGameType;

	IGameController(class CGameContext *pGameServer);
	virtual ~IGameController();

	// event
	/*
		Function: OnCharacterDeath
			Called when a CCharacter in the world dies.

		Arguments:
			victim - The CCharacter that died.
			killer - The player that killed it.
			weapon - What weapon that killed it. Can be -1 for undefined
				weapon when switching team or player suicides.
	*/
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	/*
		Function: OnCharacterSpawn
			Called when a CCharacter spawns into the game world.

		Arguments:
			chr - The CCharacter that was spawned.
	*/
	virtual void OnCharacterSpawn(class CCharacter *pChr);

	virtual void HandleCharacterTiles(class CCharacter *pChr, int MapIndex);

	/*
		Function: OnEntity
			Called when the map is loaded to process an entity
			in the map.

		Arguments:
			index - Entity index.
			pos - Where the entity is located in the world.

		Returns:
			bool?
	*/
	virtual bool OnEntity(int Index, int x, int y, int Layer, int Flags, bool Initial, int Number = 0);

	virtual void OnPlayerConnect(class CPlayer *pPlayer);
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer, const char *pReason);

	virtual void OnReset();

	// game
	void DoWarmup(int Seconds);

	void StartRound();
	void EndRound();
	void ChangeMap(const char *pToMap);

	void CheckGameInfo();
	bool IsFriendlyFire(int ClientID1, int ClientID2);
	bool IsTeamplay() const { return m_GameFlags & GAMEFLAG_TEAMS; }

	bool IsForceBalanced();

	/*

	*/
	virtual bool CanBeMovedOnBalance(int ClientID);

	virtual void Tick();

	virtual void Snap(int SnappingClient);

	//spawn
	virtual bool CanSpawn(int Team, vec2 *pOutPos, int DDTeam);

	virtual void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg = true);
	/*

	*/
	virtual const char *GetTeamName(int Team);
	virtual int GetAutoTeam(int NotThisID);
	virtual bool CanJoinTeam(int Team, int NotThisID);
	int ClampTeam(int Team);

	CClientMask GetMaskForPlayerWorldEvent(int Asker, int ExceptID = -1);
	virtual void InitTeleporter();

	// DDRace

	float m_CurrentRecord;

	std::map<int, std::vector<vec2>> m_TeleOuts;
	std::map<int, std::vector<vec2>> m_TeleCheckOuts;
	CGameTeams &Teams() { return m_Teams; }
	std::shared_ptr<CScoreLoadBestTimeResult> m_pLoadBestTimeResult;

	// gctf
	virtual void OnPlayerReadyChange(class CPlayer *pPlayer); // 0.7 ready change
	void CheckReadyStates(int WithoutID = -1);
	bool GetPlayersReadyState(int WithoutID = -1, int *pNumUnready = nullptr);
	void SetPlayersReadyState(bool ReadyState);
	bool IsPlayerReadyMode();
	int IsGameRunning() { return m_GameState == IGS_GAME_RUNNING; }
	int IsGameCountdown() { return m_GameState == IGS_START_COUNTDOWN; }
	void ToggleGamePause();
	void AbortWarmup()
	{
		if((m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER) && m_GameStateTimer != TIMER_INFINITE)
		{
			SetGameState(IGS_GAME_RUNNING);
		}
	}

	int m_aTeamSize[protocol7::NUM_TEAMS];

	// game
	enum EGameState
	{
		// internal game states
		IGS_WARMUP_GAME, // warmup started by game because there're not enough players (infinite)
		IGS_WARMUP_USER, // warmup started by user action via rcon or new match (infinite or timer)

		IGS_START_COUNTDOWN, // start countown to unpause the game or start match/round (tick timer)

		IGS_GAME_PAUSED, // game paused (infinite or tick timer)
		IGS_GAME_RUNNING, // game running (infinite)

		IGS_END_MATCH, // match is over (tick timer)
		IGS_END_ROUND, // round is over (tick timer)
	};
	EGameState m_GameState;
	int m_GameStateTimer;

	const char *GameStateToStr(EGameState GameState)
	{
		switch(GameState)
		{
		case IGS_WARMUP_GAME:
			return "IGS_WARMUP_GAME";
		case IGS_WARMUP_USER:
			return "IGS_WARMUP_USER";
		case IGS_START_COUNTDOWN:
			return "IGS_START_COUNTDOWN";
		case IGS_GAME_PAUSED:
			return "IGS_GAME_PAUSED";
		case IGS_GAME_RUNNING:
			return "IGS_GAME_RUNNING";
		case IGS_END_MATCH:
			return "IGS_END_MATCH";
		case IGS_END_ROUND:
			return "IGS_END_ROUND";
		}
		return "UNKNOWN";
	}

	virtual void DoWincheckRound() {}
	bool HasEnoughPlayers() const { return (IsTeamplay() && m_aTeamSize[TEAM_RED] > 0 && m_aTeamSize[TEAM_BLUE] > 0) || (!IsTeamplay() && m_aTeamSize[TEAM_RED] > 1); }
	void SetGameState(EGameState GameState, int Timer = 0);
	void StartMatch();

protected:
	struct CGameInfo
	{
		int m_MatchCurrent;
		int m_MatchNum;
		int m_ScoreLimit;
		int m_TimeLimit;
	} m_GameInfo;
	void UpdateGameInfo(int ClientID);
	int m_GameStartTick;
	int m_aTeamscore[protocol7::NUM_TEAMS];

	// void EndMatch() { SetGameState(IGS_END_MATCH, TIMER_END); }
	void EndMatch();

public:
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	int GetStartTeam();
	virtual bool DoWincheckMatch(); // returns true when the match is over
	virtual void OnFlagReturn(class CFlag *pFlag); // gctf
	virtual void OnFlagGrab(class CFlag *pFlag); // gctf
	virtual void OnFlagCapture(class CFlag *pFlag, float Time); // gctf
};

#endif
