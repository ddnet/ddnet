#ifndef GAME_CLIENT_COMPONENTS_RACE_H
#define GAME_CLIENT_COMPONENTS_RACE_H

#include <base/system.h>

class CRaceHelper
{
public:
	// these functions return the time in milliseconds, time 0 is invalid

	static int TimeFromSecondsStr(const char *pStr) // x.xxx
	{
		int Time = str_toint(pStr) * 1000;
		while(*pStr >= '0' && *pStr <= '9') pStr++;
		if(*pStr == '.' || *pStr == ',')
		{
			pStr++;
			const int Mult[3] = { 100, 10, 1 };
			for(int i = 0; pStr[i] >= '0' && pStr[i] <= '9' && i < 3; i++)
				Time += (pStr[i] - '0') * Mult[i];
		}
		return Time;
	}

	static int TimeFromStr(const char *pStr) // x minute(s) x.xx second(s)
	{
		static const char *pMinutesStr = " minute(s) ";
		static const char *pSecondsStr = " second(s)";

		const char *pSeconds = str_find(pStr, pSecondsStr);
		if(!pSeconds)
			return 0;

		const char *pMinutes = str_find(pStr, pMinutesStr);
		if(pMinutes)
			return str_toint(pStr) * 60 * 1000 + TimeFromSecondsStr(pMinutes + str_length(pMinutesStr));
		else
			return TimeFromSecondsStr(pStr);
	}

	static int TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize) // xxx finished in: x minute(s) x.xx second(s)
	{
		static const char *pFinishedStr = " finished in: ";
		const char *pFinished = str_find(pStr, pFinishedStr);
		int FinishedPos = pFinished - pStr;
		if(!pFinished || FinishedPos == 0 || FinishedPos >= NameBufSize)
			return 0;

		str_copy(pNameBuf, pStr, FinishedPos + 1);

		return TimeFromStr(pFinished + str_length(pFinishedStr));
	}
};

#endif
