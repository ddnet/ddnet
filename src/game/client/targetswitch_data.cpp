/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "targetswitch_data.h"

#include <game/generated/protocol.h>

CTargetSwitchData ExtractTargetSwitchInfo(const void *pData)
{
	CNetObj_DDNetTargetSwitch *pTargetSwitch = (CNetObj_DDNetTargetSwitch *)pData;

	CTargetSwitchData Result = {vec2(0, 0)};

	Result.m_Pos.x = pTargetSwitch->m_X;
	Result.m_Pos.y = pTargetSwitch->m_Y;
	Result.m_Type = pTargetSwitch->m_Type;
	Result.m_SwitchNumber = pTargetSwitch->m_SwitchNumber;
	Result.m_SwitchDelay = pTargetSwitch->m_SwitchDelay;
	Result.m_Flags = pTargetSwitch->m_Flags;

	return Result;
}
