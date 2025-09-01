// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_ENTITIES_PORTAL_H
#define GAME_SERVER_FOXNET_ENTITIES_PORTAL_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

struct CPortalData
{
	bool m_Active;
	vec2 m_Pos;
};

class CPortal : public CEntity
{
	int m_Owner;

	CPortalData m_PortalData[2];

	int m_State;
	int m_Lifetime; // In ticks

	bool m_CanTeleport[MAX_CLIENTS];

	enum States
	{
		STATE_NONE = 0,
		STATE_FIRST_SET,
		STATE_BOTH_SET,
	};

	int m_aIds[26];
	int m_aParticeIds[6];

	void RemovePortals();
	bool TrySetPortal();

public:
	CPortal(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	void OnFire();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_PORTAL_H
