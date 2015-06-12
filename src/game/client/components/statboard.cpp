#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <game/generated/client_data.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>
#include <game/client/components/motd.h>
#include <game/client/components/statboard.h>

CStatboard::CStatboard()
{
	m_Active = false;
	m_ScreenshotTaken = false;
	m_ScreenshotTime = -1;
}

void CStatboard::OnReset()
{
	for(int i=0; i<MAX_CLIENTS; i++)
		m_pClient->m_aStats[i].Reset();
	m_Active = false;
	m_ScreenshotTaken = false;
	m_ScreenshotTime = -1;
}

void CStatboard::ConKeyStats(IConsole::IResult *pResult, void *pUserData)
{
	((CStatboard *)pUserData)->m_Active = pResult->GetInteger(0) != 0;
}

void CStatboard::OnConsoleInit()
{
	Console()->Register("+statboard", "", CFGFLAG_CLIENT, ConKeyStats, this, "Show stats");
}

bool CStatboard::IsActive()
{
	return m_Active;
}

void CStatboard::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		CGameClient::CClientStats *pStats = m_pClient->m_aStats;

		pStats[pMsg->m_Victim].m_Deaths++;
		pStats[pMsg->m_Victim].m_CurrentSpree = 0;
		if(pMsg->m_Weapon >= 0)
			pStats[pMsg->m_Victim].m_aDeathsFrom[pMsg->m_Weapon]++;
		if(pMsg->m_Victim != pMsg->m_Killer)
		{
			pStats[pMsg->m_Killer].m_Frags++;
			pStats[pMsg->m_Killer].m_CurrentSpree++;

			if(pStats[pMsg->m_Killer].m_CurrentSpree > pStats[pMsg->m_Killer].m_BestSpree)
				pStats[pMsg->m_Killer].m_BestSpree = pStats[pMsg->m_Killer].m_CurrentSpree;
			if(pMsg->m_Weapon >= 0)
				pStats[pMsg->m_Killer].m_aFragsWith[pMsg->m_Weapon]++;
		}
		else
			pStats[pMsg->m_Victim].m_Suicides++;
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID < 0)
		{
			const char *p;
			const char *pLookFor = "flag was captured by '";
			if(str_find(pMsg->m_pMessage, pLookFor) != 0)
			{
				// server info
				CServerInfo CurrentServerInfo;
				Client()->GetServerInfo(&CurrentServerInfo);

				p = str_find(pMsg->m_pMessage, pLookFor);
				char aName[64];
				p += str_length(pLookFor);
				str_copy(aName, p, sizeof(aName));
				// remove capture time
				if(str_comp(aName+str_length(aName)-9, " seconds)") == 0)
				{
					char *c = aName+str_length(aName)-10;
					while(c > aName)
					{
						c--;
						if(*c == '(')
						{
							*(c-1) = 0;
							break;
						}
					}
				}
				// remove the ' at the end
				aName[str_length(aName)-1] = 0;

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_pClient->m_aStats[i].m_Active)
						continue;

					if(str_comp(m_pClient->m_aClients[i].m_aName, aName) == 0)
					{
						m_pClient->m_aStats[i].m_FlagCaptures++;
						break;
					}
				}
			}
		}
	}
}

void CStatboard::OnRender()
{
	if(g_Config.m_ClAutoStatboardScreenshot)
	{
		if(m_ScreenshotTime < 0 && m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			m_ScreenshotTime = time_get() + time_freq() * 3;
		if(m_ScreenshotTime > -1 && m_ScreenshotTime < time_get())
			m_Active = true;
		if(!m_ScreenshotTaken && m_ScreenshotTime > -1 && m_ScreenshotTime + time_freq() / 5 < time_get())
		{
			AutoStatScreenshot();
			m_ScreenshotTaken = true;
		}
	}

	if(IsActive())
		RenderGlobalStats();
}

void CStatboard::RenderGlobalStats()
{
	float Width = 400*3.0f*Graphics()->ScreenAspect();
	float Height = 400*3.0f;
	float w = 250.0f;
	float h = 750.0f;

	const CNetObj_PlayerInfo *apPlayers[MAX_CLIENTS] = {0};
	int NumPlayers = 0;
	int i = 0;

	// sort red or dm players by score
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];

		if(!pInfo || !m_pClient->m_aStats[pInfo->m_ClientID].m_Active || m_pClient->m_aClients[pInfo->m_ClientID].m_Team != TEAM_RED)
			continue;

		apPlayers[NumPlayers] = pInfo;
		NumPlayers++;
	}

	// sort blue players by score after
	if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_TEAMS)
	{
		for(i = 0; i < MAX_CLIENTS; i++)
		{
			const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByScore[i];

			if(!pInfo || !m_pClient->m_aStats[pInfo->m_ClientID].m_Active || m_pClient->m_aClients[pInfo->m_ClientID].m_Team != TEAM_BLUE)
				continue;

			apPlayers[NumPlayers] = pInfo;
			NumPlayers++;
		}
	}

	// Dirty hack. Do not show scoreboard if there are more than 16 players
	// remove as soon as support of more than 16 players is required
	if(NumPlayers > 16)
		return;

	//clear motd if it is active
	if(m_pClient->m_pMotd->IsActive())
		m_pClient->m_pMotd->Clear();

	bool gameWithFlags = m_pClient->m_Snap.m_pGameInfoObj &&
		m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS;

	for(i = 0; i < 9; i++)
	{
		if(i == 7) // Best Spree
			w += 140;
		else if(i == 2) // Suicides
			w += 110;
		else if(i == 8 && !gameWithFlags) // Don't draw "Grabs" in game with no flag
			continue;

		else
			w += 100;
	}

	if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags&GAMEFLAG_FLAGS)
		w += 100;

	bool aDisplayWeapon[NUM_WEAPONS] = {false};
	w += 10;
	for(i=0; i<NumPlayers; i++)
	{
		const CGameClient::CClientStats pStats = m_pClient->m_aStats[apPlayers[i]->m_ClientID];
		for(int j=0; j<NUM_WEAPONS; j++)
			aDisplayWeapon[j] = aDisplayWeapon[j] || pStats.m_aFragsWith[j] || pStats.m_aDeathsFrom[j];
	}
	for(i=0; i<NUM_WEAPONS; i++)
		if(aDisplayWeapon[i])
			w += 80;

	float x = Width/2-w/2;
	float y = 200.0f;

	Graphics()->MapScreen(0, 0, Width, Height);

	Graphics()->BlendNormal();
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0,0,0,0.5f);
	RenderTools()->DrawRoundRect(x-10.f, y-10.f, w, h, 17.0f);
	Graphics()->QuadsEnd();

	float tw;
	int px = 325;

	TextRender()->Text(0, x+10, y-5, 24.0f, Localize("Name"), -1);
	const char *apHeaders[] = { Localize("Frags"), Localize("Deaths"), Localize("Suicides"), Localize("Ratio"), Localize("Net"), Localize("FPM"), Localize("Spree"), Localize("Best spree"), Localize("Grabs") };
	for(i = 0; i < 9; i++)
	{
		if(i == 2)
			px += 10.0f; // Suicides
		if(i == 7) // Best Spree
			px += 40.0f;
		if(i == 8 && !gameWithFlags) // Don't draw "Grabs" in game with no flag
			continue;

		tw = TextRender()->TextWidth(0, 24.0f, apHeaders[i], -1);
		TextRender()->Text(0, x+px-tw, y-5, 24.0f, apHeaders[i], -1);
		px += 100;
	}

	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
	Graphics()->QuadsBegin();
	for(i = 0, px-=40; i < NUM_WEAPONS; i++)
	{
		if(!aDisplayWeapon[i])
			continue;
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[i].m_pSpriteBody);
		if(i == 0)
			RenderTools()->DrawSprite(x+px, y+10, g_pData->m_Weapons.m_aId[i].m_VisualSize*0.8);
		else
			RenderTools()->DrawSprite(x+px, y+10, g_pData->m_Weapons.m_aId[i].m_VisualSize);
		px += 80;
	}
	Graphics()->QuadsEnd();
	px += 40;

	if(gameWithFlags)
	{
		px -= 40;
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(0.78f);
		RenderTools()->SelectSprite(SPRITE_FLAG_RED);
		RenderTools()->DrawSprite(x+px, y+15, 48);
		Graphics()->QuadsEnd();
	}

	y += 29.0f;

	float FontSize = 30.0f;
	float LineHeight = 50.0f;
	float TeeSizemod = 1.0f;
	float TeeOffset = 0.0f;

	if(NumPlayers > 14)
	{
		FontSize = 30.0f;
		LineHeight = 40.0f;
		TeeSizemod = 0.8f;
		TeeOffset = -5.0f;
	}

	for(int j = 0; j < NumPlayers; j++)
	{
		const CNetObj_PlayerInfo *pInfo = apPlayers[j];
		const CGameClient::CClientStats Stats = m_pClient->m_aStats[pInfo->m_ClientID];

		if(m_pClient->m_Snap.m_LocalClientID == pInfo->m_ClientID || (m_pClient->m_Snap.m_SpecInfo.m_Active && pInfo->m_ClientID == m_pClient->m_Snap.m_SpecInfo.m_SpectatorID))
		{
			// background so it's easy to find the local player
			Graphics()->TextureSet(-1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1,1,1,0.25f);
			RenderTools()->DrawRoundRect(x, y, w-20, LineHeight*0.95f, 17.0f);
			Graphics()->QuadsEnd();
		}

		CTeeRenderInfo Teeinfo = m_pClient->m_aClients[pInfo->m_ClientID].m_RenderInfo;
		Teeinfo.m_Size *= TeeSizemod;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &Teeinfo, EMOTE_NORMAL, vec2(1,0), vec2(x+28, y+28+TeeOffset));

		char aBuf[128];
		CTextCursor Cursor;
		tw = TextRender()->TextWidth(0, FontSize, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, -1);
		TextRender()->SetCursor(&Cursor, x+64, y, FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = 220;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[pInfo->m_ClientID].m_aName, -1);

		px = 325;

		// DETAILED_STATS_FRAGS
		{
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_Frags);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_DEATHS
		{
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_SUICIDES
		{
			px += 10;
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_Suicides);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_RATIO
		{
			if(Stats.m_Deaths == 0)
				str_format(aBuf, sizeof(aBuf), "--");
			else
				str_format(aBuf, sizeof(aBuf), "%.2f", (float)(Stats.m_Frags)/Stats.m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_NET
		{
			str_format(aBuf, sizeof(aBuf), "%+d", Stats.m_Frags-Stats.m_Deaths);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_FPM
		{
			float Fpm = (float)(Stats.m_Frags*60)/((float)(Client()->GameTick()-Stats.m_JoinDate)/Client()->GameTickSpeed());
			str_format(aBuf, sizeof(aBuf), "%.1f", Fpm);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_SPREE
		{
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_CurrentSpree);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		// DETAILED_STATS_BESTSPREE
		{
			px += 40;
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_BestSpree);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		if(gameWithFlags)
		{
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_FlagGrabs);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		for(i = 0, px=px-40; i < NUM_WEAPONS; i++)
		{
			if(!aDisplayWeapon[i])
				continue;

			str_format(aBuf, sizeof(aBuf), "%d/%d", Stats.m_aFragsWith[i], Stats.m_aDeathsFrom[i]);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x+px-tw/2, y, FontSize, aBuf, -1);
			px += 80;
		}
		if(gameWithFlags)
		{
			str_format(aBuf, sizeof(aBuf), "%d", Stats.m_FlagCaptures);
			tw = TextRender()->TextWidth(0, FontSize, aBuf, -1);
			TextRender()->Text(0, x-tw+px, y, FontSize, aBuf, -1);
			px += 100;
		}
		y += LineHeight;
	}
}

void CStatboard::AutoStatScreenshot()
{
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Client()->AutoStatScreenshot_Start();
}
