/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PARTICLES_H
#define GAME_CLIENT_COMPONENTS_PARTICLES_H
#include <base/vmath.h>
#include <game/client/component.h>

// particles
struct CParticle
{
	void SetDefault()
	{
		m_Vel = vec2(0, 0);
		m_LifeSpan = 0;
		m_StartSize = 32;
		m_EndSize = 32;
		m_UseAlphaFading = false;
		m_StartAlpha = 1;
		m_EndAlpha = 1;
		m_Rot = 0;
		m_Rotspeed = 0;
		m_Gravity = 0;
		m_Friction = 0;
		m_FlowAffected = 1.0f;
		m_Color = ColorRGBA(1, 1, 1, 1);
		m_Collides = true;
	}

	vec2 m_Pos;
	vec2 m_Vel;

	int m_Spr = 0;

	float m_FlowAffected = 0;

	float m_LifeSpan = 0;

	float m_StartSize = 0;
	float m_EndSize = 0;

	bool m_UseAlphaFading = false;
	float m_StartAlpha = 0;
	float m_EndAlpha = 0;

	float m_Rot = 0;
	float m_Rotspeed = 0;

	float m_Gravity = 0;
	float m_Friction = 0;

	ColorRGBA m_Color;

	bool m_Collides = false;

	// set by the particle system
	float m_Life = 0;
	int m_PrevPart = 0;
	int m_NextPart = 0;
};

class CParticles : public CComponent
{
	friend class CGameClient;

public:
	enum
	{
		GROUP_PROJECTILE_TRAIL = 0,
		GROUP_EXPLOSIONS,
		GROUP_EXTRA,
		GROUP_GENERAL,
		NUM_GROUPS
	};

	CParticles();
	virtual int Sizeof() const override { return sizeof(*this); }

	void Add(int Group, CParticle *pPart, float TimePassed = 0.f);

	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnInit() override;

private:
	int m_ParticleQuadContainerIndex = 0;
	int m_ExtraParticleQuadContainerIndex = 0;

	enum
	{
		MAX_PARTICLES = 1024 * 8,
	};

	CParticle m_aParticles[MAX_PARTICLES];
	int m_FirstFree = 0;
	int m_aFirstPart[NUM_GROUPS] = {0};

	void RenderGroup(int Group);
	void Update(float TimePassed);

	template<int TGROUP>
	class CRenderGroup : public CComponent
	{
	public:
		CParticles *m_pParts = nullptr;
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual void OnRender() override { m_pParts->RenderGroup(TGROUP); }
	};

	CRenderGroup<GROUP_PROJECTILE_TRAIL> m_RenderTrail;
	CRenderGroup<GROUP_EXPLOSIONS> m_RenderExplosions;
	CRenderGroup<GROUP_EXTRA> m_RenderExtra;
	CRenderGroup<GROUP_GENERAL> m_RenderGeneral;

	bool ParticleIsVisibleOnScreen(const vec2 &CurPos, float CurSize);
};
#endif
