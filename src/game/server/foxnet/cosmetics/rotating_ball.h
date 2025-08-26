#ifndef GAME_SERVER_FOXNET_COSMETICS_ROTATING_BALL_H
#define GAME_SERVER_FOXNET_COSMETICS_ROTATING_BALL_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CRotatingBall : public CEntity
{
	int m_Owner;

	int m_Id1;

	int m_RotateDelay;
	int m_LaserDirAngle;
	int m_LaserInputDir;
	bool m_IsRotating;

	vec2 m_LaserPos;
	vec2 m_ProjPos;

	int m_TableDirV[2][2];

public:
	CRotatingBall(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_ROTATING_BALL_H
