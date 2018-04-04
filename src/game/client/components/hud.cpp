/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>

#include <game/generated/protocol.h>
#include <game/generated/client_data.h>
#include <game/layers.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/client/components/scoreboard.h>

#include <cmath>

#include "controls.h"
#include "camera.h"
#include "hud.h"
#include "voting.h"
#include "binds.h"

CHud::CHud()
{
	// won't work if zero
	m_FrameTimeAvg = 0.0f;
	m_FPSTextContainerIndex = -1;
	OnReset();
}

void CHud::ResetHudContainers()
{
	if(m_aScoreInfo[0].m_OptionalNameTextContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[0].m_OptionalNameTextContainerIndex);
	if(m_aScoreInfo[1].m_OptionalNameTextContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[1].m_OptionalNameTextContainerIndex);

	if(m_aScoreInfo[0].m_TextRankContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[0].m_TextRankContainerIndex);
	if(m_aScoreInfo[1].m_TextRankContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[1].m_TextRankContainerIndex);

	if(m_aScoreInfo[0].m_TextScoreContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[0].m_TextScoreContainerIndex);
	if(m_aScoreInfo[1].m_TextScoreContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_aScoreInfo[1].m_TextScoreContainerIndex);

	if(m_aScoreInfo[0].m_RoundRectQuadContainerIndex != -1)
		Graphics()->DeleteQuadContainer(m_aScoreInfo[0].m_RoundRectQuadContainerIndex);
	if(m_aScoreInfo[1].m_RoundRectQuadContainerIndex != -1)
		Graphics()->DeleteQuadContainer(m_aScoreInfo[1].m_RoundRectQuadContainerIndex);

	m_aScoreInfo[0].Reset();
	m_aScoreInfo[1].Reset();

	if(m_FPSTextContainerIndex != -1)
		TextRender()->DeleteTextContainer(m_FPSTextContainerIndex);
	m_FPSTextContainerIndex = -1;
}

void CHud::OnWindowResize()
{
	ResetHudContainers();
}


void CHud::OnReset()
{
	m_CheckpointDiff = 0.0f;
	m_DDRaceTime = 0;
	m_LastReceivedTimeTick = 0;
	m_CheckpointTick = 0;
	m_DDRaceTick = 0;
	m_FinishTime = false;
	m_DDRaceTimeReceived = false;
	m_ServerRecord = -1.0f;
	m_PlayerRecord = -1.0f;

	ResetHudContainers();
}

void CHud::OnInit()
{
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	PrepareHealthAmoQuads();

	// all cursors
	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteCursor);
		RenderTools()->QuadContainerAddSprite(m_HudQuadContainerIndex, 64.f);
	}
	
	// the flags
	RenderTools()->SelectSprite(SPRITE_FLAG_RED);
	RenderTools()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 8.f, 16.f);
	RenderTools()->SelectSprite(SPRITE_FLAG_BLUE);
	RenderTools()->QuadContainerAddSprite(m_HudQuadContainerIndex, 0.f, 0.f, 8.f, 16.f);
}

void CHud::RenderGameTimer()
{
	float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;

	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH))
	{
		char Buf[32];
		int Time = 0;
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			Time = m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit*60 - ((Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed());

			if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
				Time = 0;
		}
		else if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME)
		{
			//The Warmup timer is negative in this case to make sure that incompatible clients will not see a warmup timer
			Time = (Client()->GameTick()+m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer)/Client()->GameTickSpeed();
		}
		else
			Time = (Client()->GameTick()-m_pClient->m_Snap.m_pGameInfoObj->m_RoundStartTick)/Client()->GameTickSpeed();

		CServerInfo Info;
		Client()->GetServerInfo(&Info);

		if(Time <= 0 && g_Config.m_ClShowDecisecs)
			str_format(Buf, sizeof(Buf), "00:00.0");
		else if(Time <= 0)
			str_format(Buf, sizeof(Buf), "00:00");
		else if(IsRace(&Info) && !IsDDRace(&Info) && m_ServerRecord >= 0)
			str_format(Buf, sizeof(Buf), "%02d:%02d", (int)(m_ServerRecord*100)/60, ((int)(m_ServerRecord*100)%60));
		else if(g_Config.m_ClShowDecisecs)
			str_format(Buf, sizeof(Buf), "%02d:%02d.%d", Time/60, Time%60, m_DDRaceTick/10);
		else
			str_format(Buf, sizeof(Buf), "%02d:%02d", Time/60, Time%60);
		float FontSize = 10.0f;
		float w;
		if(g_Config.m_ClShowDecisecs)
			w = TextRender()->TextWidth(0, 12,"00:00.0",-1);
		else
			w = TextRender()->TextWidth(0, 12,"00:00",-1);
		// last 60 sec red, last 10 sec blink
		if(m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit && Time <= 60 && (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer <= 0))
		{
			float Alpha = Time <= 10 && (2*time_get()/time_freq()) % 2 ? 0.5f : 1.0f;
			TextRender()->TextColor(1.0f, 0.25f, 0.25f, Alpha);
		}
		TextRender()->Text(0, Half-w/2, 2, FontSize, Buf, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void CHud::RenderPauseNotification()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_PAUSED &&
		!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		const char *pText = Localize("Game paused");
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(0, FontSize,pText, -1);
		TextRender()->Text(0, 150.0f*Graphics()->ScreenAspect()+-w/2.0f, 50.0f, FontSize, pText, -1);
	}
}

void CHud::RenderSuddenDeath()
{
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_SUDDENDEATH)
	{
		float Half = 300.0f*Graphics()->ScreenAspect()/2.0f;
		const char *pText = Localize("Sudden Death");
		float FontSize = 12.0f;
		float w = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->Text(0, Half-w/2, 2, FontSize, pText, -1);
	}
}

void CHud::RenderScoreHud()
{
	// render small score hud
	if(!(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
	{
		int GameFlags = m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags;
		float Whole = 300*Graphics()->ScreenAspect();
		float StartY = 229.0f;

		bool ForceScoreInfoInit = !m_aScoreInfo[0].m_Initialized || !m_aScoreInfo[1].m_Initialized;
		m_aScoreInfo[0].m_Initialized = m_aScoreInfo[1].m_Initialized = true;

		if(GameFlags&GAMEFLAG_TEAMS && m_pClient->m_Snap.m_pGameDataObj)
		{
			char aScoreTeam[2][16];
			str_format(aScoreTeam[TEAM_RED], sizeof(aScoreTeam), "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreRed);
			str_format(aScoreTeam[TEAM_BLUE], sizeof(aScoreTeam), "%d", m_pClient->m_Snap.m_pGameDataObj->m_TeamscoreBlue);

			bool RecreateTeamScore[2] = { str_comp(aScoreTeam[0], m_aScoreInfo[0].m_aScoreText) != 0, str_comp(aScoreTeam[1], m_aScoreInfo[1].m_aScoreText) != 0 };

			int FlagCarrier[2] = { 
				m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierRed, 
				m_pClient->m_Snap.m_pGameDataObj->m_FlagCarrierBlue
			};

			bool RecreateRect = ForceScoreInfoInit;
			for(int t = 0; t < 2; t++)
			{
				if(RecreateTeamScore[t])
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(0, 14.0f, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE], -1);
					mem_copy(m_aScoreInfo[t].m_aScoreText, aScoreTeam[t == 0 ? TEAM_RED : TEAM_BLUE], sizeof(m_aScoreInfo[t].m_aScoreText));
					RecreateRect = true;
				}
			}

			static float s_TextWidth100 = TextRender()->TextWidth(0, 14.0f, "100", -1);
			float ScoreWidthMax = max(max(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth100);
			float Split = 3.0f;
			float ImageSize = GameFlags & GAMEFLAG_FLAGS ? 16.0f : Split;
			for(int t = 0; t < 2; t++)
			{
				// draw box
				if(RecreateRect)
				{	
					if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
						Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == 0)
						Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.25f);
					else
						Graphics()->SetColor(0.0f, 0.0f, 1.0f, 0.25f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = RenderTools()->CreateRoundRectQuadContainer(Whole - ScoreWidthMax - ImageSize - 2 * Split, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split, 18.0f, 5.0f, CUI::CORNER_L);
				}
				Graphics()->TextureSet(-1);
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				// draw score
				if(RecreateTeamScore[t])
				{
					if(m_aScoreInfo[t].m_TextScoreContainerIndex != -1)
						TextRender()->DeleteTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex);

					CTextCursor Cursor;
					TextRender()->SetCursor(&Cursor, Whole - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) / 2 - Split, StartY + t * 20 + (18.f - 14.f) / 2.f, 14.0f, TEXTFLAG_RENDER);
					Cursor.m_LineWidth = -1;
					m_aScoreInfo[t].m_TextScoreContainerIndex = TextRender()->CreateTextContainer(&Cursor, aScoreTeam[t]);
				}
				if(m_aScoreInfo[t].m_TextScoreContainerIndex != -1)
				{
					STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
					STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &TColor, &TOutlineColor);
				}

				if(GameFlags&GAMEFLAG_FLAGS)
				{
					int BlinkTimer = (m_pClient->m_FlagDropTick[t] != 0 &&
										(Client()->GameTick()-m_pClient->m_FlagDropTick[t])/Client()->GameTickSpeed() >= 25) ? 10 : 20;
					if(FlagCarrier[t] == FLAG_ATSTAND || (FlagCarrier[t] == FLAG_TAKEN && ((Client()->GameTick()/BlinkTimer)&1)))
					{
						// draw flag
						Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
						int QuadOffset = NUM_WEAPONS * 10 + 40 + NUM_WEAPONS + t;
						Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, QuadOffset, Whole - ScoreWidthMax - ImageSize, StartY + 1.0f + t * 20);
					}
					else if(FlagCarrier[t] >= 0)
					{
						// draw name of the flag holder
						int ID = FlagCarrier[t]%MAX_CLIENTS;
						const char *pName = m_pClient->m_aClients[ID].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0 || RecreateRect)
						{
							mem_copy(m_aScoreInfo[t].m_aPlayerNameText, pName, sizeof(m_aScoreInfo[t].m_aPlayerNameText));

							if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex != -1)
								TextRender()->DeleteTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex);

							float w = TextRender()->TextWidth(0, 8.0f, pName, -1);	
							
							CTextCursor Cursor;
							TextRender()->SetCursor(&Cursor, min(Whole - w - 1.0f, Whole - ScoreWidthMax - ImageSize - 2 * Split), StartY + (t + 1)*20.0f - 2.0f, 8.0f, TEXTFLAG_RENDER);
							Cursor.m_LineWidth = -1;
							m_aScoreInfo[t].m_OptionalNameTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex != -1)
						{
							STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
							STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &TColor, &TOutlineColor);
						}

						// draw tee of the flag holder
						CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
						Info.m_Size = 18.0f;
						RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
							vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
					}
				}
				StartY += 8.0f;
			}
		}
		else
		{
			int Local = -1;
			int aPos[2] = { 1, 2 };
			const CNetObj_PlayerInfo *apPlayerInfo[2] = { 0, 0 };
			int i = 0;
			for(int t = 0; t < 2 && i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
			{
				if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
				{
					apPlayerInfo[t] = m_pClient->m_Snap.m_paInfoByScore[i];
					if(apPlayerInfo[t]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
						Local = t;
					++t;
				}
			}
			// search local player info if not a spectator, nor within top2 scores
			if(Local == -1 && m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
			{
				for(; i < MAX_CLIENTS && m_pClient->m_Snap.m_paInfoByScore[i]; ++i)
				{
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_Team != TEAM_SPECTATORS)
						++aPos[1];
					if(m_pClient->m_Snap.m_paInfoByScore[i]->m_ClientID == m_pClient->m_Snap.m_LocalClientID)
					{
						apPlayerInfo[1] = m_pClient->m_Snap.m_paInfoByScore[i];
						Local = 1;
						break;
					}
				}
			}
			char aScore[2][16];
			for(int t = 0; t < 2; ++t)
			{
				if(apPlayerInfo[t])
				{
					CServerInfo Info;
					Client()->GetServerInfo(&Info);
					if(IsRace(&Info) && g_Config.m_ClDDRaceScoreBoard)
					{
						if(apPlayerInfo[t]->m_Score != -9999)
							str_format(aScore[t], sizeof(aScore[t]), "%02d:%02d", abs(apPlayerInfo[t]->m_Score)/60, abs(apPlayerInfo[t]->m_Score)%60);
						else
							aScore[t][0] = 0;
					}
					else
						str_format(aScore[t], sizeof(aScore)/2, "%d", apPlayerInfo[t]->m_Score);
				}
				else
					aScore[t][0] = 0;
			}

			bool RecreateScore[2] = { str_comp(aScore[0], m_aScoreInfo[0].m_aScoreText) != 0, str_comp(aScore[1], m_aScoreInfo[1].m_aScoreText) != 0 };

			bool RecreateRect = ForceScoreInfoInit;
			for(int t = 0; t < 2; t++)
			{
				if(RecreateScore[t])
				{
					m_aScoreInfo[t].m_ScoreTextWidth = TextRender()->TextWidth(0, 14.0f, aScore[t], -1);
					mem_copy(m_aScoreInfo[t].m_aScoreText, aScore[t], sizeof(m_aScoreInfo[t].m_aScoreText));
					RecreateRect = true;
				}

				if(apPlayerInfo[t])
				{
					int ID = apPlayerInfo[t]->m_ClientID;
					if(ID >= 0 && ID < MAX_CLIENTS)
					{
						const char *pName = m_pClient->m_aClients[ID].m_aName;
						if(str_comp(pName, m_aScoreInfo[t].m_aPlayerNameText) != 0)
							RecreateRect = true;
					}
				}
				else
				{
					if(m_aScoreInfo[t].m_aPlayerNameText[0] != 0)
						RecreateRect = true;
				}

				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(str_comp(aBuf, m_aScoreInfo[t].m_aRankText) != 0)
					RecreateRect = true;
			}

			static float s_TextWidth10 = TextRender()->TextWidth(0, 14.0f, "10", -1);
			float ScoreWidthMax = max(max(m_aScoreInfo[0].m_ScoreTextWidth, m_aScoreInfo[1].m_ScoreTextWidth), s_TextWidth10);
			float Split = 3.0f, ImageSize = 16.0f, PosSize = 16.0f;

			for(int t = 0; t < 2; t++)
			{
				// draw box
				if(RecreateRect)
				{
					if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
						Graphics()->DeleteQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex);

					if(t == Local)
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.25f);
					else
						Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.25f);
					m_aScoreInfo[t].m_RoundRectQuadContainerIndex = RenderTools()->CreateRoundRectQuadContainer(Whole - ScoreWidthMax - ImageSize - 2 * Split - PosSize, StartY + t * 20, ScoreWidthMax + ImageSize + 2 * Split + PosSize, 18.0f, 5.0f, CUI::CORNER_L);
				}
				Graphics()->TextureSet(-1);
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				if(m_aScoreInfo[t].m_RoundRectQuadContainerIndex != -1)
					Graphics()->RenderQuadContainer(m_aScoreInfo[t].m_RoundRectQuadContainerIndex, -1);

				if(RecreateScore[t])
				{
					if(m_aScoreInfo[t].m_TextScoreContainerIndex != -1)
						TextRender()->DeleteTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex);

					CTextCursor Cursor;
					TextRender()->SetCursor(&Cursor, Whole - ScoreWidthMax + (ScoreWidthMax - m_aScoreInfo[t].m_ScoreTextWidth) / 2 - Split, StartY + t * 20 + (18.f - 14.f) / 2.f, 14.0f, TEXTFLAG_RENDER);
					Cursor.m_LineWidth = -1;
					m_aScoreInfo[t].m_TextScoreContainerIndex = TextRender()->CreateTextContainer(&Cursor, aScore[t]);
				}
				// draw score
				if(m_aScoreInfo[t].m_TextScoreContainerIndex != -1)
				{
					STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
					STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextScoreContainerIndex, &TColor, &TOutlineColor);
				}

				if(apPlayerInfo[t])
				{
					// draw name
					int ID = apPlayerInfo[t]->m_ClientID;
					if(ID >= 0 && ID < MAX_CLIENTS)
					{
						const char *pName = m_pClient->m_aClients[ID].m_aName; 
						if(RecreateRect)
						{
							mem_copy(m_aScoreInfo[t].m_aPlayerNameText, pName, sizeof(m_aScoreInfo[t].m_aPlayerNameText));

							if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex != -1)
								TextRender()->DeleteTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex);

							float w = TextRender()->TextWidth(0, 8.0f, pName, -1);

							CTextCursor Cursor;
							TextRender()->SetCursor(&Cursor, min(Whole - w - 1.0f, Whole - ScoreWidthMax - ImageSize - 2 * Split - PosSize), StartY + (t + 1)*20.0f - 2.0f, 8.0f, TEXTFLAG_RENDER);
							Cursor.m_LineWidth = -1;
							m_aScoreInfo[t].m_OptionalNameTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, pName);
						}

						if(m_aScoreInfo[t].m_OptionalNameTextContainerIndex != -1)
						{
							STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
							STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);
							TextRender()->RenderTextContainer(m_aScoreInfo[t].m_OptionalNameTextContainerIndex, &TColor, &TOutlineColor);
						}

						// draw tee
						CTeeRenderInfo Info = m_pClient->m_aClients[ID].m_RenderInfo;
						Info.m_Size = 18.0f;
						RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, EMOTE_NORMAL, vec2(1,0),
							vec2(Whole-ScoreWidthMax-Info.m_Size/2-Split, StartY+1.0f+Info.m_Size/2+t*20));
					}
				}
				else
				{
					m_aScoreInfo[t].m_aPlayerNameText[0] = 0;
				}

				// draw position
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d.", aPos[t]);
				if(RecreateRect)
				{
					mem_copy(m_aScoreInfo[t].m_aRankText, aBuf, sizeof(m_aScoreInfo[t].m_aRankText));

					if(m_aScoreInfo[t].m_TextRankContainerIndex != -1)
						TextRender()->DeleteTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex);
					
					CTextCursor Cursor;
					TextRender()->SetCursor(&Cursor, Whole - ScoreWidthMax - ImageSize - Split - PosSize, StartY + t * 20 + (18.f - 10.f) / 2.f, 10.0f, TEXTFLAG_RENDER);
					Cursor.m_LineWidth = -1;
					m_aScoreInfo[t].m_TextRankContainerIndex = TextRender()->CreateTextContainer(&Cursor, aBuf);
				}
				if(m_aScoreInfo[t].m_TextRankContainerIndex != -1)
				{
					STextRenderColor TColor(1.f, 1.f, 1.f, 1.f);
					STextRenderColor TOutlineColor(0.f, 0.f, 0.f, 0.3f);
					TextRender()->RenderTextContainer(m_aScoreInfo[t].m_TextRankContainerIndex, &TColor, &TOutlineColor);
				}

				StartY += 8.0f;
			}
		}
	}
}

void CHud::RenderWarmupTimer()
{
	// render warmup timer
	if(m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer > 0 && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME))
	{
		char Buf[256];
		float FontSize = 20.0f;
		float w = TextRender()->TextWidth(0, FontSize, Localize("Warmup"), -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()+-w/2, 50, FontSize, Localize("Warmup"), -1);

		int Seconds = m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer/SERVER_TICK_SPEED;
		if(Seconds < 5)
			str_format(Buf, sizeof(Buf), "%d.%d", Seconds, (m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer*10/SERVER_TICK_SPEED)%10);
		else
			str_format(Buf, sizeof(Buf), "%d", Seconds);
		w = TextRender()->TextWidth(0, FontSize, Buf, -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()+-w/2, 75, FontSize, Buf, -1);
	}
}

void CHud::MapscreenToGroup(float CenterX, float CenterY, CMapItemGroup *pGroup)
{
	float Points[4];
	RenderTools()->MapscreenToWorld(CenterX, CenterY, pGroup->m_ParallaxX/100.0f, pGroup->m_ParallaxY/100.0f,
		pGroup->m_OffsetX, pGroup->m_OffsetY, Graphics()->ScreenAspect(), 1.0f, Points);
	Graphics()->MapScreen(Points[0], Points[1], Points[2], Points[3]);
}

void CHud::RenderTextInfo()
{
	if(g_Config.m_ClShowfps)
	{
		// calculate avg. fps
		m_FrameTimeAvg = m_FrameTimeAvg*0.9f + Client()->RenderFrameTime()*0.1f;
		char Buf[64];
		int FrameTime = (int)(1.0f / m_FrameTimeAvg + 0.5f);
		str_format(Buf, sizeof(Buf), "%d", FrameTime);

		static float s_TextWidth0 = TextRender()->TextWidth(0, 12.f, "0", -1);
		static float s_TextWidth00 = TextRender()->TextWidth(0, 12.f, "00", -1);
		static float s_TextWidth000 = TextRender()->TextWidth(0, 12.f, "000", -1);
		static float s_TextWidth0000 = TextRender()->TextWidth(0, 12.f, "0000", -1);
		static float s_TextWidth00000 = TextRender()->TextWidth(0, 12.f, "00000", -1);
		static float s_TextWidth[5] = { s_TextWidth0, s_TextWidth00, s_TextWidth000, s_TextWidth0000, s_TextWidth00000 };

		int DigitIndex = (int)log10((FrameTime ? FrameTime : 1));
		if(DigitIndex > 4)
			DigitIndex = 4;
		//TextRender()->Text(0, m_Width-10-TextRender()->TextWidth(0,12,Buf,-1), 5, 12, Buf, -1);

		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, m_Width - 10 - s_TextWidth[DigitIndex], 5, 12, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = -1;
		if(m_FPSTextContainerIndex == -1)
			m_FPSTextContainerIndex = TextRender()->CreateTextContainer(&Cursor, "0");
		TextRender()->RecreateTextContainerSoft(&Cursor, m_FPSTextContainerIndex, Buf);
		STextRenderColor TColor;
		TColor.m_R = 1.f;
		TColor.m_G = 1.f;
		TColor.m_B = 1.f;
		TColor.m_A = 1.f;
		STextRenderColor TOutColor;
		TOutColor.m_R = 0.f;
		TOutColor.m_G = 0.f;
		TOutColor.m_B = 0.f;
		TOutColor.m_A = 0.3f;
		TextRender()->RenderTextContainer(m_FPSTextContainerIndex, &TColor, &TOutColor);
	}
	if(g_Config.m_ClShowpred)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", Client()->GetPredictionTime());
		TextRender()->Text(0, m_Width-10-TextRender()->TextWidth(0,12,aBuf,-1), g_Config.m_ClShowfps ? 20 : 5, 12, aBuf, -1);
	}
}

void CHud::RenderConnectionWarning()
{
	if(Client()->ConnectionProblems())
	{
		const char *pText = Localize("Connection Problems...");
		float w = TextRender()->TextWidth(0, 24, pText, -1);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect()-w/2, 50, 24, pText, -1);
	}
}

void CHud::RenderTeambalanceWarning()
{
	// render prompt about team-balance
	bool Flash = time_get()/(time_freq()/2)%2 == 0;
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		int TeamDiff = m_pClient->m_Snap.m_aTeamSize[TEAM_RED]-m_pClient->m_Snap.m_aTeamSize[TEAM_BLUE];
		if(g_Config.m_ClWarningTeambalance && (TeamDiff >= 2 || TeamDiff <= -2))
		{
			const char *pText = Localize("Please balance teams!");
			if(Flash)
				TextRender()->TextColor(1,1,0.5f,1);
			else
				TextRender()->TextColor(0.7f,0.7f,0.2f,1.0f);
			TextRender()->Text(0x0, 5, 50, 6, pText, -1);
			TextRender()->TextColor(1,1,1,1);
		}
	}
}


void CHud::RenderVoting()
{
	if((!g_Config.m_ClShowVotesAfterVoting && !m_pClient->m_pScoreboard->Active() && m_pClient->m_pVoting->TakenChoice()) || !m_pClient->m_pVoting->IsVoting() || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.40f);

#if defined(__ANDROID__)
	static const float TextX = 265;
	static const float TextY = 1;
	static const float TextW = 200;
	static const float TextH = 42;
	RenderTools()->DrawRoundRect(TextX-5, TextY, TextW+15, TextH, 5.0f);
	RenderTools()->DrawRoundRect(TextX-5, TextY+TextH+2, TextW/2-10, 20, 5.0f);
	RenderTools()->DrawRoundRect(TextX+TextW/2+20, TextY+TextH+2, TextW/2-10, 20, 5.0f);
#else
	RenderTools()->DrawRoundRect(-10, 60-2, 100+10+4+5, 46, 5.0f);
#endif
	Graphics()->QuadsEnd();

	TextRender()->TextColor(1,1,1,1);

	CTextCursor Cursor;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), Localize("%ds left"), m_pClient->m_pVoting->SecondsLeft());
#if defined(__ANDROID__)
	float tw = TextRender()->TextWidth(0x0, 10, aBuf, -1);
	TextRender()->SetCursor(&Cursor, TextX+TextW-tw, 0.0f, 10.0f, TEXTFLAG_RENDER);
#else
	float tw = TextRender()->TextWidth(0x0, 6, aBuf, -1);
	TextRender()->SetCursor(&Cursor, 5.0f+100.0f-tw, 60.0f, 6.0f, TEXTFLAG_RENDER);
#endif
	TextRender()->TextEx(&Cursor, aBuf, -1);

#if defined(__ANDROID__)
	TextRender()->SetCursor(&Cursor, TextX, 0.0f, 10.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = TextW-tw;
#else
	TextRender()->SetCursor(&Cursor, 5.0f, 60.0f, 6.0f, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = 100.0f-tw;
#endif
	Cursor.m_MaxLines = 3;
	TextRender()->TextEx(&Cursor, m_pClient->m_pVoting->VoteDescription(), -1);

	// reason
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("Reason:"), m_pClient->m_pVoting->VoteReason());
#if defined(__ANDROID__)
	TextRender()->SetCursor(&Cursor, TextX, 23.0f, 10.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
#else
	TextRender()->SetCursor(&Cursor, 5.0f, 79.0f, 6.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
#endif
	Cursor.m_LineWidth = 100.0f;
	TextRender()->TextEx(&Cursor, aBuf, -1);

#if defined(__ANDROID__)
	CUIRect Base = {TextX, TextH - 8, TextW, 4};
#else
	CUIRect Base = {5, 88, 100, 4};
#endif
	m_pClient->m_pVoting->RenderBars(Base, false);

#if defined(__ANDROID__)
	Base.y += Base.h+6;
	UI()->DoLabel(&Base, Localize("Vote yes"), 16.0f, -1);
	UI()->DoLabel(&Base, Localize("Vote no"), 16.0f, 1);
	if( Input()->KeyPress(KEY_MOUSE_1) )
	{
		float mx, my;
		Input()->MouseRelative(&mx, &my);
		mx *= m_Width / Graphics()->ScreenWidth();
		my *= m_Height / Graphics()->ScreenHeight();
		if( my > TextY+TextH-40 && my < TextY+TextH+20 ) {
			if( mx > TextX-5 && mx < TextX-5+TextW/2-10 )
				m_pClient->m_pVoting->Vote(1);
			if( mx > TextX+TextW/2+20 && mx < TextX+TextW/2+20+TextW/2-10 )
				m_pClient->m_pVoting->Vote(-1);
		}
	}
#else
	const char *pYesKey = m_pClient->m_pBinds->GetKey("vote yes");
	const char *pNoKey = m_pClient->m_pBinds->GetKey("vote no");
	str_format(aBuf, sizeof(aBuf), "%s - %s", pYesKey, Localize("Vote yes"));
	Base.y += Base.h;
	Base.h = 11.f;
	UI()->DoLabel(&Base, aBuf, 6.0f, -1);

	str_format(aBuf, sizeof(aBuf), "%s - %s", Localize("Vote no"), pNoKey);
	UI()->DoLabel(&Base, aBuf, 6.0f, 1);
#endif
}

void CHud::RenderCursor()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	MapscreenToGroup(m_pClient->m_pCamera->m_Center.x, m_pClient->m_pCamera->m_Center.y, Layers()->GameGroup());
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	
	// render cursor
	int QuadOffset = NUM_WEAPONS * 10 + 40 + (m_pClient->m_Snap.m_pLocalCharacter->m_Weapon%NUM_WEAPONS);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	Graphics()->RenderQuadContainerAsSprite(m_HudQuadContainerIndex, QuadOffset, m_pClient->m_pControls->m_TargetPos[g_Config.m_ClDummy].x, m_pClient->m_pControls->m_TargetPos[g_Config.m_ClDummy].y);
}

void CHud::PrepareHealthAmoQuads()
{
	m_HudQuadContainerIndex = Graphics()->CreateQuadContainer();

	float x = 5;
	float y = 5;
	IGraphics::CQuadItem Array[10];

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i%NUM_WEAPONS].m_pSpriteProj);
		for(int n = 0; n < 10; n++)
			Array[n] = IGraphics::CQuadItem(x + n * 12, y + 24, 10, 10);
		Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
	}

	// health
	RenderTools()->SelectSprite(SPRITE_HEALTH_FULL);
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	RenderTools()->SelectSprite(SPRITE_HEALTH_EMPTY);
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y, 10, 10);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	// armor meter
	RenderTools()->SelectSprite(SPRITE_ARMOR_FULL);
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);

	RenderTools()->SelectSprite(SPRITE_ARMOR_EMPTY);
	for(int i = 0; i < 10; ++i)
		Array[i] = IGraphics::CQuadItem(x + i * 12, y + 12, 10, 10);
	Graphics()->QuadContainerAddQuads(m_HudQuadContainerIndex, Array, 10);
}

void CHud::RenderHealthAndAmmo(const CNetObj_Character *pCharacter)
{
	if(!pCharacter)
		return;
	// render ammo count
	// render gui stuff

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	
	int QuadOffset = pCharacter->m_Weapon%NUM_WEAPONS * 10;
	Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, QuadOffset, min(pCharacter->m_AmmoCount, 10));

	QuadOffset = NUM_WEAPONS * 10;
	Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, QuadOffset, min(pCharacter->m_Health, 10));
	
	QuadOffset += 10 + min(pCharacter->m_Health, 10);
	if(min(pCharacter->m_Health, 10) < 10)
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, QuadOffset, 10 - min(pCharacter->m_Health, 10));

	QuadOffset = NUM_WEAPONS * 10 + 20;
	Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, QuadOffset, min(pCharacter->m_Armor, 10));

	QuadOffset += 10 + min(pCharacter->m_Armor, 10);
	if(min(pCharacter->m_Armor, 10) < 10)
		Graphics()->RenderQuadContainer(m_HudQuadContainerIndex, QuadOffset, 10 - min(pCharacter->m_Armor, 10));
}

void CHud::RenderSpectatorHud()
{
	// draw the box
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(m_Width-180.0f, m_Height-15.0f, 180.0f, 15.0f, 5.0f, CUI::CORNER_TL);
	Graphics()->QuadsEnd();

	// draw the text
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Spectate"), m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW ?
		m_pClient->m_aClients[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_aName : Localize("Free-View"));
	TextRender()->Text(0, m_Width-174.0f, m_Height-15.0f + (15.f - 8.f) / 2.f, 8.0f, aBuf, -1);
}

void CHud::RenderLocalTime(float x)
{
	if(!g_Config.m_ClShowLocalTimeAlways && !m_pClient->m_pScoreboard->Active())
		return;

	//draw the box
	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.4f);
	RenderTools()->DrawRoundRectExt(x-30.0f, 0.0f, 25.0f, 12.5f, 3.75f, CUI::CORNER_B);
	Graphics()->QuadsEnd();

	//draw the text
	char aTimeStr[6];
	str_timestamp_format(aTimeStr, sizeof(aTimeStr), "%H:%M");
	TextRender()->Text(0, x-25.0f, (12.5f - 5.f) / 2.f, 5.0f, aTimeStr, -1);
}

void CHud::OnRender()
{
	if(!m_pClient->m_Snap.m_pGameInfoObj)
		return;

	m_Width = 300.0f*Graphics()->ScreenAspect();
	m_Height = 300.0f;
	Graphics()->MapScreen(0.0f, 0.0f, m_Width, m_Height);

	if(g_Config.m_ClShowhud)
	{
		if(m_pClient->m_Snap.m_pLocalCharacter && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER))
		{
			if(g_Config.m_ClShowhudHealthAmmo)
				RenderHealthAndAmmo(m_pClient->m_Snap.m_pLocalCharacter);
			RenderDDRaceEffects();
		}
		else if(m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			if(m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW && g_Config.m_ClShowhudHealthAmmo)
				RenderHealthAndAmmo(&m_pClient->m_Snap.m_aCharacters[m_pClient->m_Snap.m_SpecInfo.m_SpectatorID].m_Cur);
			RenderSpectatorHud();
		}

		RenderGameTimer();
		RenderPauseNotification();
		RenderSuddenDeath();
		if(g_Config.m_ClShowhudScore)
			RenderScoreHud();
		RenderWarmupTimer();
		RenderTextInfo();
		RenderLocalTime((m_Width/7)*3);
		if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderConnectionWarning();
		RenderTeambalanceWarning();
		RenderVoting();
		if(g_Config.m_ClShowRecord)
			RenderRecord();
	}
	RenderCursor();
}

void CHud::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_DDRACETIME)
	{
		m_DDRaceTimeReceived = true;

		CNetMsg_Sv_DDRaceTime *pMsg = (CNetMsg_Sv_DDRaceTime *)pRawMsg;

		m_DDRaceTime = pMsg->m_Time;
		m_DDRaceTick = 0;

		m_LastReceivedTimeTick = Client()->GameTick();

		m_FinishTime = pMsg->m_Finish ? true : false;

		if(pMsg->m_Check)
		{
			m_CheckpointDiff = (float)pMsg->m_Check/100;
			m_CheckpointTick = Client()->GameTick();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
		{
			m_CheckpointTick = 0;
			m_DDRaceTime = 0;
		}
	}
	else if(MsgType == NETMSGTYPE_SV_RECORD)
	{
		CServerInfo Info;
		Client()->GetServerInfo(&Info);

		CNetMsg_Sv_Record *pMsg = (CNetMsg_Sv_Record *)pRawMsg;

		// NETMSGTYPE_SV_RACETIME on old race servers
		if(!IsDDRace(&Info) && IsRace(&Info))
		{
			m_DDRaceTimeReceived = true;

			m_DDRaceTime = pMsg->m_ServerTimeBest; // First value: m_Time
			m_DDRaceTick = 0;

			m_LastReceivedTimeTick = Client()->GameTick();

			if(pMsg->m_PlayerTimeBest) // Second value: m_Check
			{
				m_CheckpointDiff = (float)pMsg->m_PlayerTimeBest/100;
				m_CheckpointTick = Client()->GameTick();
			}
		}
		else
		{
			m_ServerRecord = (float)pMsg->m_ServerTimeBest/100;
			m_PlayerRecord = (float)pMsg->m_PlayerTimeBest/100;
		}
	}
}

void CHud::RenderDDRaceEffects()
{
	// check racestate
	if(m_FinishTime && m_LastReceivedTimeTick + Client()->GameTickSpeed()*2 < Client()->GameTick())
	{
		m_FinishTime = false;
		m_DDRaceTimeReceived = false;
		return;
	}

	if(m_DDRaceTime)
	{
		char aBuf[64];
		if(m_FinishTime)
		{
			str_format(aBuf, sizeof(aBuf), "Finish time: %02d:%02d.%02d", m_DDRaceTime/6000, m_DDRaceTime/100-m_DDRaceTime/6000 * 60, m_DDRaceTime % 100);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect()-TextRender()->TextWidth(0,12,aBuf,-1)/2, 20, 12, aBuf, -1);
		}
		else if(m_CheckpointTick + Client()->GameTickSpeed()*6 > Client()->GameTick())
		{
			str_format(aBuf, sizeof(aBuf), "%+5.2f", m_CheckpointDiff);

			// calculate alpha (4 sec 1 than get lower the next 2 sec)
			float a = 1.0f;
			if(m_CheckpointTick+Client()->GameTickSpeed()*4 < Client()->GameTick() && m_CheckpointTick+Client()->GameTickSpeed()*6 > Client()->GameTick())
			{
				// lower the alpha slowly to blend text out
				a = ((float)(m_CheckpointTick+Client()->GameTickSpeed()*6) - (float)Client()->GameTick()) / (float)(Client()->GameTickSpeed()*2);
			}

			if(m_CheckpointDiff > 0)
				TextRender()->TextColor(1.0f,0.5f,0.5f,a); // red
			else if(m_CheckpointDiff < 0)
				TextRender()->TextColor(0.5f,1.0f,0.5f,a); // green
			else if(!m_CheckpointDiff)
				TextRender()->TextColor(1,1,1,a); // white
			TextRender()->Text(0, 150*Graphics()->ScreenAspect()-TextRender()->TextWidth(0, 10, aBuf, -1)/2, 20, 10, aBuf, -1);

			TextRender()->TextColor(1,1,1,1);
		}
	}
		/*else if(m_DDRaceTimeReceived)
		{
			str_format(aBuf, sizeof(aBuf), "%02d:%02d.%d", m_DDRaceTime/60, m_DDRaceTime%60, m_DDRaceTick/10);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect()-TextRender()->TextWidth(0, 12,"00:00.0",-1)/2, 20, 12, aBuf, -1); // use fixed value for text width so its not shaky
		}*/



	static int LastChangeTick = 0;
	if(LastChangeTick != Client()->PredGameTick())
	{
		m_DDRaceTick += 100/Client()->GameTickSpeed();
		LastChangeTick = Client()->PredGameTick();
	}

	if(m_DDRaceTick >= 100)
		m_DDRaceTick = 0;
}

void CHud::RenderRecord()
{
	if(m_ServerRecord > 0 )
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Server best:");
		TextRender()->Text(0, 5, 40, 6, aBuf, -1);
		str_format(aBuf, sizeof(aBuf), "%02d:%05.2f", (int)m_ServerRecord/60, m_ServerRecord-((int)m_ServerRecord/60*60));
		TextRender()->Text(0, 53, 40, 6, aBuf, -1);
	}

	if(m_PlayerRecord > 0 )
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "Personal best:");
		TextRender()->Text(0, 5, 47, 6, aBuf, -1);
		str_format(aBuf, sizeof(aBuf), "%02d:%05.2f", (int)m_PlayerRecord/60, m_PlayerRecord-((int)m_PlayerRecord/60*60));
		TextRender()->Text(0, 53, 47, 6, aBuf, -1);
	}
}
