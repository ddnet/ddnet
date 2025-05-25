#ifndef GAME_RACE_STATE_H
#define GAME_RACE_STATE_H

enum class ERaceState
{
	NONE,
	STARTED,
	CHEATED, // no time and won't start again unless ordered by a mod or death
	FINISHED,
};

#endif // GAME_RACE_STATE_H
