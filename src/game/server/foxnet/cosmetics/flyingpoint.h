#ifndef GAME_SERVER_FOXNET_COSMETICS_FLYINGPOINT_H
#define GAME_SERVER_FOXNET_COSMETICS_FLYINGPOINT_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CFlyingPoint : public CEntity
{
private:
	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_Owner;

	vec2 m_PrevPos;

	// Either a to clientid is set or a to position
	int m_To;
	vec2 m_ToPos;

public:
	CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos = vec2(-1, -1));

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_FLYINGPOINT_H