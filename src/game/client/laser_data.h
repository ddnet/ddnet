/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_LASER_DATA_H
#define GAME_CLIENT_LASER_DATA_H

#include <base/vmath.h>

struct CNetObj_Laser;
struct CNetObj_DDNetLaser;

class CLaserData
{
public:
	vec2 m_From;
	vec2 m_To;
	int m_StartTick;
	bool m_ExtraInfo;
	// The rest is only set if m_ExtraInfo is true.
	int m_Owner;
	int m_Type;
	// TuneZone is introduced locally
	int m_TuneZone;
};

CLaserData ExtractLaserInfo(const CNetObj_Laser *pLaser, class CGameWorld *pGameWorld);
CLaserData ExtractLaserInfoDDNet(const CNetObj_DDNetLaser *pLaser, class CGameWorld *pGameWorld);

#endif // GAME_CLIENT_LASER_DATA_H
