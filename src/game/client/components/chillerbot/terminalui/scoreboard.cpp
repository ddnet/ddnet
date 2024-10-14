// ChillerDragon 2023 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/chillerbot/curses_colors.h>
#include <base/terminalui.h>

#include "pad_utf8.h"
#include <rust-bridge-chillerbot/unicode.h>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

void CTerminalUI::RenderScoreboard(int Team, CTermWindow *pWin)
{
	if(!m_ScoreboardActive)
		return;

	int NumRenderScoreIds = 0;

	int mx = getmaxx(pWin->m_pCursesWin);
	int my = getmaxy(pWin->m_pCursesWin);
	int offY = 5;
	int offX = 40;
	int width = minimum(128, mx - 3);
	if(mx < offX + 2 + width)
		offX = 2;
	if(my < 60)
		offY = 2;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		NumRenderScoreIds++;
		if(offY + i > my - 8)
			break;
	}
	int height = minimum(NumRenderScoreIds, my - 5);
	if(height < 1)
		return;

	pWin->DrawBorders(offX, offY - 1, width, height + 2);
	// DrawBorders(pWin, 10, 5, 10, 5);

	int k = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// if(rendered++ < 0) continue;

		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_apInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		// dbg_msg("scoreboard", "i=%d name=%s", i, m_pClient->m_aClients[pInfo->m_ClientId].m_aName);

		char aScore[64];

		// score
		if(m_pClient->m_GameInfo.m_TimeScore)
		{
			if(pInfo->m_Score == -9999)
				aScore[0] = 0;
			else
				str_time((int64_t)abs(pInfo->m_Score) * 100, TIME_HOURS, aScore, sizeof(aScore));
		}
		else
			str_format(aScore, sizeof(aScore), "%d", clamp(pInfo->m_Score, -999, 99999));

		static const int NAME_COL_SIZE = 20;
		static const int CLAN_COL_SIZE = 20;

		char aBuf[1024];
		char aName[64];
		char aClan[64];
		str_copy(aName, m_pClient->m_aClients[pInfo->m_ClientId].m_aName, sizeof(aName));
		str_copy(aClan, m_pClient->m_aClients[pInfo->m_ClientId].m_aClan, sizeof(aClan));

		str_pad_right_utf8(aName, sizeof(aName), NAME_COL_SIZE);
		str_pad_right_utf8(aClan, sizeof(aClan), CLAN_COL_SIZE);

		str_format(aBuf, sizeof(aBuf),
			"%8s| %s | %s |",
			aScore,
			aName,
			aClan);
		str_pad_right_utf8(aBuf, sizeof(aBuf), width - 2);
		// if(sizeof(aBuf) > (unsigned long)(mx - 2))
		// 	aLine[mx - 2] = '\0'; // ensure no line wrapping
		mvwprintw(pWin->m_pCursesWin, offY + k, offX, "|%s|", aBuf);

		if(k++ >= height - 1)
			break;
	}
}

#endif
