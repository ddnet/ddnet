/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_EFFECTS_H

#include <base/vmath.h>

#include <game/client/component.h>

class CEffects : public CComponent
{
	bool m_Add5hz;
	bool m_Add50hz;
	bool m_Add100hz;

public:
	CEffects();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnRender() override;

	void BulletTrail(vec2 Pos, float Alpha = 1.f, float TimePassed = 0.f);
	void SmokeTrail(vec2 Pos, vec2 Vel, float Alpha = 1.f, float TimePassed = 0.f);
	void SkidTrail(vec2 Pos, vec2 Vel, float Alpha = 1.0f);
	void Explosion(vec2 Pos, float Alpha = 1.0f);
	void HammerHit(vec2 Pos, float Alpha = 1.0f);
	void AirJump(vec2 Pos, float Alpha = 1.0f);
	void DamageIndicator(vec2 Pos, vec2 Dir, float Alpha = 1.0f);
	void PlayerSpawn(vec2 Pos, float Alpha = 1.0f);
	void PlayerDeath(vec2 Pos, int ClientId, float Alpha = 1.0f);
	void PowerupShine(vec2 Pos, vec2 Size, float Alpha = 1.0f);
	void FreezingFlakes(vec2 Pos, vec2 Size, float Alpha = 1.0f);
	void SparkleTrail(vec2 Pos, float Alpha = 1.0f);
	void Confetti(vec2 Pos, float Alpha = 1.0f);

	void Update();
};
#endif
