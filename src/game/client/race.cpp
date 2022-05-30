#include <cctype>
#include <list>

#include <game/client/gameclient.h>
#include <game/mapitems.h>

#include "race.h"

int CRaceHelper::ms_aFlagIndex[2] = {-1, -1};

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
		static const int s_aMult[3] = {100, 10, 1};
		for(int i = 0; isdigit(pStr[i]) && i < 3; i++)
			Time += (pStr[i] - '0') * s_aMult[i];
	}
	return Time;
}

int CRaceHelper::TimeFromStr(const char *pStr)
{
	static const char *const s_pMinutesStr = " minute(s) ";
	static const char *const s_pSecondsStr = " second(s)";

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
	static const char *const s_pFinishedStr = " finished in: ";
	const char *pFinished = str_find(pStr, s_pFinishedStr);
	if(!pFinished)
		return -1;

	int FinishedPos = pFinished - pStr;
	if(FinishedPos == 0 || FinishedPos >= NameBufSize)
		return -1;

	str_copy(pNameBuf, pStr, FinishedPos + 1);

	return TimeFromStr(pFinished + str_length(s_pFinishedStr));
}

bool CRaceHelper::IsStart(CGameClient *pClient, vec2 Prev, vec2 Pos)
{
	CCollision *pCollision = pClient->Collision();
	if(pClient->m_GameInfo.m_FlagStartsRace)
	{
		int EnemyTeam = pClient->m_aClients[pClient->m_Snap.m_LocalClientID].m_Team ^ 1;
		return ms_aFlagIndex[EnemyTeam] != -1 && distance(Pos, pCollision->GetPos(ms_aFlagIndex[EnemyTeam])) < 32;
	}
	else
	{
		std::list<int> Indices = pCollision->GetMapIndices(Prev, Pos);
		if(!Indices.empty())
			for(int &Indice : Indices)
			{
				if(pCollision->GetTileIndex(Indice) == TILE_START)
					return true;
				if(pCollision->GetFTileIndex(Indice) == TILE_START)
					return true;
			}
		else
		{
			if(pCollision->GetTileIndex(pCollision->GetPureMapIndex(Pos)) == TILE_START)
				return true;
			if(pCollision->GetFTileIndex(pCollision->GetPureMapIndex(Pos)) == TILE_START)
				return true;
		}
	}
	return false;
}

bool CRaceHelper::IsFinish(CGameClient *pClient, vec2 Pos1, vec2 Pos2)
{
	CCollision *pCollision = pClient->Collision();
	std::list<int> Indices = pCollision->GetMapIndices(Pos2, Pos1);
	if(!Indices.empty())
		for(int &Indice : Indices)
		{
			if(pCollision->GetTileIndex(Indice) == TILE_FINISH)
				return true;
			if(pCollision->GetFTileIndex(Indice) == TILE_FINISH)
				return true;
		}
	else
	{
		if(pCollision->GetTileIndex(pCollision->GetPureMapIndex(Pos1)) == TILE_FINISH)
			return true;
		if(pCollision->GetFTileIndex(pCollision->GetPureMapIndex(Pos1)) == TILE_FINISH)
			return true;
	}
	return false;
}

bool CRaceHelper::IsNearFinish(CGameClient *pClient, vec2 Pos)
{
	CCollision *pCollision = pClient->Collision();
	float d = 4 * 32.0f;
	vec2 TL = Pos;
	vec2 TR = Pos;
	vec2 BL = Pos;
	vec2 BR = Pos;
	// top left
	TL.x = clamp(TL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TL.y = clamp(TL.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// top right
	TR.x = clamp(TR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TR.y = clamp(TR.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom left
	BL.x = clamp(BL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BL.y = clamp(BL.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom right
	BR.x = clamp(BR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BR.y = clamp(BR.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	if(IsFinish(pClient, TL, TR))
		return true;
	if(IsFinish(pClient, BL, BR))
		return true;
	if(IsFinish(pClient, TL, BL))
		return true;
	if(IsFinish(pClient, TR, BR))
		return true;
	return false;
}