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
	bool IsFinish(vec2 Pos1, vec2 Pos2) const;
	/*
		IsNearFinish

		Checks every tiles on the border of the square
		The square is located at Pos and has the size RadiusInTiles

		It does not cover tiles inside of the square. So making the radius too big
		might miss the target.

		<----- radius ---->

		+-----------------+
		|                 |
		|                 |
		|                 |
		|                 |
		|       pos       |
		|                 |
		|                 |
		|                 |
		+-----------------+
	*/
	bool IsNearFinish(vec2 Pos, int RadiusInTiles = 4) const;
	/*
		IsNearStart

		Checks every tiles on the border of the square
		The square is located at Pos and has the size RadiusInTiles

		It does not cover tiles inside of the square. So making the radius too big
		might miss the target.

		<----- radius ---->

		+-----------------+
		|                 |
		|                 |
		|                 |
		|                 |
		|       pos       |
		|                 |
		|                 |
		|                 |
		+-----------------+
	*/
	bool IsNearStart(vec2 Pos, int RadiusInTiles = 4) const;
	/*
		IsClusterRangeFinish

		Recommended for bigger areas.
		For areas smaller than 10 tiles use IsNearFinish instead.

		Calls IsNearFinish internally.
		It creates multiple 4x4 check boxes to cover the full given are.
		Around position Pos with the size of RadiusInTiles.
		Only indecies on the border are looked at.

		<----- radius ---->

		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
	*/
	bool IsClusterRangeFinish(vec2 Pos, int RadiusInTiles = 10) const;
	/*
		IsClusterRangeStart

		Recommended for bigger areas.
		For areas smaller than 10 tiles use IsNearStart instead.

		Calls IsNearStart internally.
		It creates multiple 4x4 check boxes to cover the full given are.
		Around position Pos with the size of RadiusInTiles.
		Only indecies on the border are looked at.

		<----- radius ---->

		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
		+--+ +--+ +--+ +--+
		|  | |  | |  | |  |
		+--+ +--+ +--+ +--+
	*/
	bool IsClusterRangeStart(vec2 Pos, int RadiusInTiles = 10) const;
};

#endif // GAME_CLIENT_RACE_H