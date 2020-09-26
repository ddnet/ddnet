#include "sql_string_helpers.h"

#include <base/system.h>
#include <cmath>
#include <cstring>

void sqlstr::FuzzyString(char *pString, int size)
{
	char *newString = new char[size * 4 - 1];
	int pos = 0;

	for(int i = 0; i < size; i++)
	{
		if(!pString[i])
			break;

		newString[pos++] = pString[i];
		if(pString[i] != '\\' && str_utf8_isstart(pString[i + 1]))
			newString[pos++] = '%';
	}

	newString[pos] = '\0';
	str_copy(pString, newString, size);
	delete[] newString;
}

int sqlstr::EscapeLike(char *pDst, const char *pSrc, int DstSize)
{
	int Pos = 0;
	int DstPos = 0;
	while(DstPos + 2 < DstSize)
	{
		if(pSrc[Pos] == '\0')
			break;
		if(pSrc[Pos] == '\\' || pSrc[Pos] == '%' || pSrc[Pos] == '_' || pSrc[Pos] == '[')
			pDst[DstPos++] = '\\';
		pDst[DstPos++] = pSrc[Pos++];
	}
	pDst[DstPos++] = '\0';
	return DstPos;
}

void sqlstr::AgoTimeToString(int AgoTime, char *pAgoString)
{
	char aBuf[20];
	int aTimes[7] =
		{
			60 * 60 * 24 * 365,
			60 * 60 * 24 * 30,
			60 * 60 * 24 * 7,
			60 * 60 * 24,
			60 * 60,
			60,
			1};
	char aaNames[7][6] =
		{
			"year",
			"month",
			"week",
			"day",
			"hour",
			"min",
			"sec"};

	int Seconds = 0;
	char aName[6];
	int Count = 0;
	int i = 0;

	// finding biggest match
	for(i = 0; i < 7; i++)
	{
		Seconds = aTimes[i];
		strcpy(aName, aaNames[i]);

		Count = floor((float)AgoTime / (float)Seconds);
		if(Count != 0)
		{
			break;
		}
	}

	if(Count == 1)
	{
		str_format(aBuf, sizeof(aBuf), "%d %s", 1, aName);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%d %ss", Count, aName);
	}
	strcat(pAgoString, aBuf);

	if(i + 1 < 7)
	{
		// getting second piece now
		int Seconds2 = aTimes[i + 1];
		char aName2[6];
		strcpy(aName2, aaNames[i + 1]);

		// add second piece if it's greater than 0
		int Count2 = floor((float)(AgoTime - (Seconds * Count)) / (float)Seconds2);

		if(Count2 != 0)
		{
			if(Count2 == 1)
			{
				str_format(aBuf, sizeof(aBuf), " and %d %s", 1, aName2);
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), " and %d %ss", Count2, aName2);
			}
			strcat(pAgoString, aBuf);
		}
	}
}
