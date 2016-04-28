/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/localization.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/motd.h>
#include <game/client/components/statboard.h>

#include "scoreboard.h"

#include <time.h>
#include <base/tl/string.h>
#include <engine/serverbrowser.h>

CScoreboard::CScoreboard()
{
	OnReset();
}

void CScoreboard::ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData)
{
	CScoreboard *pSelf = (CScoreboard *)pUserData;
	CServerInfo Info;

	pSelf->Client()->GetServerInfo(&Info);
	pSelf->m_IsGameTypeRace = IsRace(&Info);
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
		m_ServerRecord = (float)pMsg->m_ServerTimeBest/100;
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
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.5f);
	RenderTools()->DrawRoundRect(x, y, w, h, 10.0f);
	Graphics()->QuadsEnd();

	// render goals
	y += 10.0f;
	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit);
			TextRender()->Text(0, x+10.0f, y, 20.0f, aBuf, -1);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit);
			TextRender()->Text(0, x+230.0f, y, 20.0f, aBuf, -1);
		}
		if(m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum && m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s %d/%d", Localize("Round"), m_pClient->m_Snap.m_pGameInfoObj->m_RoundCurrent, m_pClient->m_Snap.m_pGameInfoObj->m_RoundNum);
			float tw = TextRender()->TextWidth(0, 20.0f, aBuf, -1);
			TextRender()->Text(0, x+w-tw-10.0f, y, 20.0f, aBuf, -1);
		}
	}
}

void CScoreboard::RenderSpectators(float x, float y, float w)
{
	float h = 140.0f;

	// background
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.5f);
	RenderTools()->DrawRoundRect(x, y, w, h, 10.0f);
	Graphics()->QuadsEnd();

	// Headline
	y += 10.0f;
	TextRender()->Text(0, x+10.0f, y, 28.0f, Localize("Spectators"), w-20.0f);

	// spectator names
	y += 30.0f;
	char aBuffer[1024*4];
	aBuffer[0] = 0;
	bool Multiple = false;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByName[i];
		if(!pInfo || pInfo->m_Team != TEAM_SPECTATORS)
			continue;

		if(Multiple)
			str_append(aBuffer, ", ", sizeof(aBuffer));
		if(g_Config.m_ClShowIDs)
		{
			char aId[5];
			str_format(aId,sizeof(aId),"%d: ",pInfo->m_ClientID);
			str_append(aBuffer, aId, sizeof(aBuffer));
		}
		str_append(aBuffer, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, sizeof(aBuffer));
		Multiple = true;
	}
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, x+10.0f, y, 22.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = w-20.0f;
	Cursor.m_MaxLines = 4;
	TextRender()->TextEx(&Cursor, aBuffer, -1);
}

void CScoreboard::RenderScoreboard(float x, float y, float w, int Team, const char *pTitle)
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

	float h = 760.0f;

	// background
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.5f);
	if(upper16 || upper32 || upper24)
		RenderTools()->DrawRoundRectExt(x, y, w, h, 17.0f, 10);
	else if(lower16 || lower32 || lower24)
		RenderTools()->DrawRoundRectExt(x, y, w, h, 17.0f, 5);
	else
		RenderTools()->DrawRoundRect(x, y, w, h, 17.0f);
	Graphics()->QuadsEnd();

	// render title
	float TitleFontsize = 40.0f;
	if(!pTitle)
	{
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			pTitle = Localize("Game over");
		else
			pTitle = Localize("Score board");
	}
	TextRender()->Text(0, x+20.0f, y, TitleFontsize, pTitle, -1);

	char aBuf[128] = {0};

	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
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

	if(m_IsGameTypeRace && g_Config.m_ClDDRaceScoreBoard)
	{
		if (m_ServerRecord > 0)
		{
			str_format(aBuf, sizeof(aBuf), "%02d:%02d", ((int) m_ServerRecord)/60, ((int) m_ServerRecord)%60);
		}
		else
			aBuf[0] = 0;
	}

	float tw;

	if (!lower16 && !lower32 && !lower24)
	{
		tw = TextRender()->TextWidth(0, TitleFontsize, aBuf, -1);
		TextRender()->Text(0, x+w-tw-20.0f, y, TitleFontsize, aBuf, -1);
	}

	// calculate measurements
	x += 10.0f;
	float LineHeight = 60.0f;
	float TeeSizeMod = 1.0f;
	float Spacing = 16.0f;
	float RoundRadius = 15.0f;
	if(m_pClient->m_Snap.m_aTeamSize[Team] > 48)
	{
		LineHeight = 20.0f;
		TeeSizeMod = 0.4f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
	}
	else if(m_pClient->m_Snap.m_aTeamSize[Team] > 32)
	{
		LineHeight = 27.0f;
		TeeSizeMod = 0.6f;
		Spacing = 0.0f;
		RoundRadius = 5.0f;
	}
	else if(m_pClient->m_Snap.m_aTeamSize[Team] > 12)
	{
		LineHeight = 40.0f;
		TeeSizeMod = 0.8f;
		Spacing = 0.0f;
		RoundRadius = 15.0f;
	}
	else if(m_pClient->m_Snap.m_aTeamSize[Team] > 8)
	{
		LineHeight = 50.0f;
		TeeSizeMod = 0.9f;
		Spacing = 5.0f;
		RoundRadius = 15.0f;
	}

	float ScoreOffset = x+10.0f, ScoreLength = TextRender()->TextWidth(0, 22.0f/*HeadlineFontsize*/, "00:00:0", -1);
	float TeeOffset = ScoreOffset+ScoreLength, TeeLength = 60*TeeSizeMod;
	float NameOffset = TeeOffset+TeeLength, NameLength = 300.0f-TeeLength;
	float PingOffset = x+610.0f, PingLength = 65.0f;
	float CountryOffset = PingOffset-(LineHeight-Spacing-TeeSizeMod*5.0f)*2.0f, CountryLength = (LineHeight-Spacing-TeeSizeMod*5.0f)*2.0f;
	float ClanOffset = x+370.0f, ClanLength = 230.0f-CountryLength;

	// render headlines
	y += 50.0f;
	float HeadlineFontsize = 22.0f;
	float ScoreWidth = TextRender()->TextWidth(0, HeadlineFontsize, Localize("Score"), -1);
	tw = ScoreLength > ScoreWidth ? ScoreLength : ScoreWidth;
	TextRender()->Text(0, ScoreOffset+ScoreLength-tw, y, HeadlineFontsize, Localize("Score"), -1);

	TextRender()->Text(0, NameOffset, y, HeadlineFontsize, Localize("Name"), -1);

	tw = TextRender()->TextWidth(0, HeadlineFontsize, Localize("Clan"), -1);
	TextRender()->Text(0, ClanOffset+ClanLength/2-tw/2, y, HeadlineFontsize, Localize("Clan"), -1);

	tw = TextRender()->TextWidth(0, HeadlineFontsize, Localize("Ping"), -1);
	TextRender()->Text(0, PingOffset+PingLength-tw, y, HeadlineFontsize, Localize("Ping"), -1);

	// render player entries
	y += HeadlineFontsize*2.0f;
	float FontSize = 24.0f;
	if(m_pClient->m_Snap.m_aTeamSize[Team] > 48)
		FontSize = 16.0f;
	else if(m_pClient->m_Snap.m_aTeamSize[Team] > 32)
		FontSize = 20.0f;
	CTextCursor Cursor;

	int rendered = 0;
	if (upper16)
		rendered = -16;
	if (upper32)
		rendered = -32;
	if (upper24)
		rendered = -24;

	int OldDDTeam = -1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeam[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		if (rendered++ < 0) continue;

		int DDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo->m_ClientID);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeam[j];

			if(!pInfo2 || pInfo2->m_Team != Team)
				continue;

			NextDDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo2->m_ClientID);
			break;
		}

		if (OldDDTeam == -1)
		{
			for (int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = m_pClient->m_Snap.m_paInfoByDDTeam[j];

				if(!pInfo2 || pInfo2->m_Team != Team)
					continue;

				OldDDTeam = ((CGameClient *) m_pClient)->m_Teams.Team(pInfo2->m_ClientID);
				break;
			}
		}

		if (DDTeam != TEAM_FLOCK)
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			vec3 rgb = HslToRgb(vec3(DDTeam / 64.0f, 1.0f, 0.5f));
			Graphics()->SetColor(rgb.r, rgb.g, rgb.b, 0.5f);

			int Corners = 0;

			if (OldDDTeam != DDTeam)
				Corners |= CUI::CORNER_TL | CUI::CORNER_TR;
			if (NextDDTeam != DDTeam)
				Corners |= CUI::CORNER_BL | CUI::CORNER_BR;

			RenderTools()->DrawRoundRectExt(x - 10.0f, y, w, LineHeight + Spacing, RoundRadius, Corners);

			Graphics()->QuadsEnd();

			if (NextDDTeam != DDTeam)
			{
				char aBuf[64];
				if(m_pClient->m_Snap.m_aTeamSize[0] > 8)
				{
					str_format(aBuf, sizeof(aBuf),"%d", DDTeam);
					TextRender()->SetCursor(&Cursor, x - 10.0f, y + Spacing + FontSize - (FontSize/1.5f), FontSize/1.5f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
					Cursor.m_LineWidth = NameLength+3;
				}
				else
				{
					str_format(aBuf, sizeof(aBuf),"Team %d", DDTeam);
					tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
					TextRender()->SetCursor(&Cursor, ScoreOffset+w/2.0f-tw/2.0f, y + LineHeight - Spacing/3.0f, FontSize/1.5f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
					Cursor.m_LineWidth = NameLength+3;
				}
				TextRender()->TextEx(&Cursor, aBuf, -1);
			}
		}

		OldDDTeam = DDTeam;

		// background so it's easy to find the local player or the followed one in spectator mode
		if(pInfo->m_Local || (m_pClient->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientID == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID))
		{
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
			RenderTools()->DrawRoundRect(x, y, w-20.0f, LineHeight, RoundRadius);
			Graphics()->QuadsEnd();
		}

		// score
		if(m_IsGameTypeRace && g_Config.m_ClDDRaceScoreBoard)
		{
			if (pInfo->m_Score == -9999)
				aBuf[0] = 0;
			else
			{
				int Time = abs(pInfo->m_Score);
				str_format(aBuf, sizeof(aBuf), "%02d:%02d", Time/60, Time%60);
			}
		}
		else
			str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Score, -999, 999));
		tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
		TextRender()->SetCursor(&Cursor, ScoreOffset+ScoreLength-tw, y+Spacing, FontSize, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);

		// flag
		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS &&
			m_pClient->m_Snap.m_pGameDataObj && (m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed == pInfo->m_ClientID ||
			m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == pInfo->m_ClientID))
		{
			Graphics()->BlendNormal();
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
			Graphics()->QuadsBegin();

			RenderTools()->SelectSprite(pInfo->m_Team==TEAM_RED ? SPRITE_FLAG_BLUE : SPRITE_FLAG_RED, SPRITE_FLAG_FLIP_X);

			float Size = LineHeight;
			IGraphics::CQuadItem QuadItem(TeeOffset+0.0f, y-5.0f-Spacing/2.0f, Size/2.0f, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// avatar
		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[pInfo->m_ClientID].m_RenderInfo;
		TeeInfo.m_Size *= TeeSizeMod;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), vec2(TeeOffset+TeeLength/2, y+LineHeight/2));

		// name
		TextRender()->SetCursor(&Cursor, NameOffset, y+Spacing, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		if(g_Config.m_ClShowIDs)
		{
			char aId[64] = "";
			if (pInfo->m_ClientID >= 10)
				str_format(aId, sizeof(aId),"%d: ", pInfo->m_ClientID);
			else
				str_format(aId, sizeof(aId)," %d: ", pInfo->m_ClientID);
			str_append(aId, m_pClient->m_aClients[pInfo->m_ClientID].m_aName,sizeof(aId));
			Cursor.m_LineWidth = NameLength+3;
			TextRender()->TextEx(&Cursor, aId, -1);
		}
		else
		{
			Cursor.m_LineWidth = NameLength;
			TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, -1);
		}

		// clan
		tw = TextRender()->TextWidth(0, FontSize, m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, -1);
		TextRender()->SetCursor(&Cursor, ClanOffset+ClanLength/2-tw/2, y+Spacing, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = ClanLength;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, -1);

		// country flag
		vec4 Color(1.0f, 1.0f, 1.0f, 0.5f);
		m_pClient->m_pCountryFlags->Render(m_pClient->m_aClients[pInfo->m_ClientID].m_Country, &Color,
											CountryOffset, y+(Spacing+TeeSizeMod*5.0f)/2.0f, CountryLength, LineHeight-Spacing-TeeSizeMod*5.0f);

		// ping
		str_format(aBuf, sizeof(aBuf), "%d", clamp(pInfo->m_Latency, 0, 1000));
		tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
		TextRender()->SetCursor(&Cursor, PingOffset+PingLength-tw, y+Spacing, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = PingLength;
		TextRender()->TextEx(&Cursor, aBuf, -1);

		y += LineHeight+Spacing;
		if (lower32 || upper32) {
			if (rendered == 32) break;
		} else if (lower24 || upper24) {
			if (rendered == 24) break;
		} else {
			if (rendered == 16) break;
		}
	}
}

void CScoreboard::RenderRecordingNotification(float x)
{
	if(!m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording() &&
	   !m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording() &&
	   !m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording())
	{
		return;
	}

	//draw the text
	char aBuf[64] = "\0";
	char aBuf2[64];
	int Seconds;

	if(m_pClient->DemoRecorder(RECORDER_MANUAL)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_MANUAL)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Manual %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_RACE)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_RACE)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Race %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}
	if(m_pClient->DemoRecorder(RECORDER_AUTO)->IsRecording())
	{
		Seconds = m_pClient->DemoRecorder(RECORDER_AUTO)->Length();
		str_format(aBuf2, sizeof(aBuf2), Localize("Auto %3d:%02d  "), Seconds/60, Seconds%60);
		str_append(aBuf, aBuf2, sizeof(aBuf));
	}

	float w = TextRender()->TextWidth(0, 20.0f, aBuf, -1);

	//draw the box
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(x, 0.0f, w+60.0f, 50.0f, 15.0f, CUI::CORNER_B);
	Graphics()->QuadsEnd();

	//draw the red dot
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	RenderTools()->DrawRoundRect(x+20, 15.0f, 20.0f, 20.0f, 10.0f);
	Graphics()->QuadsEnd();

	TextRender()->Text(0, x+50.0f, 10.0f, 20.0f, aBuf, -1);
}


void CScoreboard::OnRender()
{
	if(!Active())
		return;

	// if the score board is active, then we should clear the motd message aswell
	if(m_pClient->m_pMotd->IsActive())
		m_pClient->m_pMotd->Clear();


	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	float w = 700.0f;

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS))
		{
			if(m_pClient->m_Snap.m_aTeamSize[0] > 48)
			{
				RenderScoreboard(Width/2-w, 150.0f, w, -4, 0);
				RenderScoreboard(Width/2, 150.0f, w, -5, "");
			} else if(m_pClient->m_Snap.m_aTeamSize[0] > 32)
			{
				RenderScoreboard(Width/2-w, 150.0f, w, -7, 0);
				RenderScoreboard(Width/2, 150.0f, w, -8, "");
			} else if(m_pClient->m_Snap.m_aTeamSize[0] > 16)
			{
				RenderScoreboard(Width/2-w, 150.0f, w, -6, 0);
				RenderScoreboard(Width/2, 150.0f, w, -3, "");
			} else
			{
				RenderScoreboard(Width/2-w/2, 150.0f, w, 0, 0);
			}
		}
		else
		{
			const char *pRedClanName = GetClanName(TEAM_RED);
			const char *pBlueClanName = GetClanName(TEAM_BLUE);

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER && m_pClient->m_Snap.m_pGameDataObj)
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

				float w = TextRender()->TextWidth(0, 86.0f, aText, -1);
				TextRender()->Text(0, Width/2-w/2, 39, 86.0f, aText, -1);
			}

			RenderScoreboard(Width/2-w-5.0f, 150.0f, w, TEAM_RED, pRedClanName ? pRedClanName : Localize("Red team"));
			RenderScoreboard(Width/2+5.0f, 150.0f, w, TEAM_BLUE, pBlueClanName ? pBlueClanName : Localize("Blue team"));
		}
	}

	RenderGoals(Width/2-w/2, 150+760+10, w);
	RenderSpectators(Width/2-w/2, 150+760+10+50+10, w);
	RenderRecordingNotification((Width/7)*4);
}

bool CScoreboard::Active()
{
	// if statboard is active dont show scoreboard
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
	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
		return true;

	return false;
}

const char *CScoreboard::GetClanName(int Team)
{
	int ClanPlayers = 0;
	const char *pClanName = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];
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
