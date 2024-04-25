/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H
#define GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H

#include <game/client/prediction/entity.h>

class CPickupData;

class CPickup : public CEntity
{
public:
	static const int ms_CollisionExtraSize = 6;

	void Tick() override;

	CPickup(CGameWorld *pGameWorld, int Id, const CPickupData *pPickup);
	void FillInfo(CNetObj_Pickup *pPickup);
	bool Match(CPickup *pPickup);
	bool InDDNetTile() { return m_IsCoreActive; }

	int Type() const { return m_Type; }
	int Subtype() const { return m_Subtype; }

private:
	int m_Type;
	int m_Subtype;

	// DDRace

	void Move();
	vec2 m_Core;
	bool m_IsCoreActive;
};

#endif
