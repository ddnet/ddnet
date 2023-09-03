/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_TEAMSCORE_H
#define GAME_TEAMSCORE_H

#include <engine/shared/protocol.h>

enum
{
	TEAM_FLOCK = 0,
	TEAM_SUPER = MAX_CLIENTS,
	NUM_TEAMS = TEAM_SUPER + 1,
	VANILLA_TEAM_SUPER = VANILLA_MAX_CLIENTS
};

// do not change the values of the following enum
enum
{
	SV_TEAM_FORBIDDEN = 0, // teams are disabled on the map
	SV_TEAM_ALLOWED = 1, // teams are enabled on the map, but optional
	SV_TEAM_MANDATORY = 2, // map must be played with a team
	SV_TEAM_FORCED_SOLO = 3 // map forces a random team for each individual player
};

class CTeamsCore
{
	int m_aTeam[MAX_CLIENTS];
	bool m_aIsSolo[MAX_CLIENTS];

public:
	bool m_IsDDRace16;

	CTeamsCore();

	bool SameTeam(int ClientID1, int ClientID2) const;

	bool CanKeepHook(int ClientID1, int ClientID2) const;
	bool CanCollide(int ClientID1, int ClientID2) const;

	int Team(int ClientID) const;
	void Team(int ClientID, int Team);

	void Reset();
	void SetSolo(int ClientID, bool Value)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid client id");
		m_aIsSolo[ClientID] = Value;
	}

	bool GetSolo(int ClientID) const
	{
		if(ClientID < 0 || ClientID >= MAX_CLIENTS)
			return false;
		return m_aIsSolo[ClientID];
	}
};

#endif
