/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teamscore.h"
#include <engine/shared/config.h>

CTeamsCore::CTeamsCore()
{
	Reset();
}

bool CTeamsCore::SameTeam(int ClientID1, int ClientID2) const
{
	return m_aTeam[ClientID1] == TEAM_SUPER || m_aTeam[ClientID2] == TEAM_SUPER || m_aTeam[ClientID1] == m_aTeam[ClientID2];
}

int CTeamsCore::Team(int ClientID) const
{
	return m_aTeam[ClientID];
}

void CTeamsCore::Team(int ClientID, int Team)
{
	dbg_assert(Team >= TEAM_FLOCK && Team <= TEAM_SUPER, "invalid team");
	m_aTeam[ClientID] = Team;
}

bool CTeamsCore::CanKeepHook(int ClientID1, int ClientID2) const
{
	if(m_aTeam[ClientID1] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || m_aTeam[ClientID2] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || ClientID1 == ClientID2)
		return true;
	return m_aTeam[ClientID1] == m_aTeam[ClientID2];
}

bool CTeamsCore::CanCollide(int ClientID1, int ClientID2) const
{
	if(m_aTeam[ClientID1] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || m_aTeam[ClientID2] == (m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER) || ClientID1 == ClientID2)
		return true;
	if(m_aIsSolo[ClientID1] || m_aIsSolo[ClientID2])
		return false;
	return m_aTeam[ClientID1] == m_aTeam[ClientID2];
}

void CTeamsCore::Reset()
{
	m_IsDDRace16 = false;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(g_Config.m_SvTeam == SV_TEAM_FORCED_SOLO)
			m_aTeam[i] = i;
		else
			m_aTeam[i] = TEAM_FLOCK;
		m_aIsSolo[i] = false;
	}
}
