// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_COSMETICS_FIREWORK_H
#define GAME_SERVER_FOXNET_COSMETICS_FIREWORK_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CFirework : public CEntity
{
	int m_Owner;

	int m_State;

	int64_t m_StartTick;

	enum
	{
		MAX_FIREWORKS = 25,

		STATE_NONE = 0,
		STATE_START,
		STATE_EXPLOSION,
	};
	float m_aLifetime[MAX_FIREWORKS];
	vec2 m_aPos[MAX_FIREWORKS];
	vec2 m_aVel[MAX_FIREWORKS];

	int m_Ids[MAX_FIREWORKS];

public:
	CFirework(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_FIREWORK_H
