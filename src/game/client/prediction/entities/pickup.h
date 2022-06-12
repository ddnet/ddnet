/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H
#define GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H

#include <game/client/prediction/entity.h>

class CPickup : public CEntity
{
public:
	void Tick() override;

	CPickup(CGameWorld *pGameWorld, int ID, CNetObj_Pickup *pPickup, const CNetObj_EntityEx *pEntEx = 0);
	void FillInfo(CNetObj_Pickup *pPickup);
	bool Match(CPickup *pPickup);
	bool InDDNetTile() { return m_IsCoreActive; }

private:
	int m_Type;
	int m_Subtype;

	// DDRace

	void Move();
	vec2 m_Core;
	bool m_IsCoreActive;
};

#endif
