#include "teamscore.h"

CTeamsCore::CTeamsCore()
{
	Reset();
}

bool CTeamsCore::SameTeam(int ClientID1, int ClientID2)
{
	return m_Team[ClientID1] == m_Team[ClientID2];
}
	
int CTeamsCore::Team(int ClientID)
{
	return m_Team[ClientID];
}

void CTeamsCore::Team(int ClientID, int Team)
{
	m_Team[ClientID] = Team;
}

bool CTeamsCore::CanCollide(int ClientID1, int ClientID2)
{
	if(m_Team[ClientID1] == TEAM_SUPER || m_Team[ClientID2] == TEAM_SUPER)
		return true;
	return m_Team[ClientID1] == m_Team[ClientID2];
}

void CTeamsCore::Reset()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		m_Team[i] = TEAM_FLOCK;
}
