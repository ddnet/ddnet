#ifndef GAME_CLIENT_COMPONENTS_RACE_H
#define GAME_CLIENT_COMPONENTS_RACE_H

#include <base/system.h>
#include <base/vmath.h>

class CRaceHelper
{
public:
	// these functions return the time in milliseconds, time -1 is invalid
	static int TimeFromSecondsStr(const char *pStr); // x.xxx
	static int TimeFromStr(const char *pStr); // x minute(s) x.xxx second(s)
	static int TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize); // xxx finished in: x minute(s) x.xxx second(s)

	static bool IsStart(class CCollision *pCollision, vec2 Prev, vec2 Pos);
};

#endif
