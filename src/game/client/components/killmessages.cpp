/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <game/generated/protocol.h>
#include <game/generated/client_data.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include "killmessages.h"

void CKillMessages::OnWindowResize()
{
	for(int i = 0; i < MAX_KILLMSGS; i++)
	{
		if(m_aKillmsgs[i].m_VictimTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aKillmsgs[i].m_VictimTextContainerIndex);
		if(m_aKillmsgs[i].m_KillerTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aKillmsgs[i].m_KillerTextContainerIndex);
		m_aKillmsgs[i].m_VictimTextContainerIndex = m_aKillmsgs[i].m_KillerTextContainerIndex = -1;
	}
}

void CKillMessages::OnReset()
{
	m_KillmsgCurrent = 0;
	for (int i = 0; i < MAX_KILLMSGS; i++)
	{
		m_aKillmsgs[i].m_Tick = -100000;

		if(m_aKillmsgs[i].m_VictimTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aKillmsgs[i].m_VictimTextContainerIndex);

		if(m_aKillmsgs[i].m_KillerTextContainerIndex != -1)
			TextRender()->DeleteTextContainer(m_aKillmsgs[i].m_KillerTextContainerIndex);


		m_aKillmsgs[i].m_VictimTextContainerIndex = m_aKillmsgs[i].m_KillerTextContainerIndex = -1;
	}
}

void CKillMessages::OnInit()
{
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	m_SpriteQuadContainerIndex = Graphics()->CreateQuadContainer();

	RenderTools()->SelectSprite(SPRITE_FLAG_RED);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	RenderTools()->SelectSprite(SPRITE_FLAG_BLUE);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	RenderTools()->SelectSprite(SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	RenderTools()->SelectSprite(SPRITE_FLAG_BLUE, SPRITE_FLAG_FLIP_X);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody);
		RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 96.f);
	}
}

void CKillMessages::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		// unpack messages
		CKillMsg Kill;
		Kill.m_VictimID = pMsg->m_Victim;
		Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimID].m_Team;
		Kill.m_VictimDDTeam = m_pClient->m_Teams.Team(Kill.m_VictimID);
		str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_VictimID].m_aName, sizeof(Kill.m_aVictimName));
		Kill.m_VictimRenderInfo = m_pClient->m_aClients[Kill.m_VictimID].m_RenderInfo;
		Kill.m_KillerID = pMsg->m_Killer;
		Kill.m_KillerTeam = m_pClient->m_aClients[Kill.m_KillerID].m_Team;
		str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerID].m_aName, sizeof(Kill.m_aKillerName));
		Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerID].m_RenderInfo;
		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_Tick = Client()->GameTick();

		Kill.m_VitctimTextWidth = Kill.m_KillerTextWidth = 0.f;

		float Width = 400 * 3.0f*Graphics()->ScreenAspect();
		float Height = 400 * 3.0f;

		Graphics()->MapScreen(0, 0, Width*1.5f, Height*1.5f);

		float FontSize = 36.0f;
		if(Kill.m_aVictimName[0] != 0)
		{
			Kill.m_VitctimTextWidth = TextRender()->TextWidth(0, FontSize, Kill.m_aVictimName, -1);

			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = -1;

			Kill.m_VictimTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, Kill.m_aVictimName);
		}

		if(Kill.m_aKillerName[0] != 0)
		{
			Kill.m_KillerTextWidth = TextRender()->TextWidth(0, FontSize, Kill.m_aKillerName, -1);

			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
			Cursor.m_LineWidth = -1;

			Kill.m_KillerTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, Kill.m_aKillerName);
		}

		// add the message
		m_KillmsgCurrent = (m_KillmsgCurrent+1)%MAX_KILLMSGS;

		if(m_aKillmsgs[m_KillmsgCurrent].m_VictimTextContainerIndex != -1)
		{
			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_VictimTextContainerIndex);
			m_aKillmsgs[m_KillmsgCurrent].m_VictimTextContainerIndex = -1;
		}

		if(m_aKillmsgs[m_KillmsgCurrent].m_KillerTextContainerIndex != -1)
		{
			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_KillerTextContainerIndex);
			m_aKillmsgs[m_KillmsgCurrent].m_KillerTextContainerIndex = -1;
		}

		m_aKillmsgs[m_KillmsgCurrent] = Kill;
	}
}

void CKillMessages::OnRender()
{
	if(!g_Config.m_ClShowKillMessages)
		return;

	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;

	Graphics()->MapScreen(0, 0, Width*1.5f, Height*1.5f);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	float StartX = Width*1.5f-10.0f;
	float y = 30.0f + 100.0f * ((g_Config.m_ClShowfps ? 1 : 0) + g_Config.m_ClShowpred);

	for(int i = 1; i <= MAX_KILLMSGS; i++)
	{
		int r = (m_KillmsgCurrent+i)%MAX_KILLMSGS;
		if(Client()->GameTick() > m_aKillmsgs[r].m_Tick+50*10)
			continue;

		float x = StartX;

		STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
		STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);

		// render victim name
		x -= m_aKillmsgs[r].m_VitctimTextWidth;
		if(m_aKillmsgs[r].m_VictimID >= 0 && g_Config.m_ClChatTeamColors && m_aKillmsgs[r].m_VictimDDTeam)
		{
			vec3 rgb = HslToRgb(vec3(m_aKillmsgs[r].m_VictimDDTeam / 64.0f, 1.0f, 0.75f));
			TColor.Set(rgb.r, rgb.g, rgb.b, 1.0);
		}

		if(m_aKillmsgs[r].m_VictimTextContainerIndex != -1)
			TextRender()->RenderTextContainer(m_aKillmsgs[r].m_VictimTextContainerIndex, &TColor, &TOutlineColor, x, y + (46.f - 36.f) / 2.f);
		
		// render victim tee
		x -= 24.0f;

		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS)
		{
			if(m_aKillmsgs[r].m_ModeSpecial&1)
			{
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
				int QuadOffset = 0;
				if(m_aKillmsgs[r].m_VictimTeam == TEAM_RED)
					++QuadOffset;

				Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x, y-16);
			}
		}

		RenderTools()->RenderTee(CAnimState::GetIdle(), &m_aKillmsgs[r].m_VictimRenderInfo, EMOTE_PAIN, vec2(-1,0), vec2(x, y+28));
		x -= 32.0f;

		// render weapon
		x -= 44.0f;
		if(m_aKillmsgs[r].m_Weapon >= 0)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, 4 + m_aKillmsgs[r].m_Weapon, x, y + 28);
		}
		x -= 52.0f;

		if(m_aKillmsgs[r].m_VictimID != m_aKillmsgs[r].m_KillerID)
		{
			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS)
			{
				if(m_aKillmsgs[r].m_ModeSpecial&2)
				{
					Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);

					int QuadOffset = 2;
					if(m_aKillmsgs[r].m_KillerTeam == TEAM_RED)
						++QuadOffset;

					Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x - 56, y - 16);
				}
			}

			// render killer tee
			x -= 24.0f;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &m_aKillmsgs[r].m_KillerRenderInfo, EMOTE_ANGRY, vec2(1,0), vec2(x, y+28));
			x -= 32.0f;

			// render killer name
			x -= m_aKillmsgs[r].m_KillerTextWidth;

			if(m_aKillmsgs[r].m_KillerTextContainerIndex != -1)
				TextRender()->RenderTextContainer(m_aKillmsgs[r].m_KillerTextContainerIndex, &TColor, &TOutlineColor, x, y + (46.f - 36.f) / 2.f);
		}

		y += 46.0f;
	}
}
