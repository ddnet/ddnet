#ifndef GAME_SERVER_INSTAGIB_GAMECONTROLLER_H
// hack for headerguard linter
#endif

#ifndef IN_CLASS_IGAMECONTROLLER

#include <base/vmath.h>
#include <engine/map.h>
#include <engine/shared/protocol.h>
#include <game/server/teams.h>

#include <engine/shared/http.h> // ddnet-insta
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>

#include <game/server/instagib/sql_stats.h>
#include <game/server/instagib/sql_stats_player.h>

struct CScoreLoadBestTimeResult;

class IGameController
{
#endif // IN_CLASS_IGAMECONTROLLER

public:
	//      _     _            _        _           _
	//   __| | __| |_ __   ___| |_     (_)_ __  ___| |_ __ _
	//  / _` |/ _` | '_ \ / _ \ __|____| | '_ \/ __| __/ _` |
	// | (_| | (_| | | | |  __/ ||_____| | | | \__ \ || (_| |
	//  \__,_|\__,_|_| |_|\___|\__|    |_|_| |_|___/\__\__,_|
	//
	// all code below should be ddnet-insta
	//

	// virtual bool OnLaserHitCharacter(vec2 From, vec2 To, class CLaser &Laser) {};
	/*
		Function: OnCharacterTakeDamage
			this function was added in ddnet-insta and is a non standard controller method.
			neither ddnet nor teeworlds have this

		Returns:
			return true to skip ddnet CCharacter::TakeDamage() behavior
			which is applying the force and moving the damaged tee
			it also sets the happy eyes if the Dmg is not zero
	*/
	virtual bool OnCharacterTakeDamage(vec2 &Force, int &Dmg, int &From, int &Weapon, CCharacter &Character) { return false; };

	/*
		Function: OnFireWeapon
			this function was added in ddnet-insta and is a non standard controller method.
			neither ddnet nor teeworlds have this

		Returns:
			return true to skip ddnet CCharacter::FireWeapon() behavior
			which is doing standard ddnet fire weapon things
	*/
	virtual bool OnFireWeapon(CCharacter &Character, int &Weapon, vec2 &Direction, vec2 &MouseTarget, vec2 &ProjStartPos) { return false; };

	/*
		Function: OnChatMessage
			hooks into CGameContext::OnSayNetMessage()
			after unicode check and teehistorian already happend

		Returns:
			return true to not run the rest of CGameContext::OnSayNetMessage()
			which would print it to the chat or run it as a ddrace chat command
	*/
	virtual bool OnChatMessage(const CNetMsg_Cl_Say *pMsg, int Length, int &Team, CPlayer *pPlayer) { return false; };

	/*
		Function: OnChangeInfoNetMessage
			hooks into CGameContext::OnChangeInfoNetMessage()
			after spam protection check

		Returns:
			return true to not run the rest of CGameContext::OnChangeInfoNetMessage()
	*/
	virtual bool OnChangeInfoNetMessage(const CNetMsg_Cl_ChangeInfo *pMsg, int ClientId) { return false; }

	/*
		Function: OnSetTeamNetMessage
			hooks into CGameContext::OnSetTeamNetMessage()
			before any spam protection check

			See also CanJoinTeam() which is called after the validation

		Returns:
			return true to not run the rest of CGameContext::OnSetTeamNetMessage()
	*/
	virtual bool OnSetTeamNetMessage(const CNetMsg_Cl_SetTeam *pMsg, int ClientId) { return false; };

	/*
		Function: OnVoteNetMessage
			hooks into CGameContext::OnVoteNetMessage()
			before any spam protection check

		Returns:
			return true to not run the rest of CGameContext::OnVoteNetMessage()
	*/
	virtual bool OnVoteNetMessage(const CNetMsg_Cl_Vote *pMsg, int ClientId) { return false; }

	/*
		Function: GetPlayerTeam
			wraps CPlayer::GetTeam()
			to spoof fake teams for different versions
			this can be used to place players into spec for 0.6 and dead spec for 0.7

		Arguments:
			Sixup - will be true if that team value is sent to a 0.7 connection and false otherwise

		Returns:
			as integer TEAM_RED, TEAM_BLUE or TEAM_SPECTATORS
	*/
	virtual int GetPlayerTeam(class CPlayer *pPlayer, bool Sixup);

	/*
		Function: GetDefaultWeapon
			Returns the weapon the tee should spawn with.
			Is not a complete list of all weapons the tee gets on spawn.

			The complete list of weapons depends on the active controller and what it sets in
			its OnCharacterSpawn() if it is a gamemode without fixed weapons
			it depends on sv_spawn_weapons then it will call IGameController:SetSpawnWeapons()
	*/
	virtual int GetDefaultWeapon(class CPlayer *pPlayer) { return WEAPON_GUN; }

	/*
		Function: SetSpawnWeapons
			Is empty by default because ddnet and many ddnet-insta modes cover that in
			IGameController::OnCharacterSpawn()
			This method was added to set spawn weapons independently of the gamecontroller
			so we can use the same zCatch controller for laser and grenade zCatch

			It is also different from GetDefaultWeapon() because it could set more then one weapon.

			All gamemodes that allow different type of spawn weapons should call SetSpawnWeapons()
			in their OnCharacterSpawn() hook and also set the default weapon to GetDefaultWeaponBasedOnSpawnWeapons()
	*/
	virtual void SetSpawnWeapons(class CCharacter *pChr){};

	/*
		Function: UpdateSpawnWeapons
			called when the config sv_spawn_weapons is updated
			to update the internal enum

		Arguments:
			Silent - if false it might print warnings to the admin console
	*/
	virtual void UpdateSpawnWeapons(bool Silent = false){};

	/*
		Function: IsWinner
			called on disconnect and round end
			used to track stats

		Arguments:
			pPlayer - the player to check
			pMessage - should be sent to pPlayer in chat contains messages such as "you gained one win", "this win did not count because xyz"
			SizeOfMessage - size of the message buffer
	*/
	virtual bool IsWinner(const CPlayer *pPlayer, char *pMessage, int SizeOfMessage) { return false; }

	/*
		Function: IsLoser
			called on disconnect and round end
			used to track stats

		Arguments:
			pPlayer - the player to check
	*/
	virtual bool IsLoser(const CPlayer *pPlayer) { return false; }

	/*
		Function: OnShowStatsAll
			called from the main thread when a SQL worker finished querying stats from the database

		Arguments:
			pStats - stats struct to display
			pRequestingPlayer - player who initiated the stats request (might differ from the requested player)
			pRequestedName - player name the stats belong to
	*/
	virtual void OnShowStatsAll(const CSqlStatsPlayer *pStats, class CPlayer *pRequestingPlayer, const char *pRequestedName){};

	/*
		Function: OnShowRank
			called from the main thread when a SQL worker finished querying a rank from the database

		Arguments:
			Rank - is the rank the player got with its score compared to all other players (lower is better)
			RankedScore - is the score that was used to obtain the rank if its ranking kills this will be the amount of kills
			pRankType - is the displayable string that shows the type of ranks (for example "Kills")
			pRequestingPlayer - player who initiated the stats request (might differ from the requested player)
			pRequestedName - player name the stats belong to
	*/
	virtual void OnShowRank(int Rank, int RankedScore, const char *pRankType, class CPlayer *pRequestingPlayer, const char *pRequestedName){};
	virtual void OnPlayerReadyChange(class CPlayer *pPlayer); // 0.7 ready change
	virtual int GameInfoExFlags(int SnappingClient) { return 0; }; // TODO: this breaks the ddrace gametype
	virtual int GameInfoExFlags2(int SnappingClient) { return 0; };
	virtual void OnSnapDDNetCharacter(class CCharacter *pChr, CNetObj_DDNetCharacter *pDDNetCharacter, int SnappingClient){};
	virtual CClientMask FreezeDamageIndicatorMask(class CCharacter *pChr);

	int m_DefaultWeapon = WEAPON_GUN;
	void CheckReadyStates(int WithoutId = -1);
	bool GetPlayersReadyState(int WithoutId = -1, int *pNumUnready = nullptr);
	void SetPlayersReadyState(bool ReadyState);
	bool IsPlayerReadyMode();
	int IsGameRunning() { return m_GameState == IGS_GAME_RUNNING; }
	int IsGameCountdown() { return m_GameState == IGS_START_COUNTDOWN_ROUND_START || m_GameState == IGS_START_COUNTDOWN_UNPAUSE; }
	void ToggleGamePause();
	void AbortWarmup()
	{
		if((m_GameState == IGS_WARMUP_GAME || m_GameState == IGS_WARMUP_USER) && m_GameStateTimer != TIMER_INFINITE)
		{
			SetGameState(IGS_GAME_RUNNING);
		}
	}
	void SwapTeamscore()
	{
		if(!IsTeamplay())
			return;

		int Score = m_aTeamscore[TEAM_RED];
		m_aTeamscore[TEAM_RED] = m_aTeamscore[TEAM_BLUE];
		m_aTeamscore[TEAM_BLUE] = Score;
	};

	int m_aTeamSize[protocol7::NUM_TEAMS];

	// game
	enum EGameState
	{
		// internal game states
		IGS_WARMUP_GAME, // warmup started by game because there're not enough players (infinite)
		IGS_WARMUP_USER, // warmup started by user action via rcon or new match (infinite or timer)

		IGS_START_COUNTDOWN_ROUND_START, // start countown to start match/round (tick timer)
		IGS_START_COUNTDOWN_UNPAUSE, // start countown to unpause the game

		IGS_GAME_PAUSED, // game paused (infinite or tick timer)
		IGS_GAME_RUNNING, // game running (infinite)

		IGS_END_ROUND, // round is over (tick timer)
	};
	EGameState m_GameState;
	EGameState GameState() const { return m_GameState; }
	int m_GameStateTimer;

	const char *GameStateToStr(EGameState GameState)
	{
		switch(GameState)
		{
		case IGS_WARMUP_GAME:
			return "IGS_WARMUP_GAME";
		case IGS_WARMUP_USER:
			return "IGS_WARMUP_USER";
		case IGS_START_COUNTDOWN_ROUND_START:
			return "IGS_START_COUNTDOWN_ROUND_START";
		case IGS_START_COUNTDOWN_UNPAUSE:
			return "IGS_START_COUNTDOWN_UNPAUSE";
		case IGS_GAME_PAUSED:
			return "IGS_GAME_PAUSED";
		case IGS_GAME_RUNNING:
			return "IGS_GAME_RUNNING";
		case IGS_END_ROUND:
			return "IGS_END_ROUND";
		}
		return "UNKNOWN";
	}

	bool HasEnoughPlayers() const { return (IsTeamplay() && m_aTeamSize[TEAM_RED] > 0 && m_aTeamSize[TEAM_BLUE] > 0) || (!IsTeamplay() && m_aTeamSize[TEAM_RED] > 1); }
	void SetGameState(EGameState GameState, int Timer = 0);

	bool m_AllowSkinChange = true;

	// protected:
public:
	struct CGameInfo
	{
		int m_MatchCurrent;
		int m_MatchNum;
		int m_ScoreLimit;
		int m_TimeLimit;
	} m_GameInfo;
	void SendGameInfo(int ClientId);
	/*
		Variable: m_GameStartTick
			Sent in snap to 0.7 clients for timer
	*/
	int m_GameStartTick;
	int m_aTeamscore[protocol7::NUM_TEAMS];

	void EndRound() { SetGameState(IGS_END_ROUND, TIMER_END); }

	void OnEndRoundInsta();
	void GetRoundEndStatsStrCsv(char *pBuf, size_t Size);
	void PsvRowPlayer(const CPlayer *pPlayer, char *pBuf, size_t Size);
	void GetRoundEndStatsStrJson(char *pBuf, size_t Size);
	void GetRoundEndStatsStrPsv(char *pBuf, size_t Size);
	void GetRoundEndStatsStrAsciiTable(char *pBuf, size_t Size);
	void GetRoundEndStatsStrHttp(char *pBuf, size_t Size);
	void GetRoundEndStatsStrDiscord(char *pBuf, size_t Size);
	void GetRoundEndStatsStrFile(char *pBuf, size_t Size);
	void PublishRoundEndStatsStrFile(const char *pStr);
	void PublishRoundEndStatsStrDiscord(const char *pStr);
	void PublishRoundEndStatsStrHttp(const char *pStr);
	void PublishRoundEndStats();

public:
	enum
	{
		TIMER_INFINITE = -1,
		TIMER_END = 10,
	};

	virtual bool DoWincheckRound(); // returns true when the match is over
	virtual void OnFlagReturn(class CFlag *pFlag); // ddnet-insta
	virtual void OnFlagGrab(class CFlag *pFlag); // ddnet-insta
	virtual void OnFlagCapture(class CFlag *pFlag, float Time); // ddnet-insta
	virtual void OnRoundStart();
	// return true to consume the event
	// and supress default ddnet selfkill behavior
	virtual bool OnSelfkill(int ClientId) { return false; };
	virtual void OnUpdateZcatchColorConfig(){};
	virtual void OnUpdateSpectatorVotesConfig(){};
	virtual bool DropFlag(class CCharacter *pChr) { return false; };

	/*
		Variable: m_GamePauseStartTime

		gets set to time_get() when a player pauses the game
		using the ready change if sv_player_ready_mode is active

		it can then be used to track how long a game has been paused already

		it is set to -1 if the game is currently not paused
	*/
	int64_t m_GamePauseStartTime;

	bool IsSkinChangeAllowed() const { return m_AllowSkinChange; }
	int GameFlags() const { return m_GameFlags; }

	CSqlStats *m_pSqlStats = nullptr;
	const char *m_pStatsTable = "";
	const char *StatsTable() const { return m_pStatsTable; }

private:
#ifndef IN_CLASS_IGAMECONTROLLER
};
#endif
