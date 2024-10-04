/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/demo.h>

#include <engine/shared/config.h>

#include <game/generated/client_data.h>

#include <game/client/components/damageind.h>
#include <game/client/components/flow.h>
#include <game/client/components/particles.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

#include "effects.h"

CEffects::CEffects()
{
	m_Add5hz = false;
	m_Add50hz = false;
	m_Add100hz = false;
}

void CEffects::AirJump(vec2 Pos, float Alpha)
{
	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_AIRJUMP;
	p.m_Pos = Pos + vec2(-6.0f, 16.0f);
	p.m_Vel = vec2(0, -200);
	p.m_LifeSpan = 0.5f;
	p.m_StartSize = 48.0f;
	p.m_EndSize = 0;
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi * 2;
	p.m_Gravity = 500;
	p.m_Friction = 0.7f;
	p.m_FlowAffected = 0.0f;
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);

	p.m_Pos = Pos + vec2(6.0f, 16.0f);
	m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);

	if(g_Config.m_SndGame)
		m_pClient->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_AIRJUMP, 1.0f, Pos);
}

void CEffects::DamageIndicator(vec2 Pos, vec2 Dir, float Alpha)
{
	m_pClient->m_DamageInd.Create(Pos, Dir, Alpha);
}

void CEffects::PowerupShine(vec2 Pos, vec2 Size, float Alpha)
{
	if(!m_Add50hz)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SLICE;
	p.m_Pos = Pos + vec2(random_float(-0.5f, 0.5f), random_float(-0.5f, 0.5f)) * Size;
	p.m_Vel = vec2(0, 0);
	p.m_LifeSpan = 0.5f;
	p.m_StartSize = 16.0f;
	p.m_EndSize = 0;
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi * 2;
	p.m_Gravity = 500;
	p.m_Friction = 0.9f;
	p.m_FlowAffected = 0.0f;
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
}

void CEffects::FreezingFlakes(vec2 Pos, vec2 Size, float Alpha)
{
	if(!m_Add5hz)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SNOWFLAKE;
	p.m_Pos = Pos + vec2(random_float(-0.5f, 0.5f), random_float(-0.5f, 0.5f)) * Size;
	p.m_Vel = vec2(0, 0);
	p.m_LifeSpan = 1.5f;
	p.m_StartSize = random_float(0.5f, 1.5f) * 16.0f;
	p.m_EndSize = p.m_StartSize * 0.5f;
	p.m_UseAlphaFading = true;
	p.m_StartAlpha = 1.0f;
	p.m_EndAlpha = 0.0f;
	p.m_Rot = random_angle();
	p.m_Rotspeed = pi;
	p.m_Gravity = random_float(250.0f);
	p.m_Friction = 0.9f;
	p.m_FlowAffected = 0.0f;
	p.m_Collides = false;
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_EXTRA, &p);
}

void CEffects::SparkleTrail(vec2 Pos, float Alpha)
{
	if(!m_Add50hz)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SPARKLE;
	p.m_Pos = Pos + random_direction() * random_float(40.0f);
	p.m_Vel = vec2(0, 0);
	p.m_LifeSpan = 0.5f;
	p.m_StartSize = 0.0f;
	p.m_EndSize = random_float(20.0f, 30.0f);
	p.m_UseAlphaFading = true;
	p.m_StartAlpha = Alpha;
	p.m_EndAlpha = std::min(0.2f, Alpha);
	p.m_Collides = false;
	m_pClient->m_Particles.Add(CParticles::GROUP_TRAIL_EXTRA, &p);
}

void CEffects::SmokeTrail(vec2 Pos, vec2 Vel, float Alpha, float TimePassed)
{
	if(!m_Add50hz && TimePassed < 0.001f)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SMOKE;
	p.m_Pos = Pos;
	p.m_Vel = Vel + random_direction() * 50.0f;
	p.m_LifeSpan = random_float(0.5f, 1.0f);
	p.m_StartSize = random_float(12.0f, 20.0f);
	p.m_EndSize = 0;
	p.m_Friction = 0.7f;
	p.m_Gravity = random_float(-500.0f);
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, TimePassed);
}

void CEffects::SkidTrail(vec2 Pos, vec2 Vel, float Alpha)
{
	if(!m_Add100hz)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_SMOKE;
	p.m_Pos = Pos;
	p.m_Vel = Vel + random_direction() * 50.0f;
	p.m_LifeSpan = random_float(0.5f, 1.0f);
	p.m_StartSize = random_float(24.0f, 36.0f);
	p.m_EndSize = 0;
	p.m_Friction = 0.7f;
	p.m_Gravity = random_float(-500.0f);
	p.m_Color = ColorRGBA(0.75f, 0.75f, 0.75f, Alpha);
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
}

void CEffects::BulletTrail(vec2 Pos, float Alpha, float TimePassed)
{
	if(!m_Add100hz && TimePassed < 0.001f)
		return;

	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_BALL;
	p.m_Pos = Pos;
	p.m_LifeSpan = random_float(0.25f, 0.5f);
	p.m_StartSize = 8.0f;
	p.m_EndSize = 0;
	p.m_Friction = 0.7f;
	p.m_Color.a *= Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_PROJECTILE_TRAIL, &p, TimePassed);
}

void CEffects::PlayerSpawn(vec2 Pos, float Alpha)
{
	for(int i = 0; i < 32; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SHELL;
		p.m_Pos = Pos;
		p.m_Vel = random_direction() * (std::pow(random_float(), 3) * 600.0f);
		p.m_LifeSpan = random_float(0.3f, 0.6f);
		p.m_StartSize = random_float(64.0f, 96.0f);
		p.m_EndSize = 0;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float();
		p.m_Gravity = random_float(-400.0f);
		p.m_Friction = 0.7f;
		p.m_Color = ColorRGBA(0xb5 / 255.0f, 0x50 / 255.0f, 0xcb / 255.0f, Alpha);
		p.m_StartAlpha = Alpha;
		m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
	if(g_Config.m_SndGame)
		m_pClient->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SPAWN, 1.0f, Pos);
}

void CEffects::PlayerDeath(vec2 Pos, int ClientId, float Alpha)
{
	ColorRGBA BloodColor(1.0f, 1.0f, 1.0f);

	if(ClientId >= 0)
	{
		// Use m_RenderInfo.m_CustomColoredSkin instead of m_UseCustomColor
		// m_UseCustomColor says if the player's skin has a custom color (value sent from the client side)

		// m_RenderInfo.m_CustomColoredSkin Defines if in the context of the game the color is being customized,
		// Using this value if the game is teams (red and blue), this value will be true even if the skin is with the normal color.
		// And will use the team body color to create player death effect instead of tee color
		if(m_pClient->m_aClients[ClientId].m_RenderInfo.m_CustomColoredSkin)
			BloodColor = m_pClient->m_aClients[ClientId].m_RenderInfo.m_ColorBody;
		else
		{
			BloodColor = m_pClient->m_aClients[ClientId].m_RenderInfo.m_BloodColor;
		}
	}

	for(int i = 0; i < 64; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SPLAT01 + (rand() % 3);
		p.m_Pos = Pos;
		p.m_Vel = random_direction() * (random_float(0.1f, 1.1f) * 900.0f);
		p.m_LifeSpan = random_float(0.3f, 0.6f);
		p.m_StartSize = random_float(24.0f, 40.0f);
		p.m_EndSize = 0;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
		p.m_Gravity = 800.0f;
		p.m_Friction = 0.8f;
		ColorRGBA c = BloodColor.v4() * random_float(0.75f, 1.0f);
		p.m_Color = ColorRGBA(c.r, c.g, c.b, 0.75f * Alpha);
		p.m_StartAlpha = Alpha;
		m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
}

void CEffects::Confetti(vec2 Pos, float Alpha)
{
	ColorRGBA Red(1.0f, 0.4f, 0.4f);
	ColorRGBA Green(0.4f, 1.0f, 0.4f);
	ColorRGBA Blue(0.4f, 0.4f, 1.0f);
	ColorRGBA Yellow(1.0f, 1.0f, 0.4f);
	ColorRGBA Cyan(0.4f, 1.0f, 1.0f);
	ColorRGBA Magenta(1.0f, 0.4f, 1.0f);

	ColorRGBA aConfettiColors[] = {Red, Green, Blue, Yellow, Cyan, Magenta};

	// powerful confettis
	for(int i = 0; i < 32; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SPLAT01 + (rand() % 3);
		p.m_Pos = Pos;
		p.m_Vel = direction(-0.5f * pi + random_float(-0.2f, 0.2f)) * random_float(0.01f, 1.0f) * 2000.0f;
		p.m_LifeSpan = random_float(1.0f, 1.2f);
		p.m_StartSize = random_float(12.0f, 24.0f);
		p.m_EndSize = 0;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
		p.m_Gravity = -700.0f;
		p.m_Friction = 0.6f;
		ColorRGBA c = aConfettiColors[(rand() % std::size(aConfettiColors))];
		p.m_Color = c.WithMultipliedAlpha(0.75f * Alpha);
		p.m_StartAlpha = Alpha;
		m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}

	// broader confettis
	for(int i = 0; i < 32; i++)
	{
		CParticle p;
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SPLAT01 + (rand() % 3);
		p.m_Pos = Pos;
		p.m_Vel = direction(-0.5f * pi + random_float(-0.8f, 0.8f)) * random_float(0.01f, 1.0f) * 1500.0f;
		p.m_LifeSpan = random_float(0.8f, 1.0f);
		p.m_StartSize = random_float(12.0f, 24.0f);
		p.m_EndSize = 0;
		p.m_Rot = random_angle();
		p.m_Rotspeed = random_float(-0.5f, 0.5f) * pi;
		p.m_Gravity = -700.0f;
		p.m_Friction = 0.6f;
		ColorRGBA c = aConfettiColors[(rand() % std::size(aConfettiColors))];
		p.m_Color = c.WithMultipliedAlpha(0.75f * Alpha);
		p.m_StartAlpha = Alpha;
		m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
}

void CEffects::Explosion(vec2 Pos, float Alpha)
{
	// add to flow
	for(int y = -8; y <= 8; y++)
		for(int x = -8; x <= 8; x++)
		{
			if(x == 0 && y == 0)
				continue;

			float a = 1 - (length(vec2(x, y)) / length(vec2(8, 8)));
			m_pClient->m_Flow.Add(Pos + vec2(x, y) * 16, normalize(vec2(x, y)) * 5000.0f * a, 10.0f);
		}

	// add the explosion
	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_EXPL01;
	p.m_Pos = Pos;
	p.m_LifeSpan = 0.4f;
	p.m_StartSize = 150.0f;
	p.m_EndSize = 0;
	p.m_Rot = random_angle();
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_EXPLOSIONS, &p);

	// Nudge position slightly to edge of closest tile so the
	// smoke doesn't get stuck inside the tile.
	if(Collision()->CheckPoint(Pos))
	{
		const vec2 DistanceToTopLeft = Pos - vec2(round_truncate(Pos.x / 32), round_truncate(Pos.y / 32)) * 32;

		vec2 CheckOffset;
		CheckOffset.x = (DistanceToTopLeft.x > 16 ? 32 : -1);
		CheckOffset.y = (DistanceToTopLeft.y > 16 ? 32 : -1);
		CheckOffset -= DistanceToTopLeft;

		for(vec2 Mask : {vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f)})
		{
			const vec2 NewPos = Pos + CheckOffset * Mask;
			if(!Collision()->CheckPoint(NewPos))
			{
				Pos = NewPos;
				break;
			}
		}
	}

	// add the smoke
	for(int i = 0; i < 24; i++)
	{
		p.SetDefault();
		p.m_Spr = SPRITE_PART_SMOKE;
		p.m_Pos = Pos;
		p.m_Vel = random_direction() * (random_float(1.0f, 1.2f) * 1000.0f);
		p.m_LifeSpan = random_float(0.5f, 0.9f);
		p.m_StartSize = random_float(32.0f, 40.0f);
		p.m_EndSize = 0;
		p.m_Gravity = random_float(-800.0f);
		p.m_Friction = 0.4f;
		p.m_Color = mix(vec4(0.75f, 0.75f, 0.75f, 1.0f), vec4(0.5f, 0.5f, 0.5f, 1.0f), random_float());
		p.m_Color.a *= Alpha;
		p.m_StartAlpha = p.m_Color.a;
		m_pClient->m_Particles.Add(CParticles::GROUP_GENERAL, &p);
	}
}

void CEffects::HammerHit(vec2 Pos, float Alpha)
{
	// add the explosion
	CParticle p;
	p.SetDefault();
	p.m_Spr = SPRITE_PART_HIT01;
	p.m_Pos = Pos;
	p.m_LifeSpan = 0.3f;
	p.m_StartSize = 120.0f;
	p.m_EndSize = 0;
	p.m_Rot = random_angle();
	p.m_Color.a = Alpha;
	p.m_StartAlpha = Alpha;
	m_pClient->m_Particles.Add(CParticles::GROUP_EXPLOSIONS, &p);
	if(g_Config.m_SndGame)
		m_pClient->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_HAMMER_HIT, 1.0f, Pos);
}

void CEffects::OnRender()
{
	static int64_t s_LastUpdate100hz = 0;
	static int64_t s_LastUpdate50hz = 0;
	static int64_t s_LastUpdate5hz = 0;

	float Speed = 1.0f;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		Speed = DemoPlayer()->BaseInfo()->m_Speed;

	if(time() - s_LastUpdate100hz > time_freq() / (100 * Speed))
	{
		m_Add100hz = true;
		s_LastUpdate100hz = time();
	}
	else
		m_Add100hz = false;

	if(time() - s_LastUpdate50hz > time_freq() / (50 * Speed))
	{
		m_Add50hz = true;
		s_LastUpdate50hz = time();
	}
	else
		m_Add50hz = false;

	if(time() - s_LastUpdate5hz > time_freq() / (5 * Speed))
	{
		m_Add5hz = true;
		s_LastUpdate5hz = time();
	}
	else
		m_Add5hz = false;

	if(m_Add50hz)
		m_pClient->m_Flow.Update();
}
