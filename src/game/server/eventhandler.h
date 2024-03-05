/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_EVENTHANDLER_H
#define GAME_SERVER_EVENTHANDLER_H

#include <cstdint>

#include <engine/shared/protocol.h>

class CEventHandler
{
	enum
	{
		MAX_EVENTS = 128,
		MAX_DATASIZE = 128 * 64,
	};

	int m_aTypes[MAX_EVENTS]; // TODO: remove some of these arrays
	int m_aOffsets[MAX_EVENTS];
	int m_aSizes[MAX_EVENTS];
	CClientMask m_aClientMasks[MAX_EVENTS];
	char m_aData[MAX_DATASIZE];

	class CGameContext *m_pGameServer;

	int m_CurrentOffset;
	int m_NumEvents;

public:
	CGameContext *GameServer() const { return m_pGameServer; }
	void SetGameServer(CGameContext *pGameServer);

	CEventHandler();
	void *Create(int Type, int Size, CClientMask Mask = CClientMask().set());

	template<typename T>
	T *Create(CClientMask Mask = CClientMask().set())
	{
		return static_cast<T *>(Create(T::ms_MsgId, sizeof(T), Mask));
	}

	void Clear();
	void Snap(int SnappingClient);

	void EventToSixup(int *pType, int *pSize, const char **ppData);
};

#endif
