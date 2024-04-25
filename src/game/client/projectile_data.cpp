/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "projectile_data.h"

#include <engine/shared/snapshot.h>
#include <game/client/prediction/gameworld.h>
#include <game/generated/protocol.h>

#include <game/collision.h>

bool UseProjectileExtraInfo(const CNetObj_Projectile *pProj)
{
	return pProj->m_VelY >= 0 && (pProj->m_VelY & LEGACYPROJECTILEFLAG_IS_DDNET) != 0;
}

CProjectileData ExtractProjectileInfo(int NetObjType, const void *pData, CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx)
{
	CNetObj_Projectile *pProj = (CNetObj_Projectile *)pData;

	if(NetObjType == NETOBJTYPE_DDNETPROJECTILE)
	{
		return ExtractProjectileInfoDDNet((CNetObj_DDNetProjectile *)pData);
	}
	else if(NetObjType == NETOBJTYPE_DDRACEPROJECTILE || (NetObjType == NETOBJTYPE_PROJECTILE && UseProjectileExtraInfo(pProj)))
	{
		return ExtractProjectileInfoDDRace((CNetObj_DDRaceProjectile *)pData, pGameWorld, pEntEx);
	}

	CProjectileData Result = {vec2(0, 0)};
	Result.m_StartPos.x = pProj->m_X;
	Result.m_StartPos.y = pProj->m_Y;
	Result.m_StartVel.x = pProj->m_VelX / 100.0f;
	Result.m_StartVel.y = pProj->m_VelY / 100.0f;
	Result.m_Type = pProj->m_Type;
	Result.m_StartTick = pProj->m_StartTick;
	Result.m_ExtraInfo = false;
	Result.m_Owner = -1;
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_StartPos)) : 0;
	Result.m_SwitchNumber = pEntEx ? pEntEx->m_SwitchNumber : 0;
	return Result;
}

CProjectileData ExtractProjectileInfoDDRace(const CNetObj_DDRaceProjectile *pProj, CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx)
{
	CProjectileData Result = {vec2(0, 0)};

	Result.m_StartPos.x = pProj->m_X / 100.0f;
	Result.m_StartPos.y = pProj->m_Y / 100.0f;
	float Angle = pProj->m_Angle / 1000000.0f;
	Result.m_StartVel.x = std::sin(-Angle);
	Result.m_StartVel.y = std::cos(-Angle);
	Result.m_Type = pProj->m_Type;
	Result.m_StartTick = pProj->m_StartTick;

	Result.m_ExtraInfo = true;
	Result.m_Owner = pProj->m_Data & 255;
	if(pProj->m_Data & LEGACYPROJECTILEFLAG_NO_OWNER || Result.m_Owner < 0 || Result.m_Owner >= MAX_CLIENTS)
	{
		Result.m_Owner = -1;
	}
	// LEGACYPROJECTILEFLAG_BOUNCE_HORIZONTAL, LEGACYPROJECTILEFLAG_BOUNCE_VERTICAL
	Result.m_Bouncing = (pProj->m_Data >> 10) & 3;
	Result.m_Explosive = pProj->m_Data & LEGACYPROJECTILEFLAG_EXPLOSIVE;
	Result.m_Freeze = pProj->m_Data & LEGACYPROJECTILEFLAG_FREEZE;
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_StartPos)) : 0;
	Result.m_SwitchNumber = pEntEx ? pEntEx->m_SwitchNumber : 0;
	return Result;
}

CProjectileData ExtractProjectileInfoDDNet(const CNetObj_DDNetProjectile *pProj)
{
	CProjectileData Result = {vec2(0, 0)};

	Result.m_StartPos = vec2(pProj->m_X / 100.0f, pProj->m_Y / 100.0f);
	Result.m_StartVel = vec2(pProj->m_VelX / 1e6f, pProj->m_VelY / 1e6f);

	if(pProj->m_Flags & PROJECTILEFLAG_NORMALIZE_VEL)
	{
		Result.m_StartVel = normalize(Result.m_StartVel);
	}

	Result.m_Type = pProj->m_Type;
	Result.m_StartTick = pProj->m_StartTick;

	Result.m_ExtraInfo = true;
	Result.m_Owner = pProj->m_Owner;
	Result.m_SwitchNumber = pProj->m_SwitchNumber;
	Result.m_TuneZone = pProj->m_TuneZone;

	Result.m_Bouncing = 0;
	if(pProj->m_Flags & PROJECTILEFLAG_BOUNCE_HORIZONTAL)
	{
		Result.m_Bouncing |= 1;
	}
	if(pProj->m_Flags & PROJECTILEFLAG_BOUNCE_VERTICAL)
	{
		Result.m_Bouncing |= 2;
	}

	Result.m_Explosive = pProj->m_Flags & PROJECTILEFLAG_EXPLOSIVE;
	Result.m_Freeze = pProj->m_Flags & PROJECTILEFLAG_FREEZE;

	return Result;
}

void SnapshotRemoveExtraProjectileInfo(CSnapshot *pSnap)
{
	for(int Index = 0; Index < pSnap->NumItems(); Index++)
	{
		const CSnapshotItem *pItem = pSnap->GetItem(Index);
		if(pItem->Type() == NETOBJTYPE_PROJECTILE)
		{
			CNetObj_Projectile *pProj = (CNetObj_Projectile *)((void *)pItem->Data());
			if(UseProjectileExtraInfo(pProj))
			{
				CProjectileData Data = ExtractProjectileInfo(NETOBJTYPE_PROJECTILE, pProj, nullptr, nullptr);
				pProj->m_X = Data.m_StartPos.x;
				pProj->m_Y = Data.m_StartPos.y;
				pProj->m_VelX = (int)(Data.m_StartVel.x * 100.0f);
				pProj->m_VelY = (int)(Data.m_StartVel.y * 100.0f);
			}
		}
	}
}
