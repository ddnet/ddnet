/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "damageind.h"

#include <base/color.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>

#include <generated/client_data.h>

#include <game/client/gameclient.h>

#include <algorithm>

void CDamageInd::Create(vec2 Pos, vec2 Dir, float Alpha)
{
	if(m_vItems.size() >= MAX_ITEMS)
		return;

	m_vItems.emplace_back(Pos, -Dir, 0.75f, -random_angle(), ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
}

void CDamageInd::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static float s_LastLocalTime = LocalTime();
	const float LifeAdjustment = (LocalTime() - s_LastLocalTime) * GameClient()->GetAnimationPlaybackSpeed();
	s_LastLocalTime = LocalTime();

	Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteStars[0]);

	// Get screen edges to avoid rendering offscreen
	CScreenRect ScreenRect = Graphics()->GetScreen();

	// Expand screen slightly so items don't randomly pop in
	ScreenRect.Expand(m_ExpandSize);

	for(CItem &Item : m_vItems)
		Item.m_RemainingLife -= LifeAdjustment;

	m_vItems.erase(std::remove_if(m_vItems.begin(), m_vItems.end(),
			       [](const CItem &Item) { return Item.m_RemainingLife < 0.0f; }),
		m_vItems.end());

	for(CItem &Item : m_vItems)
	{
		vec2 Pos = mix(Item.m_Pos + Item.m_Dir * 75.0f, Item.m_Pos, std::clamp((Item.m_RemainingLife - 0.60f) / 0.15f, 0.0f, 1.0f));
		if(!ScreenRect.Inside(Pos))
			continue;

		const float LifeAlpha = Item.m_RemainingLife / 0.1f;
		Graphics()->SetColor(Item.m_Color.WithMultipliedAlpha(LifeAlpha));
		Graphics()->QuadsSetRotation(Item.m_StartAngle + Item.m_RemainingLife * 2.0f);
		Graphics()->RenderQuadContainerAsSprite(m_DamageIndQuadContainerIndex, 0, Pos.x, Pos.y);
	}

	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CDamageInd::OnInit()
{
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	m_DamageIndQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	float ScaleX, ScaleY;
	Graphics()->GetSpriteScale(SPRITE_STAR1, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->QuadContainerAddSprite(m_DamageIndQuadContainerIndex, 48.0f * ScaleX, 48.0f * ScaleY);
	Graphics()->QuadContainerUpload(m_DamageIndQuadContainerIndex);

	m_ExpandSize = std::max(ScaleX, ScaleY) * 24.0f;
}

void CDamageInd::OnReset()
{
	m_vItems.clear();
}
