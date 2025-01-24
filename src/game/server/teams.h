/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/teamscore.h>

class CCharacter;
class CPlayer;
struct CScoreSaveResult;

class CGameTeams
{
	// `m_TeeStarted` is used to keep track whether a given tee has hit the
	// start of the map yet. If a tee that leaves hasn't hit the start line
	// yet, the team will be marked as "not allowed to finish"
	// (`TEAMSTATE_STARTED_UNFINISHABLE`). If this were not the case, tees
	// could go around the startline on a map, leave one tee behind at
	// start, go to the finish line, let the tee start and kill, allowing
	// the team to finish instantly.
	bool m_aTeeStarted[MAX_CLIENTS];
	bool m_aTeeFinished[MAX_CLIENTS];
	int m_aLastChat[MAX_CLIENTS];

	int m_aTeamState[NUM_DDRACE_TEAMS];
	bool m_aTeamLocked[NUM_DDRACE_TEAMS];
	bool m_aTeamFlock[NUM_DDRACE_TEAMS];
	CClientMask m_aInvited[NUM_DDRACE_TEAMS];
	bool m_aPractice[NUM_DDRACE_TEAMS];
	std::shared_ptr<CScoreSaveResult> m_apSaveTeamResult[NUM_DDRACE_TEAMS];
	uint64_t m_aLastSwap[MAX_CLIENTS]; // index is id of player who initiated swap
	bool m_aTeamSentStartWarning[NUM_DDRACE_TEAMS];
	// `m_aTeamUnfinishableKillTick` is -1 by default and gets set when a
	// team becomes unfinishable. If the team hasn't entered practice mode
	// by that time, it'll get killed to prevent people not understanding
	// the message from playing for a long time in an unfinishable team.
	int m_aTeamUnfinishableKillTick[NUM_DDRACE_TEAMS];

	class CGameContext *m_pGameContext;

	/**
	* Kill the whole team.
	* @param Team The team id to kill
	* @param NewStrongId The player with that id will get strong hook on everyone else, -1 will set the normal spawning order
	* @param ExceptId The player that should not get killed
	*/
	void KillTeam(int Team, int NewStrongId, int ExceptId = -1);
	bool TeamFinished(int Team);
	void OnTeamFinish(int Team, CPlayer **Players, unsigned int Size, int TimeTicks, const char *pTimestamp);
	void OnFinish(CPlayer *Player, int TimeTicks, const char *pTimestamp);

public:
	enum
	{
		TEAMSTATE_EMPTY,
		TEAMSTATE_OPEN,
		TEAMSTATE_STARTED,
		// Happens when a tee that hasn't hit the start tiles leaves
		// the team.
		TEAMSTATE_STARTED_UNFINISHABLE,
		TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);

	// helper methods
	CCharacter *Character(int ClientId)
	{
		return GameServer()->GetPlayerChar(ClientId);
	}
	CPlayer *GetPlayer(int ClientId)
	{
		return GameServer()->m_apPlayers[ClientId];
	}

	class CGameContext *GameServer()
	{
		return m_pGameContext;
	}
	class IServer *Server()
	{
		return m_pGameContext->Server();
	}

	void OnCharacterStart(int ClientId);
	void OnCharacterFinish(int ClientId);
	void OnCharacterSpawn(int ClientId);
	void OnCharacterDeath(int ClientId, int Weapon);
	void Tick();

	// returns nullptr if successful, error string if failed
	const char *SetCharacterTeam(int ClientId, int Team);
	void CheckTeamFinished(int Team);

	void ChangeTeamState(int Team, int State);

	CClientMask TeamMask(int Team, int ExceptId = -1, int Asker = -1, int VersionFlags = CGameContext::FLAG_SIX | CGameContext::FLAG_SIXUP);

	int Count(int Team) const;

	// need to be very careful using this method. SERIOUSLY...
	void SetForceCharacterTeam(int ClientId, int Team);

	void Reset();
	void ResetRoundState(int Team);
	void ResetSwitchers(int Team);

	void SendTeamsState(int ClientId);
	void SetTeamLock(int Team, bool Lock);
	void SetTeamFlock(int Team, bool Mode);
	void ResetInvited(int Team);
	void SetClientInvited(int Team, int ClientId, bool Invited);

	int GetDDRaceState(CPlayer *Player);
	int GetStartTime(CPlayer *Player);
	float *GetCurrentTimeCp(CPlayer *Player);
	void SetDDRaceState(CPlayer *Player, int DDRaceState);
	void SetStartTime(CPlayer *Player, int StartTime);
	void SetLastTimeCp(CPlayer *Player, int LastTimeCp);
	void KillSavedTeam(int ClientId, int Team);
	void ResetSavedTeam(int ClientId, int Team);
	void RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team);
	void SwapTeamCharacters(CPlayer *pPrimaryPlayer, CPlayer *pTargetPlayer, int Team);
	void CancelTeamSwap(CPlayer *pPlayer, int Team);
	void ProcessSaveTeam();

	int GetFirstEmptyTeam() const;

	bool TeeStarted(int ClientId)
	{
		return m_aTeeStarted[ClientId];
	}

	bool TeeFinished(int ClientId)
	{
		return m_aTeeFinished[ClientId];
	}

	int GetTeamState(int Team)
	{
		return m_aTeamState[Team];
	}

	bool TeamLocked(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_aTeamLocked[Team];
	}

	bool TeamFlock(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_aTeamFlock[Team];
	}

	bool IsInvited(int Team, int ClientId)
	{
		return m_aInvited[Team].test(ClientId);
	}

	bool IsStarted(int Team)
	{
		return m_aTeamState[Team] == CGameTeams::TEAMSTATE_STARTED;
	}

	void SetStarted(int ClientId, bool Started)
	{
		m_aTeeStarted[ClientId] = Started;
	}

	void SetFinished(int ClientId, bool Finished)
	{
		m_aTeeFinished[ClientId] = Finished;
	}

	void SetSaving(int TeamId, std::shared_ptr<CScoreSaveResult> &SaveResult)
	{
		m_apSaveTeamResult[TeamId] = SaveResult;
	}

	bool GetSaving(int TeamId)
	{
		if(TeamId < TEAM_FLOCK || TeamId >= TEAM_SUPER)
			return false;
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && TeamId == TEAM_FLOCK)
			return false;

		return m_apSaveTeamResult[TeamId] != nullptr;
	}

	void SetPractice(int Team, bool Enabled)
	{
		if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
			return;
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK)
			return;

		m_aPractice[Team] = Enabled;
	}

	bool IsPractice(int Team)
	{
		if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;
		if(g_Config.m_SvPracticeByDefault && g_Config.m_SvTestingCommands)
			return true;
		if(g_Config.m_SvTeam != SV_TEAM_FORCED_SOLO && Team == TEAM_FLOCK)
			return false;

		return m_aPractice[Team];
	}
};

#endif
