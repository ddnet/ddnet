// ChillerDragon 2020 - chillerbot

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include "terminalui.h"

#if defined(CONF_PLATFORM_LINUX)

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
	int x, y, i;

	getmaxyx(screen, y, x);

	// 4 corners
	mvwprintw(screen, 0, 0, "+");
	mvwprintw(screen, y - 1, 0, "+");
	mvwprintw(screen, 0, x - 1, "+");
	mvwprintw(screen, y - 1, x - 1, "+");

	// sides
	for(i = 1; i < (y - 1); i++)
	{
		mvwprintw(screen, i, 0, "|");
		mvwprintw(screen, i, x - 1, "|");
	}

	// top and bottom
	for(i = 1; i < (x - 1); i++)
	{
		mvwprintw(screen, 0, i, "-");
		mvwprintw(screen, y - 1, i, "-");
	}
	// if(m_aSrvBroadcast[0] != '\0' && screen == m_pLogWindow)
	// {
	// 	char aBuf[1024*4];
	// 	str_format(aBuf, sizeof(aBuf), "-[ %s ]", m_aSrvBroadcast);
	// 	aBuf[x-2] = '\0';
	// 	mvwprintw(screen, 0, 1, aBuf);
	// }
}

void CTerminalUI::LogDrawBorders()
{
	if(!g_Config.m_ClNcurses)
		return;
	DrawBorders(m_pLogWindow);
	DrawBorders(m_pInfoWin);
	DrawBorders(m_pInputWin);
}

void CTerminalUI::LogDraw()
{
	if(!m_NeedLogDraw)
		return;
	int x, y;
	getmaxyx(m_pLogWindow, y, x);
	int Max = CHILLER_LOGGER_HEIGHT > y ? y : CHILLER_LOGGER_HEIGHT;
	int Top = CHILLER_LOGGER_HEIGHT-2;
	int Bottom = CHILLER_LOGGER_HEIGHT-Max;
	int line = 0;
	for(int i = Top; i > Bottom; i--)
	{
		if(m_aaChillerLogger[i][0] == '\0')
			continue; // TODO: break
		char aBuf[1024*4+16];
		str_format(aBuf, sizeof(aBuf), "%2d|%2d|%s", line++, i, m_aaChillerLogger[i]);
		aBuf[x-2] = '\0'; // prevent line wrapping and cut on screen border
		mvwprintw(m_pLogWindow, CHILLER_LOGGER_HEIGHT-i-1, 1, aBuf);
	}
}

void CTerminalUI::InfoDraw()
{
	int x = getmaxx(m_pInfoWin);
	char aBuf[1024*4+16];
	str_format(aBuf, sizeof(aBuf), "%s | %s | %s", GetInputMode(), m_aInfoStr, m_aInfoStr2);
	aBuf[x-2] = '\0'; // prevent line wrapping and cut on screen border
	mvwprintw(m_pInfoWin, 1, 1, aBuf);
}

void CTerminalUI::InputDraw()
{
	char aBuf[512];
	if (m_InputMode == INPUT_REMOTE_CONSOLE && !RconAuthed())
	{
		int i;
		for(i = 0; i < 512 && m_aInputStr[i] != '\0'; i++)
			aBuf[i] = '*';
		aBuf[i] = '\0';
	}
	else
		str_copy(aBuf, m_aInputStr, sizeof(aBuf));
	int x = getmaxx(m_pInfoWin);
	aBuf[x-2] = '\0'; // prevent line wrapping and cut on screen border
	mvwprintw(m_pInputWin, 1, 1, aBuf);
}

int CTerminalUI::CursesTick()
{
	if(!g_Config.m_ClNcurses)
		return 0;

	getmaxyx(stdscr, m_NewY, m_NewX);

	if (m_NewY != m_ParentY || m_NewX != m_ParentX) {
		m_ParentX = m_NewX;
		m_ParentY = m_NewY;

		wresize(m_pLogWindow, m_NewY - NC_INFO_SIZE*2, m_NewX);
		wresize(m_pInfoWin, NC_INFO_SIZE, m_NewX);
		wresize(m_pInputWin, NC_INFO_SIZE, m_NewX);
		mvwin(m_pInfoWin, m_NewY - NC_INFO_SIZE*2, 0);
		mvwin(m_pInputWin, m_NewY - NC_INFO_SIZE, 0);

		wclear(stdscr);
		wclear(m_pLogWindow);
		wclear(m_pInfoWin);
		wclear(m_pInputWin);

		DrawBorders(m_pLogWindow);
		DrawBorders(m_pInfoWin);
		DrawBorders(m_pInputWin);
	}

	// draw to our windows
	LogDraw();
	InfoDraw();

	int input = GetInput(); // calls InputDraw()

	// refresh each window
	if(m_NeedLogDraw)
		wrefresh(m_pLogWindow);
	wrefresh(m_pInfoWin);
	wrefresh(m_pInputWin);
	m_NeedLogDraw = false;
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
	for (int i = 0; i < CHILLER_LOGGER_HEIGHT; i++)
		m_aaChillerLogger[i][0] = '\0';
	m_InputMode = INPUT_NORMAL;
	if(g_Config.m_ClNcurses)
	{
		initscr();
		noecho();
		curs_set(FALSE);

		// set up initial windows
		getmaxyx(stdscr, m_ParentY, m_ParentX);

		m_pLogWindow = newwin(m_ParentY - NC_INFO_SIZE*2, m_ParentX, 0, 0);
		m_pInfoWin = newwin(NC_INFO_SIZE, m_ParentX, m_ParentY - NC_INFO_SIZE*2, 0);
		m_pInputWin = newwin(NC_INFO_SIZE, m_ParentX, m_ParentY - NC_INFO_SIZE, 0);

		DrawBorders(m_pLogWindow);
		DrawBorders(m_pInfoWin);
		DrawBorders(m_pInputWin);
	}
}

void CTerminalUI::OnShutdown()
{
	endwin();
}

void CTerminalUI::OnRender()
{
	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	CursesTick();

	// float X = m_pClient->m_Snap.m_pLocalCharacter->m_X;
	// float Y = m_pClient->m_Snap.m_pLocalCharacter->m_Y;
	// char aBuf[128];
	// str_format(aBuf, sizeof(aBuf),
	//     "%.2f %.2f goto(%f/%f) scoreboard=%d",
	//     X / 32, Y / 32, m_pClient->m_Controls.GetGotoXY().x, m_pClient->m_Controls.GetGotoXY().y,
	//     m_ScoreboardActive
	// );
	// Client()->ChillerInfoSetStr2(aBuf);
}

int CTerminalUI::GetInput()
{
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	cbreak();
	noecho();
	int c = getch();
	if (m_InputMode == INPUT_NORMAL)
	{
		if(c == 'q')
			return -1;
		else if (c == KEY_F(1))
			m_InputMode = INPUT_LOCAL_CONSOLE;
		else if (c == KEY_F(2))
			m_InputMode = INPUT_REMOTE_CONSOLE;
		else if (c == 't')
			m_InputMode = INPUT_CHAT;
		else if (c == 'z')
			m_InputMode = INPUT_CHAT_TEAM;
		else
		{
			InputDraw();
			OnKeyPress(c, m_pLogWindow);
		}
	}
	else if (
		m_InputMode == INPUT_LOCAL_CONSOLE ||
		m_InputMode == INPUT_REMOTE_CONSOLE ||
		m_InputMode == INPUT_CHAT ||
		m_InputMode == INPUT_CHAT_TEAM)
	{
		if(c == ERR) // nonblocking empty read
			return 0;
		else if (c == EOF || c == 10) // return
		{
			if(m_InputMode == INPUT_LOCAL_CONSOLE)
				m_pClient->Console()->ExecuteLine(m_aInputStr);
			else if (m_InputMode == INPUT_REMOTE_CONSOLE)
			{
				if(m_pClient->Client()->RconAuthed())
					m_pClient->Client()->Rcon(m_aInputStr);
				else
					m_pClient->Client()->RconAuth("", m_aInputStr);
			}
			else if (m_InputMode == INPUT_CHAT)
				m_pClient->m_Chat.Say(0, m_aInputStr);
			else if (m_InputMode == INPUT_CHAT_TEAM)
				m_pClient->m_Chat.Say(1, m_aInputStr);
			m_aInputStr[0] = '\0';
			wclear(m_pInputWin);
			DrawBorders(m_pInputWin);
			if(m_InputMode != INPUT_LOCAL_CONSOLE && m_InputMode != INPUT_REMOTE_CONSOLE)
				m_InputMode = INPUT_NORMAL;
			return 0;
		}
		else if (c == KEY_F(1)) // f1 hard toggle local console
		{
			m_aInputStr[0] = '\0';
			wclear(m_pInputWin);
			DrawBorders(m_pInputWin);
			m_InputMode = m_InputMode == INPUT_LOCAL_CONSOLE ? INPUT_NORMAL : INPUT_LOCAL_CONSOLE;
		}
		else if (c == KEY_F(2)) // f2 hard toggle local console
		{
			m_aInputStr[0] = '\0';
			wclear(m_pInputWin);
			DrawBorders(m_pInputWin);
			m_InputMode = m_InputMode == INPUT_REMOTE_CONSOLE ? INPUT_NORMAL : INPUT_REMOTE_CONSOLE;
		}
		else if (c == 27) // ESC
		{
			// esc detection is buggy idk some event sequence what ever
			m_InputMode = INPUT_NORMAL;
			return 0;
		}
		else if (c == KEY_BACKSPACE || c == 127) // delete
		{
			str_truncate(m_aInputStr, sizeof(m_aInputStr), m_aInputStr, str_length(m_aInputStr) - 1);
			wclear(m_pInputWin);
			InputDraw();
			DrawBorders(m_pInputWin);
			return 0;
		}
		char aKey[8];
		str_format(aKey, sizeof(aKey), "%c", c);
		str_append(m_aInputStr, aKey, sizeof(m_aInputStr));
		// ChillerLog("yeee", "got key d=%d c=%c", c, c);
	}
	InputDraw();
	return 0;
}

void CTerminalUI::ChillerLogPush(const char *pStr)
{
	// first empty slot
	int x, y;
	getmaxyx(m_pLogWindow, y, x);
	int Max = CHILLER_LOGGER_HEIGHT > y ? y : CHILLER_LOGGER_HEIGHT;
	int Top = CHILLER_LOGGER_HEIGHT-2;
	int Bottom = CHILLER_LOGGER_HEIGHT-Max;
	str_format(m_aInfoStr, sizeof(m_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d top=%d bottom=%d                                            ",
		Max, CHILLER_LOGGER_HEIGHT, y, Top, Bottom
	);
	m_NeedLogDraw = true;
	for(int i = Top; i > Bottom; i--)
	{
		if(m_aaChillerLogger[i][0] == '\0')
		{
			str_copy(m_aaChillerLogger[i], pStr, sizeof(m_aaChillerLogger[i]));
			// str_format(m_aInfoStr, sizeof(m_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d i=%d", Max, CHILLER_LOGGER_HEIGHT, y, i);
			return;
		}
	}
	str_format(m_aInfoStr, sizeof(m_aInfoStr), "shifitng max=%d CHILLER_LOGGER_HEIGHT=%d y=%d FULLL!!! top=%d bottom=%d                          ",
		Max, CHILLER_LOGGER_HEIGHT, y, Top, Bottom
	);
	// no free slot found -> shift all
	for(int i = Top; i > 0; i--)
	{
		str_copy(m_aaChillerLogger[i], m_aaChillerLogger[i-1], sizeof(m_aaChillerLogger[i]));
	}
	// insert newest on the bottom
	str_copy(m_aaChillerLogger[Bottom+1], pStr, sizeof(m_aaChillerLogger[Bottom+1]));
	wclear(m_pLogWindow);
	DrawBorders(m_pLogWindow);
}

// ChillerDragon: no fucking idea why on macOS vdbg needs it but dbg doesn't
//				  yes this is a format vuln but only caused if used wrong same as in dbg_msg
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void CTerminalUI::ChillerLog(const char *sys, const char *fmt, ...)
{
	// if(!g_Config.m_ClNcurses)
	// {
	// 	va_list args;
	// 	va_start(args, fmt);
	// 	vdbg_msg(sys, fmt, args);
	// 	va_end(args);
	// 	return;
	// }

	va_list args;
	char str[1024*4];
	char *msg;
	int len;

	char timestr[80];
	str_timestamp_format(timestr, sizeof(timestr), FORMAT_SPACE);

	str_format(str, sizeof(str), "[%s][%s]: ", timestr, sys);

	len = strlen(str);
	msg = (char *)str + len;

	va_start(args, fmt);
#if defined(CONF_FAMILY_WINDOWS) && !defined(__GNUC__)
	_vsprintf_p(msg, sizeof(str)-len, fmt, args);
#else
	vsnprintf(msg, sizeof(str)-len, fmt, args);
#endif
	va_end(args);

	// printf("%s\n", str);
	ChillerLogPush(str);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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

	// Client()->ChillerLog("termUI", "got key d=%d c=%c", Key, Key);
	return 0;
}

#endif
