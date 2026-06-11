/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PICKUP_DATA_H
#define GAME_CLIENT_PICKUP_DATA_H

#include "item_data.h"

#include <base/vmath.h>

struct CNetObj_Pickup;
struct CNetObj_DDNetPickup;
struct CNetObj_EntityEx;

class CPickupData : public IItemData
{
public:
	vec2 m_Pos;
	int m_Type;
	int m_Subtype;
	int m_SwitchNumber;
	int m_Flags = 0;

	bool IsVisible(float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1, bool IsPredicted) const override;
};

CPickupData ExtractPickupInfo(int NetObjType, const void *pData, const CNetObj_EntityEx *pEntEx);
CPickupData ExtractPickupInfoDDNet(const CNetObj_DDNetPickup *pPickup);

#endif // GAME_CLIENT_PICKUP_DATA_H
