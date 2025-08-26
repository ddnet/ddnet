#ifndef GAME_SERVER_FOXNET_COSMETICS_LOVELY_H
#define GAME_SERVER_FOXNET_COSMETICS_LOVELY_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CLovely : public CEntity
{
	enum
	{
		MAX_HEARTS = 4
	};

	CClientMask m_TeamMask;
	int m_Owner;
	float m_SpawnDelay;

	struct SLovelyData
	{
		int m_ID;
		vec2 m_Pos;
		float m_Lifespan;
	};
	SLovelyData m_aLovelyData[MAX_HEARTS];
	void SpawnNewHeart();

public:
	CLovely(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_LOVELY_H
