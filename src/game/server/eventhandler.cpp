/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "eventhandler.h"

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = 0;
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, int64 Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return 0;
	if(m_CurrentOffset + Size >= MAX_DATASIZE)
		return 0;

	void *p = &m_aData[m_CurrentOffset];
	m_aOffsets[m_NumEvents] = m_CurrentOffset;
	m_aTypes[m_NumEvents] = Type;
	m_aSizes[m_NumEvents] = Size;
	m_aClientMasks[m_NumEvents] = Mask;
	m_CurrentOffset += Size;
	m_NumEvents++;
	return p;
}

void CEventHandler::Clear()
{
	m_NumEvents = 0;
	m_CurrentOffset = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == -1 || CmaskIsSet(m_aClientMasks[i], SnappingClient))
		{
			CNetEvent_Common *ev = (CNetEvent_Common *)&m_aData[m_aOffsets[i]];
			if(!NetworkClipped(GameServer(), SnappingClient, vec2(ev->m_X, ev->m_Y)))
			{
				int Type = m_aTypes[i];
				int Size = m_aSizes[i];
				const char *Data = &m_aData[m_aOffsets[i]];
				if(GameServer()->Server()->IsSixup(SnappingClient))
					EventToSixup(&Type, &Size, &Data);

				void *d = GameServer()->Server()->SnapNewItem(Type, i, Size);
				if(d)
					mem_copy(d, Data, Size);
			}
		}
	}
}

void CEventHandler::EventToSixup(int *Type, int *Size, const char **pData)
{
	static char s_aEventStore[128];
	if(*Type == NETEVENTTYPE_DAMAGEIND)
	{
		const CNetEvent_DamageInd *pEvent = (const CNetEvent_DamageInd *)(*pData);
		protocol7::CNetEvent_Damage *pEvent7 = (protocol7::CNetEvent_Damage *)s_aEventStore;
		*Type = -protocol7::NETEVENTTYPE_DAMAGE;
		*Size = sizeof(*pEvent7);

		pEvent7->m_X = pEvent->m_X;
		pEvent7->m_Y = pEvent->m_Y;

		// This will need some work, perhaps an event wrapper for damageind,
		// a scan of the event array to merge multiple damageinds
		// or a separate array of "damage ind" events that's added in while snapping
		pEvent7->m_HealthAmount = 1;

		*pData = s_aEventStore;
	}
	else if(*Type == NETEVENTTYPE_SOUNDGLOBAL) // No more global sounds for the server
	{
		const CNetEvent_SoundGlobal *pEvent = (const CNetEvent_SoundGlobal *)(*pData);
		protocol7::CNetEvent_SoundWorld *pEvent7 = (protocol7::CNetEvent_SoundWorld *)s_aEventStore;

		*Type = -protocol7::NETEVENTTYPE_SOUNDWORLD;
		*Size = sizeof(*pEvent7);

		pEvent7->m_SoundID = pEvent->m_SoundID;
		pEvent7->m_X = pEvent->m_X;
		pEvent7->m_Y = pEvent->m_Y;

		*pData = s_aEventStore;
	}
}
