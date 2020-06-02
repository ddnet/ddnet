/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <game/teamscore.h>
#include <game/server/gamecontext.h>

#if defined(CONF_SQL)
class CSqlSaveResult;
#endif

class CGameTeams
{
	int m_TeamState[MAX_CLIENTS];
	int m_MembersCount[MAX_CLIENTS];
	bool m_TeeFinished[MAX_CLIENTS];
	bool m_TeamLocked[MAX_CLIENTS];
	uint64_t m_Invited[MAX_CLIENTS];
	bool m_Practice[MAX_CLIENTS];
#if defined(CONF_SQL)
	std::shared_ptr<CSqlSaveResult> m_pSaveTeamResult[MAX_CLIENTS];
#endif

	class CGameContext * m_pGameContext;

	void CheckTeamFinished(int ClientID);
	bool TeamFinished(int Team);
	void OnTeamFinish(CPlayer** Players, unsigned int Size, float Time, const char *pTimestamp);
	void OnFinish(CPlayer* Player, float Time, const char *pTimestamp);

public:
	enum
	{
		TEAMSTATE_EMPTY, TEAMSTATE_OPEN, TEAMSTATE_STARTED, TEAMSTATE_FINISHED
	};

	CTeamsCore m_Core;

	CGameTeams(CGameContext *pGameContext);

	//helper methods
	CCharacter* Character(int ClientID)
	{
		return GameServer()->GetPlayerChar(ClientID);
	}
	CPlayer* GetPlayer(int ClientID)
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

	bool SetCharacterTeam(int ClientID, int Team);

	void ChangeTeamState(int Team, int State);
	void onChangeTeamState(int Team, int State, int OldState);

	int64_t TeamMask(int Team, int ExceptID = -1, int Asker = -1);

	int Count(int Team) const;

	//need to be very careful using this method. SERIOUSLY...
	void SetForceCharacterTeam(int ClientID, int Team);
	void SetForceCharacterNewTeam(int ClientID, int Team);
	void ForceLeaveTeam(int ClientID);

	void Reset();

	void SendTeamsState(int ClientID);
	void SetTeamLock(int Team, bool Lock);
	void ResetInvited(int Team);
	void SetClientInvited(int Team, int ClientID, bool Invited);

	int m_LastChat[MAX_CLIENTS];

	int GetDDRaceState(CPlayer* Player);
	int GetStartTime(CPlayer* Player);
	float *GetCpCurrent(CPlayer* Player);
	void SetDDRaceState(CPlayer* Player, int DDRaceState);
	void SetStartTime(CPlayer* Player, int StartTime);
	void SetCpActive(CPlayer* Player, int CpActive);
#if defined(CONF_SQL)
	void KillSavedTeam(int ClientID, int Team);
	void ResetSavedTeam(int ClientID, int Team);
	void ProcessSaveTeam();
#endif

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
		if (Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_TeamLocked[Team];
	}

	bool IsInvited(int Team, int ClientID)
	{
		return m_Invited[Team] & 1LL << ClientID;
	}

	void SetFinished(int ClientID, bool finished)
	{
		m_TeeFinished[ClientID] = finished;
	}
#if defined(CONF_SQL)
	void SetSaving(int TeamID, std::shared_ptr<CSqlSaveResult> SaveResult)
	{
		m_pSaveTeamResult[TeamID] = SaveResult;
	}
#endif

	bool GetSaving(int TeamID)
	{
#if defined(CONF_SQL)
		return m_pSaveTeamResult[TeamID] != nullptr;
#else
		return false;
#endif
	}

	void EnablePractice(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return;

		m_Practice[Team] = true;
	}

	bool IsPractice(int Team)
	{
		if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
			return false;

		return m_Practice[Team];
	}
};

#endif
