#ifndef GAME_CLIENT_TARGETSWITCH_DATA_H
#define GAME_CLIENT_TARGETSWITCH_DATA_H

#include <base/vmath.h>

struct CNetObj_DDNetTargetSwitch;

class CTargetSwitchData
{
public:
	vec2 m_Pos;
	int m_Type;
	int m_SwitchNumber;
	int m_SwitchDelay;
	int m_Flags;
};

CTargetSwitchData ExtractTargetSwitchInfo(const void *pData);

#endif // GAME_CLIENT_TARGETSWITCH_DATA_H
