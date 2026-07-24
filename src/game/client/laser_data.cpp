/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "laser_data.h"

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

CLaserData ExtractLaserInfo(int NetObjType, const void *pData, CGameWorld *pGameWorld, const CNetObj_EntityEx *pEntEx)
{
	CLaserData Result = {vec2(0, 0)};

	if(NetObjType == NETOBJTYPE_DDNETLASER)
	{
		Result = ExtractLaserInfoDDNet((const CNetObj_DDNetLaser *)pData, pGameWorld);
	}
	else
	{
		CNetObj_Laser *pLaser = (CNetObj_Laser *)pData;

		Result.m_From.x = pLaser->m_FromX;
		Result.m_From.y = pLaser->m_FromY;
		Result.m_To.x = pLaser->m_X;
		Result.m_To.y = pLaser->m_Y;
		Result.m_StartTick = pLaser->m_StartTick;
		Result.m_ExtraInfo = false;
		Result.m_Owner = -1;
		Result.m_Type = -1;
		Result.m_SwitchNumber = 0;
		Result.m_Subtype = -1;
		Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_From)) : 0;
		Result.m_Predict = true;
	}

	if(pEntEx && !(NetObjType == NETOBJTYPE_DDNETLASER && Result.m_SwitchNumber >= 0))
	{
		Result.m_SwitchNumber = pEntEx->m_SwitchNumber;
		if(pEntEx->m_EntityClass == ENTITYCLASS_LIGHT)
		{
			Result.m_Type = LASERTYPE_FREEZE;
		}
		else if(pEntEx->m_EntityClass >= ENTITYCLASS_GUN_NORMAL && pEntEx->m_EntityClass <= ENTITYCLASS_GUN_UNFREEZE)
		{
			Result.m_Type = LASERTYPE_GUN;
		}
		else if(pEntEx->m_EntityClass >= ENTITYCLASS_DRAGGER_WEAK && pEntEx->m_EntityClass <= ENTITYCLASS_DRAGGER_STRONG)
		{
			Result.m_Type = LASERTYPE_DRAGGER;
		}
		else if(pEntEx->m_EntityClass == ENTITYCLASS_DOOR)
		{
			Result.m_Type = LASERTYPE_DOOR;
		}
	}

	GetLaserTunings(&Result, pGameWorld);
	return Result;
}

CLaserData ExtractLaserInfoDDNet(const CNetObj_DDNetLaser *pLaser, CGameWorld *pGameWorld)
{
	CLaserData Result = {vec2(0, 0)};
	Result.m_From.x = pLaser->m_FromX;
	Result.m_From.y = pLaser->m_FromY;
	Result.m_To.x = pLaser->m_ToX;
	Result.m_To.y = pLaser->m_ToY;
	Result.m_StartTick = pLaser->m_StartTick;
	Result.m_ExtraInfo = true;
	Result.m_Owner = pLaser->m_Owner;
	Result.m_Type = pLaser->m_Type;
	Result.m_SwitchNumber = pLaser->m_SwitchNumber;
	Result.m_Subtype = pLaser->m_Subtype;
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_From)) : 0;
	Result.m_Predict = !(pLaser->m_Flags & LASERFLAG_NO_PREDICT);
	if(pLaser->m_Flags & LASERFLAG_HAS_TUNEPARAMS)
	{
		Result.m_ShotgunStrength = pLaser->m_ShotgunStrength;
		Result.m_BounceNum = pLaser->m_BounceNum;
		Result.m_BounceCost = pLaser->m_BounceCost;
		Result.m_BounceDelay = pLaser->m_BounceDelay;
	}
	else
	{
		GetLaserTunings(&Result, pGameWorld);
	}
	return Result;
}

void GetLaserTunings(CLaserData *pData, class CGameWorld *pGameWorld)
{
	const CTuningParams *pTuning = pGameWorld ? pGameWorld->TuningFromChrOrZone(pData->m_Owner, pData->m_TuneZone) : &CTuningParams::DEFAULT;
	pData->m_ShotgunStrength = pTuning->m_ShotgunStrength;
	pData->m_BounceNum = pTuning->m_LaserBounceNum;
	pData->m_BounceCost = pTuning->m_LaserBounceCost;
	pData->m_BounceDelay = pTuning->m_LaserBounceDelay;
	pData->m_LaserReach = pTuning->m_LaserReach;
}
