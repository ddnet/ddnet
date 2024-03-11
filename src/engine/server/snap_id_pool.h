/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#ifndef ENGINE_SERVER_SNAP_ID_POOL_H
#define ENGINE_SERVER_SNAP_ID_POOL_H

class CSnapIdPool
{
	enum
	{
		MAX_IDS = 32 * 1024,
	};

	// State of a Snap ID
	enum
	{
		ID_FREE = 0,
		ID_ALLOCATED = 1,
		ID_TIMED = 2,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIds[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:
	CSnapIdPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewId();
	void TimeoutIds();
	void FreeId(int Id);
};

#endif
