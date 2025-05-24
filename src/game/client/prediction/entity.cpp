/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"

#include <game/collision.h>

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius)
{
	m_pGameWorld = pGameWorld;

	m_ObjType = ObjType;
	m_Pos = Pos;
	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Id = -1;

	m_pPrevTypeEntity = nullptr;
	m_pNextTypeEntity = nullptr;
	m_SnapTicks = -1;

	// DDRace
	m_pParent = nullptr;
	m_pChild = nullptr;
	m_DestroyTick = -1;
	m_LastRenderTick = -1;
}

CEntity::~CEntity()
{
	if(GameWorld())
		GameWorld()->RemoveEntity(this);
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > Collision()->GetWidth() + 200 ||
	       round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > Collision()->GetHeight() + 200;
}

bool CEntity::GetNearestAirPos(vec2 Pos, vec2 PrevPos, vec2 *pOutPos)
{
	for(int k = 0; k < 16 && Collision()->CheckPoint(Pos); k++)
	{
		Pos -= normalize(PrevPos - Pos);
	}

	vec2 PosInBlock = vec2(round_to_int(Pos.x) % 32, round_to_int(Pos.y) % 32);
	vec2 BlockCenter = vec2(round_to_int(Pos.x), round_to_int(Pos.y)) - PosInBlock + vec2(16.0f, 16.0f);

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f), Pos.y);
	if(!Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		return true;

	*pOutPos = vec2(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	if(!Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		return true;

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
		BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	return !Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2());
}

bool CEntity::GetNearestAirPosPlayer(vec2 PlayerPos, vec2 *pOutPos)
{
	for(int dist = 5; dist >= -1; dist--)
	{
		*pOutPos = vec2(PlayerPos.x, PlayerPos.y - dist);
		if(!Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		{
			return true;
		}
	}
	return false;
}
