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

	m_ObjType = ObjType;
	m_Pos = Pos;
	m_ProximityRadius = ProximityRadius;

	m_MarkedForDestroy = false;
	m_ID = Server()->SnapNewID();

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
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
	if(!GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f)))
		return true;

	*pOutPos = vec2(Pos.x, BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	if(!GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f)))
		return true;

	*pOutPos = vec2(BlockCenter.x + (PosInBlock.x < 16 ? -2.0f : 1.0f),
		BlockCenter.y + (PosInBlock.y < 16 ? -2.0f : 1.0f));
	return !GameServer()->Collision()->TestBox(*pOutPos, vec2(28.0f, 28.0f));
}

bool CEntity::GetNearestAirPosPlayer(vec2 PlayerPos, vec2 *OutPos)
{
	for(int dist = 5; dist >= -1; dist--)
	{
		*OutPos = vec2(PlayerPos.x, PlayerPos.y - dist);
		if(!GameServer()->Collision()->TestBox(*OutPos, vec2(28.0f, 28.0f)))
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

	float dx1 = ViewPos.x - StartPos.x;
	float dy1 = ViewPos.y - StartPos.y;
	if(absolute(dx1) <= ShowDistance.x && absolute(dy1) <= ShowDistance.x)
		return false;

	float dx2 = ViewPos.x - EndPos.x;
	float dy2 = ViewPos.y - EndPos.y;
	if(absolute(dx2) <= ShowDistance.x && absolute(dy2) <= ShowDistance.x)
		return false;

	vec2 vecLine = vec2(EndPos.x - StartPos.x, EndPos.y - StartPos.y);
	float dLengthLine = length(vecLine);
	vecLine /= dLengthLine;
	float dDistLine = dot(vecLine, StartPos);
	vec2 vecPerpLine = vec2(-vecLine.y, vecLine.x);
	float dDistPerpLine = dot(vecPerpLine, StartPos);

	vec2 vecRect1 = vec2(ViewPos.x - ShowDistance.x, ViewPos.y - ShowDistance.y);
	vec2 vecRect2 = vec2(ViewPos.x + ShowDistance.x, ViewPos.y - ShowDistance.y);
	vec2 vecRect3 = vec2(ViewPos.x + ShowDistance.x, ViewPos.y + ShowDistance.y);
	vec2 vecRect4 = vec2(ViewPos.x - ShowDistance.x, ViewPos.y + ShowDistance.y);

	float dPerpLineDist1 = dot(vecPerpLine, vecRect1) - dDistPerpLine;
	float dPerpLineDist2 = dot(vecPerpLine, vecRect2) - dDistPerpLine;
	float dPerpLineDist3 = dot(vecPerpLine, vecRect3) - dDistPerpLine;
	float dPerpLineDist4 = dot(vecPerpLine, vecRect4) - dDistPerpLine;
	float dMinPerpLineDist = minimum(dPerpLineDist1, minimum(dPerpLineDist2, dPerpLineDist3, dPerpLineDist4));
	float dMaxPerpLineDist = maximum(dPerpLineDist1, maximum(dPerpLineDist2, dPerpLineDist3, dPerpLineDist4));

	// If all distances are positive, or all distances are negative,
	// then the rectangle is on one side of the line or the other, so there's no intersection
	if((dMinPerpLineDist <= 0.0 && dMaxPerpLineDist <= 0.0) || (dMinPerpLineDist >= 0.0 && dMaxPerpLineDist >= 0.0))
	{
		return true;
	}

	float dDistLine1 = dot(vecLine, vecRect1) - dDistLine;
	float dDistLine2 = dot(vecLine, vecRect2) - dDistLine;
	float dDistLine3 = dot(vecLine, vecRect3) - dDistLine;
	float dDistLine4 = dot(vecLine, vecRect4) - dDistLine;
	float dMinLineDist = minimum(dDistLine1, minimum(dDistLine2, dDistLine3, dDistLine4));
	float dMaxLineDist = maximum(dDistLine1, maximum(dDistLine2, dDistLine3, dDistLine4));
	// If the rectangle's points don't fall within the line segment's extent, then there's no intersection
	return (dMaxLineDist <= 0.0 || dMinLineDist >= dLengthLine);
}