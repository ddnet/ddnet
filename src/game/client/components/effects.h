/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_EFFECTS_H

#include <base/vmath.h>

#include <game/client/component.h>

class CEffects : public CComponent
{
private:
	bool m_Add5hz;
	int64_t m_LastUpdate5hz = 0;

	bool m_Add50hz;
	int64_t m_LastUpdate50hz = 0;

	bool m_Add100hz;
	int64_t m_LastUpdate100hz = 0;

	int64_t m_SkidSoundTimer = 0;

public:
	CEffects();
	int Sizeof() const override { return sizeof(*this); }

	void OnRender() override;

	void BulletTrail(vec2 Pos, float Alpha, float TimePassed);
	void SmokeTrail(vec2 Pos, vec2 Vel, float Alpha, float TimePassed);
	void SkidTrail(vec2 Pos, vec2 Vel, int Direction, float Alpha);
	void Explosion(vec2 Pos, float Alpha);
	void HammerHit(vec2 Pos, float Alpha);
	void AirJump(vec2 Pos, float Alpha);
	void DamageIndicator(vec2 Pos, vec2 Dir, float Alpha);
	void PlayerSpawn(vec2 Pos, float Alpha);
	void PlayerDeath(vec2 Pos, int ClientId, float Alpha);
	void PowerupShine(vec2 Pos, vec2 Size, float Alpha);
	void FreezingFlakes(vec2 Pos, vec2 Size, float Alpha);
	void SparkleTrail(vec2 Pos, float Alpha);
	void Confetti(vec2 Pos, float Alpha);

	void Update();
};
#endif
