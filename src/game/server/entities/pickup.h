/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H

#include <game/server/entity.h>

class CPickup : public CEntity
{
public:
	static const int ms_CollisionExtraSize = 6;

	CPickup(CGameWorld *pGameWorld, int Type, int SubType = 0, int Layer = 0, int Number = 0);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	int Type() const { return m_Type; }
	int Subtype() const { return m_Subtype; }

private:
	int m_Type;
	int m_Subtype;

	// DDRace

	void Move();
	vec2 m_Core;
};

#endif
