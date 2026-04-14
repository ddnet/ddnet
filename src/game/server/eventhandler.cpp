/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "eventhandler.h"

#include "entity.h"
#include "gamecontext.h"

#include <base/mem.h>
#include <base/vmath.h>

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
CEventHandler::CEventHandler()
{
	m_pGameServer = nullptr;
	m_CurrentBuffer = 0;
	for(int i = 0; i < NUM_BUFFERS; i++)
	{
		m_NumEvents[i] = 0;
		m_CurrentOffset[i] = 0;
	}
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, CClientMask Mask)
{
	const int Cur = m_CurrentBuffer;
	const int NumEvents = m_NumEvents[Cur];
	if(NumEvents == MAX_EVENTS)
		return nullptr;
	if(m_CurrentOffset[Cur] + Size >= MAX_DATASIZE)
		return nullptr;

	void *p = &m_aData[Cur][m_CurrentOffset[Cur]];
	m_aOffsets[Cur][NumEvents] = m_CurrentOffset[Cur];
	m_aTypes[Cur][NumEvents] = Type;
	m_aSizes[Cur][NumEvents] = Size;
	m_aClientMasks[Cur][NumEvents] = Mask;
	m_CurrentOffset[Cur] += Size;
	m_NumEvents[Cur]++;
	return p;
}

void CEventHandler::Clear()
{
	// Switch buffer, store previous events to send them to low bandwidth players
	// without resending them to highbandwidth players
	m_CurrentBuffer ^= 1;

	// Reset new current buffer
	m_NumEvents[m_CurrentBuffer] = 0;
	m_CurrentOffset[m_CurrentBuffer] = 0;
}

void CEventHandler::Snap(int SnappingClient)
{
	const int Cur = m_CurrentBuffer;
	const int Prev = Cur ^ 1;

	SnapBuffer(SnappingClient, Cur);
	if(!GameServer()->Server()->GetHighBandwidth(SnappingClient))
		SnapBuffer(SnappingClient, Prev);
}

void CEventHandler::SnapBuffer(int SnappingClient, int Buf)
{
	for(int i = 0; i < m_NumEvents[Buf]; i++)
	{
		if(SnappingClient == SERVER_DEMO_CLIENT || m_aClientMasks[Buf][i].test(SnappingClient))
		{
			const char *pData = &m_aData[Buf][m_aOffsets[Buf][i]];
			CNetEvent_Common *pEvent = (CNetEvent_Common *)pData;
			if(!NetworkClipped(GameServer(), SnappingClient, vec2(pEvent->m_X, pEvent->m_Y)))
			{
				int Type = m_aTypes[Buf][i];
				int Size = m_aSizes[Buf][i];
				if(GameServer()->Server()->IsSixup(SnappingClient))
					EventToSixup(&Type, &Size, &pData);

				// offset: dont overlap ids, start where m_CurrentBuffer stopped and append previously missed events
				int SnapId = i;
				if(Buf != m_CurrentBuffer)
					SnapId += m_NumEvents[m_CurrentBuffer];

				GameServer()->Server()->SnapNewItem(Type, SnapId, pData, Size);
			}
		}
	}
}

void CEventHandler::EventToSixup(int *pType, int *pSize, const char **ppData)
{
	static char s_aEventStore[128];
	if(*pType == NETEVENTTYPE_DAMAGEIND)
	{
		const CNetEvent_DamageInd *pEvent = (const CNetEvent_DamageInd *)(*ppData);
		protocol7::CNetEvent_Damage *pEvent7 = (protocol7::CNetEvent_Damage *)s_aEventStore;
		*pType = -protocol7::NETEVENTTYPE_DAMAGE;
		*pSize = sizeof(*pEvent7);

		pEvent7->m_X = pEvent->m_X;
		pEvent7->m_Y = pEvent->m_Y;

		pEvent7->m_ClientId = 0;
		pEvent7->m_Angle = 0;

		// This will need some work, perhaps an event wrapper for damageind,
		// a scan of the event array to merge multiple damageinds
		// or a separate array of "damage ind" events that's added in while snapping
		pEvent7->m_HealthAmount = 1;

		pEvent7->m_ArmorAmount = 0;
		pEvent7->m_Self = 0;

		*ppData = s_aEventStore;
	}
	else if(*pType == NETEVENTTYPE_SOUNDGLOBAL) // No more global sounds for the server
	{
		const CNetEvent_SoundGlobal *pEvent = (const CNetEvent_SoundGlobal *)(*ppData);
		protocol7::CNetEvent_SoundWorld *pEvent7 = (protocol7::CNetEvent_SoundWorld *)s_aEventStore;

		*pType = -protocol7::NETEVENTTYPE_SOUNDWORLD;
		*pSize = sizeof(*pEvent7);

		pEvent7->m_SoundId = pEvent->m_SoundId;
		pEvent7->m_X = pEvent->m_X;
		pEvent7->m_Y = pEvent->m_Y;

		*ppData = s_aEventStore;
	}
}
