#include "race.h"

int CRaceHelper::TimeFromSecondsStr(const char *pStr) // x.xxx
{
	int Time = str_toint(pStr) * 1000;
	while(*pStr >= '0' && *pStr <= '9')
		pStr++;
	if(*pStr == '.' || *pStr == ',')
	{
		pStr++;
		static const int s_aMult[3] = { 100, 10, 1 };
		for(int i = 0; pStr[i] >= '0' && pStr[i] <= '9' && i < 3; i++)
			Time += (pStr[i] - '0') * s_aMult[i];
	}
	return Time;
}

int CRaceHelper::TimeFromStr(const char *pStr) // x minute(s) x.xxx second(s)
{
	static const char * const s_pMinutesStr = " minute(s) ";
	static const char * const s_pSecondsStr = " second(s)";

	const char *pSeconds = str_find(pStr, s_pSecondsStr);
	if(!pSeconds)
		return 0;

	const char *pMinutes = str_find(pStr, s_pMinutesStr);
	if(pMinutes)
		return str_toint(pStr) * 60 * 1000 + TimeFromSecondsStr(pMinutes + str_length(s_pMinutesStr));
	else
		return TimeFromSecondsStr(pStr);
}

int CRaceHelper::TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize) // xxx finished in: x minute(s) x.xxx second(s)
{
	static const char * const s_pFinishedStr = " finished in: ";
	const char *pFinished = str_find(pStr, s_pFinishedStr);
	if(!pFinished)
		return 0;

	int FinishedPos = pFinished - pStr;
	if(FinishedPos == 0 || FinishedPos >= NameBufSize)
		return 0;

	str_copy(pNameBuf, pStr, FinishedPos + 1);

	return TimeFromStr(pFinished + str_length(s_pFinishedStr));
}
