#ifndef GAME_SERVER_FOXNET_COSMETICS_EPIC_CIRCLE_H
#define GAME_SERVER_FOXNET_COSMETICS_EPIC_CIRCLE_H

class CEpicCircle : public CEntity
{
	enum
	{
		MAX_PARTICLES = 9
	};

	CClientMask m_TeamMask;
	int m_Owner;
	int m_aIds[MAX_PARTICLES];
	vec2 m_RotatePos[MAX_PARTICLES];

public:
	CEpicCircle(CGameWorld *pGameWorld, int Owner, vec2 Pos);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void Snap(int SnappingClient) override;
};

#endif // GAME_SERVER_FOXNET_COSMETICS_EPIC_CIRCLE_H
