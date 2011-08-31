#ifndef GAME_SERVER_TEAMS_H
#define GAME_SERVER_TEAMS_H

#include <game/teamscore.h>
#include <game/server/gamecontext.h>

class CGameTeams
{
	int m_TeamState[MAX_CLIENTS];
	int m_MembersCount[MAX_CLIENTS];
	bool m_TeeFinished[MAX_CLIENTS];
	
	class CGameContext * m_pGameContext;
	
	
public:
	enum
	{
		TEAMSTATE_EMPTY, 
		TEAMSTATE_OPEN,
		TEAMSTATE_CLOSED,
		TEAMSTATE_STARTED,
		TEAMSTATE_FINISHED
	};
	
	CTeamsCore m_Core;
	
	CGameTeams(CGameContext *pGameContext);
	
	//helper methods
	CCharacter* Character(int ClientID) { return GameServer()->GetPlayerChar(ClientID); }
	CPlayer* GetPlayer(int ClientID) { return GameServer()->m_apPlayers[ClientID]; }

	class CGameContext *GameServer() { return m_pGameContext; }
	class IServer *Server() { return m_pGameContext->Server(); }
	
	void OnCharacterStart(int ClientID);
	void OnCharacterFinish(int ClientID);
	
	bool SetCharacterTeam(int ClientID, int Team);
	
	void ChangeTeamState(int Team, int State);
	
	bool TeamFinished(int Team);

	int TeamMask(int Team, int ExceptID = -1);
	
	int Count(int Team) const;
	
	//need to be very carefull using this method
	void SetForceCharacterTeam(int id, int Team);
	
	void Reset();
	
	void SendTeamsState(int Cid);

	int m_LastChat[MAX_CLIENTS];

	int GetDDRaceState(CPlayer* Player);
	int GetStartTime(CPlayer* Player);
	float *GetCpCurrent(CPlayer* Player);
	void SetDDRaceState(CPlayer* Player, int DDRaceState);
	void SetStartTime(CPlayer* Player, int StartTime);
	void SetRefreshTime(CPlayer* Player, int RefreshTime);
	void SetCpActive(CPlayer* Player, int CpActive);
	void OnFinish(CPlayer* Player);
	bool TeeFinished(int ClientID) { return m_TeeFinished[ClientID]; };
};

#endif
