/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "laser_data.h"

#include <engine/shared/snapshot.h>
#include <game/client/prediction/gameworld.h>
#include <game/generated/protocol.h>

#include <game/collision.h>

CLaserData ExtractLaserInfo(const CNetObj_Laser *pLaser, CGameWorld *pGameWorld)
{
	CLaserData Result = {vec2(0, 0)};
	Result.m_From.x = pLaser->m_FromX;
	Result.m_From.y = pLaser->m_FromY;
	Result.m_To.x = pLaser->m_X;
	Result.m_To.y = pLaser->m_Y;
	Result.m_StartTick = pLaser->m_StartTick;
	Result.m_ExtraInfo = false;
	Result.m_Owner = -1;
	Result.m_Type = -1;
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_From)) : 0;
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
	Result.m_TuneZone = pGameWorld && pGameWorld->m_WorldConfig.m_UseTuneZones ? pGameWorld->Collision()->IsTune(pGameWorld->Collision()->GetMapIndex(Result.m_From)) : 0;
	return Result;
}
