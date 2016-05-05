/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_WORLD_ENTITIES_PROJECTILE_H
#define GAME_CLIENT_WORLD_ENTITIES_PROJECTILE_H

#include <game/client/world/entity.h>
#include "character.h"

class CProjectile : public CEntity
{
	friend class CGameWorld;
public:
	CProjectile
	(
		CGameWorld *pGameWorld,
		int Type,
		int Owner,
		vec2 Pos,
		vec2 Dir,
		int Span,
		bool Freeeze,
		bool Explosive,
		float Force,
		int SoundImpact,
		int Weapon,
		int Layer = 0,
		int Number = 0
	);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	//virtual void Snap(int SnappingClient);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	//int m_Damage;
	int m_SoundImpact;
	int m_Weapon;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;

	// DDRace

	int m_Bouncing;
	bool m_Freeze;
	int m_TuneZone;

public:

	void SetBouncing(int Value);
	void FillExtraInfo(CNetObj_Projectile *pProj);

	const vec2 &GetDirection() { return m_Direction; }
	const int &GetOwner() { return m_Owner; }
	const int &GetStartTick() { return m_StartTick; }
	CProjectile(CGameWorld *pGameWorld, int ID, CNetObj_Projectile *pProj);
};

#endif
