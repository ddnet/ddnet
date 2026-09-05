/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "plasma.h"

#include "character.h"

#include <game/client/laser_data.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <game/physics/plasma.h>

CPlasma::CPlasma(CGameWorld *pGameWorld, int Id, const CLaserData *pData) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PLASMA)
{
	m_Id = Id;

	m_Number = pData->m_SwitchNumber;
	m_Layer = m_Number > 0 ? LAYER_SWITCH : LAYER_GAME;
	m_LifeTime = (int)(GameWorld()->GameTickSpeed() * 1.5f);

	m_Explosive = false;
	m_Freeze = false;

	Read(pData);

	CCharacter *pTarget = GameWorld()->GetCharacterById(m_ForClientId);
	if(!pTarget)
	{
		Reset();
		return;
	}
	m_Core = normalize(pTarget->m_Pos - m_Pos);
}

bool CPlasma::Match(const CPlasma *pPlasma) const
{
	return pPlasma->m_EvalTick == m_EvalTick && pPlasma->m_Number == m_Number &&
	       pPlasma->m_Explosive == m_Explosive && pPlasma->m_Freeze == m_Freeze && pPlasma->m_ForClientId == m_ForClientId;
}

void CPlasma::Read(const CLaserData *pData)
{
	m_Pos = pData->m_From;
	m_EvalTick = pData->m_StartTick;
	m_ForClientId = pData->m_Owner;

	if(0 <= pData->m_Subtype && pData->m_Subtype < NUM_LASERGUNTYPES)
	{
		m_Explosive = (pData->m_Subtype & 1);
		m_Freeze = (pData->m_Subtype & 2);
	}
}

void CPlasma::Reset()
{
	m_MarkedForDestroy = true;
}

void CPlasma::Tick()
{
	CPlasmaPhysics<CPlasma, CCharacter>::Tick(this);
}

void CPlasma::Explode(CCharacter *pTarget)
{
	GameWorld()->CreateExplosion(
		m_Pos, m_ForClientId, WEAPON_GRENADE, true, pTarget->Team(), CClientMask().set(), m_ForClientId);
}
