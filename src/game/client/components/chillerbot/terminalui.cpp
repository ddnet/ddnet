// ChillerDragon 2020 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>

#include <base/terminalui.h>

#include <csignal>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

CGameClient *g_pClient;
volatile sig_atomic_t cl_InterruptSignaled = 0;

void HandleSigIntTerm(int Param)
{
	cl_InterruptSignaled = 1;

	// Exit the next time a signal is received
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

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
	mvwprintw(g_pInfoWin, 1, 1, "%s", aBuf);
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
	mvwprintw(g_pInputWin, 1, 1, "%s", aBuf);
	UpdateCursor();
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
	RenderServerList();
	RenderConnecting();
	RenderPopup();
	RenderHelpPage();
	if(m_pClient->m_Snap.m_pLocalCharacter)
		RenderScoreboard(0, g_pLogWindow);

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

void CTerminalUI::RenderHelpPage()
{
	if(!m_RenderHelpPage)
		return;
	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 40;
	int width = minimum(128, mx - 3);
	if(mx < width + 2 + offX)
		offX = 2;
	if(my < 60)
		offY = 2;

	const char aHelpLines[][128] = {
		"** basic",
		" ?     - toggle this help",
		"",
		"** serverbrowser",
		" b     - toggle server browser. Opens search prompt then waits for navigation.",
		" c     - connect to currently selected server",
		" down  - select next server",
		" up    - select previous server",
		" right - switch to next tab (internet/lan/favorites/ddnet/kog)",
		" left  - switch to previous tab (internet/lan/favorites/ddnet/kog)",
		"",
		"** in game",
		" t     - chat",
		" z     - team chat",
		" f1    - local console",
		" f2    - remote console",
		" h     - auto reply to known chat messages",
		" k     - selfkill"};

	DrawBorders(g_pLogWindow, offX, offY - 1, width, sizeof(aHelpLines) / 128 + 2);

	int i = 0;
	for(auto &aLine : aHelpLines)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "|%-*s|", width - 2, aLine);
		if(sizeof(aBuf) > (unsigned long)(mx - 2))
			aBuf[mx - 2] = '\0'; // ensure no line wrapping
		mvwprintw(g_pLogWindow, offY + i++, offX, "%s", aBuf);
	}
}

void CTerminalUI::RenderServerList()
{
	if(!m_RenderServerList)
		return;

	char aTab[128];
	aTab[0] = '\0';
	if(g_Config.m_UiPage == CMenus::PAGE_INTERNET)
		str_copy(aTab, "internet", sizeof(aTab));
	else if(g_Config.m_UiPage == CMenus::PAGE_LAN)
		str_copy(aTab, "lan", sizeof(aTab));
	else if(g_Config.m_UiPage == CMenus::PAGE_FAVORITES)
		str_copy(aTab, "favorites", sizeof(aTab));
	else if(g_Config.m_UiPage == CMenus::PAGE_DDNET)
		str_copy(aTab, "ddnet", sizeof(aTab));
	else if(g_Config.m_UiPage == CMenus::PAGE_KOG)
		str_copy(aTab, "KoG", sizeof(aTab));
	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 40;
	if(my < 60)
		offY = 2;
	int width = minimum(128, mx - 3);
	if(mx < width + 2 + offX)
		offX = 2;
	if(width < 2)
		return;
	m_NumServers = ServerBrowser()->NumSortedServers();
	int height = minimum(m_NumServers, my - (offY + 2));
	DrawBorders(g_pLogWindow, offX, offY - 1, width, height + 2);
	mvwprintw(g_pLogWindow, offY - 1, offX + 3, "[ %s ]", aTab);
	int From = 0;
	int To = height;
	if(To > 1 && m_SelectedServer >= To - 1)
	{
		From = m_SelectedServer - (To - 1);
		To = m_SelectedServer + 1;
	}
	for(int i = From, k = 0; i < To && k < height; i++, k++)
	{
		const CServerInfo *pItem = ServerBrowser()->SortedGet(i);
		if(!pItem)
			continue;

		// dbg_msg("serverbrowser", "server %s", pItem->m_aName);
		char aLine[1024];
		char aBuf[1024];
		char aName[64];
		char aMap[64];
		char aPlayers[16];
		if(m_SelectedServer == i)
		{
			str_format(aName, sizeof(aName), " %s", pItem->m_aName);
			str_format(aMap, sizeof(aMap), " %s", pItem->m_aMap);
			str_format(aPlayers, sizeof(aPlayers), " %d/%d", pItem->m_NumPlayers, pItem->m_MaxPlayers);
		}
		else
		{
			str_copy(aName, pItem->m_aName, sizeof(aName));
			str_copy(aMap, pItem->m_aMap, sizeof(aMap));
			str_format(aPlayers, sizeof(aPlayers), "%d/%d", pItem->m_NumPlayers, pItem->m_MaxPlayers);
		}
		aName[60] = '\0';
		int NameSize;
		int NameCount;
		str_utf8_stats(aName, 60, 60, &NameSize, &NameCount);
		str_format(aBuf, sizeof(aBuf),
			"%-*s | %-20s | %-16s",
			60 + ((NameSize - NameCount) / 2),
			aName,
			aMap,
			aPlayers);
		aBuf[width - 1] = '\0'; // ensure no line wrapping
		if(m_SelectedServer == i)
		{
			wattron(g_pLogWindow, A_BOLD);
			str_format(aLine, sizeof(aLine), "<%-*s>", (width - 2) + ((NameSize - NameCount) / 2), aBuf);
		}
		else
		{
			wattroff(g_pLogWindow, A_BOLD);
			str_format(aLine, sizeof(aLine), "|%-*s|", (width - 2) + ((NameSize - NameCount) / 2), aBuf);
		}
		mvwprintw(g_pLogWindow, offY + k, offX, "%s", aLine);
	}
}

void CTerminalUI::RenderScoreboard(int Team, WINDOW *pWin)
{
	if(!m_ScoreboardActive)
		return;

	int NumRenderScoreIDs = 0;

	int mx = getmaxx(pWin);
	int my = getmaxy(pWin);
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
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		NumRenderScoreIDs++;
		if(offY + i > my - 8)
			break;
	}
	int height = minimum(NumRenderScoreIDs, my - 5);
	if(height < 2)
		return;

	DrawBorders(pWin, offX, offY - 1, width, height + 2);
	// DrawBorders(pWin, 10, 5, 10, 5);

	int k = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// if(rendered++ < 0) continue;

		// make sure that we render the correct team
		const CNetObj_PlayerInfo *pInfo = m_pClient->m_Snap.m_paInfoByDDTeamScore[i];
		if(!pInfo || pInfo->m_Team != Team)
			continue;

		// dbg_msg("scoreboard", "i=%d name=%s", i, m_pClient->m_aClients[pInfo->m_ClientID].m_aName);

		char aLine[1024];
		char aBuf[1024];
		int NameSize;
		int NameCount;
		int ClanSize;
		int ClanCount;
		str_utf8_stats(m_pClient->m_aClients[pInfo->m_ClientID].m_aName, 60, 60, &NameSize, &NameCount);
		str_utf8_stats(m_pClient->m_aClients[pInfo->m_ClientID].m_aClan, 60, 60, &ClanSize, &ClanCount);
		str_format(aBuf, sizeof(aBuf),
			"%4d| %-*s | %-*s |",
			clamp(pInfo->m_Score, -999, 9999),
			20 + (NameSize - NameCount),
			m_pClient->m_aClients[pInfo->m_ClientID].m_aName,
			20 + (ClanSize - ClanCount),
			m_pClient->m_aClients[pInfo->m_ClientID].m_aClan);
		str_format(aLine, sizeof(aLine), "|%-*s|", (width - 2) + (NameSize - NameCount) + (ClanSize - ClanCount), aBuf);
		if(sizeof(aBuf) > (unsigned long)(mx - 2))
			aLine[mx - 2] = '\0'; // ensure no line wrapping
		mvwprintw(pWin, offY + k, offX, "%s", aLine);

		if(k++ >= height - 1)
			break;
	}
}

void CTerminalUI::OnInit()
{
	m_aPopupTitle[0] = '\0';
	m_aCompletionBuffer[0] = '\0';
	ResetCompletion();
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

	// set up initial windows
	getmaxyx(stdscr, g_ParentY, g_ParentX);

	g_pLogWindow = newwin(g_ParentY - NC_INFO_SIZE * 2, g_ParentX, 0, 0);
	g_pInfoWin = newwin(NC_INFO_SIZE, g_ParentX, g_ParentY - NC_INFO_SIZE * 2, 0);
	g_pInputWin = newwin(NC_INFO_SIZE, g_ParentX, g_ParentY - NC_INFO_SIZE, 0);

	DrawBorders(g_pLogWindow);
	DrawBorders(g_pInfoWin);
	DrawBorders(g_pInputWin);

	signal(SIGINT, HandleSigIntTerm);
	signal(SIGTERM, HandleSigIntTerm);

	gs_Logfile = 0;
	// if(g_Config.m_Logfile[0])
	// {
	// 	gs_Logfile = io_open(g_Config.m_Logfile, IOFLAG_WRITE);
	// }
}

void CTerminalUI::OnShutdown()
{
	endwin();
}

void CTerminalUI::OnRender()
{
	CursesTick();

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

bool CTerminalUI::DoPopup(int Popup, const char *pTitle)
{
	if(m_Popup != POPUP_NONE)
		return false;

	m_Popup = Popup;
	str_copy(m_aPopupTitle, pTitle, sizeof(m_aPopupTitle));
	// TODO: m_NewInput, gs_NeedLogDraw do not really work here
	m_NewInput = false;
	gs_NeedLogDraw = true;
	return true;
}

void CTerminalUI::RenderPopup()
{
	if(m_Popup == POPUP_NONE)
		return;

	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	int height = minimum(3, my - 2);
	if(height < 2)
		return;
	DrawBorders(g_pLogWindow, offX, offY - 1, width, height + 2);

	char aExtraText[1024];
	aExtraText[0] = '\0';
	if(m_Popup == POPUP_DISCONNECTED)
	{
		if(Client()->m_ReconnectTime > 0)
		{
			int ReconnectSecs = (int)((Client()->m_ReconnectTime - time_get()) / time_freq());
			str_format(aExtraText, sizeof(aExtraText), Localize("Reconnect in %d sec"), ReconnectSecs);
			str_copy(m_aPopupTitle, Client()->ErrorString(), sizeof(m_aPopupTitle));
			// pButtonText = Localize("Abort");
			static int LastReconnectSecs = ReconnectSecs;
			if(LastReconnectSecs != ReconnectSecs)
			{
				LastReconnectSecs = ReconnectSecs;
				m_NewInput = false;
				gs_NeedLogDraw = true;
			}
		}
	}

	if(sizeof(m_aPopupTitle) > (unsigned long)(mx - 2))
		m_aPopupTitle[mx - 2] = '\0'; // ensure no line wrapping

	int TitleLen = str_length(m_aPopupTitle);
	int ExtraTextLen = str_length(aExtraText);
	int TitleMid = TitleLen / 2;
	int ExtraTextMid = ExtraTextLen / 2;
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "%*s", (width - TitleLen) < 1 ? 0 : ((width - TitleMid) / 2), m_aPopupTitle);
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);
	str_format(aBuf, sizeof(aBuf), "%*s", (width - ExtraTextLen) < 1 ? 0 : ((width - ExtraTextMid) / 2), aExtraText);
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);
	str_format(aBuf, sizeof(aBuf), "%*s", (width - 6) < 1 ? 0 : ((width - 6) / 2), "[ OK ]");
	mvwprintw(g_pLogWindow, offY++, offX, "|%-*s|", width - 2, aBuf);
}

void CTerminalUI::RenderConnecting()
{
	if(RenderDownload())
		return;
	if(Client()->State() != IClient::STATE_CONNECTING)
		return;

	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	DrawBorders(g_pLogWindow, offX, offY - 1, width, 3);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Connecting to %s", g_Config.m_UiServerAddress);
	if(sizeof(aBuf) > (unsigned long)(mx - 2))
		aBuf[mx - 2] = '\0'; // ensure no line wrapping
	mvwprintw(g_pLogWindow, offY, offX, "|%-*s|", width - 2, aBuf);
}

bool CTerminalUI::RenderDownload()
{
	if(Client()->State() != IClient::STATE_LOADING)
		return false;
	if(Client()->MapDownloadAmount() < 1)
		return false;
	if(Client()->MapDownloadTotalsize() < 1)
		return false;

	int mx = getmaxx(g_pLogWindow);
	int my = getmaxy(g_pLogWindow);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);

	int Download = (Client()->MapDownloadAmount() * width) / Client()->MapDownloadTotalsize();
	// str_format(g_aInfoStr, sizeof(g_aInfoStr),
	// 	"download=%d%% (%d/%d KiB)",
	// 	Download,
	// 	Client()->MapDownloadAmount() / 1024, Client()->MapDownloadTotalsize() / 1024);

	DrawBorders(g_pLogWindow, offX, offY - 1, width, 3);
	DrawBorders(g_pLogWindow, offX, offY + 1, width, 3);

	char aProgress[512];
	for(auto &Progress : aProgress)
		Progress = '#';
	if(Download > 0 && Download < width)
		aProgress[Download] = '\0';
	aProgress[width - 2] = '\0';
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "|%-*s|", width - 2, aProgress);
	if(sizeof(aBuf) > (unsigned long)(mx - 2))
		aBuf[mx - 2] = '\0'; // ensure no line wrapping
	mvwprintw(g_pLogWindow, offY, offX, "%s", aBuf);
	char aMapName[128];
	str_format(aMapName, sizeof(aMapName), "Downloading map %s ", Client()->MapDownloadName());
	if(str_length(aMapName) + 24 < width)
	{
		str_format(
			aProgress,
			sizeof(aProgress),
			"%d%% (%d/%d KiB)",
			Download,
			Client()->MapDownloadAmount() / 1024, Client()->MapDownloadTotalsize() / 1024);
		str_append(aMapName, aProgress, sizeof(aMapName));
	}
	aMapName[width - 2] = '\0';
	str_format(aBuf, sizeof(aBuf), "|%-*s|", width - 2, aMapName);
	if(sizeof(aBuf) > (unsigned long)(mx - 2))
		aBuf[mx - 2] = '\0'; // ensure no line wrapping
	mvwprintw(g_pLogWindow, offY + 2, offX, "%s", aBuf);
	static int LastDownload = Download;
	if(LastDownload != Download)
	{
		LastDownload = Download;
		m_NewInput = false;
		gs_NeedLogDraw = true;
	}
	return true;
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
		m_InputMode == INPUT_CHAT_TEAM ||
		m_InputMode == INPUT_BROWSER_SEARCH ||

		m_InputMode == INPUT_SEARCH_LOCAL_CONSOLE ||
		m_InputMode == INPUT_SEARCH_REMOTE_CONSOLE ||
		m_InputMode == INPUT_SEARCH_CHAT ||
		m_InputMode == INPUT_SEARCH_CHAT_TEAM ||
		m_InputMode == INPUT_SEARCH_BROWSER_SEARCH)
	{
		if(c == ERR) // nonblocking empty read
			return 0;
		if(c == EOF || c == 10) // return
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
			else if(m_InputMode == INPUT_BROWSER_SEARCH)
			{
				m_SelectedServer = 0;
				str_copy(g_Config.m_BrFilterString, g_aInputStr, sizeof(g_Config.m_BrFilterString));
				ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
			}

			if(IsSearchInputMode())
			{
				m_InputMode -= NUM_INPUTS - 1;
				str_copy(g_aInputStr, m_aInputSearchMatch, sizeof(g_aInputStr));
			}
			else
			{
				AddInputHistory(m_InputMode, g_aInputStr);
				m_InputHistory[m_InputMode] = 0;
				g_aInputStr[0] = '\0';
				if(m_InputMode != INPUT_LOCAL_CONSOLE && m_InputMode != INPUT_REMOTE_CONSOLE)
					m_InputMode = INPUT_NORMAL;
			}
			m_InputCursor = 0;
			wclear(g_pInputWin);
			InputDraw();
			DrawBorders(g_pInputWin);
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
		else if(keyname(c)[0] == '^' && keyname(c)[1] == 'I') // tab
		{
			m_CompletionIndex++;
			bool IsSpace = true;
			const char *pInput = g_aInputStr;
			if(m_InputMode > NUM_INPUTS) // reverse i search
				pInput = m_aInputSearch;
			int Count = 0;
			if(m_CompletionIndex == 0)
			{
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
			for(auto &PlayerInfo : m_pClient->m_Snap.m_paInfoByName)
			{
				if(!PlayerInfo)
					continue;

				PlayerName = m_pClient->m_aClients[PlayerInfo->m_ClientID].m_aName;
				FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
				if(!FoundInput)
					continue;
				if(!pMatch)
					pMatch = PlayerName;
				if(Matches++ < m_CompletionIndex)
					continue;

				pMatch = PlayerName;
				Found = true;
				break;
			}
			if(!pMatch)
			{
				m_CompletionIndex = 0;
				return 0;
			}
			char aBuf[1024];
			str_copy(aBuf, g_aInputStr, sizeof(aBuf));
			int BufLen = str_length(aBuf);
			if(BufLen >= m_LastCompletionLength + Count)
				Count += m_LastCompletionLength;
			aBuf[BufLen - Count] = '\0';
			str_format(g_aInputStr, sizeof(g_aInputStr), "%s%s", aBuf, pMatch);
			wclear(g_pInputWin);
			InputDraw();
			DrawBorders(g_pInputWin);
			int MatchLen = str_length(pMatch);
			m_LastCompletionLength = MatchLen;
			m_InputCursor += MatchLen - Count;
			UpdateCursor();
			if(!Found)
				m_CompletionIndex = 0;
			return 0;
		}
		else if(keyname(c)[0] == '^' && keyname(c)[1] == '[')
		{
			c = getch();
			if(c == EOF) // ESC
			{
				m_InputMode = INPUT_NORMAL;
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
								if(m_InputMode > NUM_INPUTS) // reverse i search
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
								if(m_InputMode > NUM_INPUTS) // reverse i search
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
			if(str_length(g_aInputStr) < 1)
				return 0;
			char aRight[1024];
			char aLeft[1024];

			if(m_InputMode > NUM_INPUTS) // reverse i search
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
			wclear(g_pInputWin);
			InputDraw();
			DrawBorders(g_pInputWin);
			UpdateCursor();
			return 0;
		}
		else if(c == 260) // left arrow
		{
			// could be used for cursor movement
			m_InputCursor = clamp(m_InputCursor - 1, 0, str_length(g_aInputStr));
			UpdateCursor();
			return 0;
		}
		else if(c == 261) // right arrow
		{
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
				if(m_InputMode > NUM_INPUTS) // reverse i search
				{
					str_copy(aRight, m_aInputSearch + m_InputCursor, sizeof(aRight));
					str_copy(m_aInputSearch, aRight, sizeof(m_aInputSearch));
					RenderInputSearch();
				}
				else
				{
					str_copy(aRight, g_aInputStr + m_InputCursor, sizeof(aRight));
					str_copy(g_aInputStr, aRight, sizeof(g_aInputStr));
					wclear(g_pInputWin);
					InputDraw();
					DrawBorders(g_pInputWin);
				}
				m_InputCursor = 0;
				UpdateCursor();
			}
			if(keyname(c)[1] == 'K') // ctrl+k
			{
				ResetCompletion();
				if(m_InputMode > NUM_INPUTS) // reverse i search
				{
					m_aInputSearch[m_InputCursor] = '\0';
					RenderInputSearch();
				}
				else
				{
					g_aInputStr[m_InputCursor] = '\0';
					wclear(g_pInputWin);
					InputDraw();
					DrawBorders(g_pInputWin);
				}
				UpdateCursor();
			}
			if(keyname(c)[1] == 'R' && m_InputMode > INPUT_NORMAL && !IsSearchInputMode()) // ctrl+r
			{
				m_InputMode += NUM_INPUTS - 1;
				m_aInputSearch[0] = '\0';
				str_copy(g_aInputStr, "(reverse-i-search)`': ", sizeof(g_aInputStr));
				wclear(g_pInputWin);
				InputDraw();
				DrawBorders(g_pInputWin);
				UpdateCursor();
			}
			else if(keyname(c)[1] == 'E') // ctrl+e
			{
				ResetCompletion();
				m_InputCursor = str_length(g_aInputStr);
				if(m_InputMode > NUM_INPUTS) // reverse i search
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
		if((c == 258 || c == 259) && m_InputMode >= 0) // up/down arrow scroll history
		{
			ResetCompletion();
			str_copy(g_aInputStr, m_aaInputHistory[m_InputMode][m_InputHistory[m_InputMode]], sizeof(g_aInputStr));
			wclear(g_pInputWin);
			InputDraw();
			DrawBorders(g_pInputWin);
			// update history index after using it because index 0 is already the last item
			int OldHistory = m_InputHistory[m_InputMode];
			if(c == 258) // down arrow
				m_InputHistory[m_InputMode] = clamp(m_InputHistory[m_InputMode] - 1, 0, (int)(INPUT_HISTORY_MAX_LEN));
			else if(c == 259) // up arrow
				m_InputHistory[m_InputMode] = clamp(m_InputHistory[m_InputMode] + 1, 0, (int)(INPUT_HISTORY_MAX_LEN));

			// stop scrolling and roll back when reached an empty history element
			if(m_aaInputHistory[m_InputMode][m_InputHistory[m_InputMode]][0] == '\0')
				m_InputHistory[m_InputMode] = OldHistory;
		}
		else
		{
			ResetCompletion();
			char aKey[8];
			str_format(aKey, sizeof(aKey), "%c", c);
			// str_append(g_aInputStr, aKey, sizeof(g_aInputStr));
			char aRight[1024];
			char aLeft[1024];
			if(m_InputMode > NUM_INPUTS) // reverse i search
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
		// dbg_msg("yeee", "got key d=%d c=%c", c, c);
	}
	InputDraw();
	return 0;
}

void CTerminalUI::ResetCompletion()
{
	m_LastCompletionLength = 0;
	m_CompletionIndex = -1;
}

void CTerminalUI::UpdateCursor()
{
	int Offset = 0;
	if(IsSearchInputMode())
		Offset = str_length("(reverse-i-search)`");
	wmove(g_pInputWin, 1, m_InputCursor + 1 + Offset);
}

void CTerminalUI::_UpdateInputSearch()
{
	m_aInputSearchMatch[0] = '\0';
	if(!IsSearchInputMode())
		return;
	if(!m_aInputSearch[0])
		return;
	int Type = m_InputMode - NUM_INPUTS + 1;
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
	wclear(g_pInputWin);
	InputDraw();
	DrawBorders(g_pInputWin);
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
	else if(KeyInHistory('a', 5) || Key == 'a')
		/* m_pClient->m_Controls.SetCursesDir(-1); */ return 0;
	else if(KeyInHistory('d', 5) || Key == 'd')
		/* m_pClient->m_Controls.SetCursesDir(1); */ return 0;
	else if(KeyInHistory(' ', 3) || Key == ' ')
		/* m_pClient->m_Controls.SetCursesJump(1); */ return 0;
	else if(Key == '?')
	{
		m_RenderHelpPage = !m_RenderHelpPage;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	// else if(Key == 'p')
	// {
	// 	DoPopup(POPUP_MESSAGE, "henlo");
	// 	gs_NeedLogDraw = true;
	// 	m_NewInput = true;
	// }
	else if(Key == 10 && (m_Popup == POPUP_MESSAGE || m_Popup == POPUP_DISCONNECTED)) // return
	{
		// click "[ OK ]" on popups using enter
		m_Popup = POPUP_NONE;
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if(Key == 'h' && m_LastKeyPress < time_get() - time_freq() / 2)
	{
		Console()->ExecuteLine("reply_to_last_ping");
	}
	else if(Key == 'b' && m_LastKeyPress < time_get() - time_freq() / 2)
	{
		if((m_RenderServerList = !m_RenderServerList))
		{
			m_InputMode = INPUT_BROWSER_SEARCH;
			str_copy(g_aInputStr, g_Config.m_BrFilterString, sizeof(g_aInputStr));
		}
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
	else if((Key == KEY_F(5) || (keyname(Key)[0] == '^' && keyname(Key)[1] == 'R')) && m_RenderServerList)
	{
		ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
	}
	else if(Key == KEY_LEFT)
	{
		AimX = maximum(AimX - 10, -20);
		if(m_RenderServerList)
			SetServerBrowserPage(g_Config.m_UiPage - 1);
	}
	else if(Key == KEY_RIGHT)
	{
		AimX = minimum(AimX + 10, 20);
		if(m_RenderServerList)
			SetServerBrowserPage(g_Config.m_UiPage + 1);
	}
	else if(Key == KEY_UP)
	{
		AimY = maximum(AimY - 10, -20);
		if(m_RenderServerList && m_NumServers)
		{
			m_SelectedServer = clamp(--m_SelectedServer, 0, m_NumServers - 1);
			gs_NeedLogDraw = true;
			m_NewInput = true;
		}
	}
	else if(Key == KEY_DOWN)
	{
		AimY = minimum(AimY + 10, 20);
		if(m_RenderServerList && m_NumServers)
		{
			m_SelectedServer = clamp(++m_SelectedServer, 0, m_NumServers - 1);
			gs_NeedLogDraw = true;
			m_NewInput = true;
		}
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
	else if(Key == 'c' && m_RenderServerList)
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
	if(NewPage >= CMenus::PAGE_INTERNET && NewPage <= CMenus::PAGE_KOG)
	{
		m_SelectedServer = 0;
		g_Config.m_UiPage = NewPage;
		if(g_Config.m_UiPage == CMenus::PAGE_INTERNET)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_INTERNET);
		else if(g_Config.m_UiPage == CMenus::PAGE_LAN)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == CMenus::PAGE_FAVORITES)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_FAVORITES);
		else if(g_Config.m_UiPage == CMenus::PAGE_DDNET)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_DDNET);
		else if(g_Config.m_UiPage == CMenus::PAGE_KOG)
			ServerBrowser()->Refresh(CServerBrowser::TYPE_KOG);
		gs_NeedLogDraw = true;
		m_NewInput = true;
	}
}

#endif
