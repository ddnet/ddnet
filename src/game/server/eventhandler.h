/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_EVENTHANDLER_H
#define GAME_SERVER_EVENTHANDLER_H

#include <engine/shared/protocol.h>

#include <cstdint>

class CEventHandler
{
public:
	// Required for lowbandwidth players, so that they dont skip events
	static const int NUM_BUFFERS = 2;
	int CurrentBuffer() const { return m_CurrentBuffer; }

private:
	enum
	{
		MAX_EVENTS = 128,
		MAX_DATASIZE = 128 * 64,
	};

	int m_CurrentBuffer;

	int m_aTypes[NUM_BUFFERS][MAX_EVENTS]; // TODO: remove some of these arrays
	int m_aOffsets[NUM_BUFFERS][MAX_EVENTS];
	int m_aSizes[NUM_BUFFERS][MAX_EVENTS];
	CClientMask m_aClientMasks[NUM_BUFFERS][MAX_EVENTS];
	char m_aData[NUM_BUFFERS][MAX_DATASIZE];

	class CGameContext *m_pGameServer;

	int m_CurrentOffset[NUM_BUFFERS];
	int m_NumEvents[NUM_BUFFERS];

	void SnapBuffer(int SnappingClient, int Buf);

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
