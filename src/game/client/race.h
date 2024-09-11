#ifndef GAME_CLIENT_RACE_H
#define GAME_CLIENT_RACE_H

#include <base/vmath.h>

class CGameClient;

class CRaceHelper
{
	const CGameClient *m_pGameClient;

	int m_aFlagIndex[2] = {-1, -1};

public:
	void Init(const CGameClient *pGameClient);

	// these functions return the time in milliseconds, time -1 is invalid
	static int TimeFromSecondsStr(const char *pStr); // x.xxx
	static int TimeFromStr(const char *pStr); // x minute(s) x.xxx second(s)
	static int TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize); // xxx finished in: x minute(s) x.xxx second(s)

	bool IsStart(vec2 Prev, vec2 Pos) const;
};

#endif // GAME_CLIENT_RACE_H
