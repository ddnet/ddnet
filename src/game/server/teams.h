#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <game/teamscore.h>
#include <game/server/gamecontext.h>

class CGameTeams
{
	int m_TeamState[MAX_CLIENTS];
	int m_MembersCount[MAX_CLIENTS];
	bool m_TeeFinished[MAX_CLIENTS];
	int m_TeamLeader[MAX_CLIENTS];
	int m_TeeJoinTick[MAX_CLIENTS];
	bool m_TeamStrict[MAX_CLIENTS];
	bool m_TeePassedStart[MAX_CLIENTS];
	
	class CGameContext * m_pGameContext;
	
	
public:
	enum TeamState
	{
		TEAMSTATE_EMPTY, 
		TEAMSTATE_OPEN,
		TEAMSTATE_STARTED
	};
	
	CTeamsCore m_Core;
	
	CGameTeams(CGameContext *pGameContext);
	
	//helper methods
	CCharacter* Character(int ClientID) { return GameServer()->GetPlayerChar(ClientID); }

	class CGameContext *GameServer() { return m_pGameContext; }
	class IServer *Server() { return m_pGameContext->Server(); }
	
	void OnCharacterStart(int ClientID);
	void OnCharacterFinish(int ClientID);
	void OnCharacterDeath(int ClientID);
	
	int SetCharacterTeam(int ClientID, int Team);
	enum TeamErrors
	{
		ERROR_WRONG_PARAMS = -6,
		ERROR_CLOSED,
		ERROR_ALREADY_THERE,
		ERROR_NOT_SUPER,
		ERROR_STARTED,
		ERROR_PASSEDSTART
	};
	
	void ChangeTeamState(int Team, int State) { m_TeamState[Team] = State; };
	
	bool TeamFinished(int Team);

	int TeamMask(int Team, int ExceptID = -1);
	
	int Count(int Team) const;
	
	//need to be very careful using this method
	void SetForceCharacterTeam(int ClientID, int Team);
	
	void Reset();
	
	void SendTeamsState(int ClientID);

	int m_LastChat[MAX_CLIENTS];

	bool GetTeamState(int Team) { return m_TeamState[Team]; };

	int GetTeamLeader(int Team) { return m_TeamLeader[Team]; };

	void SetTeamLeader(int Team, int ClientID);
	void ToggleStrictness(int Team);
};

#endif
