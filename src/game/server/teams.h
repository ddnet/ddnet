/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/score.h>
#include <game/teamscore.h>

#include <utility>

class CGameTeams
{
	int m_TeamState[MAX_CLIENTS];
	bool m_TeeFinished[MAX_CLIENTS];
	bool m_TeamLocked[MAX_CLIENTS];
	uint64_t m_Invited[MAX_CLIENTS];
	bool m_Practice[MAX_CLIENTS];
	std::shared_ptr<CScoreSaveResult> m_pSaveTeamResult[MAX_CLIENTS];
	uint64_t m_LastSwap[MAX_CLIENTS];

	class CGameContext *m_pGameContext;

	bool TeamFinished(int Team);
	void OnTeamFinish(CPlayer **Players, unsigned int Size, float Time, const char *pTimestamp);
	void OnFinish(CPlayer *Player, float Time, const char *pTimestamp);

public:
	enum
	{
		TEAMSTATE_EMPTY,
		TEAMSTATE_OPEN,
		TEAMSTATE_STARTED,
		TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);

	// helper methods
	CCharacter *Character(int ClientID)
	{
		return GameServer()->GetPlayerChar(ClientID);
	}
	CPlayer *GetPlayer(int ClientID)
	{
		return GameServer()->m_apPlayers[ClientID];
	}

	class CGameContext *GameServer()
	{
		return m_pGameContext;
	}
	class IServer *Server()
	{
		return m_pGameContext->Server();
	}

	void OnCharacterStart(int ClientID);
	void OnCharacterFinish(int ClientID);
	void OnCharacterSpawn(int ClientID);
	void OnCharacterDeath(int ClientID, int Weapon);

	// returns nullptr if successful, error string if failed
	const char *SetCharacterTeam(int ClientID, int Team);
	void CheckTeamFinished(int ClientID);

	void ChangeTeamState(int Team, int State);

	int64_t TeamMask(int Team, int ExceptID = -1, int Asker = -1);

	int Count(int Team) const;

	// need to be very careful using this method. SERIOUSLY...
	void SetForceCharacterTeam(int ClientID, int Team);

	void Reset();
	void ResetRoundState(int Team);
	void ResetSwitchers(int Team);

	void SendTeamsState(int ClientID);
	void SetTeamLock(int Team, bool Lock);
	void ResetInvited(int Team);
	void SetClientInvited(int Team, int ClientID, bool Invited);

	int m_LastChat[MAX_CLIENTS];

	int GetDDRaceState(CPlayer *Player);
	int GetStartTime(CPlayer *Player);
	float *GetCpCurrent(CPlayer *Player);
	void SetDDRaceState(CPlayer *Player, int DDRaceState);
	void SetStartTime(CPlayer *Player, int StartTime);
	void SetCpActive(CPlayer *Player, int CpActive);
	void KillSavedTeam(int ClientID, int Team);
	void ResetSavedTeam(int ClientID, int Team);
	void RequestTeamSwap(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team);
	void SwapTeamCharacters(CPlayer *pPlayer, CPlayer *pTargetPlayer, int Team);
	void ProcessSaveTeam();

	bool TeeFinished(int ClientID)
	{
		return m_TeeFinished[ClientID];
	}

	int GetTeamState(int Team)
	{
		return m_TeamState[Team];
	}

	bool TeamLocked(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_TeamLocked[Team];
	}

	bool IsInvited(int Team, int ClientID)
	{
		return m_Invited[Team] & 1LL << ClientID;
	}

	bool IsStarted(int Team)
	{
		return m_TeamState[Team] == CGameTeams::TEAMSTATE_STARTED;
	}

	void SetFinished(int ClientID, bool finished)
	{
		m_TeeFinished[ClientID] = finished;
	}

	void SetSaving(int TeamID, std::shared_ptr<CScoreSaveResult> &SaveResult)
	{
		m_pSaveTeamResult[TeamID] = SaveResult;
	}

	bool GetSaving(int TeamID)
	{
		if(TeamID < TEAM_FLOCK || TeamID >= TEAM_SUPER)
			return false;
		if(g_Config.m_SvTeam != 3 && TeamID == TEAM_FLOCK)
			return false;

		return m_pSaveTeamResult[TeamID] != nullptr;
	}

	void EnablePractice(int Team)
	{
		if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
			return;
		if(g_Config.m_SvTeam != 3 && Team == TEAM_FLOCK)
			return;

		m_Practice[Team] = true;
	}

	bool IsPractice(int Team)
	{
		if(Team < TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;
		if(g_Config.m_SvTeam != 3 && Team == TEAM_FLOCK)
			return false;

		return m_Practice[Team];
	}
};

#endif
