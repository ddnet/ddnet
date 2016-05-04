#ifndef ENGINE_SERVER_SQL_STRING_HELPERS_H
#define ENGINE_SERVER_SQL_STRING_HELPERS_H

#include <cmath>
#include <cstring>
#include <ctime>
#include <base/system.h>

void FuzzyString(char *pString)
{
	char newString[32*4-1];
	int pos = 0;

	for(int i=0;i<64;i++)
	{
		if(!pString[i])
			break;

		newString[pos++] = pString[i];
		if (pString[i] != '\\' && str_utf8_isstart(pString[i+1]))
			newString[pos++] = '%';
	}

	newString[pos] = '\0';
	strcpy(pString, newString);
}

// anti SQL injection
void ClearString(char *pString, int size = 32)
{
	char *newString = (char *)malloc(size * 2 - 1);
	int pos = 0;

	for(int i=0;i<size;i++)
	{
		if(pString[i] == '\\')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\\';
		}
		else if(pString[i] == '\'')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\'';
		}
		else if(pString[i] == '"')
		{
			newString[pos++] = '\\';
			newString[pos++] = '"';
		}
		else
		{
			newString[pos++] = pString[i];
		}
	}

	newString[pos] = '\0';

	strcpy(pString,newString);
	free(newString);
}

void agoTimeToString(int agoTime, char agoString[])
{
	char aBuf[20];
	int times[7] =
	{
			60 * 60 * 24 * 365 ,
			60 * 60 * 24 * 30 ,
			60 * 60 * 24 * 7,
			60 * 60 * 24 ,
			60 * 60 ,
			60 ,
			1
	};
	char names[7][6] =
	{
			"year",
			"month",
			"week",
			"day",
			"hour",
			"min",
			"sec"
	};

	int seconds = 0;
	char name[6];
	int count = 0;
	int i = 0;

	// finding biggest match
	for(i = 0; i<7; i++)
	{
		seconds = times[i];
		strcpy(name,names[i]);

		count = floor((float)agoTime/(float)seconds);
		if(count != 0)
		{
			break;
		}
	}

	if(count == 1)
	{
		str_format(aBuf, sizeof(aBuf), "%d %s", 1 , name);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%d %ss", count , name);
	}
	strcat(agoString,aBuf);

	if (i + 1 < 7)
	{
		// getting second piece now
		int seconds2 = times[i+1];
		char name2[6];
		strcpy(name2,names[i+1]);

		// add second piece if it's greater than 0
		int count2 = floor((float)(agoTime - (seconds * count)) / (float)seconds2);

		if (count2 != 0)
		{
			if(count2 == 1)
			{
				str_format(aBuf, sizeof(aBuf), " and %d %s", 1 , name2);
			}
			else
			{
				str_format(aBuf, sizeof(aBuf), " and %d %ss", count2 , name2);
			}
			strcat(agoString,aBuf);
		}
	}
}

void getTimeStamp(char* dest, size_t size)
{
	std::time_t rawtime;
	std::time(&rawtime);

	str_timestamp_ex(rawtime, dest, size, "%Y-%m-%d %H:%M:%S");
}

#endif
