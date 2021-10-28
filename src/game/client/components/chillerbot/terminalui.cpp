// ChillerDragon 2020 - chillerbot

#include <ncurses.h>
#include "terminalui_keys.h" /* undefines conflicting ncurses key codes */

#include <game/client/gameclient.h>
#include <game/client/components/controls.h>

#include "terminalui.h"

// mvwprintw(WINDOW, y, x, const char*),

void CTerminalUI::DrawBorders(WINDOW *screen, int x, int y, int w, int h)
{
	// 4 corners
	mvwprintw(screen, y, x, "+");
	mvwprintw(screen, y+h-1, x, "+");
    mvwprintw(screen, y, x+w-1, "+");
	mvwprintw(screen, y+h-1, x+w-1, "+");

	// sides
	// for(int i = 1; i < ((y + h) - 1); i++)
	// {
	// 	mvwprintw(screen, i, y, "|");
	// 	mvwprintw(screen, i, (x + w) - 1, "|");
	// }

	// top and bottom
	for(int i = 1; i < (w - 1); i++)
	{
		mvwprintw(screen, y, x+i, "-");
		mvwprintw(screen, (y + h) - 1, x+i, "-");
	}
}

void CTerminalUI::RenderScoreboard(int Team, WINDOW *pWin)
{
    // TODO:
    /*
	int RenderScoreIDs[MAX_CLIENTS];
	int NumRenderScoreIDs = 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
		RenderScoreIDs[i] = -1;

    int mx = getmaxx(pWin);
    int my = getmaxy(pWin);
    int offY = 5;
    int offX = 40;
    if(mx < 128)
        offX = 2;
    if(my < 60)
        offY = 2;
    int width = minimum(128, mx - 3);

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        // make sure that we render the correct team
        const CGameClient::CPlayerInfoItem *pInfo = &m_pClient->m_Snap.m_aInfoByScore[i];
        if(!pInfo->m_pPlayerInfo || m_pClient->m_aClients[pInfo->m_ClientID].m_Team != Team)
            continue;

        RenderScoreIDs[NumRenderScoreIDs] = i;
        NumRenderScoreIDs++;
        if(offY+i > my - 8)
            break;
    }

    DrawBorders(pWin, offX, offY-1, width, NumRenderScoreIDs+2);
    // DrawBorders(pWin, 10, 5, 10, 5);

	for(int i = 0 ; i < NumRenderScoreIDs ; i++)
	{
		if(RenderScoreIDs[i] >= 0)
		{
			// if(rendered++ < 0) continue;

            const CGameClient::CPlayerInfoItem *pInfo = &m_pClient->m_Snap.m_aInfoByScore[RenderScoreIDs[i]];
            // Client()->ChillerLog("scoreboard", "%s", m_pClient->m_aClients[pInfo->m_ClientID].m_aName);

            char aLine[1024];
            char aBuf[1024];
            str_format(aBuf, sizeof(aBuf),
                "%4d| %-20s | %-20s |",
                clamp(pInfo->m_pPlayerInfo->m_Score, -999, 9999),
                m_pClient->m_aClients[pInfo->m_ClientID].m_aName,
                m_pClient->m_aClients[pInfo->m_ClientID].m_aClan
            );
            str_format(aLine, sizeof(aLine), "|%-*s|", width - 2, aBuf);
            aLine[mx-2] = '\0'; // ensure no line wrapping
	        mvwprintw(pWin, offY+i, offX, aLine);
        }
    }
    */
}

void CTerminalUI::OnInit()
{
	mem_zero(m_aLastPressedKey, sizeof(m_aLastPressedKey));
	AimX = 20;
	AimY = 0;
}

void CTerminalUI::OnRender()
{
    if (!m_pClient->m_Snap.m_pLocalCharacter)
        return;

    float X = m_pClient->m_Snap.m_pLocalCharacter->m_X;
    float Y = m_pClient->m_Snap.m_pLocalCharacter->m_Y;
    char aBuf[128];
    // str_format(aBuf, sizeof(aBuf),
    //     "%.2f %.2f goto(%f/%f) scoreboard=%d",
    //     X / 32, Y / 32, m_pClient->m_Controls.GetGotoXY().x, m_pClient->m_Controls.GetGotoXY().y,
    //     m_ScoreboardActive
    // );
    #pragma message "TODO info set str"
    // Client()->ChillerInfoSetStr2(aBuf);
}

int CTerminalUI::OnKeyPress(int Key, WINDOW *pWin)
{
    // m_pClient->m_Controls.SetCursesDir(-2);
    // m_pClient->m_Controls.SetCursesJump(-2);
    // m_pClient->m_Controls.SetCursesHook(-2);
    // m_pClient->m_Controls.SetCursesTargetX(AimX);
    // m_pClient->m_Controls.SetCursesTargetY(AimY);
    TrackKey(Key);
    if (Key == 9) // tab
        m_ScoreboardActive = !m_ScoreboardActive;
    else if (Key == 'k')
        /* m_pClient->SendKill(); */ return 0;
    else if (KeyInHistory('a', 5) || Key == 'a')
        /* m_pClient->m_Controls.SetCursesDir(-1); */ return 0;
    else if (KeyInHistory('d', 5) || Key == 'd')
        /* m_pClient->m_Controls.SetCursesDir(1); */ return 0;
    else if (KeyInHistory(' ', 3) || Key == ' ')
        /* m_pClient->m_Controls.SetCursesJump(1); */ return 0;
    else if (KeyInHistory('h', 10) || Key == 'h')
        /* m_pClient->m_Controls.SetCursesHook(1); */ return 0;
    else if (Key == KEY_LEFT)
        AimX = maximum(AimX - 10, -20);
    else if (Key == KEY_RIGHT)
        AimX = minimum(AimX + 10, 20);
    else if (Key == KEY_UP)
        AimY = maximum(AimY - 10, -20);
    else if (Key == KEY_DOWN)
        AimY = minimum(AimY + 10, 20);

    if(m_ScoreboardActive)
        RenderScoreboard(0, pWin);

    if(Key == EOF || Key == ERR)
        return 0;

    // Client()->ChillerLog("termUI", "got key d=%d c=%c", Key, Key);
    return 0;
}
