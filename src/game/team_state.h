#ifndef GAME_TEAM_STATE_H
#define GAME_TEAM_STATE_H

enum class ETeamState
{
	EMPTY,
	OPEN,
	STARTED,
	// Happens when a tee that hasn't hit the start tiles leaves
	// the team.
	STARTED_UNFINISHABLE,
	FINISHED
};

#endif // GAME_TEAM_STATE_H
