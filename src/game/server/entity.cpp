/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include "player.h"

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType, vec2 Pos, int ProximityRadius)
{
	m_pGameWorld = pGameWorld;
	m_pCCollision = GameServer()->Collision();

	m_ObjType = ObjType;
	m_Pos = Pos;
	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_Id = Server()->SnapNewId();

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeId(m_Id);
}

bool CEntity::NetworkClipped(int SnappingClient) const
{
	return ::NetworkClipped(m_pGameWorld->GameServer(), SnappingClient, m_Pos);
}

bool CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos) const
{
	return ::NetworkClipped(m_pGameWorld->GameServer(), SnappingClient, CheckPos);
}

bool CEntity::NetworkClippedLine(int SnappingClient, vec2 StartPos, vec2 EndPos) const
{
	return ::NetworkClippedLine(m_pGameWorld->GameServer(), SnappingClient, StartPos, EndPos);
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > GameServer()->Collision()->GetWidth() + 200 ||
	       round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > GameServer()->Collision()->GetHeight() + 200;
}

bool CEntity::GetNearestAirPos(vec2 Pos, vec2 PrevPos, vec2 *pOutPos)
{
	for(int k = 0; k < 16 && GameServer()->Collision()->CheckPoint(Pos); k++)
	{
		Pos -= normalize(PrevPos - Pos);
	}

	vec2 PosInBlock = vec2(round_to_int(Pos.x) % 32, round_to_int(Pos.y) % 32);
	vec2 BlockCenter = vec2(round_to_int(Pos.x), round_to_int(Pos.y)) - PosInBlock + vec2(16.0f, 16.0f);

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f), Pos.y);
	if(!GameServer()->Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		return true;

	*pOutPos = vec2(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	if(!GameServer()->Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		return true;

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
		BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	return !GameServer()->Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2());
}

bool CEntity::GetNearestAirPosPlayer(vec2 PlayerPos, vec2 *pOutPos)
{
	for(int dist = 5; dist >= -1; dist--)
	{
		*pOutPos = vec2(PlayerPos.x, PlayerPos.y - dist);
		if(!GameServer()->Collision()->TestBox(*pOutPos, CCharacterCore::PhysicalSizeVec2()))
		{
			return true;
		}
	}
	return false;
}

bool NetworkClipped(const CGameContext *pGameServer, int SnappingClient, vec2 CheckPos)
{
	if(SnappingClient == SERVER_DEMO_CLIENT || pGameServer->m_apPlayers[SnappingClient]->m_ShowAll)
		return false;

	float dx = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos.x - CheckPos.x;
	if(absolute(dx) > pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance.x)
		return true;

	float dy = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos.y - CheckPos.y;
	return absolute(dy) > pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance.y;
}

bool NetworkClippedLine(const CGameContext *pGameServer, int SnappingClient, vec2 StartPos, vec2 EndPos)
{
	if(SnappingClient == SERVER_DEMO_CLIENT || pGameServer->m_apPlayers[SnappingClient]->m_ShowAll)
		return false;

	vec2 &ViewPos = pGameServer->m_apPlayers[SnappingClient]->m_ViewPos;
	vec2 &ShowDistance = pGameServer->m_apPlayers[SnappingClient]->m_ShowDistance;

	vec2 DistanceToLine, ClosestPoint;
	if(closest_point_on_line(StartPos, EndPos, ViewPos, ClosestPoint))
	{
		DistanceToLine = ViewPos - ClosestPoint;
	}
	else
	{
		// No line section was passed but two equal points
		DistanceToLine = ViewPos - StartPos;
	}
	float ClippDistance = maximum(ShowDistance.x, ShowDistance.y);
	return (absolute(DistanceToLine.x) > ClippDistance || absolute(DistanceToLine.y) > ClippDistance);
}
