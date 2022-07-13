/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include "alloc.h"
#include "teeinfo.h"

#include <memory>

class CCharacter;
class CGameContext;
class IServer;
struct CNetObj_PlayerInput;
struct CScorePlayerResult;

enum
{
	WEAPON_GAME = -3, // team switching etc
	WEAPON_SELF = -2, // console kill command
	WEAPON_WORLD = -1, // death tiles etc
};

// player object
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CPlayer(CGameContext *pGameServer, uint32_t UniqueClientID, int ClientID, int Team);
	~CPlayer();

	void Reset();

	void TryRespawn();
	void Respawn(bool WeakHook = false); // with WeakHook == true the character will be spawned after all calls of Tick from other Players
	CCharacter *ForceSpawn(vec2 Pos); // required for loading savegames
	void SetTeam(int Team, bool DoChatMsg = true);
	int GetTeam() const { return m_Team; }
	int GetCID() const { return m_ClientID; }
	uint32_t GetUniqueCID() const { return m_UniqueClientID; }
	int GetClientVersion() const;
	bool SetTimerType(int TimerType);

	void Tick();
	void PostTick();

	// will be called after all Tick and PostTick calls from other players
	void PostPostTick();
	void Snap(int SnappingClient);
	void FakeSnap();

	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnPredictedEarlyInput(CNetObj_PlayerInput *pNewInput);
	void OnDisconnect();

	void KillCharacter(int Weapon = WEAPON_GAME);
	CCharacter *GetCharacter();

	void SpectatePlayerName(const char *pName);

	//---------------------------------------------------------
	// this is used for snapping so we know how we can clip the view for the player
	vec2 m_ViewPos;
	int m_TuneZone = 0;
	int m_TuneZoneOld = 0;

	// states if the client is chatting, accessing a menu etc.
	int m_PlayerFlags = 0;

	// used for snapping to just update latency if the scoreboard is active
	int m_aCurLatency[MAX_CLIENTS] = {0};

	// used for spectator mode
	int m_SpectatorID = 0;

	bool m_IsReady = false;

	//
	int m_Vote = 0;
	int m_VotePos = 0;
	//
	int m_LastVoteCall = 0;
	int m_LastVoteTry = 0;
	int m_LastChat = 0;
	int m_LastSetTeam = 0;
	int m_LastSetSpectatorMode = 0;
	int m_LastChangeInfo = 0;
	int m_LastEmote = 0;
	int m_LastKill = 0;
	int m_aLastCommands[4] = {0};
	int m_LastCommandPos = 0;
	int m_LastWhisperTo = 0;
	int m_LastInvited = 0;

	int m_SendVoteIndex = 0;

	CTeeInfo m_TeeInfos;

	int m_DieTick = 0;
	int m_PreviousDieTick = 0;
	int m_Score = 0;
	int m_JoinTick = 0;
	bool m_ForceBalanced = false;
	int m_LastActionTick = 0;
	int m_TeamChangeTick = 0;
	bool m_SentSemicolonTip = false;

	// network latency calculations
	struct
	{
		int m_Accum = 0;
		int m_AccumMin = 0;
		int m_AccumMax = 0;
		int m_Avg = 0;
		int m_Min = 0;
		int m_Max = 0;
	} m_Latency;

private:
	const uint32_t m_UniqueClientID = 0;
	CCharacter *m_pCharacter = nullptr;
	int m_NumInputs = 0;
	CGameContext *m_pGameServer = nullptr;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	//
	bool m_Spawning = false;
	bool m_WeakHookSpawn = false;
	int m_ClientID = 0;
	int m_Team = 0;

	int m_Paused = 0;
	int64_t m_ForcePauseTime = 0;
	int64_t m_LastPause = 0;

	int m_DefEmote = 0;
	int m_OverrideEmote = 0;
	int m_OverrideEmoteReset = 0;
	bool m_Halloween = false;

public:
	enum
	{
		PAUSE_NONE = 0,
		PAUSE_PAUSED,
		PAUSE_SPEC
	};

	enum
	{
		TIMERTYPE_DEFAULT = -1,
		TIMERTYPE_GAMETIMER = 0,
		TIMERTYPE_BROADCAST,
		TIMERTYPE_GAMETIMER_AND_BROADCAST,
		TIMERTYPE_SIXUP,
		TIMERTYPE_NONE,
	};

	bool m_DND = false;
	int64_t m_FirstVoteTick = 0;
	char m_aTimeoutCode[64] = {0};

	void ProcessPause();
	int Pause(int State, bool Force);
	int ForcePause(int Time);
	int IsPaused();

	bool IsPlaying();
	int64_t m_Last_KickVote = 0;
	int64_t m_Last_Team = 0;
	int m_ShowOthers = 0;
	bool m_ShowAll = false;
	vec2 m_ShowDistance;
	bool m_SpecTeam = false;
	bool m_NinjaJetpack = false;
	bool m_Afk = false;
	bool m_HasFinishScore = false;

	int m_ChatScore = 0;

	bool m_Moderating = false;

	void UpdatePlaytime();
	void AfkTimer();
	int64_t m_LastPlaytime = 0;
	int64_t m_LastEyeEmote = 0;
	int64_t m_LastBroadcast = 0;
	bool m_LastBroadcastImportance = false;

	CNetObj_PlayerInput *m_pLastTarget = nullptr;
	bool m_LastTargetInit = false;

	bool m_EyeEmoteEnabled = false;
	int m_TimerType = 0;

	int GetDefaultEmote() const;
	void OverrideDefaultEmote(int Emote, int Tick);
	bool CanOverrideDefaultEmote() const;

	bool m_FirstPacket = false;
	int64_t m_LastSQLQuery = 0;
	void ProcessScoreResult(CScorePlayerResult &Result);
	std::shared_ptr<CScorePlayerResult> m_pScoreQueryResult = nullptr;
	std::shared_ptr<CScorePlayerResult> m_pScoreFinishResult = nullptr;
	bool m_NotEligibleForFinish = false;
	int64_t m_EligibleForFinishCheck = 0;
	bool m_VotedForPractice = false;
	int m_SwapTargetsClientID = 0; //Client ID of the swap target for the given player
	bool m_BirthdayAnnounced = false;
};

#endif
