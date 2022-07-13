/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PROJECTILE_DATA_H
#define GAME_CLIENT_PROJECTILE_DATA_H

#include <base/vmath.h>

struct CNetObj_Projectile;
struct CNetObj_DDNetProjectile;

class CProjectileData
{
public:
	vec2 m_StartPos;
	vec2 m_StartVel;
	int m_Type = 0;
	int m_StartTick = 0;
	bool m_ExtraInfo = false;
	// The rest is only set if m_ExtraInfo is true.
	int m_Owner = 0;
	bool m_Explosive = false;
	int m_Bouncing = 0;
	bool m_Freeze = false;
	// TuneZone is introduced locally
	int m_TuneZone = 0;
};

CProjectileData ExtractProjectileInfo(const CNetObj_Projectile *pProj, class CGameWorld *pGameWorld);
CProjectileData ExtractProjectileInfoDDNet(const CNetObj_DDNetProjectile *pProj, class CGameWorld *pGameWorld);

#endif // GAME_CLIENT_PROJECTILE_DATA_H
