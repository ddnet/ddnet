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
	Clear();
}

void CEventHandler::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
}

void *CEventHandler::Create(int Type, int Size, CClientMask Mask)
{
	if(m_NumEvents == MAX_EVENTS)
		return nullptr;
	if(m_CurrentOffset + Size >= MAX_DATASIZE)
		return nullptr;

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
	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);
	for(int i = 0; i < m_NumEvents; i++)
	{
		if(SnappingClient == SERVER_DEMO_CLIENT || m_aClientMasks[i].test(SnappingClient))
		{
			CNetEvent_Common *pEventCommon = (CNetEvent_Common *)&m_aData[m_aOffsets[i]];
			if(!NetworkClipped(GameServer(), SnappingClient, vec2(pEventCommon->m_X, pEventCommon->m_Y)))
			{
				int Type = m_aTypes[i];
				int Size = m_aSizes[i];
				const char *pData = &m_aData[m_aOffsets[i]];

				const auto &&CommonExEvent = [&] {
					return Type == NETEVENTTYPE_FINISH ||
					       Type == NETEVENTTYPE_BIRTHDAY ||
					       Type == NETEVENTTYPE_DAMAGEINDEX ||
					       Type == NETEVENTTYPE_EXPLOSIONEX ||
					       Type == NETEVENTTYPE_HAMMERHITEX ||
					       Type == NETEVENTTYPE_SPAWNEX ||
					       Type == NETEVENTTYPE_SOUNDWORLDEX;
				};

				const auto &&SnapEvent = [&]() {
					if(GameServer()->Server()->IsSixup(SnappingClient))
						EventToSixup(&Type, &Size, &pData);
					if(SnappingClientVersion < VERSION_DDNET_EVENTS_EX)
						EventExToVanilla(&Type, &Size, &pData);
					GameServer()->Server()->SnapNewItem(Type, i, pData, Size);
				};

				const auto &&SnapTranslateEvent = [&](int *pClientId) {
					int ClientId = *pClientId; // Save real Id
					if(GameServer()->Server()->Translate(*pClientId, SnappingClient))
					{
						SnapEvent();
					}
					else if(CommonExEvent())
					{
						// Send event with ClientId = -1 if playermap is full/translation fails
						*pClientId = -1;
						SnapEvent();
					}
					// Reset Id for others
					*pClientId = ClientId;
				};

				if(Type == NETEVENTTYPE_DEATH)
				{
					CNetEvent_Death *pEvent = (CNetEvent_Death *)pData;
					SnapTranslateEvent(&pEvent->m_ClientId);
				}
				else if(CommonExEvent())
				{
					CNetEvent_CommonEx *pEvent = (CNetEvent_CommonEx *)pData;
					SnapTranslateEvent(&pEvent->m_ClientId);
				}
				else
				{
					SnapEvent();
				}
			}
		}
	}
}

void CEventHandler::EventExToVanilla(int *pType, int *pSize, const char **ppData)
{
	static char s_aEventStore[128];
	if(*pType == NETEVENTTYPE_DAMAGEINDEX)
	{
		const CNetEvent_DamageIndEx *pEventEx = (const CNetEvent_DamageIndEx *)(*ppData);
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)s_aEventStore;
		*pType = NETEVENTTYPE_DAMAGEIND;
		*pSize = sizeof(*pEvent);

		pEvent->m_X = pEventEx->m_X;
		pEvent->m_Y = pEventEx->m_Y;
		pEvent->m_Angle = pEventEx->m_Angle;

		*ppData = s_aEventStore;
	}
	else if(*pType == NETEVENTTYPE_SOUNDWORLDEX)
	{
		const CNetEvent_SoundWorldEx *pEventEx = (const CNetEvent_SoundWorldEx *)(*ppData);
		CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)s_aEventStore;
		*pType = NETEVENTTYPE_SOUNDWORLD;
		*pSize = sizeof(*pEvent);

		pEvent->m_X = pEventEx->m_X;
		pEvent->m_Y = pEventEx->m_Y;
		pEvent->m_SoundId = pEventEx->m_SoundId;

		*ppData = s_aEventStore;
	}
	else if(*pType == NETEVENTTYPE_EXPLOSIONEX || *pType == NETEVENTTYPE_HAMMERHITEX || *pType == NETEVENTTYPE_SPAWNEX)
	{
		const CNetEvent_CommonEx *pEventEx = (const CNetEvent_CommonEx *)(*ppData);
		CNetEvent_Common *pEvent = (CNetEvent_Common *)s_aEventStore;
		*pSize = sizeof(*pEvent);

		if(*pType == NETEVENTTYPE_EXPLOSIONEX)
			*pType = NETEVENTTYPE_EXPLOSION;
		else if(*pType == NETEVENTTYPE_HAMMERHITEX)
			*pType = NETEVENTTYPE_HAMMERHIT;
		else if(*pType == NETEVENTTYPE_SPAWNEX)
			*pType = NETEVENTTYPE_SPAWN;

		pEvent->m_X = pEventEx->m_X;
		pEvent->m_Y = pEventEx->m_Y;

		*ppData = s_aEventStore;
	}
}

void CEventHandler::EventToSixup(int *pType, int *pSize, const char **ppData)
{
	static char s_aEventStore[128];
	if(*pType == NETEVENTTYPE_DAMAGEINDEX)
	{
		const CNetEvent_DamageIndEx *pEvent = (const CNetEvent_DamageIndEx *)(*ppData);
		protocol7::CNetEvent_Damage *pEvent7 = (protocol7::CNetEvent_Damage *)s_aEventStore;
		*pType = -protocol7::NETEVENTTYPE_DAMAGE;
		*pSize = sizeof(*pEvent7);

		pEvent7->m_X = pEvent->m_X;
		pEvent7->m_Y = pEvent->m_Y;

		// 0.7 clamps ClientId between 0 and 63, use least used id.
		// Currently 0.7 clients rely on the playermap anyways, where 63 is not used at all.
		pEvent7->m_ClientId = pEvent->m_ClientId != -1 ? pEvent->m_ClientId : 63;
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
