/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "scoreboard.h"

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/motd.h>
#include <game/client/components/statboard.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/localization.h>

CScoreboard::CScoreboard()
{
	OnReset();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = static_cast<CScoreboard *>(pUserData);
	pSelf->m_Active = pResult->GetInteger(0) != 0;
}

void CScoreboard::OnConsoleInit()
{
	Console()->Register("+scoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
}

void CScoreboard::OnReset()
{
	m_Active = false;
	m_ServerRecord = -1.0f;
}

void CScoreboard::OnRelease()
{
	m_Active = false;
}

void CScoreboard::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_RECORD)
	{
		CNetMsg_Sv_Record *pMsg = static_cast<CNetMsg_Sv_Record *>(pRawMsg);
		m_ServerRecord = pMsg->m_ServerTimeBest / 100.0f;
	}
	else if(MsgType == NETMSGTYPE_SV_RECORDLEGACY)
	{
		CNetMsg_Sv_RecordLegacy *pMsg = static_cast<CNetMsg_Sv_RecordLegacy *>(pRawMsg);
		m_ServerRecord = pMsg->m_ServerTimeBest / 100.0f;
	}
}

void CScoreboard::RenderTitle(CUIRect TitleBar, int Team, const char *pTitle)
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;

	char aScore[128] = "";
	if(GameClient()->m_GameInfo.m_TimeScore)
	{
		if(m_ServerRecord > 0)
		{
			str_time_float(m_ServerRecord, TIME_HOURS, aScore, sizeof(aScore));
		}
	}
	else if(pGameInfoObj && (pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS))
	{
		const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
		if(pGameDataObj)
		{
			str_format(aScore, sizeof(aScore), "%d", Team == TEAM_RED ? pGameDataObj->m_TeamscoreRed : pGameDataObj->m_TeamscoreBlue);
		}
	}
	else
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active &&
			GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW &&
			GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId])
		{
			str_format(aScore, sizeof(aScore), "%d", GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId]->m_Score);
		}
		else if(GameClient()->m_Snap.m_pLocalInfo)
		{
			str_format(aScore, sizeof(aScore), "%d", GameClient()->m_Snap.m_pLocalInfo->m_Score);
		}
	}

	const float TitleFontSize = 40.0f;
	const float ScoreTextWidth = TextRender()->TextWidth(TitleFontSize, aScore);

	TitleBar.VMargin(20.0f, &TitleBar);
	CUIRect TitleLabel, ScoreLabel;
	if(Team == TEAM_RED)
	{
		TitleBar.VSplitRight(ScoreTextWidth, &TitleLabel, &ScoreLabel);
		TitleLabel.VSplitRight(10.0f, &TitleLabel, nullptr);
	}
	else
	{
		TitleBar.VSplitLeft(ScoreTextWidth, &ScoreLabel, &TitleLabel);
		TitleLabel.VSplitLeft(10.0f, nullptr, &TitleLabel);
	}

	{
		SLabelProperties Props;
		Props.m_MaxWidth = TitleLabel.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&TitleLabel, pTitle, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_ML : TEXTALIGN_MR, Props);
	}

	if(aScore[0] != '\0')
	{
		Ui()->DoLabel(&ScoreLabel, aScore, TitleFontSize, Team == TEAM_RED ? TEXTALIGN_MR : TEXTALIGN_ML);
	}
}

void CScoreboard::RenderGoals(CUIRect Goals)
{
	Goals.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 15.0f);
	Goals.VMargin(10.0f, &Goals);

	const float FontSize = 20.0f;
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	char aBuf[64];

	if(pGameInfoObj->m_ScoreLimit)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), pGameInfoObj->m_ScoreLimit);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_ML);
	}

	if(pGameInfoObj->m_TimeLimit)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), pGameInfoObj->m_TimeLimit);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MC);
	}

	if(pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Round %d/%d"), pGameInfoObj->m_RoundCurrent, pGameInfoObj->m_RoundNum);
		Ui()->DoLabel(&Goals, aBuf, FontSize, TEXTALIGN_MR);
	}
}

void CScoreboard::RenderSpectators(CUIRect Spectators)
{
	Spectators.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 15.0f);
	Spectators.Margin(10.0f, &Spectators);

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, Spectators.x, Spectators.y, 22.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = Spectators.w;
	Cursor.m_MaxLines = round_truncate(Spectators.h / Cursor.m_FontSize);

	int RemainingSpectators = 0;
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;
		++RemainingSpectators;
	}

	TextRender()->TextEx(&Cursor, Localize("Spectators"));

	if(RemainingSpectators > 0)
	{
		TextRender()->TextEx(&Cursor, ": ");
	}

	bool CommaNeeded = false;
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByName)
	{
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;

		if(CommaNeeded)
		{
			TextRender()->TextEx(&Cursor, ", ");
		}

		if(Cursor.m_LineCount == Cursor.m_MaxLines && RemainingSpectators >= 2)
		{
			// This is less expensive than checking with a separate invisible
			// text cursor though we waste some space at the end of the line.
			char aRemaining[64];
			str_format(aRemaining, sizeof(aRemaining), Localize("%d others…", "Spectators"), RemainingSpectators);
			TextRender()->TextEx(&Cursor, aRemaining);
			break;
		}

		if(GameClient()->m_aClients[pInfo->m_ClientId].m_AuthLevel)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor)));
		}

		if(g_Config.m_ClShowIds)
		{
			char aClientId[5];
			str_format(aClientId, sizeof(aClientId), "%d: ", pInfo->m_ClientId);
			TextRender()->TextEx(&Cursor, aClientId);
		}
		TextRender()->TextEx(&Cursor, GameClient()->m_aClients[pInfo->m_ClientId].m_aName);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		CommaNeeded = true;
		--RemainingSpectators;
	}
}

void CScoreboard::RenderScoreboard(CUIRect Scoreboard, int Team, int CountStart, int CountEnd)
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
	const bool TimeScore = GameClient()->m_GameInfo.m_TimeScore;
	const int NumPlayers = CountEnd - CountStart;

	// calculate measurements
	float LineHeight;
	float TeeSizeMod;
	float Spacing;
	float RoundRadius;
	float FontSize;
	if(NumPlayers <= 8)
	{
		LineHeight = 60.0f;
		TeeSizeMod = 1.0f;
		Spacing = 16.0f;
		RoundRadius = 10.0f;
		FontSize = 24.0f;
	}
	else if(NumPlayers <= 12)
	{
		LineHeight = 50.0f;
		TeeSizeMod = 0.9f;
		Spacing = 5.0f;
		RoundRadius = 10.0f;
		FontSize = 24.0f;
	}
	else if(NumPlayers <= 16)
	{
		LineHeight = 40.0f;
		TeeSizeMod = 0.8f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
		FontSize = 24.0f;
	}
	else if(NumPlayers <= 24)
	{
		LineHeight = 27.0f;
		TeeSizeMod = 0.6f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
		FontSize = 20.0f;
	}
	else
	{
		LineHeight = 20.0f;
		TeeSizeMod = 0.4f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
		FontSize = 16.0f;
	}

	const float ScoreOffset = Scoreboard.x + 10.0f + 10.0f;
	const float ScoreLength = TextRender()->TextWidth(FontSize, TimeScore ? "00:00:00" : "99999");
	const float TeeOffset = ScoreOffset + ScoreLength + 15.0f;
	const float TeeLength = 60.0f * TeeSizeMod;
	const float NameOffset = TeeOffset + TeeLength;
	const float NameLength = 300.0f - TeeLength;
	const float CountryLength = (LineHeight - Spacing - TeeSizeMod * 5.0f) * 2.0f;
	const float PingLength = 65.0f;
	const float PingOffset = Scoreboard.x + Scoreboard.w - PingLength - 10.0f - 10.0f;
	const float CountryOffset = PingOffset - CountryLength;
	const float ClanLength = Scoreboard.w - ((NameOffset - Scoreboard.x) + NameLength) - (Scoreboard.w - (CountryOffset - Scoreboard.x));
	const float ClanOffset = CountryOffset - ClanLength;

	// render headlines
	const float HeadlineFontsize = 22.0f;
	CUIRect Headline;
	Scoreboard.HSplitTop(HeadlineFontsize * 2.0f, &Headline, &Scoreboard);
	const float HeadlineY = Headline.y + Headline.h / 2.0f - HeadlineFontsize / 2.0f;
	const char *pScore = TimeScore ? Localize("Time") : Localize("Score");
	TextRender()->Text(ScoreOffset + ScoreLength - TextRender()->TextWidth(HeadlineFontsize, pScore), HeadlineY, HeadlineFontsize, pScore);
	TextRender()->Text(NameOffset, HeadlineY, HeadlineFontsize, Localize("Name"));
	const char *pClanLabel = Localize("Clan");
	TextRender()->Text(ClanOffset + (ClanLength - TextRender()->TextWidth(HeadlineFontsize, pClanLabel)) / 2.0f, HeadlineY, HeadlineFontsize, pClanLabel);
	const char *pPingLabel = Localize("Ping");
	TextRender()->Text(PingOffset + PingLength - TextRender()->TextWidth(HeadlineFontsize, pPingLabel), HeadlineY, HeadlineFontsize, pPingLabel);

	// render player entries
	int CountRendered = 0;
	int PrevDDTeam = -1;

	char aBuf[64];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(CountRendered++ < CountStart)
			continue;

		int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfoNext = GameClient()->m_Snap.m_apInfoByDDTeamScore[j];
			if(!pInfoNext || pInfoNext->m_Team != Team)
				continue;

			NextDDTeam = GameClient()->m_Teams.Team(pInfoNext->m_ClientId);
			break;
		}

		if(PrevDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfoPrev = GameClient()->m_Snap.m_apInfoByDDTeamScore[j];
				if(!pInfoPrev || pInfoPrev->m_Team != Team)
					continue;

				PrevDDTeam = GameClient()->m_Teams.Team(pInfoPrev->m_ClientId);
				break;
			}
		}

		CUIRect RowAndSpacing, Row;
		Scoreboard.HSplitTop(LineHeight + Spacing, &RowAndSpacing, &Scoreboard);
		RowAndSpacing.HSplitTop(LineHeight, &Row, nullptr);

		// team background
		if(DDTeam != TEAM_FLOCK)
		{
			const ColorRGBA Color = GameClient()->GetDDTeamColor(DDTeam).WithAlpha(0.5f);
			int TeamRectCorners = 0;
			if(PrevDDTeam != DDTeam)
				TeamRectCorners |= IGraphics::CORNER_T;
			if(NextDDTeam != DDTeam)
				TeamRectCorners |= IGraphics::CORNER_B;
			RowAndSpacing.Draw(Color, TeamRectCorners, RoundRadius);

			if(NextDDTeam != DDTeam)
			{
				const float TeamFontSize = FontSize / 1.5f;
				if(NumPlayers > 8)
				{
					if(DDTeam == TEAM_SUPER)
						str_copy(aBuf, Localize("Super"));
					else
						str_format(aBuf, sizeof(aBuf), "%d", DDTeam);
					TextRender()->Text(Row.x, Row.y + Row.h / 2.0f - TeamFontSize / 2.0f, TeamFontSize, aBuf);
				}
				else
				{
					if(DDTeam == TEAM_SUPER)
						str_copy(aBuf, Localize("Super"));
					else
						str_format(aBuf, sizeof(aBuf), Localize("Team %d"), DDTeam);
					TextRender()->Text(Row.x + Row.w / 2.0f - TextRender()->TextWidth(TeamFontSize, aBuf) / 2.0f + 10.0f, Row.y + Row.h, TeamFontSize, aBuf);
				}
			}
		}
		PrevDDTeam = DDTeam;

		// background so it's easy to find the local player or the followed one in spectator mode
		if((!GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_Local) ||
			(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW && pInfo->m_Local) ||
			(GameClient()->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId))
		{
			CUIRect Highlight;
			Row.VMargin(10.0f, &Highlight);
			Highlight.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, RoundRadius);
		}

		// score
		if(TimeScore)
		{
			if(pInfo->m_Score == -9999)
			{
				aBuf[0] = '\0';
			}
			else
			{
				str_time((int64_t)absolute(pInfo->m_Score) * 100, TIME_HOURS, aBuf, sizeof(aBuf));
			}
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Score, -999, 99999));
		}
		TextRender()->Text(ScoreOffset + ScoreLength - TextRender()->TextWidth(FontSize, aBuf), Row.y + (Row.h - FontSize) / 2.0f, FontSize, aBuf);

		// CTF flag
		if(pGameInfoObj && (pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) &&
			pGameDataObj && (pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientId || pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId))
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientId ? GameClient()->m_GameSkin.m_SpriteFlagBlue : GameClient()->m_GameSkin.m_SpriteFlagRed);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetSubset(1.0f, 0.0f, 0.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(TeeOffset, Row.y - 5.0f - Spacing / 2.0f, Row.h / 2.0f, Row.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		const CGameClient::CClientData &ClientData = GameClient()->m_aClients[pInfo->m_ClientId];

		// skin
		{
			CTeeRenderInfo TeeInfo = ClientData.m_RenderInfo;
			TeeInfo.m_Size *= TeeSizeMod;
			vec2 OffsetToMid;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &TeeInfo, OffsetToMid);
			const vec2 TeeRenderPos = vec2(TeeOffset + TeeLength / 2, Row.y + Row.h / 2.0f + OffsetToMid.y);
			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
		}

		// name
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, NameOffset, Row.y + (Row.h - FontSize) / 2.0f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END);
			Cursor.m_LineWidth = NameLength;
			if(ClientData.m_AuthLevel)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor)));
			}
			if(g_Config.m_ClShowIds)
			{
				str_format(aBuf, sizeof(aBuf), "%s%d: %s", pInfo->m_ClientId < 10 ? " " : "", pInfo->m_ClientId, ClientData.m_aName);
				TextRender()->TextEx(&Cursor, aBuf);
			}
			else
			{
				TextRender()->TextEx(&Cursor, ClientData.m_aName);
			}
		}

		// clan
		{
			if(str_comp(ClientData.m_aClan, GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_aClan) == 0)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor)));
			}
			else
			{
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, ClanOffset + (ClanLength - minimum(TextRender()->TextWidth(FontSize, ClientData.m_aClan), ClanLength)) / 2.0f, Row.y + (Row.h - FontSize) / 2.0f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END);
			Cursor.m_LineWidth = ClanLength;
			TextRender()->TextEx(&Cursor, ClientData.m_aClan);
		}

		// country flag
		GameClient()->m_CountryFlags.Render(ClientData.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f),
			CountryOffset, Row.y + (Spacing + TeeSizeMod * 5.0f) / 2.0f, CountryLength, Row.h - Spacing - TeeSizeMod * 5.0f);

		// ping
		if(g_Config.m_ClEnablePingColor)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA((300.0f - clamp(pInfo->m_Latency, 0, 300)) / 1000.0f, 1.0f, 0.5f)));
		}
		else
		{
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Latency, 0, 999));
		TextRender()->Text(PingOffset + PingLength - TextRender()->TextWidth(FontSize, aBuf), Row.y + (Row.h - FontSize) / 2.0f, FontSize, aBuf);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		if(CountRendered == CountEnd)
			break;
	}
}

void CScoreboard::RenderRecordingNotification(float x)
{
	char aBuf[512] = "";

	const auto &&AppendRecorderInfo = [&](int Recorder, const char *pName) {
		if(GameClient()->DemoRecorder(Recorder)->IsRecording())
		{
			char aTime[32];
			str_time((int64_t)GameClient()->DemoRecorder(Recorder)->Length() * 100, TIME_HOURS, aTime, sizeof(aTime));
			str_append(aBuf, pName);
			str_append(aBuf, " ");
			str_append(aBuf, aTime);
			str_append(aBuf, "  ");
		}
	};

	AppendRecorderInfo(RECORDER_MANUAL, Localize("Manual"));
	AppendRecorderInfo(RECORDER_RACE, Localize("Race"));
	AppendRecorderInfo(RECORDER_AUTO, Localize("Auto"));
	AppendRecorderInfo(RECORDER_REPLAYS, Localize("Replay"));

	if(aBuf[0] == '\0')
		return;

	const float FontSize = 20.0f;

	CUIRect Rect = {x, 0.0f, TextRender()->TextWidth(FontSize, aBuf) + 60.0f, 50.0f};
	Rect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.4f), IGraphics::CORNER_B, 15.0f);
	Rect.VSplitLeft(20.0f, nullptr, &Rect);
	Rect.VSplitRight(10.0f, &Rect, nullptr);

	CUIRect Circle;
	Rect.VSplitLeft(20.0f, &Circle, &Rect);
	Circle.HMargin((Circle.h - Circle.w) / 2.0f, &Circle);
	Circle.Draw(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), IGraphics::CORNER_ALL, Circle.h / 2.0f);

	Rect.VSplitLeft(10.0f, nullptr, &Rect);
	Ui()->DoLabel(&Rect, aBuf, FontSize, TEXTALIGN_ML);
}

void CScoreboard::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!Active())
		return;

	// if the score board is active, then we should clear the motd message as well
	if(GameClient()->m_Motd.IsActive())
		GameClient()->m_Motd.Clear();

	const float Height = 400.0f * 3.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, Height);

	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	const bool Teams = pGameInfoObj && (pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS);
	const int NumPlayers = maximum(GameClient()->m_Snap.m_aTeamSize[TEAM_RED], GameClient()->m_Snap.m_aTeamSize[TEAM_BLUE]);

	const float ScoreboardSmallWidth = 750.0f + 20.0f;
	const float ScoreboardWidth = !Teams && NumPlayers <= 16 ? ScoreboardSmallWidth : 1500.0f;
	const float TitleHeight = 60.0f;

	CUIRect Scoreboard = {(Width - ScoreboardWidth) / 2.0f, 150.0f, ScoreboardWidth, 710.0f + TitleHeight};

	if(Teams)
	{
		const char *pRedTeamName = GetTeamName(TEAM_RED);
		const char *pBlueTeamName = GetTeamName(TEAM_BLUE);

		// Game over title
		const CNetObj_GameData *pGameDataObj = GameClient()->m_Snap.m_pGameDataObj;
		if((pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER) && pGameDataObj)
		{
			char aTitle[256];
			if(pGameDataObj->m_TeamscoreRed > pGameDataObj->m_TeamscoreBlue)
			{
				TextRender()->TextColor(ColorRGBA(0.975f, 0.17f, 0.17f, 1.0f));
				str_format(aTitle, sizeof(aTitle), Localize("%s wins!"), pRedTeamName);
			}
			else if(pGameDataObj->m_TeamscoreBlue > pGameDataObj->m_TeamscoreRed)
			{
				TextRender()->TextColor(ColorRGBA(0.17f, 0.46f, 0.975f, 1.0f));
				str_format(aTitle, sizeof(aTitle), Localize("%s wins!"), pBlueTeamName);
			}
			else
			{
				TextRender()->TextColor(ColorRGBA(0.91f, 0.78f, 0.33f, 1.0f));
				str_copy(aTitle, Localize("Draw!"));
			}

			const float TitleFontSize = 72.0f;
			CUIRect GameOverTitle = {Scoreboard.x, Scoreboard.y - TitleFontSize - 12.0f, Scoreboard.w, TitleFontSize};
			Ui()->DoLabel(&GameOverTitle, aTitle, TitleFontSize, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}

		CUIRect RedScoreboard, BlueScoreboard, RedTitle, BlueTitle;
		Scoreboard.VSplitMid(&RedScoreboard, &BlueScoreboard, 15.0f);
		RedScoreboard.HSplitTop(TitleHeight, &RedTitle, &RedScoreboard);
		BlueScoreboard.HSplitTop(TitleHeight, &BlueTitle, &BlueScoreboard);

		RedTitle.Draw(ColorRGBA(0.975f, 0.17f, 0.17f, 0.5f), IGraphics::CORNER_T, 15.0f);
		BlueTitle.Draw(ColorRGBA(0.17f, 0.46f, 0.975f, 0.5f), IGraphics::CORNER_T, 15.0f);
		RedScoreboard.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_B, 15.0f);
		BlueScoreboard.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_B, 15.0f);

		RenderTitle(RedTitle, TEAM_RED, pRedTeamName);
		RenderTitle(BlueTitle, TEAM_BLUE, pBlueTeamName);
		RenderScoreboard(RedScoreboard, TEAM_RED, 0, NumPlayers);
		RenderScoreboard(BlueScoreboard, TEAM_BLUE, 0, NumPlayers);
	}
	else
	{
		Scoreboard.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 15.0f);

		const char *pTitle;
		if(pGameInfoObj && (pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER))
		{
			pTitle = Localize("Game over");
		}
		else
		{
			pTitle = Client()->GetCurrentMap();
		}

		CUIRect Title;
		Scoreboard.HSplitTop(TitleHeight, &Title, &Scoreboard);
		RenderTitle(Title, TEAM_RED, pTitle);

		if(NumPlayers <= 16)
		{
			RenderScoreboard(Scoreboard, TEAM_RED, 0, NumPlayers);
		}
		else
		{
			int PlayersPerSide;
			if(NumPlayers <= 24)
				PlayersPerSide = 12;
			else if(NumPlayers <= 32)
				PlayersPerSide = 16;
			else if(NumPlayers <= 48)
				PlayersPerSide = 24;
			else
				PlayersPerSide = 32;

			CUIRect LeftScoreboard, RightScoreboard;
			Scoreboard.VSplitMid(&LeftScoreboard, &RightScoreboard);
			RenderScoreboard(LeftScoreboard, TEAM_RED, 0, PlayersPerSide);
			RenderScoreboard(RightScoreboard, TEAM_RED, PlayersPerSide, 2 * PlayersPerSide);
		}
	}

	CUIRect Spectators = {(Width - ScoreboardSmallWidth) / 2.0f, Scoreboard.y + Scoreboard.h + 10.0f, ScoreboardSmallWidth, 200.0f};
	if(pGameInfoObj && (pGameInfoObj->m_ScoreLimit || pGameInfoObj->m_TimeLimit || (pGameInfoObj->m_RoundNum && pGameInfoObj->m_RoundCurrent)))
	{
		CUIRect Goals;
		Spectators.HSplitTop(50.0f, &Goals, &Spectators);
		Spectators.HSplitTop(10.0f, nullptr, &Spectators);
		RenderGoals(Goals);
	}
	RenderSpectators(Spectators);

	RenderRecordingNotification((Width / 7) * 4 + 20);
}

bool CScoreboard::Active() const
{
	// if statboard is active don't show scoreboard
	if(GameClient()->m_Statboard.IsActive())
		return false;

	if(m_Active)
		return true;

	if(GameClient()->m_Snap.m_pLocalInfo && !GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		// we are not a spectator, check if we are dead
		if(!GameClient()->m_Snap.m_pLocalCharacter && g_Config.m_ClScoreboardOnDeath)
			return true;
	}

	// if the game is over
	const CNetObj_GameInfo *pGameInfoObj = GameClient()->m_Snap.m_pGameInfoObj;
	if(pGameInfoObj && pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetTeamName(int Team) const
{
	dbg_assert(Team == TEAM_RED || Team == TEAM_BLUE, "Team invalid");

	int ClanPlayers = 0;
	const char *pClanName = nullptr;
	const char *pDefaultTeamName = Team == TEAM_RED ? Localize("Red team") : Localize("Blue team");
	for(const CNetObj_PlayerInfo *pInfo : GameClient()->m_Snap.m_apInfoByScore)
	{
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(!pClanName)
		{
			pClanName = GameClient()->m_aClients[pInfo->m_ClientId].m_aClan;
			ClanPlayers++;
		}
		else
		{
			if(str_comp(GameClient()->m_aClients[pInfo->m_ClientId].m_aClan, pClanName) == 0)
				ClanPlayers++;
			else
				return pDefaultTeamName;
		}
	}

	if(ClanPlayers > 1 && pClanName[0] != '\0')
		return pClanName;
	else
		return pDefaultTeamName;
}
