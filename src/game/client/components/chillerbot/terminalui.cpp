// ChillerDragon 2020 - chillerbot

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/terminalui.h>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

CGameClient *g_pClient;

// mvwprintw(WINDOW, y, x, const char*),

void CTerminalUI::DrawBorders(WINDOW *screen, int x, int y, int w, int h)
{
	// 4 corners
	mvwprintw(screen, y, x, "+");
	mvwprintw(screen, y + h - 1, x, "+");
	mvwprintw(screen, y, x + w - 1, "+");
	mvwprintw(screen, y + h - 1, x + w - 1, "+");

	// sides
	// for(int i = 1; i < ((y + h) - 1); i++)
	// {
	// 	mvwprintw(screen, i, y, "|");
	// 	mvwprintw(screen, i, (x + w) - 1, "|");
	// }

	// top and bottom
	for(int i = 1; i < (w - 1); i++)
	{
		mvwprintw(screen, y, x + i, "-");
		mvwprintw(screen, (y + h) - 1, x + i, "-");
	}
}

void CTerminalUI::DrawBorders(WINDOW *screen)
{
	draw_borders(screen);
}

void CTerminalUI::DrawAllBorders()
{
	// TODO: call in broadcast.cpp
	DrawBorders(g_pLogWindow);
	DrawBorders(g_pInfoWin);
	DrawBorders(g_pInputWin);
}

void CTerminalUI::LogDraw()
{
	log_draw();
}

void CTerminalUI::InfoDraw()
{
	int x = getmaxx(g_pInfoWin);
	char aBuf[1024 * 4 + 16];
	str_format(aBuf, sizeof(aBuf), "%s | %s | %s", GetInputMode(), g_aInfoStr, g_aInfoStr2);
	aBuf[x - 2] = '\0'; // prevent line wrapping and cut on screen border
	mvwprintw(g_pInfoWin, 1, 1, aBuf);
}

void CTerminalUI::InputDraw()
{
	char aBuf[512];
	if(m_InputMode == INPUT_REMOTE_CONSOLE && !RconAuthed())
	{
		int i;
		for(i = 0; i < 512 && g_aInputStr[i] != '\0'; i++)
			aBuf[i] = '*';
		aBuf[i] = '\0';
	}
	else
		str_copy(aBuf, g_aInputStr, sizeof(aBuf));
	int x = getmaxx(g_pInfoWin);
	aBuf[x - 2] = '\0'; // prevent line wrapping and cut on screen border
	mvwprintw(g_pInputWin, 1, 1, aBuf);
}

int CTerminalUI::CursesTick()
{
	getmaxyx(stdscr, g_NewY, g_NewX);

	if(g_NewY != g_ParentY || g_NewX != g_ParentX)
	{
		g_ParentX = g_NewX;
		g_ParentY = g_NewY;

		wresize(g_pLogWindow, g_NewY - NC_INFO_SIZE * 2, g_NewX);
		wresize(g_pInfoWin, NC_INFO_SIZE, g_NewX);
		wresize(g_pInputWin, NC_INFO_SIZE, g_NewX);
		mvwin(g_pInfoWin, g_NewY - NC_INFO_SIZE * 2, 0);
		mvwin(g_pInputWin, g_NewY - NC_INFO_SIZE, 0);

		wclear(stdscr);
		wclear(g_pLogWindow);
		wclear(g_pInfoWin);
		wclear(g_pInputWin);

		DrawBorders(g_pLogWindow);
		DrawBorders(g_pInfoWin);
		DrawBorders(g_pInputWin);
		gs_NeedLogDraw = true;
	}

	// draw to our windows
	LogDraw();
	InfoDraw();

	int input = GetInput(); // calls InputDraw()

	// refresh each window
	curses_refresh_windows();
	return input == -1;
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
	g_pClient = m_pClient;
	mem_zero(m_aLastPressedKey, sizeof(m_aLastPressedKey));
	AimX = 20;
	AimY = 0;
	curses_init();
	m_InputMode = INPUT_NORMAL;
	initscr();
	noecho();
	curs_set(FALSE);

	// set up initial windows
	getmaxyx(stdscr, g_ParentY, g_ParentX);

	g_pLogWindow = newwin(g_ParentY - NC_INFO_SIZE * 2, g_ParentX, 0, 0);
	g_pInfoWin = newwin(NC_INFO_SIZE, g_ParentX, g_ParentY - NC_INFO_SIZE * 2, 0);
	g_pInputWin = newwin(NC_INFO_SIZE, g_ParentX, g_ParentY - NC_INFO_SIZE, 0);

	DrawBorders(g_pLogWindow);
	DrawBorders(g_pInfoWin);
	DrawBorders(g_pInputWin);
}

void CTerminalUI::OnShutdown()
{
	endwin();
}

void CTerminalUI::OnRender()
{
	CursesTick();

	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	float X = m_pClient->m_Snap.m_pLocalCharacter->m_X;
	float Y = m_pClient->m_Snap.m_pLocalCharacter->m_Y;
	str_format(g_aInfoStr2, sizeof(g_aInfoStr2),
		"%.2f %.2f scoreboard=%d",
		X / 32, Y / 32,
		m_ScoreboardActive);
}

int CTerminalUI::GetInput()
{
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	cbreak();
	noecho();
	int c = getch();
	if(m_InputMode == INPUT_NORMAL)
	{
		if(c == 'q')
			return -1;
		else if(c == KEY_F(1))
			m_InputMode = INPUT_LOCAL_CONSOLE;
		else if(c == KEY_F(2))
			m_InputMode = INPUT_REMOTE_CONSOLE;
		else if(c == 't')
			m_InputMode = INPUT_CHAT;
		else if(c == 'z')
			m_InputMode = INPUT_CHAT_TEAM;
		else
		{
			InputDraw();
			OnKeyPress(c, g_pLogWindow);
		}
	}
	else if(
		m_InputMode == INPUT_LOCAL_CONSOLE ||
		m_InputMode == INPUT_REMOTE_CONSOLE ||
		m_InputMode == INPUT_CHAT ||
		m_InputMode == INPUT_CHAT_TEAM)
	{
		if(c == ERR) // nonblocking empty read
			return 0;
		else if(c == EOF || c == 10) // return
		{
			if(m_InputMode == INPUT_LOCAL_CONSOLE)
				m_pClient->Console()->ExecuteLine(g_aInputStr);
			else if(m_InputMode == INPUT_REMOTE_CONSOLE)
			{
				if(m_pClient->Client()->RconAuthed())
					m_pClient->Client()->Rcon(g_aInputStr);
				else
					m_pClient->Client()->RconAuth("", g_aInputStr);
			}
			else if(m_InputMode == INPUT_CHAT)
				m_pClient->m_Chat.Say(0, g_aInputStr);
			else if(m_InputMode == INPUT_CHAT_TEAM)
				m_pClient->m_Chat.Say(1, g_aInputStr);
			g_aInputStr[0] = '\0';
			wclear(g_pInputWin);
			DrawBorders(g_pInputWin);
			if(m_InputMode != INPUT_LOCAL_CONSOLE && m_InputMode != INPUT_REMOTE_CONSOLE)
				m_InputMode = INPUT_NORMAL;
			return 0;
		}
		else if(c == KEY_F(1)) // f1 hard toggle local console
		{
			g_aInputStr[0] = '\0';
			wclear(g_pInputWin);
			DrawBorders(g_pInputWin);
			m_InputMode = m_InputMode == INPUT_LOCAL_CONSOLE ? INPUT_NORMAL : INPUT_LOCAL_CONSOLE;
		}
		else if(c == KEY_F(2)) // f2 hard toggle local console
		{
			g_aInputStr[0] = '\0';
			wclear(g_pInputWin);
			DrawBorders(g_pInputWin);
			m_InputMode = m_InputMode == INPUT_REMOTE_CONSOLE ? INPUT_NORMAL : INPUT_REMOTE_CONSOLE;
		}
		else if(c == 27) // ESC
		{
			// esc detection is buggy idk some event sequence what ever
			m_InputMode = INPUT_NORMAL;
			return 0;
		}
		else if(c == KEY_BACKSPACE || c == 127) // delete
		{
			str_truncate(g_aInputStr, sizeof(g_aInputStr), g_aInputStr, str_length(g_aInputStr) - 1);
			wclear(g_pInputWin);
			InputDraw();
			DrawBorders(g_pInputWin);
			return 0;
		}
		char aKey[8];
		str_format(aKey, sizeof(aKey), "%c", c);
		str_append(g_aInputStr, aKey, sizeof(g_aInputStr));
		// dbg_msg("yeee", "got key d=%d c=%c", c, c);
	}
	InputDraw();
	return 0;
}

int CTerminalUI::OnKeyPress(int Key, WINDOW *pWin)
{
	// m_pClient->m_Controls.SetCursesDir(-2);
	// m_pClient->m_Controls.SetCursesJump(-2);
	// m_pClient->m_Controls.SetCursesHook(-2);
	// m_pClient->m_Controls.SetCursesTargetX(AimX);
	// m_pClient->m_Controls.SetCursesTargetY(AimY);
	TrackKey(Key);
	if(Key == 9) // tab
		m_ScoreboardActive = !m_ScoreboardActive;
	else if(Key == 'k')
		m_pClient->SendKill(g_Config.m_ClDummy);
	else if(KeyInHistory('a', 5) || Key == 'a')
		/* m_pClient->m_Controls.SetCursesDir(-1); */ return 0;
	else if(KeyInHistory('d', 5) || Key == 'd')
		/* m_pClient->m_Controls.SetCursesDir(1); */ return 0;
	else if(KeyInHistory(' ', 3) || Key == ' ')
		/* m_pClient->m_Controls.SetCursesJump(1); */ return 0;
	else if(KeyInHistory('h', 10) || Key == 'h')
		/* m_pClient->m_Controls.SetCursesHook(1); */ return 0;
	else if(Key == KEY_LEFT)
		AimX = maximum(AimX - 10, -20);
	else if(Key == KEY_RIGHT)
		AimX = minimum(AimX + 10, 20);
	else if(Key == KEY_UP)
		AimY = maximum(AimY - 10, -20);
	else if(Key == KEY_DOWN)
		AimY = minimum(AimY + 10, 20);

	if(m_ScoreboardActive)
		RenderScoreboard(0, pWin);

	if(Key == EOF)
		return 0;

	// dbg_msg("termUI", "got key d=%d c=%c", Key, Key);
	return 0;
}

#endif
