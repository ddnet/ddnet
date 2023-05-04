/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "pickup_data.h"

#include <engine/shared/snapshot.h>
#include <game/collision.h>
#include <game/generated/protocol.h>

CPickupData ExtractPickupInfo(int NetObjType, const void *pData, const CNetObj_EntityEx *pEntEx)
{
	if(NetObjType == NETOBJTYPE_DDNETPICKUP)
	{
		return ExtractPickupInfoDDNet((CNetObj_DDNetPickup *)pData);
	}

	CNetObj_Pickup *pPickup = (CNetObj_Pickup *)pData;

	CPickupData Result = {vec2(0, 0)};

	Result.m_Pos.x = pPickup->m_X;
	Result.m_Pos.y = pPickup->m_Y;
	Result.m_Type = pPickup->m_Type;
	Result.m_Subtype = pPickup->m_Subtype;
	Result.m_SwitchNumber = pEntEx ? pEntEx->m_SwitchNumber : 0;

	return Result;
}

CPickupData ExtractPickupInfoDDNet(const CNetObj_DDNetPickup *pPickup)
{
	CPickupData Result = {vec2(0, 0)};

	Result.m_Pos.x = pPickup->m_X;
	Result.m_Pos.y = pPickup->m_Y;
	Result.m_Type = pPickup->m_Type;
	Result.m_Subtype = pPickup->m_Subtype;
	Result.m_SwitchNumber = pPickup->m_SwitchNumber;

	return Result;
}
