// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_COSMETICS_HEARTHAT_H
#define GAME_SERVER_FOXNET_COSMETICS_HEARTHAT_H

#include <game/server/gameworld.h>
#include <game/server/entity.h>

#include <base/vmath.h>

class CHeartHat : public CEntity
{
	enum
	{
		HEART_BACK = 0,
		HEART_MIDDLE = 1,
		HEART_FRONT = 2,
		NUM_HEARTS = 3
	};

	int m_Owner;
	int m_Ids[NUM_HEARTS];
	float m_Dist[NUM_HEARTS];
	bool m_switch;
	vec2 m_aPos[NUM_HEARTS];

public:
	CHeartHat(CGameWorld *pGameWorld, int Owner);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_HEARTHAT_H
