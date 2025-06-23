/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <engine/shared/protocol.h>
#include <game/race_state.h>
#include <game/server/gamecontext.h>
#include <game/team_state.h>
#include <game/teamscore.h>

class CCharacter;
class CPlayer;
struct CScoreSaveResult;

class CGameTeams
{
	// `m_TeeStarted` is used to keep track whether a given tee has hit the
	// start of the map yet. If a tee that leaves hasn't hit the start line
	// yet, the team will be marked as "not allowed to finish"
	// (`ETeamState::STARTED_UNFINISHABLE`). If this were not the case, tees
	// could go around the startline on a map, leave one tee behind at
	// start, go to the finish line, let the tee start and kill, allowing
	// the team to finish instantly.
	bool m_aTeeStarted[MAX_CLIENTS];
	bool m_aTeeFinished[MAX_CLIENTS];
	int m_aLastChat[MAX_CLIENTS];

	ETeamState m_aTeamState[NUM_DDRACE_TEAMS];
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
	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);

	// helper methods
	CCharacter *Character(int ClientId);
	CPlayer *GetPlayer(int ClientId);
	class CGameContext *GameServer();
	class IServer *Server();

	void OnCharacterStart(int ClientId);
	void OnCharacterFinish(int ClientId);
	void OnCharacterSpawn(int ClientId);
	void OnCharacterDeath(int ClientId, int Weapon);
	void Tick();

	// returns nullptr if successful, error string if failed
	const char *SetCharacterTeam(int ClientId, int Team);
	void CheckTeamFinished(int Team);

	void ChangeTeamState(int Team, ETeamState State);

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

	ERaceState GetDDRaceState(const CPlayer *Player) const;
	int GetStartTime(CPlayer *Player);
	float *GetCurrentTimeCp(CPlayer *Player);
	void SetDDRaceState(CPlayer *Player, ERaceState DDRaceState);
	void SetStartTime(CPlayer *Player, int StartTime);
	void SetLastTimeCp(CPlayer *Player, int LastTimeCp);
	void KillCharacterOrTeam(int ClientId, int Team);
	void ResetSavedTeam(int ClientId, int Team);
	void RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team);
	void SwapTeamCharacters(CPlayer *pPrimaryPlayer, CPlayer *pTargetPlayer, int Team);
	void CancelTeamSwap(CPlayer *pPlayer, int Team);
	void ProcessSaveTeam();
	int GetFirstEmptyTeam() const;
	bool TeeStarted(int ClientId) const;
	bool TeeFinished(int ClientId) const;
	ETeamState GetTeamState(int Team) const;
	bool TeamLocked(int Team) const;
	bool TeamFlock(int Team) const;
	bool IsInvited(int Team, int ClientId) const;
	bool IsStarted(int Team) const;
	void SetStarted(int ClientId, bool Started);
	void SetFinished(int ClientId, bool Finished);
	void SetSaving(int TeamId, std::shared_ptr<CScoreSaveResult> &SaveResult);
	bool GetSaving(int TeamId) const;
	void SetPractice(int Team, bool Enabled);
	bool IsPractice(int Team);
};

#endif
