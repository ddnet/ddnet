// ChillerDragon 2020 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/chillerbot/curses_colors.h>
#include <base/terminalui.h>

#include "pad_utf8.h"

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

#include <csignal>
#include <sys/ioctl.h>

CGameClient *g_pClient;
volatile sig_atomic_t cl_InterruptSignaled = 0;

void HandleSigIntTerm(int Param)
{
	cl_InterruptSignaled = 1;

	// Exit the next time a signal is received
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

void CTerminalUI::OnConsoleInit()
{
	Console()->Register("term", "?s[action]?s[arg]", CFGFLAG_CLIENT, ConTerm, this, "Command for the terminal/curses client (see 'term help')");
}

void CTerminalUI::ConTerm(IConsole::IResult *pResult, void *pUserData)
{
	CTerminalUI *pSelf = (CTerminalUI *)pUserData;
	if(!str_comp(pResult->GetString(0), "help") || pResult->NumArguments() < 1)
	{
		dbg_msg("term-ux", "/----------------- term -------------\\");
		dbg_msg("term-ux", "usage: term ?s[action]?s[arg]");
		dbg_msg("term-ux", "description: commands for the term-ux client (not for gui clients)");
		dbg_msg("term-ux", "actions:");
		dbg_msg("term-ux", "  help                show this help");
		dbg_msg("term-ux", "  visual <on|off>     show this help");
		dbg_msg("term-ux", "  hist <load|save>    load/save history");
		dbg_msg("term-ux", "  page <up|down>      scroll in chat/console log");
		dbg_msg("term-ux", "  scroll <offset>     scroll in chat/console log");
		dbg_msg("term-ux", "\\----------------- term -------------/");
		return;
	}
	if(!str_comp(pResult->GetString(0), "hist"))
	{
		if(!str_comp(pResult->GetString(1), "load"))
		{
			for(int i = 0; i < NUM_INPUTS; i++)
				pSelf->LoadInputHistoryFile(i);
		}
		else if(!str_comp(pResult->GetString(1), "save"))
		{
			for(int i = 0; i < NUM_INPUTS; i++)
				pSelf->SaveCurrentHistoryBufferToDisk(i);
		}
		else
		{
			dbg_msg("term-ux", "usage: term hist s[arg]");
			dbg_msg("term-ux", "args:");
			dbg_msg("term-ux", " load - load history files for all inputs");
			dbg_msg("term-ux", " save - save history files for all inputs");
		}
	}
	else if(!str_comp(pResult->GetString(0), "visual"))
	{
		bool TurnOn = !str_comp(pResult->GetString(1), "on") || !str_comp(pResult->GetString(1), "1");
		if(TurnOn == pSelf->m_RenderGame)
			dbg_msg("term-ux", "visual mode is already %s", TurnOn ? "on" : "off");
		else
			dbg_msg("term-ux", "visual mode is now %s", TurnOn ? "on" : "off");
		pSelf->m_RenderGame = TurnOn;
	}
	else if(!str_comp(pResult->GetString(0), "page"))
	{
		bool PageUp = !str_comp(pResult->GetString(1), "up") || !str_comp(pResult->GetString(1), "prev");
		dbg_msg("term-ux", "scrolling page %s", PageUp ? "up" : "down");
		int _x, y;
		getmaxyx(g_LogWindow.m_pCursesWin, y, _x);
		scroll_curses_log(PageUp ? -y : y);
	}
	else if(!str_comp(pResult->GetString(0), "scroll"))
	{
		int offset = atoi(pResult->GetString(1));
		dbg_msg("term-ux", "setting scroll offset to %d after clamp [%d/%d]", offset, set_curses_log_scroll(offset), CHILLER_LOGGER_WIDTH);
	}
	else
	{
		dbg_msg("term-ux", "Invalid term action '%s'", pResult->GetString(0));
	}
}

void CTerminalUI::DrawAllBorders()
{
	g_LogWindow.DrawBorders();
	g_InfoWin.DrawBorders();
	g_InputWin.DrawBorders();
}

void CTerminalUI::LogDraw()
{
	log_draw();
}

void CTerminalUI::InfoDraw()
{
	int x = getmaxx(g_InfoWin.m_pCursesWin);
	char aBuf[1024 * 4 + 16];
	str_format(aBuf, sizeof(aBuf), "%s | %s | %s", GetInputMode(), g_aInfoStr, g_aInfoStr2);
	aBuf[x - 2] = '\0'; // prevent line wrapping and cut on screen border
	mvwprintw(g_InfoWin.m_pCursesWin, 1, 1, "%s", aBuf);
}

void CTerminalUI::InputDraw()
{
	if(!g_InputWin.IsActive())
		return;

	char aBuf[512];
	if(InputMode() == INPUT_REMOTE_CONSOLE && !RconAuthed())
	{
		int i;
		for(i = 0; i < 512 && g_aInputStr[i] != '\0'; i++)
			aBuf[i] = '*';
		aBuf[i] = '\0';
	}
	else
		str_copy(aBuf, g_aInputStr, sizeof(aBuf));
	int x = getmaxx(g_InputWin.m_pCursesWin);
	if(x < (int)sizeof(aBuf))
		aBuf[x - 2] = '\0'; // prevent line wrapping and cut on screen border
	wattron(g_InputWin.m_pCursesWin, COLOR_PAIR(WHITE_ON_BLACK));
	wattron(g_InputWin.m_pCursesWin, A_BOLD);
	mvwprintw(g_InputWin.m_pCursesWin, 1, 1 + g_InputWin.m_CurserOffset, "%s", aBuf);
	refresh();
	wattroff(g_InputWin.m_pCursesWin, A_BOLD);
	wattroff(g_InputWin.m_pCursesWin, COLOR_PAIR(CYAN_ON_BLACK));
	if(m_aCompletionPreview[0])
	{
		int Offset = str_length(aBuf) + 1;
		int LineWrap = x - (Offset + 2);
		if(LineWrap > 0)
		{
			m_aCompletionPreview[LineWrap] = '\0';
			mvwprintw(g_InputWin.m_pCursesWin, 1, Offset, " %s", m_aCompletionPreview);
		}
	}
	refresh();
	UpdateCursor();
}

int CTerminalUI::CursesTick()
{
	// getmaxyx does not work with gdb see https://github.com/chillerbot/chillerbot-ux/issues/128
	getmaxyx(stdscr, g_NewY, g_NewX);

	// Works better with gdb but breaks the github pipeline
	// struct winsize w;
	// ioctl(0, TIOCGWINSZ, &w);
	// g_NewX = w.ws_col;
	// g_NewY = w.ws_row;

	if(g_NewY != g_ParentY || g_NewX != g_ParentX || g_TriggerResize)
	{
		g_TriggerResize = false;

		g_ParentX = g_NewX;
		g_ParentY = g_NewY;

		wresize(g_InfoWin.m_pCursesWin, INFO_WIN_HEIGHT, g_NewX);

		bool ForceBottomBecauseNoSpace = getmaxy(g_LogWindow.m_pCursesWin) < HEIGHT_NEEDED_FOR_SERVER_BROWSER_OFFSET_TOP;
		bool SearchBarOnTop = InputMode() == INPUT_BROWSER_SEARCH && g_Config.m_ClTermBrowserSearchTop;
		// search bar on top
		if(!ForceBottomBecauseNoSpace && SearchBarOnTop)
		{
			// this works if you want to move the log down instead of overlap
			// mvwin(g_LogWindow.m_pCursesWin, g_InputWin.m_Height, 0);

			wresize(g_LogWindow.m_pCursesWin, g_NewY - INFO_WIN_HEIGHT, g_NewX);
			mvwin(g_LogWindow.m_pCursesWin, 0, 0);

			mvwin(g_InfoWin.m_pCursesWin, g_NewY - INFO_WIN_HEIGHT, 0);
			wresize(g_InputWin.m_pCursesWin, g_InputWin.m_Height, m_WinServerBrowser.m_Width);
			// dbg_msg("chiller", "%d %d", m_WinServerBrowser.m_Y - g_InputWin.m_Height, m_WinServerBrowser.m_X);
			// mvwin(g_InputWin.m_pCursesWin, m_WinServerBrowser.m_Y - g_InputWin.m_Height, m_WinServerBrowser.m_X);
			mvwin(g_InputWin.m_pCursesWin, m_WinServerBrowser.m_Y - (g_InputWin.m_Height + 1), m_WinServerBrowser.m_X);
		}
		else // search bar on bottom
		{
			// mvwin(g_LogWindow.m_pCursesWin, 0, 0);
			wresize(g_LogWindow.m_pCursesWin, g_NewY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), g_NewX);
			wresize(g_InputWin.m_pCursesWin, g_InputWin.m_Height, g_NewX);

			mvwin(g_InfoWin.m_pCursesWin, g_NewY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), 0);
			mvwin(g_InputWin.m_pCursesWin, g_NewY - g_InputWin.m_Height, 0);
		}

		wclear(stdscr);
		wclear(g_InfoWin.m_pCursesWin);
		if(g_InputWin.IsActive())
		{
			wclear(g_InputWin.m_pCursesWin);
			g_InputWin.DrawBorders();
			InputDraw();
		}

		g_LogWindow.DrawBorders();
		g_InfoWin.DrawBorders();

		if(m_pClient->m_Snap.m_pLocalCharacter && m_RenderGame)
		{
			wresize(g_GameWindow.m_pCursesWin, g_NewY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), g_NewX); // TODO: fix this size
			wclear(g_LogWindow.m_pCursesWin);
			g_GameWindow.DrawBorders();
		}

		gs_NeedLogDraw = true;
	}

	// draw to our windows
	LogDraw();
	InfoDraw();
	if(m_pClient->m_Snap.m_pLocalCharacter && m_RenderGame)
		RenderGame();
	RenderServerList();
	RenderConnecting();
	RenderPopup();
	RenderHelpPage();
	if(m_pClient->m_Snap.m_pLocalCharacter)
		RenderScoreboard(0, &g_LogWindow);

	int input = GetInput(); // calls InputDraw()

	// refresh each window
	curses_refresh_windows();
	static int s_LastLogsPushed = 0;
	if(gs_LogsPushed != s_LastLogsPushed)
	{
		gs_LogsPushed = s_LastLogsPushed;
		gs_NeedLogDraw = true;
	}
	if(m_NewInput)
	{
		m_NewInput = false;
		gs_NeedLogDraw = true;
	}
	return input == -1;
}

void CTerminalUI::OnInit()
{
	str_copy(m_aTileSolidTexture, "█", sizeof(m_aTileSolidTexture));
	str_copy(m_aTileUnhookTexture, "▓", sizeof(m_aTileUnhookTexture));
	m_UpdateCompletionBuffer = true;
	m_aCompletionPreview[0] = '\0';
	m_CompletionEnumerationCount = -1;
	m_CompletionChosen = -1;
	m_RenderGame = false;
	m_SendData[0] = false;
	m_SendData[1] = false;
	m_NextRender = 0;
	m_aPopupTitle[0] = '\0';
	m_aCompletionBuffer[0] = '\0';
	ResetCompletion();
	m_LockKeyUntilRelease = EOF;
	m_InputCursor = 0;
	m_NumServers = 0;
	m_SelectedServer = 0;
	m_RenderServerList = false;
	m_ScoreboardActive = false;
	m_RenderHelpPage = false;
	g_pClient = m_pClient;
	mem_zero(m_aLastPressedKey, sizeof(m_aLastPressedKey));
	mem_zero(m_aaInputHistory, sizeof(m_aaInputHistory));
	mem_zero(m_InputHistory, sizeof(m_InputHistory));
	AimX = 20;
	AimY = 0;
	setlocale(LC_CTYPE, "");
	curses_init();
	m_InputMode = INPUT_NORMAL;
	initscr();
	noecho();
	curs_set(TRUE);
	start_color();
	init_curses_colors();

	// set up initial windows
	getmaxyx(stdscr, g_ParentY, g_ParentX);

	g_LogWindow.m_pCursesWin = newwin(g_ParentY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), g_ParentX, 0, 0);
	g_GameWindow.m_pCursesWin = newwin(g_ParentY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), g_ParentX, 0, 0); // TODO: fix this size
	g_InfoWin.m_pCursesWin = newwin(g_InputWin.m_Height, g_ParentX, g_ParentY - (g_InputWin.m_Height + INFO_WIN_HEIGHT), 0);
	g_InputWin.m_pCursesWin = newwin(g_InputWin.m_Height, g_ParentX, g_ParentY - g_InputWin.m_Height, 0);

	g_LogWindow.DrawBorders();
	g_InfoWin.DrawBorders();
	g_InputWin.DrawBorders();

	signal(SIGINT, HandleSigIntTerm);
	signal(SIGTERM, HandleSigIntTerm);

	gs_Logfile = 0;
	// if(g_Config.m_Logfile[0])
	// {
	// 	gs_Logfile = io_open(g_Config.m_Logfile, IOFLAG_WRITE);
	// }

	if(g_Config.m_ClTermHistory)
		for(int i = 0; i < NUM_INPUTS; i++)
			LoadInputHistoryFile(i);

	const char aaWeclomePopup[][128] = {
		"Keybindings:",
		"?       - opens a help page",
		"b       - to open server list",
		"enter   - to close popups like this one"};

	DoPopup(POPUP_NOT_IMPORTANT, "term-ux - chillerbot-ux in the terminal", aaWeclomePopup[0], 128, sizeof(aaWeclomePopup) / 128);
}

void CTerminalUI::OnReset()
{
	m_BroadcastTick = 0;
}

void CTerminalUI::OnShutdown()
{
	endwin();
	if(g_Config.m_ClTermHistory)
		for(int i = 0; i < NUM_INPUTS; i++)
			SaveCurrentHistoryBufferToDisk(i);
}

void CTerminalUI::OnMapLoad()
{
	str_copy(m_aTileSolidTexture, "█", sizeof(m_aTileSolidTexture));
	str_copy(m_aTileUnhookTexture, "▓", sizeof(m_aTileUnhookTexture));
	if(!str_comp(Client()->GetCurrentMap(), "Multimap") ||
		!str_comp(Client()->GetCurrentMap(), "Multeasymap"))
	{
		str_copy(m_aTileSolidTexture, "🟩", sizeof(m_aTileSolidTexture));
	}
	else if(!str_comp(Client()->GetCurrentMap(), "Copy Love Box") ||
		!str_comp(Client()->GetCurrentMap(), "Copy Love Box 2s"))
	{
		str_copy(m_aTileUnhookTexture, "🟦", sizeof(m_aTileUnhookTexture));
	}
}

void CTerminalUI::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_BROADCAST)
	{
		if(!g_Config.m_ClShowBroadcasts)
			return;
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
		g_LogWindow.SetTextTopLeft(pMsg->m_pMessage);
		m_BroadcastTick = Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() * 10;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
}

void CTerminalUI::OnRender()
{
	m_SendData[0] = false;
	m_SendData[1] = false;
	CursesTick();

	if(m_BroadcastTick && Client()->GameTick(g_Config.m_ClDummy) > m_BroadcastTick)
	{
		g_LogWindow.SetTextTopLeft("");
		gs_NeedLogDraw = true;
		m_NewInput = true;
		m_BroadcastTick = 0;
	}

	if(cl_InterruptSignaled)
		Console()->ExecuteLine("quit");

	if(!m_pClient->m_Snap.m_pLocalCharacter)
		return;
	float X = m_pClient->m_Snap.m_pLocalCharacter->m_X;
	float Y = m_pClient->m_Snap.m_pLocalCharacter->m_Y;
	str_format(g_aInfoStr2, sizeof(g_aInfoStr2),
		"%.2f %.2f scoreboard=%d",
		X / 32, Y / 32,
		m_ScoreboardActive);
}

void CTerminalUI::SetInputMode(int Mode)
{
	int Old = InputMode();
	m_InputMode = Mode;
	OnInputModeChange(Old, Mode);
}

void CTerminalUI::OnInputModeChange(int Old, int New)
{
	g_InputWin.SetSearch(New == INPUT_BROWSER_SEARCH);
	CloseMenu();
	if(New == INPUT_NORMAL)
	{
		if(g_InputWin.IsActive())
		{
			g_InputWin.Close();
			g_TriggerResize = true;
		}
	}
	else
	{
		if(!g_InputWin.IsActive())
		{
			g_InputWin.Open();
			g_TriggerResize = true;
		}
	}
	ResetCompletion();
	ClearInput();
	if(New == INPUT_BROWSER_SEARCH)
	{
		str_copy(g_aInputStr, g_Config.m_BrFilterString, sizeof(g_aInputStr));
		m_InputCursor = str_length(g_aInputStr);
	}
	m_aCompletionPreview[0] = '\0';
	if(g_InputWin.IsActive())
		wclear(g_InputWin.m_pCursesWin);
	InputDraw();
	g_InputWin.DrawBorders();
	UpdateCursor();
}

static void StrCopyUntilSpace(char *pDest, size_t DestSize, const char *pSrc)
{
	const char *pSpace = str_find(pSrc, " ");
	// if no space found consume the whole thing
	if(!pSpace)
	{
		str_copy(pDest, pSrc, DestSize);
		return;
	}
	str_copy(pDest, pSrc, minimum<size_t>(pSpace ? pSpace - pSrc + 1 : 1, DestSize));
}

int CTerminalUI::GetInput()
{
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	cbreak();
	noecho();
	int c = getch();

	if(c == m_LockKeyUntilRelease)
		return EOF;

	// always skip
	// nonblocking empty read
	m_LockKeyUntilRelease = EOF;

	if(InputMode() == INPUT_NORMAL)
	{
		if(c == 'q')
			return -1;
		else if(c == KEY_F(1))
			SetInputMode(INPUT_LOCAL_CONSOLE);
		else if(c == KEY_F(2))
			SetInputMode(INPUT_REMOTE_CONSOLE);
		else if(c == 't')
			SetInputMode(INPUT_CHAT);
		else if(c == 'z')
			SetInputMode(INPUT_CHAT_TEAM);
		else
		{
			InputDraw();
			OnKeyPress(c, g_LogWindow.m_pCursesWin);
		}
	}
	else if(
		InputMode() == INPUT_LOCAL_CONSOLE ||
		InputMode() == INPUT_REMOTE_CONSOLE ||
		InputMode() == INPUT_CHAT ||
		InputMode() == INPUT_CHAT_TEAM ||
		InputMode() == INPUT_BROWSER_SEARCH ||

		InputMode() == INPUT_SEARCH_LOCAL_CONSOLE ||
		InputMode() == INPUT_SEARCH_REMOTE_CONSOLE ||
		InputMode() == INPUT_SEARCH_CHAT ||
		InputMode() == INPUT_SEARCH_CHAT_TEAM ||
		InputMode() == INPUT_SEARCH_BROWSER_SEARCH)
	{
		if(c == 10) // return / enter
		{
			// do not repeat return input
			// if the enter key is being held
			m_LockKeyUntilRelease = c;

			if(InputMode() == INPUT_LOCAL_CONSOLE)
				m_pClient->Console()->ExecuteLine(g_aInputStr);
			else if(InputMode() == INPUT_REMOTE_CONSOLE)
			{
				if(m_pClient->Client()->RconAuthed())
					m_pClient->Client()->Rcon(g_aInputStr);
				else
					m_pClient->Client()->RconAuth("", g_aInputStr);
			}
			else if(InputMode() == INPUT_CHAT)
				m_pClient->m_Chat.SendChat(0, g_aInputStr);
			else if(InputMode() == INPUT_CHAT_TEAM)
				m_pClient->m_Chat.SendChat(1, g_aInputStr);
			else if(InputMode() == INPUT_BROWSER_SEARCH)
			{
				m_SelectedServer = 0;
				str_copy(g_Config.m_BrFilterString, g_aInputStr, sizeof(g_Config.m_BrFilterString));
				ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
			}

			if(IsSearchInputMode())
			{
				SetInputMode(InputMode() - (NUM_INPUTS - 1));
				str_copy(g_aInputStr, m_aInputSearchMatch, sizeof(g_aInputStr));
			}
			else
			{
				// do not store rcon password in history
				if(!(InputMode() == INPUT_REMOTE_CONSOLE && !RconAuthed()))
					AddInputHistory(InputMode(), g_aInputStr);
				m_InputHistory[InputMode()] = 0;
				ClearInput();
				if(InputMode() != INPUT_LOCAL_CONSOLE && InputMode() != INPUT_REMOTE_CONSOLE)
					SetInputMode(INPUT_NORMAL);
			}
			m_InputCursor = 0;
			wclear(g_InputWin.m_pCursesWin);
			InputDraw();
			g_InputWin.DrawBorders();
			return 0;
		}
		else if(c == KEY_F(1)) // f1 hard toggle local console
		{
			ClearInput();
			wclear(g_InputWin.m_pCursesWin);
			g_InputWin.DrawBorders();
			SetInputMode(InputMode() == INPUT_LOCAL_CONSOLE ? INPUT_NORMAL : INPUT_LOCAL_CONSOLE);
			InputDraw();
			return 0;
		}
		else if(c == KEY_F(2)) // f2 hard toggle local console
		{
			ClearInput();
			wclear(g_InputWin.m_pCursesWin);
			g_InputWin.DrawBorders();
			SetInputMode(InputMode() == INPUT_REMOTE_CONSOLE ? INPUT_NORMAL : INPUT_REMOTE_CONSOLE);
			InputDraw();
			return 0;
		}
		else if(c == KEY_BTAB)
		{
			if(InputMode() == INPUT_REMOTE_CONSOLE || InputMode() == INPUT_LOCAL_CONSOLE)
				CompleteCommands(true);
			else
				CompleteNames(true);
			RefreshConsoleCmdHelpText();
			return 0;
		}
		else if(keyname(c)[0] == '^' && keyname(c)[1] == 'I') // tab
		{
			if(InputMode() == INPUT_REMOTE_CONSOLE || InputMode() == INPUT_LOCAL_CONSOLE)
				CompleteCommands();
			else
				CompleteNames();
			RefreshConsoleCmdHelpText();
			return 0;
		}
		else if(keyname(c)[0] == '^' && keyname(c)[1] == '[')
		{
			c = getch();
			if(c == EOF) // ESC
			{
				SetInputMode(INPUT_NORMAL);
				return 0;
			}
			// TODO: make this code nicer omg xd
			else if(keyname(c)[0] == '[')
			{
				c = getch();
				if(keyname(c)[0] == '1')
				{
					c = getch();
					if(keyname(c)[0] == ';')
					{
						c = getch();
						if(keyname(c)[0] == '5')
						{
							c = getch();
							if(keyname(c)[0] == 'D') // ctrl+left
							{
								bool IsSpace = true;
								const char *pInput = g_aInputStr;
								if(InputMode() > NUM_INPUTS) // reverse i search
									pInput = m_aInputSearch;
								for(int i = m_InputCursor; i > 0; i--)
								{
									if(pInput[i] == ' ' && IsSpace)
										continue;
									if(i == 1) // reach beginning of line no spaces
									{
										m_InputCursor = 0;
										UpdateCursor();
										break;
									}
									if(pInput[i] != ' ')
									{
										IsSpace = false;
										continue;
									}
									m_InputCursor = i;
									UpdateCursor();
									break;
								}
								return 0;
							}
							else if(keyname(c)[0] == 'C') // ctrl+right
							{
								bool IsSpace = true;
								int InputLen = str_length(g_aInputStr);
								const char *pInput = g_aInputStr;
								if(InputMode() > NUM_INPUTS) // reverse i search
								{
									InputLen = str_length(m_aInputSearch);
									pInput = m_aInputSearch;
								}
								for(int i = m_InputCursor; i < InputLen; i++)
								{
									if(pInput[i] == ' ' && IsSpace)
										continue;
									if(i == InputLen - 1) // reach end of line no spaces
									{
										m_InputCursor = InputLen;
										UpdateCursor();
										break;
									}
									if(pInput[i] != ' ')
									{
										IsSpace = false;
										continue;
									}
									m_InputCursor = i;
									UpdateCursor();
									break;
								}
								return 0;
							}
						}
					}
				}
			}
		}
		else if(c == KEY_BACKSPACE || c == 127) // delete
		{
			ResetCompletion();
			int InputLen = str_length(g_aInputStr);
			if(InputLen < 2)
			{
				// deleting until the start
				// is a clear indication of not wanting to
				// see tab completion preview anymore
				ClearCompletionPreview();
				if(InputLen < 1)
					return 0;
			}
			char aRight[1024];
			char aLeft[1024];

			if(InputMode() > NUM_INPUTS) // reverse i search
			{
				str_copy(aRight, m_aInputSearch + m_InputCursor, sizeof(aRight));
				str_copy(aLeft, m_aInputSearch, sizeof(aLeft));
				aLeft[m_InputCursor > 0 ? m_InputCursor - 1 : m_InputCursor] = '\0';
				str_format(m_aInputSearch, sizeof(m_aInputSearch), "%s%s", aLeft, aRight);
				RenderInputSearch();
			}
			else
			{
				str_copy(aRight, g_aInputStr + m_InputCursor, sizeof(aRight));
				str_copy(aLeft, g_aInputStr, sizeof(aLeft));
				aLeft[m_InputCursor > 0 ? m_InputCursor - 1 : m_InputCursor] = '\0';
				str_format(g_aInputStr, sizeof(g_aInputStr), "%s%s", aLeft, aRight);
			}
			m_InputCursor = clamp(m_InputCursor - 1, 0, str_length(g_aInputStr));
			wclear(g_InputWin.m_pCursesWin);
			InputDraw();
			g_InputWin.DrawBorders();
			UpdateCursor();
			return 0;
		}
		else if(c == 260) // left arrow
		{
			g_InputWin.NextMenuItem();
			// could be used for cursor movement
			m_InputCursor = clamp(m_InputCursor - 1, 0, str_length(g_aInputStr));
			UpdateCursor();
			return 0;
		}
		else if(c == 261) // right arrow
		{
			g_InputWin.PrevMenuItem();
			// could be used for cursor movement
			m_InputCursor = clamp(m_InputCursor + 1, 0, str_length(g_aInputStr));
			UpdateCursor();
			return 0;
		}
		else if(keyname(c)[0] == '^')
		{
			if(keyname(c)[1] == 'U') // ctrl+u
			{
				ResetCompletion();
				char aRight[1024];
				if(InputMode() > NUM_INPUTS) // reverse i search
				{
					str_copy(aRight, m_aInputSearch + m_InputCursor, sizeof(aRight));
					str_copy(m_aInputSearch, aRight, sizeof(m_aInputSearch));
					RenderInputSearch();
				}
				else
				{
					str_copy(aRight, g_aInputStr + m_InputCursor, sizeof(aRight));
					str_copy(g_aInputStr, aRight, sizeof(g_aInputStr));
					if(str_length(g_aInputStr) < 2)
						ClearCompletionPreview();
					wclear(g_InputWin.m_pCursesWin);
					InputDraw();
					g_InputWin.DrawBorders();
				}
				m_InputCursor = 0;
				UpdateCursor();
			}
			if(keyname(c)[1] == 'K') // ctrl+k
			{
				ResetCompletion();
				if(InputMode() > NUM_INPUTS) // reverse i search
				{
					m_aInputSearch[m_InputCursor] = '\0';
					RenderInputSearch();
				}
				else
				{
					g_aInputStr[m_InputCursor] = '\0';
					if(str_length(g_aInputStr) < 2)
						ClearCompletionPreview();
					wclear(g_InputWin.m_pCursesWin);
					InputDraw();
					g_InputWin.DrawBorders();
				}
				UpdateCursor();
			}
			if(keyname(c)[1] == 'R' && InputMode() > INPUT_NORMAL && !IsSearchInputMode()) // ctrl+r
			{
				SetInputMode(InputMode() + (NUM_INPUTS - 1));
				m_aInputSearch[0] = '\0';
				str_copy(g_aInputStr, "(reverse-i-search)`': ", sizeof(g_aInputStr));
				wclear(g_InputWin.m_pCursesWin);
				InputDraw();
				g_InputWin.DrawBorders();
				UpdateCursor();
			}
			else if(keyname(c)[1] == 'E') // ctrl+e
			{
				ResetCompletion();
				m_InputCursor = str_length(g_aInputStr);
				if(InputMode() > NUM_INPUTS) // reverse i search
					m_InputCursor = str_length(m_aInputSearch);
				UpdateCursor();
			}
			else if(keyname(c)[1] == 'A') // ctrl+a
			{
				ResetCompletion();
				m_InputCursor = 0;
				UpdateCursor();
			}
			return 0;
		}
		if((c == 258 || c == 259) && InputMode() >= 0) // up/down arrow scroll history
		{
			ResetCompletion();
			str_copy(g_aInputStr, m_aaInputHistory[InputMode()][m_InputHistory[InputMode()]], sizeof(g_aInputStr));
			wclear(g_InputWin.m_pCursesWin);
			InputDraw();
			g_InputWin.DrawBorders();
			// update history index after using it because index 0 is already the last item
			int OldHistory = m_InputHistory[InputMode()];
			if(c == 258) // down arrow
				m_InputHistory[InputMode()] = clamp(m_InputHistory[InputMode()] - 1, 0, (int)(INPUT_HISTORY_MAX_LEN));
			else if(c == 259) // up arrow
				m_InputHistory[InputMode()] = clamp(m_InputHistory[InputMode()] + 1, 0, (int)(INPUT_HISTORY_MAX_LEN));

			// stop scrolling and roll back when reached an empty history element
			if(m_aaInputHistory[InputMode()][m_InputHistory[InputMode()]][0] == '\0')
				m_InputHistory[InputMode()] = OldHistory;
		}
		else
		{
			ResetCompletion();
			char aKey[8];
			str_format(aKey, sizeof(aKey), "%c", c);
			// pressing space clears completion suggestions
			if(c == ' ')
				ClearCompletionPreview();
			// str_append(g_aInputStr, aKey, sizeof(g_aInputStr));
			char aRight[1024];
			char aLeft[1024];
			if(InputMode() > NUM_INPUTS) // reverse i search
			{
				str_copy(aRight, m_aInputSearch + m_InputCursor, sizeof(aRight));
				str_copy(aLeft, m_aInputSearch, sizeof(aLeft));
				aLeft[m_InputCursor] = '\0';
				str_format(m_aInputSearch, sizeof(m_aInputSearch), "%s%s%s", aLeft, aKey, aRight);
				RenderInputSearch();
			}
			else
			{
				str_copy(aRight, g_aInputStr + m_InputCursor, sizeof(aRight));
				str_copy(aLeft, g_aInputStr, sizeof(aLeft));
				aLeft[m_InputCursor] = '\0';
				str_format(g_aInputStr, sizeof(g_aInputStr), "%s%s%s", aLeft, aKey, aRight);
			}
			m_InputCursor += str_length(aKey);
		}

		// can we do this more performant?
		// do we need to check command help
		// after EVERY key press?
		//
		// has to stay at the end to show hint
		// of the latest input not of the prev input
		RefreshConsoleCmdHelpText();

		// dbg_msg("yeee", "got key d=%d c=%c", c, c);
	}
	InputDraw();
	return 0;
}

void CTerminalUI::RefreshConsoleCmdHelpText()
{
	// show command hints in console

	m_aCommandName[0] = '\0';
	m_aCommandHelp[0] = '\0';
	m_aCommandParams[0] = '\0';

	g_InputWin.SetTextTopLeftYellow("");
	g_InputWin.SetTextTopLeft("");

	if(InputMode() == INPUT_REMOTE_CONSOLE || InputMode() == INPUT_LOCAL_CONSOLE)
	{
		int CompletionFlagmask = CFGFLAG_CLIENT;
		int Type = CGameConsole::CONSOLETYPE_LOCAL;
		if(InputMode() == INPUT_REMOTE_CONSOLE)
		{
			CompletionFlagmask = CFGFLAG_SERVER;
			Type = CGameConsole::CONSOLETYPE_REMOTE;
		}

		// find the current command
		char aCurrentCmd[sizeof(m_aCommandName)];
		StrCopyUntilSpace(aCurrentCmd, sizeof(aCurrentCmd), g_aInputStr);
		const IConsole::CCommandInfo *pCommand = Console()->GetCommandInfo(aCurrentCmd, CompletionFlagmask,
			Type != CGameConsole::CONSOLETYPE_LOCAL && Client()->RconAuthed() && Client()->UseTempRconCommands());

		// use this to highlight command as green
		// and make it white otherwise
		// like zsh does it
		// m_IsCommand = false;
		if(pCommand)
		{
			// m_IsCommand = true;
			str_copy(m_aCommandName, pCommand->m_pName);
			str_copy(m_aCommandHelp, pCommand->m_pHelp);
			str_copy(m_aCommandParams, pCommand->m_pParams);
			// dbg_msg("cmd", "help: %s", m_aCommandHelp);
			char aBuf[512];
			// str_format(aBuf, sizeof(aBuf), "%s %s - %s", aCurrentCmd, m_aCommandParams, m_aCommandHelp);
			str_format(
				aBuf,
				sizeof(aBuf),
				"%s%s%s",
				aCurrentCmd,
				m_aCommandParams[0] == '\0' ? "" : " ",
				m_aCommandParams);
			g_InputWin.SetTextTopLeftYellow(aBuf);
			str_format(aBuf, sizeof(aBuf), " - %s", m_aCommandHelp);
			g_InputWin.SetTextTopLeft(aBuf);
			wclear(g_InputWin.m_pCursesWin);
			InputDraw();
			g_InputWin.DrawBorders();
		}
	}
}

void CTerminalUI::CompleteCommands(bool IsReverse)
{
	int CompletionFlagmask = 0;
	if(InputMode() == INPUT_LOCAL_CONSOLE)
		CompletionFlagmask = CFGFLAG_CLIENT;
	else
		CompletionFlagmask = CFGFLAG_SERVER;

	if(m_CompletionEnumerationCount == -1)
		str_copy(m_aCompletionBuffer, g_aInputStr, sizeof(m_aCompletionBuffer));

	// dbg_msg("complete", "buffer='%s' index=%d count=%d", m_aCompletionBuffer, m_CompletionChosen, m_CompletionEnumerationCount);

	ClearCompletionPreview();
	m_CompletionEnumerationCount = 0;
	if(IsReverse)
		m_CompletionChosen--;
	else
		m_CompletionChosen++;
	Console()->PossibleCommands(m_aCompletionBuffer, CompletionFlagmask, InputMode() != INPUT_LOCAL_CONSOLE && Client()->RconAuthed() && Client()->UseTempRconCommands(), PossibleCommandsCompleteCallback, this);

	// handle wrapping
	if(m_CompletionEnumerationCount && (m_CompletionChosen >= m_CompletionEnumerationCount || m_CompletionChosen < 0))
	{
		m_CompletionChosen = (m_CompletionChosen + m_CompletionEnumerationCount) % m_CompletionEnumerationCount;
		m_CompletionEnumerationCount = 0;
		Console()->PossibleCommands(m_aCompletionBuffer, CompletionFlagmask, InputMode() != INPUT_LOCAL_CONSOLE && Client()->RconAuthed() && Client()->UseTempRconCommands(), PossibleCommandsCompleteCallback, this);
	}

	wclear(g_InputWin.m_pCursesWin);
	InputDraw();
	g_InputWin.DrawBorders();
	UpdateCursor();
}

void CTerminalUI::PossibleCommandsCompleteCallback(int Index, const char *pStr, void *pUser)
{
	CTerminalUI *pSelf = (CTerminalUI *)pUser;
	if(pSelf->m_CompletionChosen == pSelf->m_CompletionEnumerationCount)
	{
		str_copy(g_aInputStr, pStr, sizeof(g_aInputStr));
		pSelf->m_InputCursor = str_length(pStr);
	}
	else if(pSelf->m_CompletionChosen < pSelf->m_CompletionEnumerationCount)
	{
		str_append(pSelf->m_aCompletionPreview, pStr, sizeof(pSelf->m_aCompletionPreview));
		str_append(pSelf->m_aCompletionPreview, " ", sizeof(pSelf->m_aCompletionPreview));
	}
	pSelf->m_CompletionEnumerationCount++;
}

void CTerminalUI::CompleteNames(bool IsReverse)
{
	bool IsReverseEnd = false;
	if(IsReverse)
		m_CompletionIndex--;
	else
		m_CompletionIndex++;
	if(m_CompletionIndex < 0 && IsReverse)
		IsReverseEnd = true;
	bool IsSpace = true;
	const char *pInput = g_aInputStr;
	if(InputMode() > NUM_INPUTS) // reverse i search
		pInput = m_aInputSearch;
	int Count = 0;
	if(m_UpdateCompletionBuffer)
	{
		m_UpdateCompletionBuffer = false;
		for(int i = m_InputCursor; i > 0; i--)
		{
			if(pInput[i] == ' ' && IsSpace)
				continue;
			Count++;
			if(i == 1) // reach beginning of line no spaces
			{
				str_copy(m_aCompletionBuffer, pInput, sizeof(m_aCompletionBuffer));
				break;
			}
			if(pInput[i] != ' ')
			{
				IsSpace = false;
				continue;
			}
			// tbh idk what this is. Helps with offset on multiple words
			// probably counting the space in front of the word or something
			if(Count > 1)
				Count -= 2;
			str_copy(m_aCompletionBuffer, pInput + i + 1, sizeof(m_aCompletionBuffer));
			break;
		}
		m_aCompletionBuffer[Count] = '\0';
	}
	const char *PlayerName, *FoundInput;
	int Matches = 0;
	const char *pMatch = NULL;
	bool Found = false;
	for(auto &PlayerInfo : m_pClient->m_Snap.m_apInfoByName)
	{
		if(!PlayerInfo)
			continue;

		PlayerName = m_pClient->m_aClients[PlayerInfo->m_ClientId].m_aName;
		FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
		if(!FoundInput)
			continue;
		if(!pMatch)
			pMatch = PlayerName;
		if(Matches++ < m_CompletionIndex)
			if(!IsReverseEnd)
				continue;

		pMatch = PlayerName;
		Found = true;
		if(!IsReverseEnd)
			break;
	}
	if(!pMatch)
	{
		m_CompletionIndex = 0;
		return;
	}
	char aBuf[1024];
	str_copy(aBuf, g_aInputStr, sizeof(aBuf));
	int BufLen = str_length(aBuf);
	if(BufLen >= m_LastCompletionLength + Count)
		Count += m_LastCompletionLength;
	aBuf[BufLen - Count] = '\0';
	str_format(g_aInputStr, sizeof(g_aInputStr), "%s%s", aBuf, pMatch);
	wclear(g_InputWin.m_pCursesWin);
	InputDraw();
	g_InputWin.DrawBorders();
	int MatchLen = str_length(pMatch);
	m_LastCompletionLength = MatchLen;
	m_InputCursor += MatchLen - Count;
	UpdateCursor();
	if(!Found)
		m_CompletionIndex = 0;
	if(IsReverseEnd)
		m_CompletionIndex = Matches - 1;
}

void CTerminalUI::ClearInput()
{
	g_aInputStr[0] = '\0';
	m_InputCursor = 0;
}

void CTerminalUI::ResetCompletion()
{
	m_UpdateCompletionBuffer = true;
	m_aCompletionBuffer[0] = '\0';
	m_LastCompletionLength = 0;
	m_CompletionIndex = -1;
	m_CompletionEnumerationCount = -1;
}

void CTerminalUI::UpdateCursor()
{
	int Offset = g_InputWin.m_CurserOffset;
	if(IsSearchInputMode())
		Offset = str_length("(reverse-i-search)`");
	wmove(g_InputWin.m_pCursesWin, 1, m_InputCursor + 1 + Offset);
}

void CTerminalUI::_UpdateInputSearch()
{
	m_aInputSearchMatch[0] = '\0';
	if(!IsSearchInputMode())
		return;
	if(!m_aInputSearch[0])
		return;
	int Type = InputMode() - NUM_INPUTS + 1;
	for(auto &HistEntry : m_aaInputHistory[Type])
	{
		if(!HistEntry[0])
			continue;
		if(!str_find(HistEntry, m_aInputSearch))
			continue;

		str_copy(m_aInputSearchMatch, HistEntry, sizeof(m_aInputSearchMatch));
		break;
	}
}

void CTerminalUI::RenderInputSearch()
{
	_UpdateInputSearch();
	str_format(g_aInputStr, sizeof(g_aInputStr), "(reverse-i-search)`%s': %s", m_aInputSearch, m_aInputSearchMatch);
	wclear(g_InputWin.m_pCursesWin);
	InputDraw();
	g_InputWin.DrawBorders();
}

int CTerminalUI::OnKeyPress(int Key, WINDOW *pWin)
{
	bool KeyPressed = true;
	// m_pClient->m_Controls.SetCursesDir(-2);
	// m_pClient->m_Controls.SetCursesJump(-2);
	// m_pClient->m_Controls.SetCursesHook(-2);
	// m_pClient->m_Controls.SetCursesTargetX(AimX);
	// m_pClient->m_Controls.SetCursesTargetY(AimY);
	TrackKey(Key);
	if(Key == 9) // tab
	{
		m_ScoreboardActive = !m_ScoreboardActive;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == 'k')
		m_pClient->SendKill(g_Config.m_ClDummy);
	else if(KeyInHistory(' ', 5) || Key == ' ')
	{
		m_aInputData[g_Config.m_ClDummy] = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];
		m_aInputData[g_Config.m_ClDummy].m_Jump = 1;
		m_SendData[g_Config.m_ClDummy] = true;
	}
	else if(KeyInHistory('a', 5) || Key == 'a')
	{
		m_aInputData[g_Config.m_ClDummy] = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];
		m_aInputData[g_Config.m_ClDummy].m_Direction = -1;
		m_SendData[g_Config.m_ClDummy] = true;
	}
	else if(KeyInHistory('d', 5) || Key == 'd')
	{
		m_aInputData[g_Config.m_ClDummy] = m_pClient->m_Controls.m_aInputData[g_Config.m_ClDummy];
		m_aInputData[g_Config.m_ClDummy].m_Direction = 1;
		m_SendData[g_Config.m_ClDummy] = true;
	}
	else if(KeyInHistory(' ', 3) || Key == ' ')
		/* m_pClient->m_Controls.SetCursesJump(1); */ return 0;
	else if(Key == '?')
	{
		OpenHelpPopup();
	}
	else if(Key == 'v')
	{
		m_LockKeyUntilRelease = Key;
		m_RenderGame = !m_RenderGame;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	// else if(Key == 'p')
	// {
	// 	DoPopup(POPUP_MESSAGE, "henlo");
	// 	gs_NeedLogDraw = true;
	// 	m_NewInput = true;
	// }
	else if(Key == 10) // return / enter
	{
		m_LockKeyUntilRelease = Key;

		if(m_Popup == POPUP_MESSAGE || m_Popup == POPUP_DISCONNECTED || m_Popup == POPUP_NOT_IMPORTANT)
		{
			// click "[ OK ]" on popups using enter
			if(m_Popup == POPUP_DISCONNECTED && Client()->ReconnectTime() > 0)
				Client()->SetReconnectTime(0);
			m_Popup = POPUP_NONE;
			gs_NeedLogDraw = true;
			m_NewInput = true;
		}
		else if(PickMenuItem())
		{
		}
		else if(m_RenderServerList)
		{
			m_RenderServerList = false;
			const CServerInfo *pItem = ServerBrowser()->SortedGet(m_SelectedServer);
			if(pItem && m_pClient->Client()->State() != IClient::STATE_CONNECTING)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "connect %s", pItem->m_aAddress);
				Console()->ExecuteLine(aBuf);
			}
		}
	}
	else if(Key == '/') // search key
	{
		if(m_RenderServerList)
			SetInputMode(INPUT_BROWSER_SEARCH);
	}
	else if(Key == 'h' && m_LastKeyPress < time_get() - time_freq() / 2)
	{
		Console()->ExecuteLine("reply_to_last_ping");
	}
	else if(Key == 'b')
	{
		m_LockKeyUntilRelease = Key;
		OpenServerList();
	}
	else if((Key == KEY_F(5) || (keyname(Key)[0] == '^' && keyname(Key)[1] == 'R')) && m_RenderServerList)
	{
		ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
	}
	else if(Key == KEY_LEFT)
	{
		g_InputWin.PrevMenuItem();
		AimX = maximum(AimX - 10, -20);
		if(m_RenderServerList)
			SetServerBrowserPage(g_Config.m_UiPage - 1);
	}
	else if(Key == KEY_RIGHT)
	{
		g_InputWin.NextMenuItem();
		AimX = minimum(AimX + 10, 20);
		if(m_RenderServerList)
			SetServerBrowserPage(g_Config.m_UiPage + 1);
	}
	else if(Key == KEY_UP)
	{
		AimY = maximum(AimY - 10, -20);
		if(m_RenderServerList && m_NumServers)
			m_SelectedServer = clamp(m_SelectedServer - 1, 0, m_NumServers - 1);
		else
			scroll_curses_log(1);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == KEY_DOWN)
	{
		AimY = minimum(AimY + 10, 20);
		if(m_RenderServerList && m_NumServers)
			m_SelectedServer = clamp(m_SelectedServer + 1, 0, m_NumServers - 1);
		else
			scroll_curses_log(-1);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == KEY_NPAGE)
	{
		int _x, y;
		getmaxyx(g_LogWindow.m_pCursesWin, y, _x);
		scroll_curses_log(-y);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == KEY_PPAGE)
	{
		int _x, y;
		getmaxyx(g_LogWindow.m_pCursesWin, y, _x);
		scroll_curses_log(y);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == 'G' && m_RenderServerList && m_NumServers)
	{
		m_SelectedServer = m_NumServers - 1;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == 'g' && m_LastKeyPressed == 'g')
	{
		m_SelectedServer = 0;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else
		KeyPressed = false;

	if(KeyPressed)
		m_LastKeyPress = time_get();

	if(Key == EOF)
		return 0;

	m_LastKeyPressed = Key;
	// dbg_msg("termUI", "got key d=%d c=%c", Key, Key);
	return 0;
}

void CTerminalUI::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_OFFLINE)
	{
		m_Popup = POPUP_NONE;
		if(Client()->ErrorString() && Client()->ErrorString()[0] != 0)
		{
			// if(str_find(Client()->ErrorString(), "password"))
			// {
			// 	m_Popup = POPUP_PASSWORD;
			// }
			// else
			DoPopup(POPUP_DISCONNECTED, "Disconnected");
		}
	}
	// else if(NewState == IClient::STATE_LOADING)
	// {
	// 	m_Popup = POPUP_CONNECTING;
	// 	m_DownloadLastCheckTime = time_get();
	// 	m_DownloadLastCheckSize = 0;
	// 	m_DownloadSpeed = 0.0f;
	// }
	// else if(NewState == IClient::STATE_CONNECTING)
	// 	m_Popup = POPUP_CONNECTING;
	else if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_Popup != POPUP_WARNING)
		{
			m_Popup = POPUP_NONE;
		}
	}
}

void CTerminalUI::SetServerBrowserPage(int NewPage)
{
	if(NewPage >= CMenus::PAGE_INTERNET && NewPage <= CMenus::PAGE_FAVORITES)
	{
		m_SelectedServer = 0;
		g_Config.m_UiPage = NewPage;
		if(g_Config.m_UiPage == CMenus::PAGE_INTERNET)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_INTERNET);
		else if(g_Config.m_UiPage == CMenus::PAGE_LAN)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == CMenus::PAGE_FAVORITES)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_FAVORITES);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
}

#endif
