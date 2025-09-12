// Made by qxdFox
#ifndef GAME_SERVER_FOXNET_ENTITIES_PORTAL_H
#define GAME_SERVER_FOXNET_ENTITIES_PORTAL_H

#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/gameworld.h>

struct CPortalData
{
	float m_PortalRadius;
	bool m_Active;
	vec2 m_Pos;
	int m_Team;
};

class CPortal : public CEntity
{
	enum
	{
		NUM_PORTALS = 2,
		SEGMENTS = 12,
		NUM_IDS = SEGMENTS + 1,
		NUM_POS = SEGMENTS + 1,
		NUM_PRTCL = 3
	};

	struct SSnapPortal
	{
		int m_aIds[NUM_IDS];
		vec2 m_aFrom[NUM_POS];
		vec2 m_aTo[NUM_POS];
		int m_aParticleIds[NUM_PRTCL];
	};
	SSnapPortal m_Snap[NUM_PORTALS];

	vec2 CirclePos(int Portal, int Part) const;
	void SetPortalVisual();

	int m_Owner;

	CPortalData m_aData[NUM_PORTALS];

	int m_State;
	int m_Lifetime; // In ticks

	bool m_aCanTeleport[MAX_CLIENTS];

	CClientMask m_TeamMask;

	enum States
	{
		STATE_NONE = 0,
		STATE_FIRST_SET,
		STATE_BOTH_SET,
	};

	void RemovePortals();
	bool TrySetPortal();
	void HandleTele();

public:
	CPortal(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	void OnFire();

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_ENTITIES_PORTAL_H
