/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
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

bool CTeamsCore::CanKeepHook(int ClientID1, int ClientID2)
{
	if (m_Team[ClientID1] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || m_Team[ClientID2] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER)
			|| ClientID1 == ClientID2)
		return true;
	return m_Team[ClientID1] == m_Team[ClientID2];
}

bool CTeamsCore::CanCollide(int ClientID1, int ClientID2)
{
	if (m_Team[ClientID1] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || m_Team[ClientID2] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER)
			|| ClientID1 == ClientID2)
		return true;
	if (m_IsSolo[ClientID1] || m_IsSolo[ClientID2])
		return false;
	return m_Team[ClientID1] == m_Team[ClientID2];
}

void CTeamsCore::Reset()
{
	m_IsDDRace16 = false;

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_Team[i] = TEAM_FLOCK;
		m_IsSolo[i] = false;
	}
}
