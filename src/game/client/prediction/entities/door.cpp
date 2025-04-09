/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "door.h"
#include "character.h"
#include <game/client/laser_data.h>
#include <game/collision.h>
#include <game/mapitems.h>

CDoor::CDoor(CGameWorld *pGameWorld, int Id, const CLaserData *pData) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DOOR)
{
	m_Id = Id;
	m_Active = false;

	m_Number = pData->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;

	Read(pData);
}

void CDoor::ResetCollision()
{
	if(Collision()->GetTile(m_Pos.x, m_Pos.y) || Collision()->GetFrontTile(m_Pos.x, m_Pos.y))
		return;

	m_Active = true;

	vec2 Dir = m_To - m_Pos;
	m_Length = length(Dir);
	m_Direction = normalize_pre_length(Dir, static_cast<float>(m_Length));

	for(int i = 0; i < m_Length - 1; i++)
	{
		vec2 CurrentPos = m_Pos + m_Direction * i;

		CDoorTile DoorTile;
		Collision()->GetDoorTile(Collision()->GetPureMapIndex(CurrentPos), &DoorTile);
		// switch door always has priority, because it can turn off doors on all layers if they intersect
		if(DoorTile.m_Index && DoorTile.m_Number > 0)
			continue;

		if(Collision()->CheckPoint(CurrentPos))
			break;
		else
			Collision()->SetDoorCollisionAt(CurrentPos.x, CurrentPos.y, TILE_STOPA, 0, m_Number);
	}
}

void CDoor::Destroy()
{
	if(m_Active)
	{
		for(int i = 0; i < m_Length - 1; i++)
		{
			vec2 CurrentPos = m_Pos + m_Direction * i;
			if(Collision()->CheckPoint(CurrentPos))
				break;
			else
				Collision()->SetDoorCollisionAt(CurrentPos.x, CurrentPos.y, TILE_AIR, 0, 0);
		}
	}
	delete this;
}

void CDoor::Read(const CLaserData *pData)
{
	// it's flipped in the laser object
	m_Pos = pData->m_To;
	m_To = pData->m_From;
}

bool CDoor::Match(const CDoor *pDoor) const
{
	return pDoor->m_Pos == m_Pos && pDoor->m_To == m_To && pDoor->m_Number == m_Number;
}
