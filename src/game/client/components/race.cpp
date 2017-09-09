#include <ctype.h>
#include <list>

#include <base/math.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include "race.h"

int CRaceHelper::TimeFromSecondsStr(const char *pStr)
{
	while(*pStr == ' ') // skip leading spaces
		pStr++;
	if(!isdigit(*pStr))
		return -1;
	int Time = str_toint(pStr) * 1000;
	while(isdigit(*pStr))
		pStr++;
	if(*pStr == '.' || *pStr == ',')
	{
		pStr++;
		static const int s_aMult[3] = { 100, 10, 1 };
		for(int i = 0; isdigit(pStr[i]) && i < 3; i++)
			Time += (pStr[i] - '0') * s_aMult[i];
	}
	return Time;
}

int CRaceHelper::TimeFromStr(const char *pStr)
{
	static const char * const s_pMinutesStr = " minute(s) ";
	static const char * const s_pSecondsStr = " second(s)";

	const char *pSeconds = str_find(pStr, s_pSecondsStr);
	if(!pSeconds)
		return -1;

	const char *pMinutes = str_find(pStr, s_pMinutesStr);
	if(pMinutes)
	{
		while(*pStr == ' ') // skip leading spaces
			pStr++;
		int SecondsTime = TimeFromSecondsStr(pMinutes + str_length(s_pMinutesStr));
		if(SecondsTime == -1 || !isdigit(*pStr))
			return -1;
		return str_toint(pStr) * 60 * 1000 + SecondsTime;
	}
	else
		return TimeFromSecondsStr(pStr);
}

int CRaceHelper::TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize)
{
	static const char * const s_pFinishedStr = " finished in: ";
	const char *pFinished = str_find(pStr, s_pFinishedStr);
	if(!pFinished)
		return -1;

	int FinishedPos = pFinished - pStr;
	if(FinishedPos == 0 || FinishedPos >= NameBufSize)
		return -1;

	str_copy(pNameBuf, pStr, FinishedPos + 1);

	return TimeFromStr(pFinished + str_length(s_pFinishedStr));
}

bool CRaceHelper::IsStart(CCollision *pCollision, vec2 Prev, vec2 Pos)
{
	std::list < int > Indices = pCollision->GetMapIndices(Prev, Pos);
	if(!Indices.empty())
		for(std::list < int >::iterator i = Indices.begin(); i != Indices.end(); i++)
		{
			if(pCollision->GetTileIndex(*i) == TILE_BEGIN)
				return true;
			if(pCollision->GetFTileIndex(*i) == TILE_BEGIN)
				return true;
		}
	else
	{
		if(pCollision->GetTileIndex(pCollision->GetPureMapIndex(Pos)) == TILE_BEGIN)
			return true;
		if(pCollision->GetFTileIndex(pCollision->GetPureMapIndex(Pos)) == TILE_BEGIN)
			return true;
	}
	return false;
}
