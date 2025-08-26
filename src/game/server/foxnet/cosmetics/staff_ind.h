#ifndef GAME_SERVER_FOXNET_COSMETICS_STAFF_IND_H
#define GAME_SERVER_FOXNET_COSMETICS_STAFF_IND_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CStaffInd : public CEntity
{
	enum
	{
		BALL,
		ARMOR,
		BALL_FRONT,
		NUM_IDS
	};

	int m_aIds[NUM_IDS];
	vec2 m_aPos[2];

	CClientMask m_TeamMask;
	int m_Owner;
	float m_Dist;
	bool m_BallFirst;

public:
	CStaffInd(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_STAFF_IND_H
