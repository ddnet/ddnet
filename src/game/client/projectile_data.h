/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PROJECTILE_DATA_H
#define GAME_CLIENT_PROJECTILE_DATA_H

#include <base/vmath.h>

struct CNetObj_Projectile;
struct CNetObj_DDRaceProjectile;
struct CNetObj_DDNetProjectile;
struct CNetObj_EntityEx;

class CProjectileData
{
public:
	vec2 m_StartPos;
	vec2 m_StartVel;
	int m_Type;
	int m_StartTick;
	bool m_ExtraInfo;
	// The rest is only set if m_ExtraInfo is true.
	int m_Owner;
	bool m_Explosive = false;
	int m_Bouncing = 0;
	bool m_Freeze = false;
	int m_SwitchNumber;
	// TuneZone is introduced locally
	int m_TuneZone;
	float m_Curvature = 0.0f;
	float m_Speed = 0.0f;
	float m_Lifetime = 0.0f;
};

CProjectileData ExtractProjectileInfo(int NetObjType, const void *pData, class CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx);
CProjectileData ExtractProjectileInfoDDRace(const CNetObj_DDRaceProjectile *pProj, class CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx);
CProjectileData ExtractProjectileInfoDDNet(const CNetObj_DDNetProjectile *pProj, class CGameWorld *pGameWorld);

void GetProjectileTunings(CProjectileData *pData, class CGameWorld *pGameWorld);

void DemoObjectRemoveExtraProjectileInfo(CNetObj_Projectile *pProj);

#endif // GAME_CLIENT_PROJECTILE_DATA_H
