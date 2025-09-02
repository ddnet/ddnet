// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_COSMETICS_DOT_TRAIL_H
#define GAME_SERVER_FOXNET_COSMETICS_DOT_TRAIL_H

#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <base/vmath.h>

class CDotTrail : public CEntity
{
	int m_Owner;

public:
	CDotTrail(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_DOT_TRAIL_H
