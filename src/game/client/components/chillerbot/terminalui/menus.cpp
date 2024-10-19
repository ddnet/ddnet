// ChillerDragon 2022 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <base/terminalui.h>

#include <csignal>

#include "pad_utf8.h"
#include <rust-bridge-chillerbot/unicode.h>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

bool CTerminalUI::PickMenuItem()
{
	if(!g_InputWin.IsMenu())
		return false;

	CloseMenu();

	switch(g_InputWin.Item())
	{
	case CInputWindow::MENU_ITEM_BROWSER:
		OpenServerList();
		break;
	case CInputWindow::MENU_ITEM_HELP:
		OpenHelpPopup();
		break;
	case CInputWindow::MENU_ITEM_QUIT:
		m_pClient->Client()->Quit();
		break;
	default:
		break;
	}
	return true;
}

void CTerminalUI::CloseMenu()
{
	g_InputWin.CloseMenu();
	g_InputWin.Close();
	g_TriggerResize = true;
}

void CTerminalUI::OpenHelpPopup()
{
	if(m_Popup == POPUP_NOT_IMPORTANT)
		m_Popup = POPUP_NONE;
	m_RenderHelpPage = !m_RenderHelpPage;
	gs_NeedLogDraw = true;
	m_NewInput = true;
}

void CTerminalUI::OpenServerList()
{
	CloseMenu();
	if(m_Popup == POPUP_NOT_IMPORTANT)
		m_Popup = POPUP_NONE;
	m_RenderHelpPage = false;
	m_RenderServerList = !m_RenderServerList;
	gs_NeedLogDraw = true;
	m_NewInput = true;
}

bool CTerminalUI::DoPopup(int Popup, const char *pTitle, const char *pBody, size_t BodyWidth, size_t BodyHeight)
{
	if(m_Popup != POPUP_NONE)
		return false;

	m_Popup = Popup;
	str_copy(m_aPopupTitle, pTitle, sizeof(m_aPopupTitle));

	m_aaPopupBody[0][0] = '\0';
	if(pBody)
	{
		m_PopupBodyWidth = BodyWidth;
		m_PopupBodyHeight = BodyHeight;
		dbg_assert(BodyWidth > 0, "popup body width too low");
		dbg_assert(BodyHeight > 0, "popup body height too low");
		dbg_assert(BodyWidth < MAX_POPUP_BODY_WIDTH, "popup body too wide");
		dbg_assert(BodyHeight <= MAX_POPUP_BODY_HEIGHT, "popup body too high");
		for(size_t y = 0; y < BodyHeight; y++)
			str_copy(m_aaPopupBody[y], &pBody[y * BodyWidth], sizeof(m_aaPopupBody[y]));
	}

	m_NewInput = false;
	gs_NeedLogDraw = true;
	return true;
}

void CTerminalUI::RenderHelpPage()
{
	if(!m_RenderHelpPage)
		return;
	int mx = getmaxx(g_LogWindow.m_pCursesWin);
	int my = getmaxy(g_LogWindow.m_pCursesWin);
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
		" b     - toggle server browser",
		" enter - connect to currently selected server",
		" /     - to search",
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
		" v     - toggle visual mode (render game world)",
		" tab   - show scoreboard",
		" a     - walk left",
		" d     - walk right",
		" space - to jump",
		" k     - selfkill"};

	g_LogWindow.DrawBorders(offX, offY - 1, width, sizeof(aHelpLines) / 128 + 2);

	int i = 0;
	for(auto &aLine : aHelpLines)
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "|%-*s|", width - 2, aLine);
		if(sizeof(aBuf) > (unsigned long)(mx - 2))
			aBuf[mx - 2] = '\0'; // ensure no line wrapping
		mvwprintw(g_LogWindow.m_pCursesWin, offY + i++, offX, "%s", aBuf);
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
	int mx = getmaxx(g_LogWindow.m_pCursesWin);
	int my = getmaxy(g_LogWindow.m_pCursesWin);
	int offY = 5;
	int offX = 40;
	if(my < HEIGHT_NEEDED_FOR_SERVER_BROWSER_OFFSET_TOP)
		offY = 2;
	int width = minimum(128, mx - 3);
	if(mx < width + 2 + offX)
		offX = 2;
	if(width < 2)
		return;
	m_NumServers = ServerBrowser()->NumSortedServers();
	int height = minimum(m_NumServers, my - (offY + 2));
	g_LogWindow.DrawBorders(offX, offY - 1, width, height + 2);
	mvwprintw(g_LogWindow.m_pCursesWin, offY - 1, offX + 3, "[ %s ]", aTab);
	int From = 0;
	int To = height;
	m_WinServerBrowser.m_X = offX;
	m_WinServerBrowser.m_Y = offY;
	m_WinServerBrowser.m_Width = width;
	m_WinServerBrowser.m_Height = height;
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

		static const int NAME_COL_SIZE = 65;
		static const int MAP_COL_SIZE = 24;
		static const int PLAYER_COL_SIZE = 16;

		// TODO: add country column using these emojis https://emojipedia.org/flags/

		char aLine[1024];
		char aBuf[1024];
		char aName[512];
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

		// the name buffer in serverbrowser is 64
		// so this should never overflow
		str_pad_right_utf8(aName, sizeof(aName), NAME_COL_SIZE);

		// MAX_MAP_LENGTH is 128
		// so this could overflow
		int MapLen = str_width_unicode(aMap);
		if(MapLen >= MAP_COL_SIZE)
		{
			aMap[MAP_COL_SIZE - 2] = '\0';
			str_format(aBuf, sizeof(aBuf), "%s..", aMap);
			str_copy(aMap, aBuf, sizeof(aMap));
		}
		str_pad_right_utf8(aMap, sizeof(aMap), MAP_COL_SIZE);

		// 999/999 players is 7 characters
		// so this should be safe
		str_pad_right_utf8(aPlayers, sizeof(aPlayers), PLAYER_COL_SIZE);

		str_format(aBuf, sizeof(aBuf),
			" %s | %s | %s",
			aName,
			aMap,
			aPlayers);
		aBuf[width - 1] = '\0'; // ensure no line wrapping
		if(m_SelectedServer == i)
		{
			wattron(g_LogWindow.m_pCursesWin, A_BOLD);
			str_pad_right_utf8(aBuf, sizeof(aBuf), width - 2);
			str_format(aLine, sizeof(aLine), "<%s>", aBuf);
		}
		else
		{
			str_pad_right_utf8(aBuf, sizeof(aBuf), width - 2);
			str_format(aLine, sizeof(aLine), "|%s|", aBuf);
		}
		mvwprintw(g_LogWindow.m_pCursesWin, offY + k, offX, "%s", aLine);
		wattroff(g_LogWindow.m_pCursesWin, A_BOLD);
	}
}

void CTerminalUI::RenderPopup()
{
	if(m_Popup == POPUP_NONE)
		return;

	int mx = getmaxx(g_LogWindow.m_pCursesWin);
	int my = getmaxy(g_LogWindow.m_pCursesWin);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	int height = minimum(3 + (int)m_PopupBodyHeight, my - 2);
	if(height < 2)
		return;
	g_LogWindow.DrawBorders(offX, offY - 1, width, height + 2);

	char aExtraText[1024];
	aExtraText[0] = '\0';
	if(m_Popup == POPUP_DISCONNECTED)
	{
		if(Client()->ReconnectTime() > 0)
		{
			int ReconnectSecs = (int)((Client()->ReconnectTime() - time_get()) / time_freq());
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
	mvwprintw(g_LogWindow.m_pCursesWin, offY++, offX, "|%-*s|", width - 2, aBuf);
	str_format(aBuf, sizeof(aBuf), "%*s", (width - ExtraTextLen) < 1 ? 0 : ((width - ExtraTextMid) / 2), aExtraText);
	mvwprintw(g_LogWindow.m_pCursesWin, offY++, offX, "|%-*s|", width - 2, aBuf);
	if(m_aaPopupBody[0][0] != '\0')
		for(size_t y = 0; y < m_PopupBodyHeight; y++)
			mvwprintw(g_LogWindow.m_pCursesWin, offY++, offX, "| %-*s|", width - 3, m_aaPopupBody[y]);
	if(m_Popup == POPUP_DISCONNECTED)
		str_format(aBuf, sizeof(aBuf), "%*s", (width - 9) < 1 ? 0 : ((width - 9) / 2), "[ ABORT ]");
	else
		str_format(aBuf, sizeof(aBuf), "%*s", (width - 6) < 1 ? 0 : ((width - 6) / 2), "[ OK ]");
	mvwprintw(g_LogWindow.m_pCursesWin, offY++, offX, "|%-*s|", width - 2, aBuf);
}

void CTerminalUI::RenderConnecting()
{
	if(RenderDownload())
		return;
	if(Client()->State() != IClient::STATE_CONNECTING)
		return;

	int mx = getmaxx(g_LogWindow.m_pCursesWin);
	int my = getmaxy(g_LogWindow.m_pCursesWin);
	int offY = 5;
	int offX = 2;
	if(my < 20)
		offY = 2;
	int width = minimum(128, mx - 3);
	g_LogWindow.DrawBorders(offX, offY - 1, width, 3);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Connecting to %s", g_Config.m_UiServerAddress);
	if(sizeof(aBuf) > (unsigned long)(mx - 2))
		aBuf[mx - 2] = '\0'; // ensure no line wrapping
	mvwprintw(g_LogWindow.m_pCursesWin, offY, offX, "|%-*s|", width - 2, aBuf);
}

bool CTerminalUI::RenderDownload()
{
	if(Client()->State() != IClient::STATE_LOADING)
		return false;
	if(Client()->MapDownloadAmount() < 1)
		return false;
	if(Client()->MapDownloadTotalsize() < 1)
		return false;

	int mx = getmaxx(g_LogWindow.m_pCursesWin);
	int my = getmaxy(g_LogWindow.m_pCursesWin);
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

	g_LogWindow.DrawBorders(offX, offY - 1, width, 3);
	g_LogWindow.DrawBorders(offX, offY + 1, width, 3);

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
	mvwprintw(g_LogWindow.m_pCursesWin, offY, offX, "%s", aBuf);
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
	mvwprintw(g_LogWindow.m_pCursesWin, offY + 2, offX, "%s", aBuf);
	static int LastDownload = Download;
	if(LastDownload != Download)
	{
		LastDownload = Download;
		m_NewInput = false;
		gs_NeedLogDraw = true;
	}
	return true;
}

#endif
