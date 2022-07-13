/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

#include <game/server/entity.h>

class CProjectile : public CEntity
{
public:
	CProjectile(
		CGameWorld *pGameWorld,
		int Type,
		int Owner,
		vec2 Pos,
		vec2 Dir,
		int Span,
		bool Freeze,
		bool Explosive,
		float Force,
		int SoundImpact,
		int Layer = 0,
		int Number = 0);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void TickPaused() override;
	virtual void Snap(int SnappingClient) override;
	virtual void SwapClients(int Client1, int Client2) override;

private:
	vec2 m_Direction;
	int m_LifeSpan = 0;
	int m_Owner = 0;
	int m_Type = 0;
	//int m_Damage = 0;
	int m_SoundImpact = 0;
	float m_Force = 0;
	int m_StartTick = 0;
	bool m_Explosive = false;

	// DDRace

	int m_Bouncing = 0;
	bool m_Freeze = false;
	int m_TuneZone = 0;
	bool m_BelongsToPracticeTeam = false;

public:
	void SetBouncing(int Value);
	bool FillExtraInfo(CNetObj_DDNetProjectile *pProj);
};

#endif
