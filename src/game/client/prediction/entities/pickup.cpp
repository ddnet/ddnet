/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"

#include "character.h"

#include <generated/protocol.h>

#include <game/client/pickup_data.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <game/physics/pickup.h>

static constexpr int PICKUP_PHYSICS_RADIUS = 14;

void CPickup::Tick()
{
	CPickupPhysics<CPickup, CCharacter>::Tick(this);
}

void CPickup::CreatePickupSound(int Sound, CCharacter *pChr)
{
	GameWorld()->CreatePredictedSound(m_Pos, Sound, pChr->GetCid());
}

CPickup::CPickup(CGameWorld *pGameWorld, int Id, const CPickupData *pPickup) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, vec2(0, 0), PICKUP_PHYSICS_RADIUS)
{
	m_Pos = pPickup->m_Pos;
	m_Type = pPickup->m_Type;
	m_Subtype = pPickup->m_Subtype;
	m_Core = vec2(0.f, 0.f);
	m_IsCoreActive = false;
	m_Id = Id;
	m_Number = pPickup->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;
	m_Flags = pPickup->m_Flags;
}

void CPickup::FillInfo(CNetObj_Pickup *pPickup)
{
	pPickup->m_X = (int)m_Pos.x;
	pPickup->m_Y = (int)m_Pos.y;
	pPickup->m_Type = m_Type;
	pPickup->m_Subtype = m_Subtype;
}

bool CPickup::Match(CPickup *pPickup)
{
	if(pPickup->m_Type != m_Type || pPickup->m_Subtype != m_Subtype)
		return false;
	if(distance(pPickup->m_Pos, m_Pos) > 2.0f)
		return false;
	return true;
}
