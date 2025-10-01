// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_ENTITIES_POWERUP_H
#define GAME_SERVER_FOXNET_ENTITIES_POWERUP_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

enum class EPowerUp
{
	INVALID = 0,
	XP,
	MONEY,
	NUM_TYPES
};

struct SPowerupData
{
	EPowerUp m_Type = EPowerUp::INVALID;
	int m_Value = 0;
};

class CPowerUp : public CEntity
{
	enum
	{
		NUM_LASERS = 5
	};

	struct SSnap
	{
		int m_aLaserIds[NUM_LASERS];
		vec2 m_aTo[NUM_LASERS];
		vec2 m_aFrom[NUM_LASERS];
	};
	SSnap m_Snap;

	int m_Lifetime;

	bool m_Switch;

	CClientMask m_TeamMask;

	void SetPowerupVisual();

	SPowerupData m_Data;

	void SetData();

public:
	CPowerUp(CGameWorld *pGameWorld, vec2 Pos, EPowerUp Type);

	void OnFire();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_PORTAL_H
