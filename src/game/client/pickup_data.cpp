/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "pickup_data.h"

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <game/collision.h>

bool CPickupData::IsVisible(float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1, bool IsPredicted) const
{
	constexpr float TileSize = 64.0f;
	// weapon pickups are wider
	float ExtraSpace = (m_Type == POWERUP_WEAPON || m_Type == POWERUP_NINJA) ? 1.75f * TileSize : 0.75f * TileSize;
	return in_range(m_Pos.x, ScreenX0 - ExtraSpace, ScreenX1 + ExtraSpace) && in_range(m_Pos.y, ScreenY0 - ExtraSpace, ScreenY1 + ExtraSpace);
}

CPickupData ExtractPickupInfo(int NetObjType, const void *pData, const CNetObj_EntityEx *pEntEx)
{
	if(NetObjType == NETOBJTYPE_DDNETPICKUP)
	{
		return ExtractPickupInfoDDNet((CNetObj_DDNetPickup *)pData);
	}

	CNetObj_Pickup *pPickup = (CNetObj_Pickup *)pData;

	CPickupData Result;

	Result.m_Pos.x = pPickup->m_X;
	Result.m_Pos.y = pPickup->m_Y;
	Result.m_Type = pPickup->m_Type;
	Result.m_Subtype = pPickup->m_Subtype;
	Result.m_SwitchNumber = pEntEx ? pEntEx->m_SwitchNumber : 0;

	return Result;
}

CPickupData ExtractPickupInfoDDNet(const CNetObj_DDNetPickup *pPickup)
{
	CPickupData Result;

	Result.m_Pos.x = pPickup->m_X;
	Result.m_Pos.y = pPickup->m_Y;
	Result.m_Type = pPickup->m_Type;
	Result.m_Subtype = pPickup->m_Subtype;
	Result.m_SwitchNumber = pPickup->m_SwitchNumber;
	Result.m_Flags = pPickup->m_Flags;

	return Result;
}
