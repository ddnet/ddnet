// ChillerDragon 2022 - chillerbot

#include <engine/client/serverbrowser.h>
#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <base/terminalui.h>

#include <csignal>

#include "terminalui.h"

#if defined(CONF_CURSES_CLIENT)

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
		" v     - toggle visual mode (render game world)",
		" a     - walk left",
		" d     - walk right",
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
	if(m_Popup == POPUP_DISCONNECTED)
		str_format(aBuf, sizeof(aBuf), "%*s", (width - 9) < 1 ? 0 : ((width - 9) / 2), "[ ABORT ]");
	else
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

#endif
