/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_EVENTHANDLER_H
#define GAME_SERVER_EVENTHANDLER_H

#include <stdint.h>

class CEventHandler
{
	enum
	{
		MAX_EVENTS = 128,
		MAX_DATASIZE = 128 * 64,
	};

	int m_aTypes[MAX_EVENTS] = {0}; // TODO: remove some of these arrays
	int m_aOffsets[MAX_EVENTS] = {0};
	int m_aSizes[MAX_EVENTS] = {0};
	int64_t m_aClientMasks[MAX_EVENTS] = {0};
	char m_aData[MAX_DATASIZE] = {0};

	class CGameContext *m_pGameServer = nullptr;

	int m_CurrentOffset = 0;
	int m_NumEvents = 0;

public:
	CGameContext *GameServer() const { return m_pGameServer; }
	void SetGameServer(CGameContext *pGameServer);

	CEventHandler();
	void *Create(int Type, int Size, int64_t Mask = -1LL);
	void Clear();
	void Snap(int SnappingClient);

	void EventToSixup(int *pType, int *pSize, const char **ppData);
};

#endif
