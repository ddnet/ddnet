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
	int TimeFromSecondsStr(const char *pStr) const; // x.xxx
	int TimeFromStr(const char *pStr) const; // x minute(s) x.xxx second(s)
	int TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize) const; // xxx finished in: x minute(s) x.xxx second(s)

	bool IsStart(vec2 Prev, vec2 Pos) const;
};

#endif // GAME_CLIENT_RACE_H
