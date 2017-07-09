/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include <game/animation.h>

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType)
{
	m_pGameWorld = pGameWorld;

	m_ObjType = ObjType;
	m_Pos = vec2(0,0);
	m_ProximityRadius = 0;

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

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if(SnappingClient == -1)
		return 0;

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if(distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 4000.0f)
		return 1;
	return 0;
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x)/32 < -200 || round_to_int(CheckPos.x)/32 > GameServer()->Collision()->GetWidth()+200 ||
			round_to_int(CheckPos.y)/32 < -200 || round_to_int(CheckPos.y)/32 > GameServer()->Collision()->GetHeight()+200 ? true : false;
}

CAnimatedEntity::CAnimatedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pivot) :
	CEntity(pGameWorld, Objtype),
	m_Pivot(Pivot),
	m_RelPosition(vec2(0.0f, 0.0f)),
	m_PosEnv(-1)
{
	
}

CAnimatedEntity::CAnimatedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pivot, vec2 RelPosition, int PosEnv) :
	CEntity(pGameWorld, Objtype),
	m_Pivot(Pivot),
	m_RelPosition(RelPosition),
	m_PosEnv(PosEnv)
{
	
}

void CAnimatedEntity::Tick()
{
	vec2 Position(0.0f, 0.0f);
	float Angle = 0.0f;
	if(m_PosEnv >= 0)
	{
		GetAnimationTransform(GameServer()->m_pController->GetTime(), m_PosEnv, GameServer()->Layers(), Position, Angle);
	}
	
	float x = (m_RelPosition.x * cosf(Angle) - m_RelPosition.y * sinf(Angle));
	float y = (m_RelPosition.x * sinf(Angle) + m_RelPosition.y * cosf(Angle));
	
	m_Pos = Position + m_Pivot + vec2(x, y);
}
