/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H
#define GAME_CLIENT_PREDICTION_ENTITIES_PICKUP_H

#include <game/client/prediction/entity.h>

const int PickupPhysSize = 14;

class CPickup : public CEntity
{
public:
	virtual void Tick();

	CPickup(CGameWorld *pGameWorld, int ID, CNetObj_Pickup *pPickup);
	void FillInfo(CNetObj_Pickup *pPickup);
	bool Match(CPickup *pPickup);
	bool PredictMoving() { return m_IsCoreActive && PredictActive(); }

	// only predict pickups that have been seen for at least one second, to avoid predicting disabled switch layer
	bool PredictActive() { return GameWorld()->GameTick() - m_FirstSnapTick > 50; }

private:
	int m_Type;
	int m_Subtype;

	// DDRace

	void Move();
	vec2 m_Core;
	bool m_IsCoreActive;
	int m_FirstSnapTick;
};

#endif
