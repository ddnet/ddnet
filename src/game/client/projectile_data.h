/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PROJECTILE_DATA_H
#define GAME_CLIENT_PROJECTILE_DATA_H

#include "item_data.h"

#include <base/vmath.h>

struct CNetObj_Projectile;
struct CNetObj_DDRaceProjectile;
struct CNetObj_DDNetProjectile;
struct CNetObj_EntityEx;

class CProjectileData : public IItemData
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

	bool IsVisible(float ScreenX0, float ScreenY0, float ScreenX1, float ScreenY1, bool IsPredicted) const override;
};

CProjectileData ExtractProjectileInfo(int NetObjType, const void *pData, class CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx);
CProjectileData ExtractProjectileInfoDDRace(const CNetObj_DDRaceProjectile *pProj, class CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx);
CProjectileData ExtractProjectileInfoDDNet(const CNetObj_DDNetProjectile *pProj);

void DemoObjectRemoveExtraProjectileInfo(CNetObj_Projectile *pProj);

#endif // GAME_CLIENT_PROJECTILE_DATA_H
