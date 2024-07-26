/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/color.h>
#include <engine/demo.h>
#include <engine/graphics.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include "damageind.h"

CDamageInd::CDamageInd()
{
	m_NumItems = 0;
}

void CDamageInd::Create(vec2 Pos, vec2 Dir, float Alpha)
{
	if(m_NumItems >= MAX_ITEMS)
		return;

	CItem *pItem = &m_aItems[m_NumItems];
	pItem->m_Pos = Pos;
	pItem->m_Dir = -Dir;
	pItem->m_RemainingLife = 0.75f;
	pItem->m_StartAngle = -random_angle();
	pItem->m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, Alpha);
	++m_NumItems;
}

void CDamageInd::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static float s_LastLocalTime = LocalTime();
	float LifeAdjustment;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(pInfo->m_Paused)
			LifeAdjustment = 0.0f;
		else
			LifeAdjustment = (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
	}
	else
	{
		const auto &pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
		if(pGameInfoObj && pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
			LifeAdjustment = 0.0f;
		else
			LifeAdjustment = LocalTime() - s_LastLocalTime;
	}
	s_LastLocalTime = LocalTime();

	Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteStars[0]);
	for(int i = 0; i < m_NumItems;)
	{
		m_aItems[i].m_RemainingLife -= LifeAdjustment;
		if(m_aItems[i].m_RemainingLife < 0.0f)
		{
			--m_NumItems;
			m_aItems[i] = m_aItems[m_NumItems];
		}
		else
		{
			vec2 Pos = mix(m_aItems[i].m_Pos + m_aItems[i].m_Dir * 75.0f, m_aItems[i].m_Pos, clamp((m_aItems[i].m_RemainingLife - 0.60f) / 0.15f, 0.0f, 1.0f));
			const float LifeAlpha = m_aItems[i].m_RemainingLife / 0.1f;
			Graphics()->SetColor(m_aItems[i].m_Color.WithMultipliedAlpha(LifeAlpha));
			Graphics()->QuadsSetRotation(m_aItems[i].m_StartAngle + m_aItems[i].m_RemainingLife * 2.0f);
			Graphics()->RenderQuadContainerAsSprite(m_DmgIndQuadContainerIndex, 0, Pos.x, Pos.y);
			i++;
		}
	}

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
}

void CDamageInd::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_DmgIndQuadContainerIndex = Graphics()->CreateQuadContainer(false);
	float ScaleX, ScaleY;
	RenderTools()->GetSpriteScale(SPRITE_STAR1, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_DmgIndQuadContainerIndex, 48.f * ScaleX, 48.f * ScaleY);
	Graphics()->QuadContainerUpload(m_DmgIndQuadContainerIndex);
}

void CDamageInd::OnReset()
{
	m_NumItems = 0;
}
