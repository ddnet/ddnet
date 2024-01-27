/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/localization.h>

#include "infomessages.h"
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

void CInfoMessages::OnWindowResize()
{
	for(auto &InfoMsg : m_aInfoMsgs)
	{
		DeleteTextContainers(&InfoMsg);
	}
}

void CInfoMessages::OnReset()
{
	m_InfoMsgCurrent = 0;
	for(auto &InfoMsg : m_aInfoMsgs)
	{
		InfoMsg.m_Tick = -100000;
		DeleteTextContainers(&InfoMsg);
	}
}

void CInfoMessages::DeleteTextContainers(CInfoMsg *pInfoMsg)
{
	TextRender()->DeleteTextContainer(pInfoMsg->m_VictimTextContainerIndex);
	TextRender()->DeleteTextContainer(pInfoMsg->m_KillerTextContainerIndex);
	TextRender()->DeleteTextContainer(pInfoMsg->m_DiffTextContainerIndex);
	TextRender()->DeleteTextContainer(pInfoMsg->m_TimeTextContainerIndex);
}

void CInfoMessages::OnInit()
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

void CInfoMessages::AddInfoMsg(EType Type, CInfoMsg NewMsg)
{
	NewMsg.m_Type = Type;
	NewMsg.m_Tick = Client()->GameTick(g_Config.m_ClDummy);

	m_InfoMsgCurrent = (m_InfoMsgCurrent + 1) % MAX_INFOMSGS;
	DeleteTextContainers(&m_aInfoMsgs[m_InfoMsgCurrent]);
	m_aInfoMsgs[m_InfoMsgCurrent] = NewMsg;
}

void CInfoMessages::CreateNamesIfNotCreated(CInfoMsg *pInfoMsg)
{
	const float FontSize = 36.0f;
	if(!pInfoMsg->m_VictimTextContainerIndex.Valid() && pInfoMsg->m_aVictimName[0] != 0)
	{
		pInfoMsg->m_VictimTextWidth = TextRender()->TextWidth(FontSize, pInfoMsg->m_aVictimName);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		unsigned Color = g_Config.m_ClKillMessageNormalColor;
		if(pInfoMsg->m_aVictimIds[0] == m_pClient->m_Snap.m_LocalClientID)
		{
			Color = g_Config.m_ClKillMessageHighlightColor;
		}
		TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(Color)));

		TextRender()->CreateTextContainer(pInfoMsg->m_VictimTextContainerIndex, &Cursor, pInfoMsg->m_aVictimName);
	}

	if(!pInfoMsg->m_KillerTextContainerIndex.Valid() && pInfoMsg->m_aKillerName[0] != 0)
	{
		pInfoMsg->m_KillerTextWidth = TextRender()->TextWidth(FontSize, pInfoMsg->m_aKillerName);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		unsigned Color = g_Config.m_ClKillMessageNormalColor;
		if(pInfoMsg->m_KillerID == m_pClient->m_Snap.m_LocalClientID)
		{
			Color = g_Config.m_ClKillMessageHighlightColor;
		}
		TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(Color)));

		TextRender()->CreateTextContainer(pInfoMsg->m_KillerTextContainerIndex, &Cursor, pInfoMsg->m_aKillerName);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CInfoMessages::CreateFinishTextContainersIfNotCreated(CInfoMsg *pInfoMsg)
{
	const float FontSize = 36.0f;
	if(!pInfoMsg->m_DiffTextContainerIndex.Valid() && pInfoMsg->m_aDiffText[0] != 0)
	{
		pInfoMsg->m_DiffTextWidth = TextRender()->TextWidth(FontSize, pInfoMsg->m_aDiffText);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		if(pInfoMsg->m_Diff > 0)
			TextRender()->TextColor(1.0f, 0.5f, 0.5f, 1); // red
		else if(pInfoMsg->m_Diff < 0)
			TextRender()->TextColor(0.5f, 1.0f, 0.5f, 1); // green
		else
			TextRender()->TextColor(TextRender()->DefaultTextColor());

		TextRender()->CreateTextContainer(pInfoMsg->m_DiffTextContainerIndex, &Cursor, pInfoMsg->m_aDiffText);
	}
	if(!pInfoMsg->m_TimeTextContainerIndex.Valid() && pInfoMsg->m_aTimeText[0] != 0)
	{
		pInfoMsg->m_TimeTextWidth = TextRender()->TextWidth(FontSize, pInfoMsg->m_aTimeText);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;

		TextRender()->TextColor(TextRender()->DefaultTextColor());

		TextRender()->CreateTextContainer(pInfoMsg->m_TimeTextContainerIndex, &Cursor, pInfoMsg->m_aTimeText);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CInfoMessages::OnMessage(int MsgType, void *pRawMsg)
{
	if(m_pClient->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_KILLMSGTEAM)
	{
		CNetMsg_Sv_KillMsgTeam *pMsg = (CNetMsg_Sv_KillMsgTeam *)pRawMsg;

		CInfoMsg Kill{};

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

		Kill.m_VictimDDTeam = pMsg->m_Team;
		for(int i = 0; i < Kill.m_TeamSize; i++)
		{
			if(m_pClient->m_aClients[vStrongWeakSorted[i].first].m_Active)
			{
				Kill.m_aVictimIds[i] = vStrongWeakSorted[i].first;
				Kill.m_aVictimRenderInfo[i] = m_pClient->m_aClients[vStrongWeakSorted[i].first].m_RenderInfo;
			}
			else
			{
				Kill.m_aVictimIds[i] = -1;
				Kill.m_aVictimRenderInfo[i].Reset();
			}
		}
		for(int i = Kill.m_TeamSize; i < MAX_KILLMSG_TEAM_MEMBERS; i++)
		{
			Kill.m_aVictimIds[i] = -1;
			Kill.m_aVictimRenderInfo[i].Reset();
		}
		str_format(Kill.m_aVictimName, sizeof(Kill.m_aVictimName), Localize("Team %d"), pMsg->m_Team);

		Kill.m_KillerID = -1;
		Kill.m_aKillerName[0] = '\0';
		Kill.m_KillerRenderInfo.Reset();

		Kill.m_Weapon = -1;
		Kill.m_ModeSpecial = 0;

		Kill.m_VictimTextWidth = Kill.m_KillerTextWidth = 0.f;

		float Height = 400 * 3.0f;
		float Width = Height * Graphics()->ScreenAspect();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);

		CreateNamesIfNotCreated(&Kill);

		bool KillMsgValid = true;
		for(int i = 0; i < Kill.m_TeamSize; i++)
		{
			KillMsgValid = KillMsgValid && Kill.m_aVictimIds[i] >= 0 && ((Kill.m_aVictimRenderInfo[i].m_CustomColoredSkin && Kill.m_aVictimRenderInfo[i].m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_aVictimRenderInfo[i].m_CustomColoredSkin && Kill.m_aVictimRenderInfo[i].m_OriginalRenderSkin.m_Body.IsValid()));
		}

		if(KillMsgValid)
		{
			AddInfoMsg(EType::TYPE_KILL, Kill);
		}
		else
		{
			DeleteTextContainers(&Kill);
		}

		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;

		CInfoMsg Kill{};

		Kill.m_TeamSize = 1;
		Kill.m_aVictimIds[0] = pMsg->m_Victim;
		if(Kill.m_aVictimIds[0] >= 0 && Kill.m_aVictimIds[0] < MAX_CLIENTS)
		{
			Kill.m_VictimDDTeam = m_pClient->m_Teams.Team(Kill.m_aVictimIds[0]);
			str_copy(Kill.m_aVictimName, m_pClient->m_aClients[Kill.m_aVictimIds[0]].m_aName);
			Kill.m_aVictimRenderInfo[0] = m_pClient->m_aClients[Kill.m_aVictimIds[0]].m_RenderInfo;
		}
		else
		{
			Kill.m_VictimDDTeam = 0;
			Kill.m_aVictimName[0] = '\0';
		}
		for(int i = Kill.m_TeamSize; i < MAX_KILLMSG_TEAM_MEMBERS; i++)
		{
			Kill.m_aVictimIds[i] = -1;
			Kill.m_aVictimRenderInfo[i].Reset();
		}

		Kill.m_KillerID = pMsg->m_Killer;
		if(Kill.m_KillerID >= 0 && Kill.m_KillerID < MAX_CLIENTS)
		{
			str_copy(Kill.m_aKillerName, m_pClient->m_aClients[Kill.m_KillerID].m_aName);
			Kill.m_KillerRenderInfo = m_pClient->m_aClients[Kill.m_KillerID].m_RenderInfo;
		}
		else
		{
			Kill.m_aKillerName[0] = '\0';
			Kill.m_KillerRenderInfo.Reset();
		}

		Kill.m_Weapon = pMsg->m_Weapon;
		Kill.m_ModeSpecial = pMsg->m_ModeSpecial;

		Kill.m_FlagCarrierBlue = m_pClient->m_Snap.m_pGameDataObj ? m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue : -1;

		Kill.m_VictimTextWidth = Kill.m_KillerTextWidth = 0.f;

		float Height = 400 * 3.0f;
		float Width = Height * Graphics()->ScreenAspect();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);

		CreateNamesIfNotCreated(&Kill);

		bool KillMsgValid = (Kill.m_aVictimRenderInfo[0].m_CustomColoredSkin && Kill.m_aVictimRenderInfo[0].m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_aVictimRenderInfo[0].m_CustomColoredSkin && Kill.m_aVictimRenderInfo[0].m_OriginalRenderSkin.m_Body.IsValid());
		KillMsgValid &= Kill.m_KillerID == -1 || ((Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_ColorableRenderSkin.m_Body.IsValid()) || (!Kill.m_KillerRenderInfo.m_CustomColoredSkin && Kill.m_KillerRenderInfo.m_OriginalRenderSkin.m_Body.IsValid()));
		if(KillMsgValid)
		{
			AddInfoMsg(EType::TYPE_KILL, Kill);
		}
		else
		{
			DeleteTextContainers(&Kill);
		}

		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	if(MsgType == NETMSGTYPE_SV_RACEFINISH)
	{
		CNetMsg_Sv_RaceFinish *pMsg = (CNetMsg_Sv_RaceFinish *)pRawMsg;

		char aBuf[256];

		CInfoMsg Finish;
		Finish.m_TeamSize = 1;
		Finish.m_aVictimIds[0] = pMsg->m_ClientID;
		Finish.m_VictimDDTeam = m_pClient->m_Teams.Team(Finish.m_aVictimIds[0]);
		str_copy(Finish.m_aVictimName, m_pClient->m_aClients[Finish.m_aVictimIds[0]].m_aName);
		Finish.m_aVictimRenderInfo[0] = m_pClient->m_aClients[pMsg->m_ClientID].m_RenderInfo;

		Finish.m_aKillerName[0] = '\0';

		Finish.m_Diff = pMsg->m_Diff;
		Finish.m_RecordPersonal = (pMsg->m_RecordPersonal || pMsg->m_RecordServer);

		// diff time text
		if(Finish.m_Diff)
		{
			if(Finish.m_Diff < 0)
			{
				str_time_float(-Finish.m_Diff / 1000.0f, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
				str_format(Finish.m_aDiffText, sizeof(Finish.m_aDiffText), "(-%s)", aBuf);
			}
			else
			{
				str_time_float(Finish.m_Diff / 1000.0f, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
				str_format(Finish.m_aDiffText, sizeof(Finish.m_aDiffText), "(+%s)", aBuf);
			}
		}
		else
		{
			Finish.m_aDiffText[0] = '\0';
		}

		// finish time text
		str_time_float(pMsg->m_Time / 1000.0f, TIME_HOURS_CENTISECS, Finish.m_aTimeText, sizeof(Finish.m_aTimeText));

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		float Height = 400 * 3.0f;
		float Width = Height * Graphics()->ScreenAspect();
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);

		CreateNamesIfNotCreated(&Finish);
		CreateFinishTextContainersIfNotCreated(&Finish);

		AddInfoMsg(EType::TYPE_FINISH, Finish);

		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}
}

void CInfoMessages::RenderKillMsg(CInfoMsg *pInfoMsg, float x, float y)
{
	// render victim name
	x -= pInfoMsg->m_VictimTextWidth;
	ColorRGBA TextColor;
	if(g_Config.m_ClChatTeamColors && pInfoMsg->m_VictimDDTeam)
		TextColor = m_pClient->GetDDTeamColor(pInfoMsg->m_VictimDDTeam, 0.75f);
	else
		TextColor = TextRender()->DefaultTextColor();

	CreateNamesIfNotCreated(pInfoMsg);

	if(pInfoMsg->m_VictimTextContainerIndex.Valid())
		TextRender()->RenderTextContainer(pInfoMsg->m_VictimTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (46.f - 36.f) / 2.f);

	// render victim tee
	x -= 24.0f;

	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
	{
		if(pInfoMsg->m_ModeSpecial & 1)
		{
			int QuadOffset = 0;
			if(pInfoMsg->m_aVictimIds[0] == pInfoMsg->m_FlagCarrierBlue)
				++QuadOffset;

			if(QuadOffset == 0)
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
			else
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);

			Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, QuadOffset, x, y - 16);
		}
	}

	for(int j = (pInfoMsg->m_TeamSize - 1); j >= 0; j--)
	{
		if(pInfoMsg->m_aVictimIds[j] < 0)
			continue;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &pInfoMsg->m_aVictimRenderInfo[j], OffsetToMid);
		const vec2 TeeRenderPos = vec2(x, y + 46.0f / 2.0f + OffsetToMid.y);
		RenderTools()->RenderTee(pIdleState, &pInfoMsg->m_aVictimRenderInfo[j], EMOTE_PAIN, vec2(-1, 0), TeeRenderPos);

		x -= 44.0f;
	}

	// render weapon
	x -= 32.0f;
	if(pInfoMsg->m_Weapon >= 0)
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeapons[pInfoMsg->m_Weapon]);
		Graphics()->RenderQuadContainerAsSprite(m_SpriteQuadContainerIndex, 4 + pInfoMsg->m_Weapon, x, y + 28);
	}
	x -= 52.0f;

	if(pInfoMsg->m_aVictimIds[0] != pInfoMsg->m_KillerID)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS)
		{
			if(pInfoMsg->m_ModeSpecial & 2)
			{
				int QuadOffset = 2;
				if(pInfoMsg->m_KillerID == pInfoMsg->m_FlagCarrierBlue)
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

		if(pInfoMsg->m_KillerID >= 0)
		{
			const CAnimState *pIdleState = CAnimState::GetIdle();
			vec2 OffsetToMid;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &pInfoMsg->m_KillerRenderInfo, OffsetToMid);
			const vec2 TeeRenderPos = vec2(x, y + 46.0f / 2.0f + OffsetToMid.y);
			RenderTools()->RenderTee(pIdleState, &pInfoMsg->m_KillerRenderInfo, EMOTE_ANGRY, vec2(1, 0), TeeRenderPos);
		}

		x -= 32.0f;

		// render killer name
		x -= pInfoMsg->m_KillerTextWidth;

		if(pInfoMsg->m_KillerTextContainerIndex.Valid())
			TextRender()->RenderTextContainer(pInfoMsg->m_KillerTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (46.f - 36.f) / 2.f);
	}
}

void CInfoMessages::RenderFinishMsg(CInfoMsg *pInfoMsg, float x, float y)
{
	// render time diff
	CreateFinishTextContainersIfNotCreated(pInfoMsg);

	if(pInfoMsg->m_Diff)
	{
		x -= pInfoMsg->m_DiffTextWidth;

		if(pInfoMsg->m_DiffTextContainerIndex.Valid())
			TextRender()->RenderTextContainer(pInfoMsg->m_DiffTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), x, y + (46.f - 36.f) / 2.f);
	}

	// render time
	x -= pInfoMsg->m_TimeTextWidth;

	if(pInfoMsg->m_TimeTextContainerIndex.Valid())
		TextRender()->RenderTextContainer(pInfoMsg->m_TimeTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), x, y + (46.f - 36.f) / 2.f);

	// render flag
	x -= 52.0f;

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_RACEFLAG].m_Id);
	Graphics()->QuadsBegin();
	IGraphics::CQuadItem QuadItem(x, y, 52, 52);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render victim name
	ColorRGBA TextColor;
	x -= pInfoMsg->m_VictimTextWidth;
	if(g_Config.m_ClChatTeamColors && pInfoMsg->m_VictimDDTeam)
		TextColor = m_pClient->GetDDTeamColor(pInfoMsg->m_VictimDDTeam, 0.75f);
	else
		TextColor = TextRender()->DefaultTextColor();

	CreateNamesIfNotCreated(pInfoMsg);

	if(pInfoMsg->m_VictimTextContainerIndex.Valid())
		TextRender()->RenderTextContainer(pInfoMsg->m_VictimTextContainerIndex, TextColor, TextRender()->DefaultTextOutlineColor(), x, y + (46.f - 36.f) / 2.f);

	// render victim tee
	x -= 24.0f;

	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &pInfoMsg->m_aVictimRenderInfo[0], OffsetToMid);
	const vec2 TeeRenderPos = vec2(x, y + 46.0f / 2.0f + OffsetToMid.y);
	const int Emote = pInfoMsg->m_RecordPersonal ? EMOTE_HAPPY : EMOTE_NORMAL;
	RenderTools()->RenderTee(pIdleState, &pInfoMsg->m_aVictimRenderInfo[0], Emote, vec2(-1, 0), TeeRenderPos);
}

void CInfoMessages::OnRender()
{
	float Height = 400 * 3.0f;
	float Width = Height * Graphics()->ScreenAspect();

	Graphics()->MapScreen(0, 0, Width * 1.5f, Height * 1.5f);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	float StartX = Width * 1.5f - 10.0f;
	float y = 30.0f + 100.0f * ((g_Config.m_ClShowfps ? 1 : 0) + (g_Config.m_ClShowpred && Client()->State() != IClient::STATE_DEMOPLAYBACK));

	for(int i = 1; i <= MAX_INFOMSGS; i++)
	{
		CInfoMsg *pInfoMsg = &m_aInfoMsgs[(m_InfoMsgCurrent + i) % MAX_INFOMSGS];
		if(Client()->GameTick(g_Config.m_ClDummy) > pInfoMsg->m_Tick + Client()->GameTickSpeed() * 10)
			continue;

		if(pInfoMsg->m_Type == EType::TYPE_KILL && g_Config.m_ClShowKillMessages)
		{
			RenderKillMsg(pInfoMsg, StartX, y);
			y += 46.0f;
		}
		else if(pInfoMsg->m_Type == EType::TYPE_FINISH && g_Config.m_ClShowFinishMessages)
		{
			RenderFinishMsg(pInfoMsg, StartX, y);
			y += 46.0f;
		}
	}
}

void CInfoMessages::RefindSkins()
{
	for(auto &InfoMsg : m_aInfoMsgs)
	{
		InfoMsg.m_KillerRenderInfo.Reset();
		if(InfoMsg.m_KillerID >= 0)
		{
			const CGameClient::CClientData &Client = GameClient()->m_aClients[InfoMsg.m_KillerID];
			if(Client.m_Active && Client.m_aSkinName[0] != '\0')
				InfoMsg.m_KillerRenderInfo = Client.m_RenderInfo;
			else
				InfoMsg.m_KillerID = -1;
		}

		for(int i = 0; i < MAX_KILLMSG_TEAM_MEMBERS; i++)
		{
			InfoMsg.m_aVictimRenderInfo[i].Reset();
			if(InfoMsg.m_aVictimIds[i] >= 0)
			{
				const CGameClient::CClientData &Client = GameClient()->m_aClients[InfoMsg.m_aVictimIds[i]];
				if(Client.m_Active && Client.m_aSkinName[0] != '\0')
					InfoMsg.m_aVictimRenderInfo[i] = Client.m_RenderInfo;
				else
					InfoMsg.m_aVictimIds[i] = -1;
			}
		}
	}
}
