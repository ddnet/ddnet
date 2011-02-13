#ifndef GAME_TEAMSCORE_H
#define GAME_TEAMSCORE_H

#include <engine/shared/protocol.h>

enum {
	TEAM_FLOCK = 0,
	TEAM_SUPER = 16
};

class CTeamsCore {
	
	int m_Team[MAX_CLIENTS];
public:
	
	CTeamsCore(void);
	
	bool SameTeam(int ClientID1, int ClientID2);

	bool CanCollide(int ClientID1, int ClientID2);
	
	int Team(int ClientID);
	void Team(int ClientID, int Team);

	void Reset();
};

#endif
