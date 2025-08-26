// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_COSMETICS_HEAD_POWERUP_H
#define GAME_SERVER_FOXNET_COSMETICS_HEAD_POWERUP_H

#include <base/vmath.h>
#include <engine/shared/protocol.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

class CHeadItem : public CEntity
{
	CClientMask m_TeamMask;
	int m_Owner;

	int m_Type;
	float m_Offset;

public:
	CHeadItem(CGameWorld *pGameWorld, int Owner, vec2 Pos, int Type, float Offset = 78.0f);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_HEAD_POWERUP_H
