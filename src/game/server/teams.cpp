#include "teams.h"
#include <engine/shared/config.h>
#include <engine/server/server.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) : m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_TeamState[i] = TEAMSTATE_EMPTY;
		m_TeeFinished[i] = false;
		m_MembersCount[i] = 0;
		m_LastChat[i] = 0;
		m_TeamLeader[i] = -1;
		m_TeeJoinTick[i] = -1;
		m_TeamStrict[i] = g_Config.m_SvTeamStrict;
		m_TeePassedStart[i] = false;
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	int Tick = Server()->Tick();
	int Team = m_Core.Team(ClientID);
	CCharacter* StartingChar = Character(ClientID);
	m_TeePassedStart[ClientID] = true;
	if(!StartingChar)
		return;
	if(StartingChar->m_DDRaceState == DDRACE_FINISHED)
		StartingChar->m_DDRaceState = DDRACE_NONE;
	if(Team == TEAM_FLOCK || Team == TEAM_SUPER)
	{
		StartingChar->m_DDRaceState = DDRACE_STARTED;
		StartingChar->m_StartTime = Tick;
		StartingChar->m_RefreshTime = Tick;
	}
	else if(Count(Team) == 1)
	{
		if(m_TeamState[Team] < TEAMSTATE_STARTED)
		{
			ChangeTeamState(Team, TEAMSTATE_STARTED);
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(Team == m_Core.Team(i))
				{
					CCharacter* pChar = Character(i);
					if(pChar && pChar->IsAlive())
					{
						pChar->m_DDRaceState = DDRACE_STARTED;
						pChar->m_StartTime = Tick;
						pChar->m_RefreshTime = Tick;
					}
				}
			}
		}
	}
	else
	{
		bool Waiting = false;
		if(Count(Team) > 1)
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(i != ClientID && Team == m_Core.Team(i))
				{
					CCharacter* pChar = Character(i);
					if(pChar && pChar->IsAlive() && pChar->m_DDRaceState == DDRACE_FINISHED)
					{
						Waiting = true;
						if(m_LastChat[ClientID] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "%s has finished and didn't go through start yet, wait for him or join another team.", Server()->ClientName(i));
							GameServer()->SendChatTarget(ClientID, aBuf);
							m_LastChat[ClientID] = Tick;
						}
						if(m_LastChat[i] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "%s wants to start a new round, kill or walk to start.", Server()->ClientName(ClientID));
							GameServer()->SendChatTarget(i, aBuf);
							m_LastChat[i] = Tick;
						}
					}
					else if(pChar && pChar->IsAlive() && pChar->m_DDRaceState == DDRACE_STARTED)
					{
						Waiting = true;
						if(m_LastChat[ClientID] + Server()->TickSpeed() + g_Config.m_SvChatDelay < Tick)
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "%s has started, wait for him, ask him to kill or join another team.", Server()->ClientName(i));
							GameServer()->SendChatTarget(ClientID, aBuf);
							m_LastChat[ClientID] = Tick;
						}
					}
				}
		}

		if(m_TeamState[Team] < TEAMSTATE_STARTED && !Waiting)
		{
			ChangeTeamState(Team, TEAMSTATE_STARTED);
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(Team == m_Core.Team(i))
				{
					CCharacter* pChar = Character(i);
					if(pChar && pChar->IsAlive())
					{
						pChar->m_DDRaceState = DDRACE_STARTED;
						pChar->m_StartTime = Tick;
						pChar->m_RefreshTime = Tick;
					}
				}
			}
		}
	}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	int Team = m_Core.Team(ClientID);
	if(Team == TEAM_FLOCK || Team == TEAM_SUPER)
	{
		Character(ClientID)->OnFinish();
	}
	else if(Count(Team) == 1)
	{
		m_TeeFinished[ClientID] = true;
		if(TeamFinished(Team))
		{
			ChangeTeamState(Team, TEAMSTATE_OPEN);
			Character(ClientID)->OnFinish();
			m_TeeFinished[ClientID] = false;
		}
	}
	else
	{
		m_TeeFinished[ClientID] = true;
		if(TeamFinished(Team))
		{
			ChangeTeamState(Team, TEAMSTATE_OPEN);
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(Team == m_Core.Team(i))
				{
					CCharacter* pChar = Character(i);
					if(pChar != 0 && pChar->m_DDRaceState == DDRACE_STARTED)
					{
						pChar->OnFinish();
						m_TeeFinished[i] = false;
					}
				}
			}
		}
	}
}

void CGameTeams::OnCharacterDeath(int ClientID)
{
	int OldTeam = m_Core.Team(ClientID);
	// int Team = TEAM_FLOCK;
	m_TeePassedStart[ClientID] = false;
	if(m_TeamStrict[OldTeam] && OldTeam != TEAM_FLOCK && OldTeam != TEAM_SUPER)
	{
		for(int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
		{
			if(OldTeam == m_Core.Team(LoopClientID))
			{
				CCharacter* pChar = Character(LoopClientID);
				if(pChar)
					pChar->Die(ClientID, WEAPON_SELF);
			}
		}
		ChangeTeamState(OldTeam, TEAMSTATE_OPEN);
	}
	// If teams are sticky, stay in the same team
	if((!g_Config.m_SvStickyTeams && OldTeam != TEAM_FLOCK) || OldTeam == TEAM_SUPER)
		SetForceCharacterTeam(ClientID, TEAM_FLOCK);
	// Else check if the team state needs changing
	else
	{
		if(Count(OldTeam) > 1)
		{
			bool Started = false;
			for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			{
				if(m_Core.Team(LoopClientID) == OldTeam)
					if(Character(LoopClientID) && Character(LoopClientID)->m_DDRaceState == DDRACE_STARTED)
						Started = true;
			}
		}
		else if(Count(OldTeam) == 1)
		{
			m_TeamState[OldTeam] = TEAMSTATE_OPEN;
		}
		else
			dbg_msg("Teams","Please report if you saw this!");
	}

	m_TeeFinished[ClientID] = false;
}

int CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	// Check on wrong parameters. +1 for TEAM_SUPER
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || Team < 0 || Team >= MAX_CLIENTS + 1)
		return ERROR_WRONG_PARAMS;
	// You can join to TEAM_SUPER at any time, but any other group you cannot if it started
	if(Team != TEAM_SUPER && m_TeamState[Team] == TEAMSTATE_STARTED)
		return ERROR_CLOSED;
	// No need to switch team if you there
	if(m_Core.Team(ClientID) == Team)
		return ERROR_ALREADY_THERE;
	// You cannot be in TEAM_SUPER if you not super
	if(Team == TEAM_SUPER && !Character(ClientID)->m_Super)
		return ERROR_NOT_SUPER;
	// If you begin race
	if(Character(ClientID)->m_DDRaceState != DDRACE_NONE && Team != TEAM_SUPER)
		return ERROR_STARTED;
	// If he is past the start, don't let him change teams
	if(m_TeePassedStart[ClientID])
		return ERROR_PASSEDSTART;
	SetForceCharacterTeam(ClientID, Team);
	
	return 0;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	char aBuf[64];
	CServer* pServ = (CServer*)Server();
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	m_TeeFinished[ClientID] = false;
	int OldTeam = m_Core.Team(ClientID);
	// If the old team is not Flock nor Super and it's state is not empty
	if(OldTeam != TEAM_FLOCK && OldTeam != TEAM_SUPER && m_TeamState[OldTeam] != TEAMSTATE_EMPTY)
	{
		bool NoOneInOldTeam = true;
		// Make sure it's not empty
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(i != ClientID && OldTeam == m_Core.Team(i))
			{
				NoOneInOldTeam = false;//all good exists someone in old team
				break;
			}
		// If it's empty set it's state to empty
		if(NoOneInOldTeam)
			m_TeamState[OldTeam] = TEAMSTATE_EMPTY;
	}

	m_MembersCount[OldTeam]--;
	m_Core.Team(ClientID, Team);
	
	// If The new team is not Super
	if(Team != TEAM_SUPER)
	{
		// Announce the player joining the team
		str_format(aBuf, sizeof(aBuf), "\'%s\' joined team %d.", pServ->ClientName(pPlayer->GetCID()), Team);
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}

	// If the new team has a leader and it's not team flock nor super, keep record of when the tee joined
	if(GetTeamLeader(Team) != -1 && Team != TEAM_FLOCK && Team != TEAM_SUPER)
		m_TeeJoinTick[ClientID] = Server()->Tick();
	else
		m_TeeJoinTick[ClientID] = 0;

	if(Team != TEAM_SUPER)
		m_MembersCount[Team]++;

	if(Team != TEAM_SUPER && m_TeamState[Team] == TEAMSTATE_EMPTY)
		ChangeTeamState(Team, TEAMSTATE_OPEN);

	// If the player leaving the Old Team is Team Leader
	if(OldTeam != TEAM_FLOCK && OldTeam != TEAM_SUPER && ClientID == GetTeamLeader(OldTeam))
	{
		// And there are more than one in the team
		if(Count(OldTeam) > 1)
		{
			int FirstJoinedID = -1;
			int Tick = Server()->Tick();
			//Check who joined first
			for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
				if(m_Core.Team(LoopClientID) == OldTeam)
					if(m_TeeJoinTick[LoopClientID] < Tick)
					{
						FirstJoinedID = LoopClientID;
						Tick = m_TeeJoinTick[LoopClientID];
					}
			// Make him leader
			if(FirstJoinedID != -1)
				SetTeamLeader(OldTeam, FirstJoinedID);
		}
		// Else if there is 1 in the team
		else if(Count(OldTeam))
		{
			for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
				if(m_Core.Team(LoopClientID) == OldTeam)
				{
					// Make him leader
					SetTeamLeader(OldTeam, LoopClientID);
					break;
				}
		}
		else
		{
			// No team leader atm.
			SetTeamLeader(OldTeam, -1);
			m_TeamStrict[OldTeam] = g_Config.m_SvTeamStrict;
		}
	}

	dbg_msg1("Teams", "Id = %d Team = %d", ClientID, Team);

	// Send the Teams state to guys with DDRace Client
	for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
	{
		if(Character(LoopClientID) && Character(LoopClientID)->GetPlayer()->m_IsUsingDDRaceClient)
			SendTeamsState(LoopClientID);
	}
}

int CGameTeams::Count(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;
	return m_MembersCount[Team];
}

bool CGameTeams::TeamFinished(int Team)
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CCharacter *pChar = (GameServer()->m_apPlayers[i]) ? GameServer()->m_apPlayers[i]->GetCharacter() : 0;
		if(pChar)
			if(m_Core.Team(i) == Team && (!m_TeeFinished[i] && pChar->m_DDRaceState == DDRACE_STARTED))
				return false;
	}
	return true;
}

int CGameTeams::TeamMask(int Team, int ExceptID)
{
	if(Team == TEAM_SUPER) return -1;
	int Mask = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(i != ExceptID)
			if((Character(i) && (m_Core.Team(i) == Team || m_Core.Team(i) == TEAM_SUPER))
				|| (GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == -1))
				Mask |= 1 << i;
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	CNetMsg_Cl_TeamsState Msg;
	Msg.m_Tee0 = m_Core.Team(0);
	Msg.m_Tee1 = m_Core.Team(1);
	Msg.m_Tee2 = m_Core.Team(2);
	Msg.m_Tee3 = m_Core.Team(3);
	Msg.m_Tee4 = m_Core.Team(4);
	Msg.m_Tee5 = m_Core.Team(5);
	Msg.m_Tee6 = m_Core.Team(6);
	Msg.m_Tee7 = m_Core.Team(7);
	Msg.m_Tee8 = m_Core.Team(8);
	Msg.m_Tee9 = m_Core.Team(9);
	Msg.m_Tee10 = m_Core.Team(10);
	Msg.m_Tee11 = m_Core.Team(11);
	Msg.m_Tee12 = m_Core.Team(12);
	Msg.m_Tee13 = m_Core.Team(13);
	Msg.m_Tee14 = m_Core.Team(14);
	Msg.m_Tee15 = m_Core.Team(15);
	
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	
}

void CGameTeams::SetTeamLeader(int Team, int ClientID)
{
	if(Team == TEAM_FLOCK || Team == TEAM_SUPER)
		return;
	m_TeamLeader[Team] = ClientID;
	if(ClientID == -1)
		return;
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "\'%s\' is now the team %d leader.", Server()->ClientName(ClientID), Team);
	if(Count(Team) > 1)
		for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(LoopClientID != ClientID)
				if(m_Core.Team(LoopClientID) == Team)
					GameServer()->SendChatTarget(LoopClientID, aBuf);

	str_format(aBuf, sizeof(aBuf), "You are now the team %d leader.", Team);
	GameServer()->SendChatTarget(ClientID, aBuf);
}

void CGameTeams::ToggleStrictness(int Team)
{
	m_TeamStrict[Team] = !m_TeamStrict[Team];
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "\'%s\' Toggled Team Strictness to %d.", Server()->ClientName(m_TeamLeader[Team]), m_TeamStrict[Team]);
	if(Count(Team) > 1)
	{
		for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
			if(m_Core.Team(LoopClientID) == Team)
				GameServer()->SendChatTarget(LoopClientID, aBuf);
	}
	else
		GameServer()->SendChatTarget(m_TeamLeader[Team], aBuf);
}
