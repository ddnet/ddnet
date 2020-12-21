/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/motd.h>
#include <game/client/components/skins.h>
#include <game/client/components/statboard.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/localization.h>

#include "scoreboard.h"

#include <base/tl/string.h>
#include <engine/serverbrowser.h>

CScoreboard::CScoreboard()
{
	OnReset();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = (CScoreboard *)pUserData;
	pSelf->m_Active = pResult->GetInteger(0) != 0;
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
		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;
		m_ServerRecord = (float)pMsg->m_ServerTimeBest / 100;
		//m_PlayerRecord = (float)pMsg->m_PlayerTimeBest/100;
	}
}

void CScoreboard::OnConsoleInit()
{
	Console()->Register("+scoreboard", "", CFGFLAG_CLIENT, ConKeyScoreboard, this, "Show scoreboard");
}

void CScoreboard::RenderGoals(float x, float y, float w)
{
	float h = 50.0f;

	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.5f);
	RenderTools()->DrawRoundRect(x, y, w, h, 10.0f);
	Graphics()->QuadsEnd();

	// render goals
	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit);
			TextRender()->Text(0, x + 10.0f, y + (h - 20.f) / 2.f, 20.0f, aBuf, -1.0f);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit);
			TextRender()->Text(0, x + 230.0f, y + (h - 20.f) / 2.f, 20.0f, aBuf, -1.0f);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum && m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s %d/%d", Localize("Round"), m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent, m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum);
			float tw = TextRender()->TextWidth(0, 20.0f, aBuf, -1, -1.0f);
			TextRender()->Text(0, x + w - tw - 10.0f, y + (h - 20.f) / 2.f, 20.0f, aBuf, -1.0f);
		}
	}
}

void CScoreboard::RenderSpectators(float x, float y, float w)
{
	float h = 140.0f;

	// background
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0, 0, 0, 0.5f);
	RenderTools()->DrawRoundRect(x, y, w, h, 10.0f);
	Graphics()->QuadsEnd();

	// Headline
	y += 10.0f;
	TextRender()->Text(0, x + 10.0f, y + (30.f - 28.f) / 2.f, 28.0f, Localize("Spectators"), w - 20.0f);

	// spectator names
	y += 30.0f;
	bool Multiple = false;

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, x + 10.0f, y, 22.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = w - 20.0f;
	Cursor.m_MaxLines = 4;

	for(const auto *pInfo : m_pClient->m_Snap.m_paInfoByName)
	{
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;

		if(Multiple)
			TextRender()->TextEx(&Cursor, ", ", 2);

		if(m_pClient->m_aClients[pInfo->m_ClientID].m_AuthLevel)
		{
			ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
			TextRender()->TextColor(Color);
		}

		if(g_Config.m_ClShowIDs)
		{
			char aBuffer[5];
			int size = str_format(aBuffer, sizeof(aBuffer), "%d: ", pInfo->m_ClientID);
			TextRender()->TextEx(&Cursor, aBuffer, size);
		}
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		Multiple = true;
	}
}

void CScoreboard::RenderScoreboard(float x, float y, float w, int Team, const char *pTitle, int NumPlayers)
{
	if(Team == TEAM_SPECTATORS)
		return;

	bool lower16 = false;
	bool upper16 = false;
	bool lower24 = false;
	bool upper24 = false;
	bool lower32 = false;
	bool upper32 = false;

	if(Team == -3)
		upper16 = true;
	else if(Team == -4)
		lower32 = true;
	else if(Team == -5)
		upper32 = true;
	else if(Team == -6)
		lower16 = true;
	else if(Team == -7)
		lower24 = true;
	else if(Team == -8)
		upper24 = true;

	if(Team < -1)
		Team = 0;

	if(NumPlayers < 0)
		NumPlayers = m_pClient->m_Snap.m_aTeamSize[Team];

	float h = 760.0f;

	// background
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.5f);
	if(upper16 || upper32 || upper24)
		RenderTools()->DrawRoundRectExt(x, y, w, h, 17.0f, 10);
	else if(lower16 || lower32 || lower24)
		RenderTools()->DrawRoundRectExt(x, y, w, h, 17.0f, 5);
	else
		RenderTools()->DrawRoundRect(x, y, w, h, 17.0f);
	Graphics()->QuadsEnd();

	char aBuf[128] = {0};

	// render title
	float TitleFontsize = 40.0f;
	if(!pTitle)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
			pTitle = Localize("Game over");
		else
		{
			str_utf8_truncate(aBuf, sizeof(aBuf), Client()->GetCurrentMap(), 16);
			if(str_comp(aBuf, Client()->GetCurrentMap()))
				str_append(aBuf, "…", sizeof(aBuf));
			pTitle = aBuf;
		}
	}
	TextRender()->Text(0, x + 20.0f, y + (50.f - TitleFontsize) / 2.f, TitleFontsize, pTitle, -1.0f);

	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
	{
		if(m_pClient->m_Snap.m_pGameDataObj)
		{
			int Score = Team == TEAM_RED ? m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed : m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
	}
	else
	{
		if(m_pClient->m_Snap.m_SpecInfo.m_Active && m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW &&
			m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID])
		{
			int Score = m_pClient->m_Snap.m_paPlayerInfos[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID]->m_Score;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
		else if(m_pClient->m_Snap.m_pLocalInfo)
		{
			int Score = m_pClient->m_Snap.m_pLocalInfo->m_Score;
			str_format(aBuf, sizeof(aBuf), "%d", Score);
		}
	}

	if(m_pClient->m_GameInfo.m_TimeScore && g_Config.m_ClDDRaceScoreBoard)
	{
		if(m_ServerRecord > 0)
			str_time_float(m_ServerRecord, TIME_HOURS, aBuf, sizeof(aBuf));
		else
			aBuf[0] = 0;
	}

	float tw;

	if(!lower16 && !lower32 && !lower24)
	{
		tw = TextRender()->TextWidth(0, TitleFontsize, aBuf, -1, -1.0f);
		TextRender()->Text(0, x + w - tw - 20.0f, y + (50.f - TitleFontsize) / 2.f, TitleFontsize, aBuf, -1.0f);
	}

	// calculate measurements
	x += 10.0f;
	float LineHeight = 60.0f;
	float TeeSizeMod = 1.0f;
	float Spacing = 16.0f;
	float RoundRadius = 15.0f;
	float FontSize = 24.0f;
	if(NumPlayers > 48)
	{
		LineHeight = 20.0f;
		TeeSizeMod = 0.4f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
		FontSize = 16.0f;
	}
	else if(NumPlayers > 32)
	{
		LineHeight = 27.0f;
		TeeSizeMod = 0.6f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
		FontSize = 20.0f;
	}
	else if(NumPlayers > 12)
	{
		LineHeight = 40.0f;
		TeeSizeMod = 0.8f;
		Spacing = 0.0f;
		RoundRadius = 15.0f;
	}
	else if(NumPlayers > 8)
	{
		LineHeight = 50.0f;
		TeeSizeMod = 0.9f;
		Spacing = 5.0f;
		RoundRadius = 15.0f;
	}

	float ScoreOffset = x + 10.0f, ScoreLength = TextRender()->TextWidth(0, FontSize, "00:00:00", -1, -1.0f);
	float TeeOffset = ScoreOffset + ScoreLength + 15.0f, TeeLength = 60 * TeeSizeMod;
	float NameOffset = TeeOffset + TeeLength, NameLength = 300.0f - TeeLength;
	float PingOffset = x + 610.0f, PingLength = 65.0f;
	float CountryOffset = PingOffset - (LineHeight - Spacing - TeeSizeMod * 5.0f) * 2.0f, CountryLength = (LineHeight - Spacing - TeeSizeMod * 5.0f) * 2.0f;
	float ClanOffset = x + 360.0f, ClanLength = 240.0f - CountryLength;

	// render headlines
	y += 50.0f;
	float HeadlineFontsize = 22.0f;
	const char *pScore = (m_pClient->m_GameInfo.m_TimeScore && g_Config.m_ClDDRaceScoreBoard) ? Localize("Time") : Localize("Score");
	tw = TextRender()->TextWidth(0, HeadlineFontsize, pScore, -1, -1.0f);
	TextRender()->Text(0, ScoreOffset + ScoreLength - tw, y + (HeadlineFontsize * 2.f - HeadlineFontsize) / 2.f, HeadlineFontsize, pScore, -1.0f);

	TextRender()->Text(0, NameOffset, y + (HeadlineFontsize * 2.f - HeadlineFontsize) / 2.f, HeadlineFontsize, Localize("Name"), -1.0f);

	tw = TextRender()->TextWidth(0, HeadlineFontsize, Localize("Clan"), -1, -1.0f);
	TextRender()->Text(0, ClanOffset + ClanLength / 2 - tw / 2, y + (HeadlineFontsize * 2.f - HeadlineFontsize) / 2.f, HeadlineFontsize, Localize("Clan"), -1.0f);

	tw = TextRender()->TextWidth(0, HeadlineFontsize, Localize("Ping"), -1, -1.0f);
	TextRender()->Text(0, PingOffset + PingLength - tw, y + (HeadlineFontsize * 2.f - HeadlineFontsize) / 2.f, HeadlineFontsize, Localize("Ping"), -1.0f);

	// render player entries
	y += HeadlineFontsize * 2.0f;
	CTextCursor Cursor;

	int rendered = 0;
	if(upper16)
		rendered = -16;
	if(upper32)
		rendered = -32;
	if(upper24)
		rendered = -24;

	int OldDDTeam = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(rendered++ < 0)
			continue;

		int DDTeam = m_pClient->m_Teams.Team(pInfo->m_ClientID);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeamScore[j];

			if(!pInfo2 || pInfo2->m_Team != Team)
				continue;

			NextDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientID);
			break;
		}

		if(OldDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeamScore[j];

				if(!pInfo2 || pInfo2->m_Team != Team)
					continue;

				OldDDTeam = m_pClient->m_Teams.Team(pInfo2->m_ClientID);
				break;
			}
		}

		if(DDTeam != TEAM_FLOCK)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			ColorRGBA rgb = color_cast<ColorRGBA>(ColorHSLA(DDTeam / 64.0f, 1.0f, 0.5f, 0.5f));
			Graphics()->SetColor(rgb);

			int Corners = 0;

			if(OldDDTeam != DDTeam)
				Corners |= CUI::CORNER_TL | CUI::CORNER_TR;
			if(NextDDTeam != DDTeam)
				Corners |= CUI::CORNER_BL | CUI::CORNER_BR;

			RenderTools()->DrawRoundRectExt(x - 10.0f, y, w, LineHeight + Spacing, RoundRadius, Corners);

			Graphics()->QuadsEnd();

			if(NextDDTeam != DDTeam)
			{
				char aBuf[64];
				if(m_pClient->m_Snap.m_aTeamSize[0] > 8)
				{
					str_format(aBuf, sizeof(aBuf), "%d", DDTeam);
					TextRender()->SetCursor(&Cursor, x - 10.0f, y + Spacing + FontSize - (FontSize / 1.5f), FontSize / 1.5f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
					Cursor.m_LineWidth = NameLength + 3;
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "Team %d", DDTeam);
					tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
					TextRender()->SetCursor(&Cursor, ScoreOffset + w / 2.0f - tw / 2.0f, y + LineHeight, FontSize / 1.5f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
					Cursor.m_LineWidth = NameLength + 3;
				}
				TextRender()->TextEx(&Cursor, aBuf, -1);
			}
		}

		OldDDTeam = DDTeam;

		// background so it's easy to find the local player or the followed one in spectator mode
		if(pInfo->m_Local || (m_pClient->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientID == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID))
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
			RenderTools()->DrawRoundRect(x, y, w - 20.0f, LineHeight, RoundRadius);
			Graphics()->QuadsEnd();
		}

		// score
		if(m_pClient->m_GameInfo.m_TimeScore && g_Config.m_ClDDRaceScoreBoard)
		{
			if(pInfo->m_Score == -9999)
				aBuf[0] = 0;
			else
				str_time((int64)abs(pInfo->m_Score) * 100, TIME_HOURS, aBuf, sizeof(aBuf));
		}
		else
			str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Score, -999, 99999));
		tw = TextRender()->TextWidth(0, FontSize, aBuf, -1, -1.0f);
		TextRender()->SetCursor(&Cursor, ScoreOffset + ScoreLength - tw, y + (LineHeight - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);

		// flag
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS &&
			m_pClient->m_Snap.m_pGameDataObj && (m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientID || m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientID))
		{
			Graphics()->BlendNormal();
			if(m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientID)
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
			else
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);

			Graphics()->QuadsBegin();
			Graphics()->QuadsSetSubset(1, 0, 0, 1);

			float Size = LineHeight;
			IGraphics::CQuadItem QuadItem(TeeOffset + 0.0f, y - 5.0f - Spacing / 2.0f, Size / 2.0f, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// avatar
		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[pInfo->m_ClientID].m_RenderInfo;
		TeeInfo.m_Size *= TeeSizeMod;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), vec2(TeeOffset + TeeLength / 2, y + LineHeight / 2));

		// name
		TextRender()->SetCursor(&Cursor, NameOffset, y + (LineHeight - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		if(m_pClient->m_aClients[pInfo->m_ClientID].m_AuthLevel)
		{
			ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClAuthedPlayerColor));
			TextRender()->TextColor(Color);
		}

		if(g_Config.m_ClShowIDs)
		{
			char aId[64] = "";
			str_format(aId, sizeof(aId), "%2d: %s", pInfo->m_ClientID, m_pClient->m_aClients[pInfo->m_ClientID].m_aName);
			Cursor.m_LineWidth = NameLength;
			TextRender()->TextEx(&Cursor, aId, -1);
		}
		else
		{
			Cursor.m_LineWidth = NameLength;
			TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, -1);
		}

		// clan
		if(str_comp(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan,
			   m_pClient->m_aClients[GameClient()->m_LocalIDs[g_Config.m_ClDummy]].m_aClan) == 0)
		{
			ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClSameClanColor));
			TextRender()->TextColor(Color);
		}
		else
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		tw = TextRender()->TextWidth(nullptr, FontSize, m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, -1, -1.0f);
		TextRender()->SetCursor(&Cursor, ClanOffset + ClanLength / 2 - tw / 2, y + (LineHeight - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = ClanLength;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, -1);

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		// country flag
		ColorRGBA Color(1.0f, 1.0f, 1.0f, 0.5f);
		m_pClient->m_pCountryFlags->Render(m_pClient->m_aClients[pInfo->m_ClientID].m_Country, &Color,
			CountryOffset, y + (Spacing + TeeSizeMod * 5.0f) / 2.0f, CountryLength, LineHeight - Spacing - TeeSizeMod * 5.0f);

		// ping
		if(g_Config.m_ClEnablePingColor)
		{
			ColorRGBA rgb = color_cast<ColorRGBA>(ColorHSLA((300.0f - clamp(pInfo->m_Latency, 0, 300)) / 1000.0f, 1.0f, 0.5f));
			TextRender()->TextColor(rgb);
		}
		str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Latency, 0, 999));
		tw = TextRender()->TextWidth(nullptr, FontSize, aBuf, -1, -1.0f);
		TextRender()->SetCursor(&Cursor, PingOffset + PingLength - tw, y + (LineHeight - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = PingLength;
		TextRender()->TextEx(&Cursor, aBuf, -1);

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

		y += LineHeight + Spacing;
		if(lower32 || upper32)
		{
			if(rendered == 32)
				break;
		}
		else if(lower24 || upper24)
		{
			if(rendered == 24)
				break;
		}
		else
		{
			if(rendered == 16)
				break;
		}
	}
}

void CScoreboard::RenderRecordingNotification(float x)
{
	if(!m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording() &&
		!m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording() &&
		!m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording() &&
		!m_pClient->DemoRecorder(RECORDER_REPLAYS)->IsRecording())
	{
		return;
	}

	//draw the text
	char aBuf[64] = "\0";
	char aBuf2[64];
	char aTime[32];

	if(m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		str_time((int64)m_pClient->DemoRecorder(RECORDER_MANUAL)->Length() * 100, TIME_HOURS, aTime, sizeof(aTime));
		str_format(aBuf2, sizeof(aBuf2), "%s %s  ", Localize("Manual"), aTime);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording())
	{
		str_time((int64)m_pClient->DemoRecorder(RECORDER_RACE)->Length() * 100, TIME_HOURS, aTime, sizeof(aTime));
		str_format(aBuf2, sizeof(aBuf2), "%s %s  ", Localize("Race"), aTime);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording())
	{
		str_time((int64)m_pClient->DemoRecorder(RECORDER_AUTO)->Length() * 100, TIME_HOURS, aTime, sizeof(aTime));
		str_format(aBuf2, sizeof(aBuf2), "%s %s  ", Localize("Auto"), aTime);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_REPLAYS)->IsRecording())
	{
		str_time((int64)m_pClient->DemoRecorder(RECORDER_REPLAYS)->Length() * 100, TIME_HOURS, aTime, sizeof(aTime));
		str_format(aBuf2, sizeof(aBuf2), "%s %s  ", Localize("Replay"), aTime);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}

	float w = TextRender()->TextWidth(0, 20.0f, aBuf, -1, -1.0f);

	//draw the box
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(x, 0.0f, w + 60.0f, 50.0f, 15.0f, CUI::CORNER_B);
	Graphics()->QuadsEnd();

	//draw the red dot
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	RenderTools()->DrawRoundRect(x + 20, 15.0f, 20.0f, 20.0f, 10.0f);
	Graphics()->QuadsEnd();

	TextRender()->Text(0, x + 50.0f, (50.f - 20.f) / 2.f, 20.0f, aBuf, -1.0f);
}

void CScoreboard::OnRender()
{
	if(!Active())
		return;

	// if the score board is active, then we should clear the motd message as well
	if(m_pClient->m_pMotd->IsActive())
		m_pClient->m_pMotd->Clear();

	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	float w = 700.0f;

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS))
		{
			if(m_pClient->m_Snap.m_aTeamSize[0] > 48)
			{
				RenderScoreboard(Width / 2 - w, 150.0f, w, -4, 0);
				RenderScoreboard(Width / 2, 150.0f, w, -5, "");
			}
			else if(m_pClient->m_Snap.m_aTeamSize[0] > 32)
			{
				RenderScoreboard(Width / 2 - w, 150.0f, w, -7, 0);
				RenderScoreboard(Width / 2, 150.0f, w, -8, "");
			}
			else if(m_pClient->m_Snap.m_aTeamSize[0] > 16)
			{
				RenderScoreboard(Width / 2 - w, 150.0f, w, -6, 0);
				RenderScoreboard(Width / 2, 150.0f, w, -3, "");
			}
			else
			{
				RenderScoreboard(Width / 2 - w / 2, 150.0f, w, 0, 0);
			}
		}
		else
		{
			const char *pRedClanName = GetClanName(TEAM_RED);
			const char *pBlueClanName = GetClanName(TEAM_BLUE);

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER && m_pClient->m_Snap.m_pGameDataObj)
			{
				char aText[256];
				str_copy(aText, Localize("Draw!"), sizeof(aText));

				if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue)
				{
					if(pRedClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pRedClanName);
					else
						str_copy(aText, Localize("Red team wins!"), sizeof(aText));
				}
				else if(m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue > m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed)
				{
					if(pBlueClanName)
						str_format(aText, sizeof(aText), Localize("%s wins!"), pBlueClanName);
					else
						str_copy(aText, Localize("Blue team wins!"), sizeof(aText));
				}

				float w = TextRender()->TextWidth(0, 86.0f, aText, -1, -1.0f);
				TextRender()->Text(0, Width / 2 - w / 2, 39, 86.0f, aText, -1.0f);
			}

			int NumPlayers = maximum(m_pClient->m_Snap.m_aTeamSize[TEAM_RED], m_pClient->m_Snap.m_aTeamSize[TEAM_BLUE]);
			RenderScoreboard(Width / 2 - w - 5.0f, 150.0f, w, TEAM_RED, pRedClanName ? pRedClanName : Localize("Red team"), NumPlayers);
			RenderScoreboard(Width / 2 + 5.0f, 150.0f, w, TEAM_BLUE, pBlueClanName ? pBlueClanName : Localize("Blue team"), NumPlayers);
		}
	}

	RenderGoals(Width / 2 - w / 2, 150 + 760 + 10, w);
	RenderSpectators(Width / 2 - w / 2, 150 + 760 + 10 + 50 + 10, w);
	RenderRecordingNotification((Width / 7) * 4);
}

bool CScoreboard::Active()
{
	// if statboard is active don't show scoreboard
	if(m_pClient->m_pStatboard->IsActive())
		return false;

	if(m_Active)
		return true;

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
	{
		// we are not a spectator, check if we are dead
		if(!m_pClient->m_Snap.m_pLocalCharacter && g_Config.m_ClScoreboardOnDeath)
			return true;
	}

	// if the game is over
	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetClanName(int Team)
{
	int ClanPlayers = 0;
	const char *pClanName = 0;
	for(const auto *pInfo : m_pClient->m_Snap.m_paInfoByScore)
	{
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if(!pClanName)
		{
			pClanName = m_pClient->m_aClients[pInfo->m_ClientID].m_aClan;
			ClanPlayers++;
		}
		else
		{
			if(str_comp(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, pClanName) == 0)
				ClanPlayers++;
			else
				return 0;
		}
	}

	if(ClanPlayers > 1 && pClanName[0])
		return pClanName;
	else
		return 0;
}
