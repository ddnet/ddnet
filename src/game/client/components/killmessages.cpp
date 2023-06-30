/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/localization.h>

#include "killmessages.h"
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

void CKillMessages::OnWindowResize()
{
	for(auto &Killmsg : m_aKillmsgs)
	{
		TextRender()->DeleteTextContainer(Killmsg.m_VictimTextContainerIndex);
		TextRender()->DeleteTextContainer(Killmsg.m_KillerTextContainerIndex);
	}
}

void CKillMessages::OnReset()
{
	m_KillmsgCurrent = 0;

	for(auto &Killmsg : m_aKillmsgs)
	{
		Killmsg.m_Tick = -100000;

		TextRender()->DeleteTextContainer(Killmsg.m_VictimTextContainerIndex);
		TextRender()->DeleteTextContainer(Killmsg.m_KillerTextContainerIndex);
	}
}

void CKillMessages::OnInit()
{
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	m_SpriteQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	Graphics()->QuadsSetSubset(1, 0, 0, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);
	Graphics()->QuadsSetSubset(1, 0, 0, 1);
	RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 0.f, 0.f, 28.f, 56.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		RenderTools()->QuadContainerAddSprite(m_SpriteQuadContainerIndex, 96.f * ScaleX, 96.f * ScaleY);
	}
	Graphics()->QuadContainerUpload(m_SpriteQuadContainerIndex);
}

void CKillMessages::CreateKillmessageNamesIfNotCreated(CKillMsg &Kill)
{
	const float FontSize = 36.0f;
	if(!Kill.m_VictimTextContainerIndex.Valid() && Kill.m_aVictimName[0] != 0)
	{
		Kill.m_VictimTextWidth = TextRender()->TextWidth(FontSize, Kill.m_aVictimName, -1, -1.0f);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		unsigned Color = g_Config.m_ClKillMessageNormalColor;
		if(Kill.m_VictimID == m_pClient->m_Snap.m_LocalClientID)
		{
			Color = g_Config.m_ClKillMessageHighlightColor;
		}
		TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(Color)));

		TextRender()->CreateTextContainer(Kill.m_VictimTextContainerIndex, &Cursor, Kill.m_aVictimName);
	}

	if(!Kill.m_KillerTextContainerIndex.Valid() && Kill.m_aKillerName[0] != 0)
	{
		Kill.m_KillerTextWidth = TextRender()->TextWidth(FontSize, Kill.m_aKillerName, -1, -1.0f);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		unsigned Color = g_Config.m_ClKillMessageNormalColor;
		if(Kill.m_KillerID == m_pClient->m_Snap.m_LocalClientID)
		{
			Color = g_Config.m_ClKillMessageHighlightColor;
		}
		TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(Color)));

		TextRender()->CreateTextContainer(Kill.m_KillerTextContainerIndex, &Cursor, Kill.m_aKillerName);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CKillMessages::OnMessage(int MsgType, void *pRawMsg)
{
	if(m_pClient->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_KILLMSGTEAM)
	{
		CNetMsg_Sv_KillMsgTeam *pMsg = (CNetMsg_Sv_KillMsgTeam *)pRawMsg;

		// unpack messages
		CKillMsg Kill{};
		Kill.m_aVictimName[0] = '\0';
		Kill.m_aKillerName[0] = '\0';

		std::vector<std::pair<int, int>> vStrongWeakSorted;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_pClient->m_Teams.Team(i) == pMsg->m_Team)
			{
				CCharacter *pChr = m_pClient->m_GameWorld.GetCharacterByID(i);
				vStrongWeakSorted.emplace_back(i, pMsg->m_First == i ? MAX_CLIENTS : pChr ? pChr->GetStrongWeakID() : 0);
			}
		}

		std::stable_sort(vStrongWeakSorted.begin(), vStrongWeakSorted.end(), [](auto &Left, auto &Right) { return Left.second > Right.second; });

		Kill.m_TeamSize = vStrongWeakSorted.size();
		if(Kill.m_TeamSize > MAX_KILLMSG_TEAM_MEMBERS)
			Kill.m_TeamSize = MAX_KILLMSG_TEAM_MEMBERS;

		Kill.m_VictimID = vStrongWeakSorted.empty() ? -1 : vStrongWeakSorted[0].first;
		if(Kill.m_VictimID >= 0 && Kill.m_VictimID < MAX_CLIENTS)
		{
			Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimID].m_Team;
			Kill.m_VictimDDTeam = pMsg->m_Team;
			Kill.m_VictimRenderInfo[0] = m_pClient->m_aClients[Kill.m_VictimID].m_RenderInfo;

			for(int i = 1; i < Kill.m_TeamSize; i++)
				Kill.m_VictimRenderInfo[i] = m_pClient->m_aClients[vStrongWeakSorted[i].first].m_RenderInfo;
		}
		str_format(Kill.m_aVictimName, sizeof(Kill.m_aVictimName), Localize("Team %d"), pMsg->m_Team);

		Kill.m_KillerID = Kill.m_VictimID;

		Kill.m_Weapon = -1;
		Kill.m_ModeSpecial = 0;
		Kill.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

		Kill.m_VictimTextWidth = Kill.m_KillerTextWidth = 0.f;

		float Height = 400 * 3.0f;
		float Width = Height * Graphics()->ScreenAspect();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);

		CreateKillmessageNamesIfNotCreated(Kill);

		int VictimSkinsValid = 0;
		for(int i = 0; i < Kill.m_TeamSize; i++)
		{
			if((Kill.m_VictimRenderInfo[i].m_CustomColoredSkin && Kill.m_VictimRenderInfo[i].m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_VictimRenderInfo[i].m_CustomColoredSkin && Kill.m_VictimRenderInfo[i].m_OriginalRenderSkin.m_Body.IsValid()))
				VictimSkinsValid++;
		}
		bool KillMsgValid = VictimSkinsValid == Kill.m_TeamSize;

		if(KillMsgValid)
		{
			// add the message
			m_KillmsgCurrent = (m_KillmsgCurrent + 1) % MAX_KILLMSGS;

			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_VictimTextContainerIndex);
			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_KillerTextContainerIndex);

			m_aKillmsgs[m_KillmsgCurrent] = Kill;
		}
		else
		{
			TextRender()->DeleteTextContainer(Kill.m_VictimTextContainerIndex);
			TextRender()->DeleteTextContainer(Kill.m_KillerTextContainerIndex);
		}

		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		// unpack messages
		CKillMsg Kill;
		Kill.m_aVictimName[0] = '\0';
		Kill.m_aKillerName[0] = '\0';

		Kill.m_TeamSize = 1;

		Kill.m_VictimID = pMsg->m_Victim;
		if(Kill.m_VictimID >= 0 && Kill.m_VictimID < MAX_CLIENTS)
		{
			Kill.m_VictimTeam = m_pClient->m_aClients[Kill.m_VictimID].m_Team;
			Kill.m_VictimDDTeam = m_pClient->m_Teams.Team(Kill.m_VictimID);
			str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_VictimID].m_aName);
			Kill.m_VictimRenderInfo[0] = m_pClient->m_aClients[Kill.m_VictimID].m_RenderInfo;
		}

		Kill.m_KillerID = pMsg->m_Killer;
		if(Kill.m_KillerID >= 0 && Kill.m_KillerID < MAX_CLIENTS)
		{
			Kill.m_KillerTeam = m_pClient->m_aClients[Kill.m_KillerID].m_Team;
			str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerID].m_aName);
			Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerID].m_RenderInfo;
		}

		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;
		Kill.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

		Kill.m_FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataObj ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : -1;

		Kill.m_VictimTextWidth = Kill.m_KillerTextWidth = 0.f;

		float Height = 400 * 3.0f;
		float Width = Height * Graphics()->ScreenAspect();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);

		CreateKillmessageNamesIfNotCreated(Kill);

		bool KillMsgValid = (Kill.m_VictimRenderInfo[0].m_CustomColoredSkin && Kill.m_VictimRenderInfo[0].m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_VictimRenderInfo[0].m_CustomColoredSkin && Kill.m_VictimRenderInfo[0].m_OriginalRenderSkin.m_Body.IsValid());
		// if killer != victim, killer must be valid too
		KillMsgValid &= Kill.m_KillerID == Kill.m_VictimID || ((Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_OriginalRenderSkin.m_Body.IsValid()));
		if(KillMsgValid)
		{
			// add the message
			m_KillmsgCurrent = (m_KillmsgCurrent + 1) % MAX_KILLMSGS;

			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_VictimTextContainerIndex);
			TextRender()->DeleteTextContainer(m_aKillmsgs[m_KillmsgCurrent].m_KillerTextContainerIndex);

			m_aKillmsgs[m_KillmsgCurrent] = Kill;
		}
		else
		{
			TextRender()->DeleteTextContainer(Kill.m_VictimTextContainerIndex);
			TextRender()->DeleteTextContainer(Kill.m_KillerTextContainerIndex);
		}

		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
}

void CKillMessages::OnRender()
{
	if(!g_Config.m_ClShowKillMessages)
		return;

	float Height = 400 * 3.0f;
	float Width = Height * Graphics()->ScreenAspect();

	Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	float StartX = Width * 1.5f - 10.0f;
	float y = 30.0f + 100.0f * ((g_Config.m_ClShowfps ? 1 : 0) + g_Config.m_ClShowpred);

	for(int i = 1; i <= MAX_KILLMSGS; i++)
	{
		int r = (m_KillmsgCurrent + i) % MAX_KILLMSGS;
		if(Client()->GameTick(g_Config.m_ClDummy) > m_aKillmsgs[r].m_Tick + 50 * 10)
			continue;

		float x = StartX;

		ColorRGBA TColor(1.f, 1.f, 1.f, 1.f);
		ColorRGBA TOutlineColor(0.f, 0.f, 0.f, 0.3f);

		// render victim name
		x -= m_aKillmsgs[r].m_VictimTextWidth;
		if(m_aKillmsgs[r].m_VictimID >= 0 && g_Config.m_ClChatTeamColors && m_aKillmsgs[r].m_VictimDDTeam)
		{
			TColor = color_cast<ColorRGBA>(ColorHSLA(m_aKillmsgs[r].m_VictimDDTeam / 64.0f, 1.0f, 0.75f));
			TColor.a = 1.f;
		}

		CreateKillmessageNamesIfNotCreated(m_aKillmsgs[r]);

		if(m_aKillmsgs[r].m_VictimTextContainerIndex.Valid())
			TextRender()->RenderTextContainer(m_aKillmsgs[r].m_VictimTextContainerIndex, TColor, TOutlineColor, x, y + (46.f - 36.f) / 2.f);

		// render victim tee
		x -= 24.0f;

		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
		{
			if(m_aKillmsgs[r].m_ModeSpecial & 1)
			{
				int QuadOffset = 0;
				if(m_aKillmsgs[r].m_VictimID == m_aKillmsgs[r].m_FlagCarrierBlue)
					++QuadOffset;

				if(QuadOffset == 0)
					Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
				else
					Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);

				Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x, y - 16);
			}
		}

		if(m_aKillmsgs[r].m_VictimID >= 0)
		{
			for(int j = (m_aKillmsgs[r].m_TeamSize - 1); j >= 0; j--)
			{
				CTeeRenderInfo TeeInfo = m_aKillmsgs[r].m_VictimRenderInfo[j];

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
				vec2 TeeRenderPos(x, y + 46.0f / 2.0f + OffsetToMid.y);

				RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_PAIN, vec2(-1, 0), TeeRenderPos);

				x -= 44.0f;
			}
		}

		// render weapon
		x -= 32.0f;
		if(m_aKillmsgs[r].m_Weapon >= 0)
		{
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeapons[m_aKillmsgs[r].m_Weapon]);
			Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, 4 + m_aKillmsgs[r].m_Weapon, x, y + 28);
		}
		x -= 52.0f;

		if(m_aKillmsgs[r].m_VictimID != m_aKillmsgs[r].m_KillerID)
		{
			if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
			{
				if(m_aKillmsgs[r].m_ModeSpecial & 2)
				{
					int QuadOffset = 2;
					if(m_aKillmsgs[r].m_KillerID == m_aKillmsgs[r].m_FlagCarrierBlue)
						++QuadOffset;

					if(QuadOffset == 2)
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
					else
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);

					Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x - 56, y - 16);
				}
			}

			// render killer tee
			x -= 24.0f;

			if(m_aKillmsgs[r].m_KillerID >= 0)
			{
				CTeeRenderInfo TeeInfo = m_aKillmsgs[r].m_KillerRenderInfo;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
				vec2 TeeRenderPos(x, y + 46.0f / 2.0f + OffsetToMid.y);

				RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_ANGRY, vec2(1, 0), TeeRenderPos);
			}

			x -= 32.0f;

			// render killer name
			x -= m_aKillmsgs[r].m_KillerTextWidth;

			if(m_aKillmsgs[r].m_KillerTextContainerIndex.Valid())
				TextRender()->RenderTextContainer(m_aKillmsgs[r].m_KillerTextContainerIndex, TColor, TOutlineColor, x, y + (46.f - 36.f) / 2.f);
		}

		y += 46.0f;
	}
}

void CKillMessages::RefindSkins()
{
	for(int i = 0; i < MAX_KILLMSGS; i++)
	{
		int r = i % MAX_KILLMSGS;
		if(Client()->GameTick(g_Config.m_ClDummy) > m_aKillmsgs[r].m_Tick + 50 * 10)
			continue;

		if(m_aKillmsgs[r].m_KillerID >= 0)
		{
			const CGameClient::CClientData &Client = GameClient()->m_aClients[m_aKillmsgs[r].m_KillerID];
			if(Client.m_aSkinName[0] != '\0')
				m_aKillmsgs[r].m_KillerRenderInfo = Client.m_RenderInfo;
			else
				m_aKillmsgs[r].m_KillerID = -1;
		}

		if(m_aKillmsgs[r].m_VictimID >= 0)
		{
			for(auto &VictimRenderInfo : m_aKillmsgs[r].m_VictimRenderInfo)
			{
				VictimRenderInfo = {};
			}
			const CGameClient::CClientData &Client = GameClient()->m_aClients[m_aKillmsgs[r].m_VictimID];
			if(Client.m_aSkinName[0] != '\0')
				m_aKillmsgs[r].m_VictimRenderInfo[0] = Client.m_RenderInfo;
			else
				m_aKillmsgs[r].m_VictimID = -1;
		}
	}
}
