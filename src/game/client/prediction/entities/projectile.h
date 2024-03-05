/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_PREDICTION_ENTITIES_PROJECTILE_H
#define GAME_CLIENT_PREDICTION_ENTITIES_PROJECTILE_H

#include <game/client/prediction/entity.h>

class CProjectileData;

class CProjectile : public CEntity
{
	friend class CGameWorld;
	friend class CItems;

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
		int SoundImpact,
		int Layer = 0,
		int Number = 0);

	vec2 GetPos(float Time);
	CProjectileData GetData() const;

	void Tick() override;

	bool Match(CProjectile *pProj);
	void SetBouncing(int Value);

	const vec2 &GetDirection() { return m_Direction; }
	const int &GetOwner() { return m_Owner; }
	const int &GetStartTick() { return m_StartTick; }
	CProjectile(CGameWorld *pGameWorld, int Id, const CProjectileData *pProj);

private:
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	int m_SoundImpact;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;

	// DDRace

	int m_Bouncing;
	bool m_Freeze;
	int m_TuneZone;
};

#endif
