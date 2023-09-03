/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "projectile_data.h"

#include <engine/shared/snapshot.h>
#include <game/client/prediction/gameworld.h>
#include <game/generated/protocol.h>

#include <game/collision.h>

bool UseProjectileExtraInfo(const CNetObj_Projectile *pProj)
{
	return pProj->m_VelY >= 0 && (pProj->m_VelY & PROJECTILEFLAG_IS_DDNET) != 0;
}

CProjectileData ExtractProjectileInfo(const CNetObj_Projectile *pProj, CGameWorld *pGameWorld)
{
	if(UseProjectileExtraInfo(pProj))
	{
		CNetObj_DDNetProjectile Proj;
		mem_copy(&Proj, pProj, sizeof(Proj));
		return ExtractProjectileInfoDDNet(&Proj, pGameWorld);
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
	return Result;
}

CProjectileData ExtractProjectileInfoDDNet(const CNetObj_DDNetProjectile *pProj, CGameWorld *pGameWorld)
{
	CProjectileData Result = {vec2(0, 0)};

	Result.m_StartPos.x = pProj->m_X / 100.0f;
	Result.m_StartPos.y = pProj->m_Y / 100.0f;
	float Angle = pProj->m_Angle / 1000000.0f;
	Result.m_StartVel.x = sin(-Angle);
	Result.m_StartVel.y = cos(-Angle);
	Result.m_Type = pProj->m_Type;
	Result.m_StartTick = pProj->m_StartTick;

	Result.m_ExtraInfo = true;
	Result.m_Owner = pProj->m_Data & 255;
	if(pProj->m_Data & PROJECTILEFLAG_NO_OWNER || Result.m_Owner < 0 || Result.m_Owner >= MAX_CLIENTS)
	{
		Result.m_Owner = -1;
	}
	// PROJECTILEFLAG_BOUNCE_HORIZONTAL, PROJECTILEFLAG_BOUNCE_VERTICAL
	Result.m_Bouncing = (pProj->m_Data >> 10) & 3;
	Result.m_Explosive = pProj->m_Data & PROJECTILEFLAG_EXPLOSIVE;
	Result.m_Freeze = pProj->m_Data & PROJECTILEFLAG_FREEZE;
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_StartPos)) : 0;
	return Result;
}

void SnapshotRemoveExtraProjectileInfo(unsigned char *pData)
{
	CSnapshot *pSnap = (CSnapshot *)pData;
	for(int Index = 0; Index < pSnap->NumItems(); Index++)
	{
		const CSnapshotItem *pItem = pSnap->GetItem(Index);
		if(pItem->Type() == NETOBJTYPE_PROJECTILE)
		{
			CNetObj_Projectile *pProj = (CNetObj_Projectile *)((void *)pItem->Data());
			if(UseProjectileExtraInfo(pProj))
			{
				CProjectileData Data = ExtractProjectileInfo(pProj, nullptr);
				pProj->m_X = Data.m_StartPos.x;
				pProj->m_Y = Data.m_StartPos.y;
				pProj->m_VelX = (int)(Data.m_StartVel.x * 100.0f);
				pProj->m_VelY = (int)(Data.m_StartVel.y * 100.0f);
			}
		}
	}
}
