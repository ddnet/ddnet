/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "snap_id_pool.h"

#include <base/system.h>

CSnapIdPool::CSnapIdPool()
{
	Reset();
}

void CSnapIdPool::Reset()
{
	for(int i = 0; i < MAX_IDS; i++)
	{
		m_aIds[i].m_Next = i + 1;
		m_aIds[i].m_State = ID_FREE;
	}

	m_aIds[MAX_IDS - 1].m_Next = -1;
	m_FirstFree = 0;
	m_FirstTimed = -1;
	m_LastTimed = -1;
	m_Usage = 0;
	m_InUsage = 0;
}

void CSnapIdPool::RemoveFirstTimeout()
{
	int NextTimed = m_aIds[m_FirstTimed].m_Next;

	// add it to the free list
	m_aIds[m_FirstTimed].m_Next = m_FirstFree;
	m_aIds[m_FirstTimed].m_State = ID_FREE;
	m_FirstFree = m_FirstTimed;

	// remove it from the timed list
	m_FirstTimed = NextTimed;
	if(m_FirstTimed == -1)
		m_LastTimed = -1;

	m_Usage--;
}

int CSnapIdPool::NewId()
{
	int64_t Now = time_get();

	// process timed ids
	while(m_FirstTimed != -1 && m_aIds[m_FirstTimed].m_Timeout < Now)
		RemoveFirstTimeout();

	int Id = m_FirstFree;
	if(Id == -1)
	{
		dbg_msg("server", "invalid id");
		return Id;
	}
	m_FirstFree = m_aIds[m_FirstFree].m_Next;
	m_aIds[Id].m_State = ID_ALLOCATED;
	m_Usage++;
	m_InUsage++;
	return Id;
}

void CSnapIdPool::TimeoutIds()
{
	// process timed ids
	while(m_FirstTimed != -1)
		RemoveFirstTimeout();
}

void CSnapIdPool::FreeId(int Id)
{
	if(Id < 0)
		return;
	dbg_assert((size_t)Id < std::size(m_aIds), "id is out of range");
	dbg_assert(m_aIds[Id].m_State == ID_ALLOCATED, "id is not allocated");

	m_InUsage--;
	m_aIds[Id].m_State = ID_TIMED;
	m_aIds[Id].m_Timeout = time_get() + time_freq() * 5;
	m_aIds[Id].m_Next = -1;

	if(m_LastTimed != -1)
	{
		m_aIds[m_LastTimed].m_Next = Id;
		m_LastTimed = Id;
	}
	else
	{
		m_FirstTimed = Id;
		m_LastTimed = Id;
	}
}
