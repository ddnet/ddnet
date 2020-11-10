/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType)
{
	m_pGameWorld = pGameWorld;

	m_ObjType = ObjType;
	m_Pos = vec2(0, 0);
	m_ProximityRadius = 0;

	m_MarkedForDestroy = false;
	m_ID = -1;

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;
	m_SnapTicks = -1;

	// DDRace
	m_pParent = 0;
	m_DestroyTick = -1;
	m_LastRenderTick = -1;
}

CEntity::~CEntity()
{
	if(GameWorld())
		GameWorld()->RemoveEntity(this);
}

int CEntity::NetworkClipped(vec2 ViewPos)
{
	return NetworkClipped(m_Pos, ViewPos);
}

int CEntity::NetworkClipped(vec2 CheckPos, vec2 ViewPos)
{
	float dx = ViewPos.x - CheckPos.x;
	float dy = ViewPos.y - CheckPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return 1;

	if(distance(ViewPos, CheckPos) > 4000.0f)
		return 1;
	return 0;
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x) / 32 < -200 || round_to_int(CheckPos.x) / 32 > Collision()->GetWidth() + 200 ||
			       round_to_int(CheckPos.y) / 32 < -200 || round_to_int(CheckPos.y) / 32 > Collision()->GetHeight() + 200 ?
                       true :
                       false;
}
