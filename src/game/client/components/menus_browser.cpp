/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>

#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/console.h>
#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include "menus.h"

static const int gs_OffsetColFlagLock = 2;
static const int gs_OffsetColFav = gs_OffsetColFlagLock + 3;
static const int gs_OffsetColOff = gs_OffsetColFav + 3;
static const int gs_OffsetColName = gs_OffsetColOff + 3;
static const int gs_OffsetColGameType = gs_OffsetColName + 3;
static const int gs_OffsetColMap = gs_OffsetColGameType + 3;
static const int gs_OffsetColPlayers = gs_OffsetColMap + 3;
static const int gs_OffsetColPing = gs_OffsetColPlayers + 3;
static const int gs_OffsetColVersion = gs_OffsetColPing + 3;

void FormatServerbrowserPing(char *pBuffer, int BufferLength, const CServerInfo *pInfo)
{
	if(!pInfo->m_LatencyIsEstimated)
	{
		str_format(pBuffer, BufferLength, "%d", pInfo->m_Latency);
		return;
	}
	static const char *LOCATION_NAMES[CServerInfo::NUM_LOCS] = {
		"", // LOC_UNKNOWN
		Localizable("AFR"), // LOC_AFRICA
		Localizable("ASI"), // LOC_ASIA
		Localizable("AUS"), // LOC_AUSTRALIA
		Localizable("EUR"), // LOC_EUROPE
		Localizable("NA"), // LOC_NORTH_AMERICA
		Localizable("SA"), // LOC_SOUTH_AMERICA
		Localizable("CHN"), // LOC_CHINA
	};
	dbg_assert(0 <= pInfo->m_Location && pInfo->m_Location < CServerInfo::NUM_LOCS, "location out of range");
	str_copy(pBuffer, Localize(LOCATION_NAMES[pInfo->m_Location]), BufferLength);
}

void CMenus::RenderServerbrowserServerList(CUIRect View)
{
	CUIRect Headers;
	CUIRect Status;

	View.HSplitTop(ms_ListheaderHeight, &Headers, &View);
	View.HSplitBottom(65.0f, &View, &Status);

	// split of the scrollbar
	Headers.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(20.0f, &Headers, 0);

	struct CColumn
	{
		int m_ID;
		int m_Sort;
		CLocConstString m_Caption;
		int m_Direction;
		float m_Width;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		COL_FLAG_LOCK = 0,
		COL_FLAG_FAV,
		COL_FLAG_OFFICIAL,
		COL_NAME,
		COL_GAMETYPE,
		COL_MAP,
		COL_PLAYERS,
		COL_PING,
		COL_VERSION,
	};

	CColumn s_aCols[] = {
		{-1, -1, " ", -1, 2.0f, {0}, {0}},
		{COL_FLAG_LOCK, -1, " ", -1, 14.0f, {0}, {0}},
		{COL_FLAG_FAV, -1, " ", -1, 14.0f, {0}, {0}},
		{COL_FLAG_OFFICIAL, -1, " ", -1, 14.0f, {0}, {0}},
		{COL_NAME, IServerBrowser::SORT_NAME, "Name", 0, 50.0f, {0}, {0}}, // Localize - these strings are localized within CLocConstString
		{COL_GAMETYPE, IServerBrowser::SORT_GAMETYPE, Localizable("Type"), 1, 50.0f, {0}, {0}},
		{COL_MAP, IServerBrowser::SORT_MAP, "Map", 1, 120.0f + (Headers.w - 480) / 8, {0}, {0}},
		{COL_PLAYERS, IServerBrowser::SORT_NUMPLAYERS, "Players", 1, 85.0f, {0}, {0}},
		{-1, -1, " ", 1, 10.0f, {0}, {0}},
		{COL_PING, IServerBrowser::SORT_PING, "Ping", 1, 40.0f, {0}, {0}},
	};

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				Headers.VSplitLeft(2, &s_aCols[i].m_Spacer, &Headers);
			}
		}
	}

	for(int i = NumCols - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
			Headers.VSplitRight(2, &Headers, &s_aCols[i].m_Spacer);
		}
	}

	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == 0)
			s_aCols[i].m_Rect = Headers;
	}

	const bool PlayersOrPing = (g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS || g_Config.m_BrSort == IServerBrowser::SORT_PING);

	// do headers
	for(int i = 0; i < NumCols; i++)
	{
		int Checked = g_Config.m_BrSort == s_aCols[i].m_Sort;
		if(PlayersOrPing && g_Config.m_BrSortOrder == 2 && (s_aCols[i].m_Sort == IServerBrowser::SORT_NUMPLAYERS || s_aCols[i].m_Sort == IServerBrowser::SORT_PING))
			Checked = 2;

		if(DoButton_GridHeader(s_aCols[i].m_Caption, Localize(s_aCols[i].m_Caption), Checked, &s_aCols[i].m_Rect))
		{
			if(s_aCols[i].m_Sort != -1)
			{
				if(g_Config.m_BrSort == s_aCols[i].m_Sort)
				{
					if(PlayersOrPing)
						g_Config.m_BrSortOrder = (g_Config.m_BrSortOrder + 1) % 3;
					else
						g_Config.m_BrSortOrder = (g_Config.m_BrSortOrder + 1) % 2;
				}
				else
					g_Config.m_BrSortOrder = 0;
				g_Config.m_BrSort = s_aCols[i].m_Sort;
			}
		}
	}

	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 0);

	int NumServers = ServerBrowser()->NumSortedServers();

	// display important messages in the middle of the screen so no
	// users misses it
	{
		CUIRect MsgBox = View;

		if(!ServerBrowser()->NumServers() && ServerBrowser()->IsGettingServerlist())
			UI()->DoLabel(&MsgBox, Localize("Getting server list from master server"), 16.0f, TEXTALIGN_CENTER);
		else if(!ServerBrowser()->NumServers())
			UI()->DoLabel(&MsgBox, Localize("No servers found"), 16.0f, TEXTALIGN_CENTER);
		else if(ServerBrowser()->NumServers() && !NumServers)
			UI()->DoLabel(&MsgBox, Localize("No servers match your filter criteria"), 16.0f, TEXTALIGN_CENTER);
	}

	if(UI()->ConsumeHotkey(CUI::HOTKEY_TAB))
	{
		const int Direction = Input()->ShiftIsPressed() ? -1 : 1;
		g_Config.m_UiToolboxPage = (g_Config.m_UiToolboxPage + 3 + Direction) % 3;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(ms_ListheaderHeight, NumServers, 1, 3, -1, &View, false);

	int NumPlayers = 0;
	static int s_PrevSelectedIndex = -1;
	if(s_PrevSelectedIndex != m_SelectedIndex)
	{
		s_ListBox.ScrollToSelected();
		s_PrevSelectedIndex = m_SelectedIndex;
	}
	m_SelectedIndex = -1;

	// reset friend counter
	for(auto &Friend : m_vFriends)
		Friend.m_NumFound = 0;

	auto RenderBrowserIcons = [this](CUIElement::SUIElementRect &UIRect, CUIRect *pRect, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor, const char *pText, ETextAlignment TextAlign, bool SmallFont = false) {
		float FontSize = 14.0f;
		if(SmallFont)
			FontSize = 6.0f;
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		TextRender()->TextColor(TextColor);
		TextRender()->TextOutlineColor(TextOutlineColor);
		UI()->DoLabelStreamed(UIRect, pRect, pText, FontSize, TextAlign, -1, 0);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(nullptr);
	};

	for(int i = 0; i < NumServers; i++)
	{
		const CServerInfo *pItem = ServerBrowser()->SortedGet(i);
		NumPlayers += pItem->m_NumFilteredPlayers;

		if(pItem->m_pUIElement == nullptr)
		{
			const int UIRectCount = 2 + (COL_VERSION + 1) * 3;
			pItem->m_pUIElement = UI()->GetNewUIElement(UIRectCount);
		}

		const CListboxItem ListItem = s_ListBox.DoNextItem(pItem, str_comp(pItem->m_aAddress, g_Config.m_UiServerAddress) == 0);
		if(ListItem.m_Selected)
			m_SelectedIndex = i;

		// update friend counter
		int FriendsOnServer = 0;
		if(pItem->m_FriendState != IFriends::FRIEND_NO)
		{
			for(int j = 0; j < pItem->m_NumReceivedClients; ++j)
			{
				if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO)
				{
					FriendsOnServer++;
					unsigned NameHash = str_quickhash(pItem->m_aClients[j].m_aName);
					unsigned ClanHash = str_quickhash(pItem->m_aClients[j].m_aClan);
					for(auto &Friend : m_vFriends)
					{
						if(((g_Config.m_ClFriendsIgnoreClan && Friend.m_pFriendInfo->m_aName[0]) || (ClanHash == Friend.m_pFriendInfo->m_ClanHash && !str_comp(Friend.m_pFriendInfo->m_aClan, pItem->m_aClients[j].m_aClan))) &&
							(!Friend.m_pFriendInfo->m_aName[0] || (NameHash == Friend.m_pFriendInfo->m_NameHash && !str_comp(Friend.m_pFriendInfo->m_aName, pItem->m_aClients[j].m_aName))))
						{
							Friend.m_NumFound++;
							if(Friend.m_pFriendInfo->m_aName[0])
								break;
						}
					}
				}
			}
		}

		if(!ListItem.m_Visible)
		{
			// reset active item, if not visible
			if(UI()->CheckActiveItem(pItem))
				UI()->SetActiveItem(nullptr);

			// don't render invisible items
			continue;
		}

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			char aTemp[64];
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = ListItem.m_Rect.y;
			Button.h = ListItem.m_Rect.h;
			Button.w = s_aCols[c].m_Rect.w;

			int ID = s_aCols[c].m_ID;

			if(ID == COL_FLAG_LOCK)
			{
				if(pItem->m_Flags & SERVER_FLAG_PASSWORD)
				{
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFlagLock + 0), &Button, {0.75f, 0.75f, 0.75f, 1}, TextRender()->DefaultTextOutlineColor(), "\xEF\x80\xA3", TEXTALIGN_CENTER);
				}
			}
			else if(ID == COL_FLAG_FAV)
			{
				if(pItem->m_Favorite != TRISTATE::NONE)
				{
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFav + 0), &Button, {0.94f, 0.4f, 0.4f, 1}, TextRender()->DefaultTextOutlineColor(), "\xEF\x80\x84", TEXTALIGN_CENTER);
				}
			}
			else if(ID == COL_FLAG_OFFICIAL)
			{
				if(pItem->m_Official && g_Config.m_UiPage != PAGE_DDNET && g_Config.m_UiPage != PAGE_KOG)
				{
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColOff + 0), &Button, {0.4f, 0.7f, 0.94f, 1}, {0.0f, 0.0f, 0.0f, 1.0f}, "\xEF\x82\xA3", TEXTALIGN_CENTER);
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColOff + 1), &Button, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, "\xEF\x80\x8C", TEXTALIGN_CENTER, true);
				}
			}
			else if(ID == COL_NAME)
			{
				float FontSize = 12.0f;
				bool Printed = false;

				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_SERVERNAME))
					Printed = PrintHighlighted(pItem->m_aName, [this, pItem, FontSize, &Button](const char *pFilteredStr, const int FilterLen) {
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 0), &Button, pItem->m_aName, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)(pFilteredStr - pItem->m_aName));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 1), &Button, pFilteredStr, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, FilterLen, &pItem->m_pUIElement->Rect(gs_OffsetColName + 0)->m_Cursor);
						TextRender()->TextColor(1, 1, 1, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 2), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, -1, &pItem->m_pUIElement->Rect(gs_OffsetColName + 1)->m_Cursor);
					});
				if(!Printed)
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName), &Button, pItem->m_aName, FontSize, TEXTALIGN_LEFT, Button.w, 1, true);
			}
			else if(ID == COL_MAP)
			{
				if(g_Config.m_UiPage == PAGE_DDNET)
				{
					CUIRect Icon;
					Button.VMargin(4.0f, &Button);
					Button.VSplitLeft(Button.h, &Icon, &Button);
					Icon.Margin(2.0f, &Icon);

					if(g_Config.m_BrIndicateFinished && pItem->m_HasRank == 1)
					{
						RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFlagLock + 1), &Icon, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), "\xEF\x84\x9E", TEXTALIGN_CENTER);
					}
				}

				float FontSize = 12.0f;
				bool Printed = false;
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_MAPNAME))
					Printed = PrintHighlighted(pItem->m_aMap, [this, pItem, FontSize, &Button](const char *pFilteredStr, const int FilterLen) {
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 0), &Button, pItem->m_aMap, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)(pFilteredStr - pItem->m_aMap));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 1), &Button, pFilteredStr, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, FilterLen, &pItem->m_pUIElement->Rect(gs_OffsetColMap + 0)->m_Cursor);
						TextRender()->TextColor(1, 1, 1, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 2), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, -1, &pItem->m_pUIElement->Rect(gs_OffsetColMap + 1)->m_Cursor);
					});
				if(!Printed)
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap), &Button, pItem->m_aMap, FontSize, TEXTALIGN_LEFT, Button.w, 1, true);
			}
			else if(ID == COL_PLAYERS)
			{
				CUIRect Icon, IconText;
				Button.VMargin(2.0f, &Button);
				if(pItem->m_FriendState != IFriends::FRIEND_NO)
				{
					Button.VSplitRight(50.0f, &Icon, &Button);
					Icon.Margin(2.0f, &Icon);
					Icon.HSplitBottom(6.0f, 0, &IconText);
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFav + 1), &Icon, {0.94f, 0.4f, 0.4f, 1}, TextRender()->DefaultTextOutlineColor(), "\xEF\x80\x84", TEXTALIGN_CENTER);
					if(FriendsOnServer > 1)
					{
						char aBufFriendsOnServer[64];
						str_format(aBufFriendsOnServer, sizeof(aBufFriendsOnServer), "%i", FriendsOnServer);
						TextRender()->TextColor(0.94f, 0.8f, 0.8f, 1);
						UI()->DoLabel(&IconText, aBufFriendsOnServer, 10.0f, TEXTALIGN_CENTER);
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1);
					}
				}

				str_format(aTemp, sizeof(aTemp), "%i/%i", pItem->m_NumFilteredPlayers, ServerBrowser()->Max(*pItem));
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_PLAYER))
					TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
				float FontSize = 12.0f;
				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColPlayers), &Button, aTemp, FontSize, TEXTALIGN_RIGHT, -1, 1, false);
				TextRender()->TextColor(1, 1, 1, 1);
			}
			else if(ID == COL_PING)
			{
				Button.VMargin(4.0f, &Button);
				FormatServerbrowserPing(aTemp, sizeof(aTemp), pItem);
				if(g_Config.m_UiColorizePing)
				{
					ColorRGBA rgb = color_cast<ColorRGBA>(ColorHSLA((300.0f - clamp(pItem->m_Latency, 0, 300)) / 1000.0f, 1.0f, 0.5f));
					TextRender()->TextColor(rgb);
				}

				float FontSize = 12.0f;
				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColPing), &Button, aTemp, FontSize, TEXTALIGN_RIGHT, -1, 1, false);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else if(ID == COL_VERSION)
			{
				const char *pVersion = pItem->m_aVersion;
				float FontSize = 12.0f;
				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColVersion), &Button, pVersion, FontSize, TEXTALIGN_RIGHT, -1, 1, false);
			}
			else if(ID == COL_GAMETYPE)
			{
				float FontSize = 12.0f;

				if(g_Config.m_UiColorizeGametype)
				{
					ColorHSLA hsl = ColorHSLA(1.0f, 1.0f, 1.0f);

					if(str_comp(pItem->m_aGameType, "DM") == 0 || str_comp(pItem->m_aGameType, "TDM") == 0 || str_comp(pItem->m_aGameType, "CTF") == 0)
						hsl = ColorHSLA(0.33f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "catch"))
						hsl = ColorHSLA(0.17f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "idm") || str_find_nocase(pItem->m_aGameType, "itdm") || str_find_nocase(pItem->m_aGameType, "ictf") || str_find_nocase(pItem->m_aGameType, "f-ddrace"))
						hsl = ColorHSLA(0.00f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "fng"))
						hsl = ColorHSLA(0.83f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "gores"))
						hsl = ColorHSLA(0.525f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "BW"))
						hsl = ColorHSLA(0.050f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "ddracenet") || str_find_nocase(pItem->m_aGameType, "ddnet"))
						hsl = ColorHSLA(0.58f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "ddrace") || str_find_nocase(pItem->m_aGameType, "mkrace"))
						hsl = ColorHSLA(0.75f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "race") || str_find_nocase(pItem->m_aGameType, "fastcap"))
						hsl = ColorHSLA(0.46f, 1.0f, 0.75f);

					ColorRGBA rgb = color_cast<ColorRGBA>(hsl);
					TextRender()->TextColor(rgb);
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColGameType), &Button, pItem->m_aGameType, FontSize, TEXTALIGN_LEFT, Button.w, 1, true);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				}
				else
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColGameType), &Button, pItem->m_aGameType, FontSize, TEXTALIGN_LEFT, Button.w, 1, true);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != m_SelectedIndex)
	{
		m_SelectedIndex = NewSelected;
		if(m_SelectedIndex >= 0)
		{
			// select the new server
			const CServerInfo *pItem = ServerBrowser()->SortedGet(NewSelected);
			if(pItem)
			{
				str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress);
			}
		}
	}
	if(s_ListBox.WasItemActivated())
		Connect(g_Config.m_UiServerAddress);

	// Render bar that shows the loading progression.
	// The bar is only shown while loading and fades out when it's done.
	CUIRect RefreshBar;
	Status.HSplitTop(5.0f, &RefreshBar, &Status);
	static float s_LoadingProgressionFadeEnd = 0.0f;
	if(ServerBrowser()->IsRefreshing() && ServerBrowser()->LoadingProgression() < 100)
	{
		s_LoadingProgressionFadeEnd = Client()->GlobalTime() + 2.0f;
	}
	const float LoadingProgressionTimeDiff = s_LoadingProgressionFadeEnd - Client()->GlobalTime();
	if(LoadingProgressionTimeDiff > 0.0f)
	{
		const float RefreshBarAlpha = minimum(LoadingProgressionTimeDiff, 0.8f);
		RefreshBar.h = 2.0f;
		RefreshBar.w *= ServerBrowser()->LoadingProgression() / 100.0f;
		RefreshBar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, RefreshBarAlpha), IGraphics::CORNER_NONE, 0.0f);
	}

	CUIRect SearchInfoAndAddr, ServersAndConnect, Status3;
	Status.VSplitRight(250.0f, &SearchInfoAndAddr, &ServersAndConnect);
	if(SearchInfoAndAddr.w > 350.0f)
		SearchInfoAndAddr.VSplitLeft(350.0f, &SearchInfoAndAddr, NULL);
	CUIRect SearchAndInfo, ServerAddr, ConnectButtons;
	SearchInfoAndAddr.HSplitTop(40.0f, &SearchAndInfo, &ServerAddr);
	ServersAndConnect.HSplitTop(35.0f, &Status3, &ConnectButtons);
	CUIRect QuickSearch, QuickExclude;

	SearchAndInfo.HSplitTop(20.f, &QuickSearch, &QuickExclude);
	QuickSearch.Margin(2.f, &QuickSearch);
	QuickExclude.Margin(2.f, &QuickExclude);

	float SearchExcludeAddrStrMax = 130.0f;

	float SearchIconWidth = 0;
	float ExcludeIconWidth = 0;
	float ExcludeSearchIconMax = 0;
	const char *pSearchLabel = "\xEF\x80\x82";
	const char *pExcludeLabel = "\xEF\x81\x9E";

	// render quick search
	{
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		SLabelProperties Props;
		Props.m_AlignVertically = 0;
		UI()->DoLabel(&QuickSearch, pSearchLabel, 16.0f, TEXTALIGN_LEFT, Props);
		SearchIconWidth = TextRender()->TextWidth(0, 16.0f, pSearchLabel, -1, -1.0f);
		ExcludeIconWidth = TextRender()->TextWidth(0, 16.0f, pExcludeLabel, -1, -1.0f);
		ExcludeSearchIconMax = maximum(SearchIconWidth, ExcludeIconWidth);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);
		QuickSearch.VSplitLeft(ExcludeSearchIconMax, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);

		char aBufSearch[64];
		str_format(aBufSearch, sizeof(aBufSearch), "%s:", Localize("Search"));
		UI()->DoLabel(&QuickSearch, aBufSearch, 14.0f, TEXTALIGN_LEFT);
		QuickSearch.VSplitLeft(SearchExcludeAddrStrMax, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);

		SUIExEditBoxProperties EditProps;
		if(Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			UI()->SetActiveItem(&g_Config.m_BrFilterString);

			EditProps.m_SelectText = true;
		}
		static int s_ClearButton = 0;
		static float s_Offset = 0.0f;
		if(UI()->DoClearableEditBox(&g_Config.m_BrFilterString, &s_ClearButton, &QuickSearch, g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString), 12.0f, &s_Offset, false, IGraphics::CORNER_ALL, EditProps))
			Client()->ServerBrowserUpdate();
	}

	// render quick exclude
	{
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		SLabelProperties Props;
		Props.m_AlignVertically = 0;
		UI()->DoLabel(&QuickExclude, pExcludeLabel, 16.0f, TEXTALIGN_LEFT, Props);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);
		QuickExclude.VSplitLeft(ExcludeSearchIconMax, 0, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, 0, &QuickExclude);

		char aBufExclude[64];
		str_format(aBufExclude, sizeof(aBufExclude), "%s:", Localize("Exclude"));
		UI()->DoLabel(&QuickExclude, aBufExclude, 14.0f, TEXTALIGN_LEFT);
		QuickExclude.VSplitLeft(SearchExcludeAddrStrMax, 0, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, 0, &QuickExclude);

		static int s_ClearButton = 0;
		static float s_Offset = 0.0f;
		if(Input()->KeyPress(KEY_X) && Input()->ShiftIsPressed() && Input()->ModifierIsPressed())
			UI()->SetActiveItem(&g_Config.m_BrExcludeString);
		if(UI()->DoClearableEditBox(&g_Config.m_BrExcludeString, &s_ClearButton, &QuickExclude, g_Config.m_BrExcludeString, sizeof(g_Config.m_BrExcludeString), 12.0f, &s_Offset, false, IGraphics::CORNER_ALL))
			Client()->ServerBrowserUpdate();
	}

	// render status
	char aBufSvr[128];
	char aBufPyr[128];
	if(ServerBrowser()->NumServers() != 1)
		str_format(aBufSvr, sizeof(aBufSvr), Localize("%d of %d servers"), ServerBrowser()->NumSortedServers(), ServerBrowser()->NumServers());
	else
		str_format(aBufSvr, sizeof(aBufSvr), Localize("%d of %d server"), ServerBrowser()->NumSortedServers(), ServerBrowser()->NumServers());
	if(NumPlayers != 1)
		str_format(aBufPyr, sizeof(aBufPyr), Localize("%d players"), NumPlayers);
	else
		str_format(aBufPyr, sizeof(aBufPyr), Localize("%d player"), NumPlayers);

	CUIRect SvrsOnline, PlysOnline;
	Status3.HSplitTop(20.f, &PlysOnline, &SvrsOnline);
	PlysOnline.VSplitRight(TextRender()->TextWidth(0, 12.0f, aBufPyr, -1, -1.0f), 0, &PlysOnline);
	UI()->DoLabel(&PlysOnline, aBufPyr, 12.0f, TEXTALIGN_LEFT);
	SvrsOnline.VSplitRight(TextRender()->TextWidth(0, 12.0f, aBufSvr, -1, -1.0f), 0, &SvrsOnline);
	UI()->DoLabel(&SvrsOnline, aBufSvr, 12.0f, TEXTALIGN_LEFT);

	// status box
	{
		CUIRect ButtonRefresh, ButtonConnect, ButtonArea;

		ServerAddr.Margin(2.0f, &ServerAddr);

		// address info
		UI()->DoLabel(&ServerAddr, Localize("Server address:"), 14.0f, TEXTALIGN_LEFT);
		ServerAddr.VSplitLeft(SearchExcludeAddrStrMax + 5.0f + ExcludeSearchIconMax + 5.0f, NULL, &ServerAddr);
		static int s_ClearButton = 0;
		static float s_Offset = 0.0f;
		UI()->DoClearableEditBox(&g_Config.m_UiServerAddress, &s_ClearButton, &ServerAddr, g_Config.m_UiServerAddress, sizeof(g_Config.m_UiServerAddress), 12.0f, &s_Offset);

		// button area
		ButtonArea = ConnectButtons;
		ButtonArea.VSplitMid(&ButtonRefresh, &ButtonConnect);
		ButtonRefresh.HSplitTop(5.0f, NULL, &ButtonRefresh);
		ButtonConnect.HSplitTop(5.0f, NULL, &ButtonConnect);
		ButtonConnect.VSplitLeft(5.0f, NULL, &ButtonConnect);

		static int s_RefreshButton = 0;
		auto Func = [this]() mutable -> const char * {
			if(ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist())
				str_copy(m_aLocalStringHelper, Localize("Refreshing..."));
			else
				str_copy(m_aLocalStringHelper, Localize("Refresh"));

			return m_aLocalStringHelper;
		};

		if(DoButtonMenu(m_RefreshButton, &s_RefreshButton, Func, 0, &ButtonRefresh, true, false, IGraphics::CORNER_ALL) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
		{
			RefreshBrowserTab(g_Config.m_UiPage);
		}

		static int s_JoinButton = 0;

		if(DoButtonMenu(
			   m_ConnectButton, &s_JoinButton, []() -> const char * { return Localize("Connect"); }, 0, &ButtonConnect, false, false, IGraphics::CORNER_ALL, 5, 0, vec4(0.7f, 1, 0.7f, 0.1f), vec4(0.7f, 1, 0.7f, 0.2f)) ||
			UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
		{
			Connect(g_Config.m_UiServerAddress);
		}
	}
}

void CMenus::Connect(const char *pAddress)
{
	if(Client()->State() == IClient::STATE_ONLINE && Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
	{
		str_copy(m_aNextServer, pAddress);
		PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect and switch to a different server?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSwitchServer);
	}
	else
		Client()->Connect(pAddress);
}

void CMenus::PopupConfirmSwitchServer()
{
	Client()->Connect(m_aNextServer);
}

void CMenus::RenderServerbrowserFilters(CUIRect View)
{
	CUIRect ServerFilter = View, FilterHeader;
	const float FontSize = 12.0f;
	ServerFilter.HSplitBottom(0.0f, &ServerFilter, 0);

	// server filter
	ServerFilter.HSplitTop(ms_ListheaderHeight, &FilterHeader, &ServerFilter);
	FilterHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);

	ServerFilter.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	UI()->DoLabel(&FilterHeader, Localize("Server filter"), FontSize + 2.0f, TEXTALIGN_CENTER);
	CUIRect Button, Button2;

	ServerFilter.VSplitLeft(5.0f, 0, &ServerFilter);
	ServerFilter.Margin(3.0f, &ServerFilter);
	ServerFilter.VMargin(5.0f, &ServerFilter);

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterEmpty, Localize("Has people playing"), g_Config.m_BrFilterEmpty, &Button))
		g_Config.m_BrFilterEmpty ^= 1;

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterSpectators, Localize("Count players only"), g_Config.m_BrFilterSpectators, &Button))
		g_Config.m_BrFilterSpectators ^= 1;

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterFull, Localize("Server not full"), g_Config.m_BrFilterFull, &Button))
		g_Config.m_BrFilterFull ^= 1;

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterFriends, Localize("Show friends only"), g_Config.m_BrFilterFriends, &Button))
		g_Config.m_BrFilterFriends ^= 1;

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterPw, Localize("No password"), g_Config.m_BrFilterPw, &Button))
		g_Config.m_BrFilterPw ^= 1;

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterGametypeStrict, Localize("Strict gametype filter"), g_Config.m_BrFilterGametypeStrict, &Button))
		g_Config.m_BrFilterGametypeStrict ^= 1;

	ServerFilter.HSplitTop(5.0f, 0, &ServerFilter);

	ServerFilter.HSplitTop(19.0f, &Button, &ServerFilter);
	UI()->DoLabel(&Button, Localize("Game types:"), FontSize, TEXTALIGN_LEFT);
	Button.VSplitRight(60.0f, 0, &Button);
	ServerFilter.HSplitTop(3.0f, 0, &ServerFilter);
	static float s_OffsetGametype = 0.0f;
	if(UI()->DoEditBox(&g_Config.m_BrFilterGametype, &Button, g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype), FontSize, &s_OffsetGametype))
		Client()->ServerBrowserUpdate();

	// server address
	ServerFilter.HSplitTop(3.0f, 0, &ServerFilter);
	ServerFilter.HSplitTop(19.0f, &Button, &ServerFilter);
	UI()->DoLabel(&Button, Localize("Server address:"), FontSize, TEXTALIGN_LEFT);
	Button.VSplitRight(60.0f, 0, &Button);
	static float s_OffsetAddr = 0.0f;
	if(UI()->DoEditBox(&g_Config.m_BrFilterServerAddress, &Button, g_Config.m_BrFilterServerAddress, sizeof(g_Config.m_BrFilterServerAddress), FontSize, &s_OffsetAddr))
		Client()->ServerBrowserUpdate();

	// player country
	{
		CUIRect Rect;
		ServerFilter.HSplitTop(3.0f, 0, &ServerFilter);
		ServerFilter.HSplitTop(26.0f, &Button, &ServerFilter);
		Button.VSplitRight(60.0f, &Button, &Rect);
		Button.HMargin(3.0f, &Button);
		if(DoButton_CheckBox(&g_Config.m_BrFilterCountry, Localize("Player country:"), g_Config.m_BrFilterCountry, &Button))
			g_Config.m_BrFilterCountry ^= 1;

		float OldWidth = Rect.w;
		Rect.w = Rect.h * 2;
		Rect.x += (OldWidth - Rect.w) / 2.0f;
		ColorRGBA Color(1.0f, 1.0f, 1.0f, g_Config.m_BrFilterCountry ? 1.0f : 0.5f);
		m_pClient->m_CountryFlags.Render(g_Config.m_BrFilterCountryIndex, &Color, Rect.x, Rect.y, Rect.w, Rect.h);

		if(g_Config.m_BrFilterCountry && UI()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, 0, &Rect))
			m_Popup = POPUP_COUNTRY;
	}

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterConnectingPlayers, Localize("Filter connecting players"), g_Config.m_BrFilterConnectingPlayers, &Button))
		g_Config.m_BrFilterConnectingPlayers ^= 1;

	CUIRect FilterTabs;
	ServerFilter.HSplitBottom(23, &ServerFilter, &FilterTabs);

	CUIRect ResetButton;

	//ServerFilter.HSplitBottom(5.0f, &ServerFilter, 0);
	ServerFilter.HSplitBottom(ms_ButtonHeight - 5.0f, &ServerFilter, &ResetButton);

	// ddnet country filters
	if(g_Config.m_UiPage == PAGE_DDNET)
	{
		ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
		if(DoButton_CheckBox(&g_Config.m_BrIndicateFinished, Localize("Indicate map finish"), g_Config.m_BrIndicateFinished, &Button))
		{
			g_Config.m_BrIndicateFinished ^= 1;

			if(g_Config.m_BrIndicateFinished)
				ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
		}

		if(g_Config.m_BrIndicateFinished)
		{
			ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
			if(DoButton_CheckBox(&g_Config.m_BrFilterUnfinishedMap, Localize("Unfinished map"), g_Config.m_BrFilterUnfinishedMap, &Button))
				g_Config.m_BrFilterUnfinishedMap ^= 1;
		}
		else
		{
			g_Config.m_BrFilterUnfinishedMap = 0;
		}
	}

	if(g_Config.m_UiPage == PAGE_DDNET || g_Config.m_UiPage == PAGE_KOG)
	{
		int Network = g_Config.m_UiPage == PAGE_DDNET ? IServerBrowser::NETWORK_DDNET : IServerBrowser::NETWORK_KOG;
		// add more space
		ServerFilter.HSplitTop(5.0f, 0, &ServerFilter);
		ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
		ServerFilter.HSplitTop(120.0f, &ServerFilter, 0);

		ServerFilter.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

		Button.VSplitMid(&Button, &Button2);

		static int s_ActivePage = 0;

		static CButtonContainer s_CountriesButton;
		if(DoButton_MenuTab(&s_CountriesButton, Localize("Countries"), s_ActivePage == 0, &Button, IGraphics::CORNER_TL))
		{
			s_ActivePage = 0;
		}

		static CButtonContainer s_TypesButton;
		if(DoButton_MenuTab(&s_TypesButton, Localize("Types"), s_ActivePage == 1, &Button2, IGraphics::CORNER_TR))
		{
			s_ActivePage = 1;
		}

		if(s_ActivePage == 1)
		{
			char *pFilterExcludeTypes = Network == IServerBrowser::NETWORK_DDNET ? g_Config.m_BrFilterExcludeTypes : g_Config.m_BrFilterExcludeTypesKoG;
			int MaxTypes = ServerBrowser()->NumTypes(Network);
			int NumTypes = ServerBrowser()->NumTypes(Network);
			int PerLine = 3;

			ServerFilter.HSplitTop(4.0f, 0, &ServerFilter);
			ServerFilter.HSplitBottom(4.0f, &ServerFilter, 0);

			const float TypesWidth = 40.0f;
			const float TypesHeight = ServerFilter.h / ceil(MaxTypes / (float)PerLine);

			CUIRect TypesRect, Left, Right;

			static int s_aTypeButtons[64];

			while(NumTypes > 0)
			{
				ServerFilter.HSplitTop(TypesHeight, &TypesRect, &ServerFilter);
				TypesRect.VSplitMid(&Left, &Right);

				for(int i = 0; i < PerLine && NumTypes > 0; i++, NumTypes--)
				{
					int TypeIndex = MaxTypes - NumTypes;
					const char *pName = ServerBrowser()->GetType(Network, TypeIndex);
					bool Active = !ServerBrowser()->DDNetFiltered(pFilterExcludeTypes, pName);

					vec2 Pos = vec2(TypesRect.x + TypesRect.w * ((i + 0.5f) / (float)PerLine), TypesRect.y);

					// correct pos
					Pos.x -= TypesWidth / 2.0f;

					// create button logic
					CUIRect Rect;

					Rect.x = Pos.x;
					Rect.y = Pos.y;
					Rect.w = TypesWidth;
					Rect.h = TypesHeight;

					int Click = UI()->DoButtonLogic(&s_aTypeButtons[TypeIndex], 0, &Rect);
					if(Click == 1 || Click == 2)
					{
						// left/right click to toggle filter
						if(pFilterExcludeTypes[0] == '\0')
						{
							// when all are active, only activate one
							for(int j = 0; j < MaxTypes; ++j)
							{
								if(j != TypeIndex)
									ServerBrowser()->DDNetFilterAdd(pFilterExcludeTypes, ServerBrowser()->GetType(Network, j));
							}
						}
						else
						{
							bool AllFilteredExceptUs = true;
							for(int j = 0; j < MaxTypes; ++j)
							{
								if(j != TypeIndex && !ServerBrowser()->DDNetFiltered(pFilterExcludeTypes, ServerBrowser()->GetType(Network, j)))
								{
									AllFilteredExceptUs = false;
									break;
								}
							}
							// when last one is removed, reset (re-enable all)
							if(AllFilteredExceptUs)
							{
								pFilterExcludeTypes[0] = '\0';
							}
							else if(Active)
							{
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeTypes, pName);
							}
							else
							{
								ServerBrowser()->DDNetFilterRem(pFilterExcludeTypes, pName);
							}
						}

						ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
					}
					else if(Click == 3)
					{
						// middle click to reset (re-enable all)
						pFilterExcludeTypes[0] = '\0';
						ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
					}

					TextRender()->TextColor(1.0f, 1.0f, 1.0f, Active ? 1.0f : 0.2f);
					UI()->DoLabel(&Rect, pName, FontSize, TEXTALIGN_CENTER);
					TextRender()->TextColor(1.0, 1.0, 1.0, 1.0f);
				}
			}
		}
		else
		{
			char *pFilterExcludeCountries = Network == IServerBrowser::NETWORK_DDNET ? g_Config.m_BrFilterExcludeCountries : g_Config.m_BrFilterExcludeCountriesKoG;
			ServerFilter.HSplitTop(15.0f, &ServerFilter, &ServerFilter);

			const float FlagWidth = 40.0f;
			const float FlagHeight = 20.0f;

			int MaxFlags = ServerBrowser()->NumCountries(Network);
			int NumFlags = ServerBrowser()->NumCountries(Network);
			int PerLine = MaxFlags > 8 ? 5 : 4;

			CUIRect FlagsRect;

			static int s_aFlagButtons[64];

			while(NumFlags > 0)
			{
				ServerFilter.HSplitTop(23.0f, &FlagsRect, &ServerFilter);

				for(int i = 0; i < PerLine && NumFlags > 0; i++, NumFlags--)
				{
					int CountryIndex = MaxFlags - NumFlags;
					const char *pName = ServerBrowser()->GetCountryName(Network, CountryIndex);
					bool Active = !ServerBrowser()->DDNetFiltered(pFilterExcludeCountries, pName);
					int FlagID = ServerBrowser()->GetCountryFlag(Network, CountryIndex);

					vec2 Pos = vec2(FlagsRect.x + FlagsRect.w * ((i + 0.5f) / (float)PerLine), FlagsRect.y);

					// correct pos
					Pos.x -= FlagWidth / 2.0f;
					Pos.y -= FlagHeight / 2.0f;

					// create button logic
					CUIRect Rect;

					Rect.x = Pos.x;
					Rect.y = Pos.y;
					Rect.w = FlagWidth;
					Rect.h = FlagHeight;

					int Click = UI()->DoButtonLogic(&s_aFlagButtons[CountryIndex], 0, &Rect);
					if(Click == 1 || Click == 2)
					{
						// left/right click to toggle filter
						if(pFilterExcludeCountries[0] == '\0')
						{
							// when all are active, only activate one
							for(int j = 0; j < MaxFlags; ++j)
							{
								if(j != CountryIndex)
									ServerBrowser()->DDNetFilterAdd(pFilterExcludeCountries, ServerBrowser()->GetCountryName(Network, j));
							}
						}
						else
						{
							bool AllFilteredExceptUs = true;
							for(int j = 0; j < MaxFlags; ++j)
							{
								if(j != CountryIndex && !ServerBrowser()->DDNetFiltered(pFilterExcludeCountries, ServerBrowser()->GetCountryName(Network, j)))
								{
									AllFilteredExceptUs = false;
									break;
								}
							}
							// when last one is removed, reset (re-enable all)
							if(AllFilteredExceptUs)
							{
								pFilterExcludeCountries[0] = '\0';
							}
							else if(Active)
							{
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeCountries, pName);
							}
							else
							{
								ServerBrowser()->DDNetFilterRem(pFilterExcludeCountries, pName);
							}
						}

						ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
					}
					else if(Click == 3)
					{
						// middle click to reset (re-enable all)
						pFilterExcludeCountries[0] = '\0';
						ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
					}

					ColorRGBA Color(1.0f, 1.0f, 1.0f, Active ? 1.0f : 0.2f);
					m_pClient->m_CountryFlags.Render(FlagID, &Color, Pos.x, Pos.y, FlagWidth, FlagHeight);
				}
			}
		}
	}

	static CButtonContainer s_ClearButton;
	if(DoButton_Menu(&s_ClearButton, Localize("Reset filter"), 0, &ResetButton))
	{
		g_Config.m_BrFilterString[0] = 0;
		g_Config.m_BrExcludeString[0] = 0;
		g_Config.m_BrFilterFull = 0;
		g_Config.m_BrFilterEmpty = 0;
		g_Config.m_BrFilterSpectators = 0;
		g_Config.m_BrFilterFriends = 0;
		g_Config.m_BrFilterCountry = 0;
		g_Config.m_BrFilterCountryIndex = -1;
		g_Config.m_BrFilterPw = 0;
		g_Config.m_BrFilterGametype[0] = 0;
		g_Config.m_BrFilterGametypeStrict = 0;
		g_Config.m_BrFilterConnectingPlayers = 1;
		g_Config.m_BrFilterUnfinishedMap = 0;
		g_Config.m_BrFilterServerAddress[0] = 0;
		g_Config.m_BrFilterExcludeCountries[0] = 0;
		g_Config.m_BrFilterExcludeTypes[0] = 0;
		if(g_Config.m_UiPage == PAGE_DDNET || g_Config.m_UiPage == PAGE_KOG)
			ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
		else
			Client()->ServerBrowserUpdate();
	}
}

void CMenus::RenderServerbrowserServerDetail(CUIRect View)
{
	CUIRect ServerDetails = View;
	CUIRect ServerScoreBoard, ServerHeader;

	const CServerInfo *pSelectedServer = ServerBrowser()->SortedGet(m_SelectedIndex);

	// split off a piece to use for scoreboard
	ServerDetails.HSplitTop(110.0f, &ServerDetails, &ServerScoreBoard);

	// server details
	CTextCursor Cursor;
	const float FontSize = 12.0f;
	ServerDetails.HSplitTop(ms_ListheaderHeight, &ServerHeader, &ServerDetails);
	ServerHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);
	ServerDetails.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	UI()->DoLabel(&ServerHeader, Localize("Server details"), FontSize + 2.0f, TEXTALIGN_CENTER);

	if(pSelectedServer)
	{
		ServerDetails.Margin(5.0f, &ServerDetails);

		CUIRect Row;
		static CLocConstString s_aLabels[] = {
			"Version", // Localize - these strings are localized within CLocConstString
			"Game type",
			"Ping"};

		// copy info button
		{
			CUIRect Button;
			ServerDetails.HSplitBottom(15.0f, &ServerDetails, &Button);
			static CButtonContainer s_CopyButton;
			if(DoButton_Menu(&s_CopyButton, Localize("Copy info"), 0, &Button))
			{
				char aInfo[256];
				pSelectedServer->InfoToString(aInfo, sizeof(aInfo));
				Input()->SetClipboardText(aInfo);
			}
		}

		ServerDetails.HSplitBottom(2.5f, &ServerDetails, nullptr);

		// favorite checkbox
		{
			CUIRect Button;
			ServerDetails.HSplitBottom(20.0f, &ServerDetails, &Button);
			CUIRect ButtonAddFav;
			CUIRect ButtonLeakIp;
			Button.VSplitMid(&ButtonAddFav, &ButtonLeakIp);
			static int s_AddFavButton = 0;
			static int s_LeakIpButton = 0;
			if(DoButton_CheckBox_Tristate(&s_AddFavButton, Localize("Favorite"), pSelectedServer->m_Favorite, &ButtonAddFav))
			{
				if(pSelectedServer->m_Favorite != TRISTATE::NONE)
				{
					Favorites()->Remove(pSelectedServer->m_aAddresses, pSelectedServer->m_NumAddresses);
				}
				else
				{
					Favorites()->Add(pSelectedServer->m_aAddresses, pSelectedServer->m_NumAddresses);
					if(g_Config.m_UiPage == PAGE_LAN)
					{
						Favorites()->AllowPing(pSelectedServer->m_aAddresses, pSelectedServer->m_NumAddresses, true);
					}
				}
				Client()->ServerBrowserUpdate();
			}
			if(pSelectedServer->m_Favorite != TRISTATE::NONE)
			{
				if(DoButton_CheckBox_Tristate(&s_LeakIpButton, Localize("Leak IP"), pSelectedServer->m_FavoriteAllowPing, &ButtonLeakIp))
				{
					Favorites()->AllowPing(pSelectedServer->m_aAddresses, pSelectedServer->m_NumAddresses, pSelectedServer->m_FavoriteAllowPing == TRISTATE::NONE);
				}
				Client()->ServerBrowserUpdate();
			}
		}

		CUIRect LeftColumn, RightColumn;
		ServerDetails.VSplitLeft(80.0f, &LeftColumn, &RightColumn);

		for(auto &Label : s_aLabels)
		{
			LeftColumn.HSplitTop(15.0f, &Row, &LeftColumn);
			UI()->DoLabel(&Row, Localize(Label), FontSize, TEXTALIGN_LEFT);
		}

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y + (15.f - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, pSelectedServer->m_aVersion, -1);

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y + (15.f - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, pSelectedServer->m_aGameType, -1);

		char aTemp[16];
		FormatServerbrowserPing(aTemp, sizeof(aTemp), pSelectedServer);
		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		TextRender()->SetCursor(&Cursor, Row.x, Row.y + (15.f - FontSize) / 2.f, FontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Row.w;
		TextRender()->TextEx(&Cursor, aTemp, -1);
	}

	// server scoreboard
	ServerScoreBoard.HSplitBottom(23.0f, &ServerScoreBoard, 0x0);

	if(pSelectedServer)
	{
		static CListBox s_ListBox;
		s_ListBox.DoAutoSpacing(1.0f);
		s_ListBox.DoStart(25.0f, pSelectedServer->m_NumReceivedClients, 1, 3, -1, &ServerScoreBoard);

		for(int i = 0; i < pSelectedServer->m_NumReceivedClients; i++)
		{
			const CServerInfo::CClient &CurrentClient = pSelectedServer->m_aClients[i];
			const CListboxItem Item = s_ListBox.DoNextItem(&CurrentClient);

			if(!Item.m_Visible)
				continue;

			const bool HasTeeToRender = pSelectedServer->m_aClients[i].m_aSkin[0] != '\0';

			CUIRect Skin, Name, Clan, Score, Flag;
			Name = Item.m_Rect;

			ColorRGBA Color = CurrentClient.m_FriendState == IFriends::FRIEND_NO ?
						  ColorRGBA(1.0f, 1.0f, 1.0f, (i % 2 + 1) * 0.05f) :
						  ColorRGBA(0.5f, 1.0f, 0.5f, 0.15f + (i % 2 + 1) * 0.05f);
			Name.Draw(Color, IGraphics::CORNER_ALL, 4.0f);
			Name.VSplitLeft(1.0f, nullptr, &Name);
			Name.VSplitLeft(34.0f, &Score, &Name);
			Name.VSplitLeft(18.0f, &Skin, &Name);
			Name.VSplitRight(20.0, &Name, &Flag);
			Flag.HMargin(4.0f, &Flag);
			Name.HSplitTop(12.0f, &Name, &Clan);

			// score
			char aTemp[16];

			if(!CurrentClient.m_Player)
				str_copy(aTemp, "SPEC");
			else if((str_find_nocase(pSelectedServer->m_aGameType, "race") || str_find_nocase(pSelectedServer->m_aGameType, "fastcap")) && g_Config.m_ClDDRaceScoreBoard)
			{
				if(CurrentClient.m_Score == -9999 || CurrentClient.m_Score == 0)
					aTemp[0] = 0;
				else
					str_time((int64_t)abs(CurrentClient.m_Score) * 100, TIME_HOURS, aTemp, sizeof(aTemp));
			}
			else
				str_format(aTemp, sizeof(aTemp), "%d", CurrentClient.m_Score);

			float ScoreFontSize = 12.0f;
			while(ScoreFontSize >= 4.0f && TextRender()->TextWidth(0, ScoreFontSize, aTemp, -1, -1.0f) > Score.w)
				ScoreFontSize--;

			TextRender()->SetCursor(&Cursor, Score.x, Score.y + (Score.h - ScoreFontSize) / 2.0f, ScoreFontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Score.w;
			TextRender()->TextEx(&Cursor, aTemp, -1);

			// render tee if available
			if(HasTeeToRender)
			{
				CTeeRenderInfo TeeInfo;
				const CSkin *pSkin = m_pClient->m_Skins.Find(CurrentClient.m_aSkin);
				TeeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
				TeeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
				TeeInfo.m_SkinMetrics = pSkin->m_Metrics;
				TeeInfo.m_CustomColoredSkin = CurrentClient.m_CustomSkinColors;
				if(CurrentClient.m_CustomSkinColors)
				{
					TeeInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(CurrentClient.m_CustomSkinColorBody).UnclampLighting());
					TeeInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(CurrentClient.m_CustomSkinColorFeet).UnclampLighting());
				}
				else
				{
					TeeInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
					TeeInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
				}
				TeeInfo.m_Size = minimum(Skin.w, Skin.h);

				CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
				vec2 TeeRenderPos(Skin.x + TeeInfo.m_Size / 2, Skin.y + Skin.h / 2 + OffsetToMid.y);

				RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
			}

			// name
			TextRender()->SetCursor(&Cursor, Name.x, Name.y + (Name.h - (FontSize - 2)) / 2.f, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Name.w;
			const char *pName = CurrentClient.m_aName;
			bool Printed = false;
			if(g_Config.m_BrFilterString[0])
				Printed = PrintHighlighted(pName, [this, &Cursor, pName](const char *pFilteredStr, const int FilterLen) {
					TextRender()->TextEx(&Cursor, pName, (int)(pFilteredStr - pName));
					TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, pFilteredStr, FilterLen);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, pFilteredStr + FilterLen, -1);
				});
			if(!Printed)
				TextRender()->TextEx(&Cursor, pName, -1);

			// clan
			TextRender()->SetCursor(&Cursor, Clan.x, Clan.y + (Clan.h - (FontSize - 2)) / 2.f, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Clan.w;
			const char *pClan = CurrentClient.m_aClan;
			Printed = false;
			if(g_Config.m_BrFilterString[0])
				Printed = PrintHighlighted(pClan, [this, &Cursor, pClan](const char *pFilteredStr, const int FilterLen) {
					TextRender()->TextEx(&Cursor, pClan, (int)(pFilteredStr - pClan));
					TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, pFilteredStr, FilterLen);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
					TextRender()->TextEx(&Cursor, pFilteredStr + FilterLen, -1);
				});
			if(!Printed)
				TextRender()->TextEx(&Cursor, pClan, -1);

			// flag
			ColorRGBA FColor(1.0f, 1.0f, 1.0f, 0.5f);
			m_pClient->m_CountryFlags.Render(CurrentClient.m_Country, &FColor, Flag.x,
				Flag.y + ((Flag.h - Flag.w / 2) / 2),
				Flag.w, Flag.w / 2);
		}

		const int NewSelected = s_ListBox.DoEnd();
		if(s_ListBox.WasItemSelected())
		{
			const CServerInfo::CClient &SelectedClient = pSelectedServer->m_aClients[NewSelected];
			if(SelectedClient.m_FriendState == IFriends::FRIEND_PLAYER)
				m_pClient->Friends()->RemoveFriend(SelectedClient.m_aName, SelectedClient.m_aClan);
			else
				m_pClient->Friends()->AddFriend(SelectedClient.m_aName, SelectedClient.m_aClan);
			FriendlistOnUpdate();
			Client()->ServerBrowserUpdate();
		}
	}
}

template<typename F>
bool CMenus::PrintHighlighted(const char *pName, F &&PrintFn)
{
	const char *pStr = g_Config.m_BrFilterString;
	char aFilterStr[sizeof(g_Config.m_BrFilterString)];
	while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
	{
		// highlight the parts that matches
		const char *pFilteredStr;
		int FilterLen = str_length(aFilterStr);
		if(aFilterStr[0] == '"' && aFilterStr[FilterLen - 1] == '"')
		{
			aFilterStr[FilterLen - 1] = '\0';
			pFilteredStr = str_comp(pName, &aFilterStr[1]) == 0 ? pName : nullptr;
			FilterLen -= 2;
		}
		else
		{
			pFilteredStr = str_utf8_find_nocase(pName, aFilterStr);
		}
		if(pFilteredStr)
		{
			PrintFn(pFilteredStr, FilterLen);
			return true;
		}
	}
	return false;
}

void CMenus::FriendlistOnUpdate()
{
	m_vFriends.clear();
	for(int i = 0; i < m_pClient->Friends()->NumFriends(); ++i)
		m_vFriends.emplace_back(m_pClient->Friends()->GetFriend(i));
	std::sort(m_vFriends.begin(), m_vFriends.end());
}

void CMenus::RenderServerbrowserFriends(CUIRect View)
{
	static int s_Inited = 0;
	if(!s_Inited)
	{
		FriendlistOnUpdate();
		s_Inited = 1;
	}

	CUIRect ServerFriends = View, FilterHeader;
	const float FontSize = 10.0f;

	ServerFriends.HSplitBottom(18.0f, &ServerFriends, NULL);

	// header
	ServerFriends.HSplitTop(ms_ListheaderHeight, &FilterHeader, &ServerFriends);
	FilterHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);
	ServerFriends.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 4.0f);
	UI()->DoLabel(&FilterHeader, Localize("Friends"), FontSize + 4.0f, TEXTALIGN_CENTER);

	CUIRect List;
	ServerFriends.Margin(3.0f, &ServerFriends);
	ServerFriends.VMargin(3.0f, &ServerFriends);
	ServerFriends.HSplitBottom(100.0f, &List, &ServerFriends);

	// friends list(remove friend)
	static CListBox s_ListBox;
	if(m_FriendlistSelectedIndex >= (int)m_vFriends.size())
		m_FriendlistSelectedIndex = m_vFriends.size() - 1;
	s_ListBox.DoAutoSpacing(3.0f);
	s_ListBox.DoStart(30.0f, m_vFriends.size(), 1, 3, m_FriendlistSelectedIndex, &List);

	std::sort(m_vFriends.begin(), m_vFriends.end());
	for(size_t i = 0; i < m_vFriends.size(); ++i)
	{
		const auto &Friend = m_vFriends[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&Friend, m_FriendlistSelectedIndex >= 0 && (size_t)m_FriendlistSelectedIndex == i);
		if(!Item.m_Visible)
			continue;

		CUIRect NameClanLabels, NameLabel, ClanLabel, OnState;
		Item.m_Rect.VSplitRight(30.0f, &NameClanLabels, &OnState);
		NameClanLabels.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f), IGraphics::CORNER_L, 4.0f);

		NameClanLabels.VMargin(2.5f, &NameClanLabels);
		NameClanLabels.HSplitTop(12.0f, &NameLabel, &ClanLabel);
		UI()->DoLabel(&NameLabel, Friend.m_pFriendInfo->m_aName, FontSize, TEXTALIGN_LEFT);
		UI()->DoLabel(&ClanLabel, Friend.m_pFriendInfo->m_aClan, FontSize, TEXTALIGN_LEFT);

		OnState.Draw(Friend.m_NumFound ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(1.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_R, 4.0f);
		OnState.HMargin((OnState.h - FontSize) / 3, &OnState);
		OnState.VMargin(5.0f, &OnState);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%i", Friend.m_NumFound);
		UI()->DoLabel(&OnState, aBuf, FontSize + 2, TEXTALIGN_RIGHT);
	}

	m_FriendlistSelectedIndex = s_ListBox.DoEnd();

	// activate found server with friend
	if(s_ListBox.WasItemActivated() && m_vFriends[m_FriendlistSelectedIndex].m_NumFound)
	{
		bool Found = false;
		int NumServers = ServerBrowser()->NumSortedServers();
		for(int i = 0; i < NumServers && !Found; i++)
		{
			int ItemIndex = m_SelectedIndex != -1 ? (m_SelectedIndex + i + 1) % NumServers : i;
			const CServerInfo *pItem = ServerBrowser()->SortedGet(ItemIndex);
			if(pItem->m_FriendState != IFriends::FRIEND_NO)
			{
				for(int j = 0; j < pItem->m_NumReceivedClients && !Found; ++j)
				{
					if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO &&
						((g_Config.m_ClFriendsIgnoreClan && m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName[0]) || str_quickhash(pItem->m_aClients[j].m_aClan) == m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_ClanHash) &&
						(!m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName[0] ||
							str_quickhash(pItem->m_aClients[j].m_aName) == m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_NameHash))
					{
						str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress);
						m_SelectedIndex = ItemIndex;
						Found = true;
					}
				}
			}
		}
	}

	CUIRect Button;
	ServerFriends.HSplitTop(2.5f, 0, &ServerFriends);
	ServerFriends.HSplitTop(20.0f, &Button, &ServerFriends);
	if(m_FriendlistSelectedIndex != -1)
	{
		static CButtonContainer s_RemoveButton;
		if(DoButton_Menu(&s_RemoveButton, Localize("Remove"), 0, &Button))
		{
			const CFriendInfo *pRemoveFriend = m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo;
			const bool IsPlayer = m_pClient->Friends()->GetFriendState(pRemoveFriend->m_aName, pRemoveFriend->m_aClan) == IFriends::FRIEND_PLAYER;
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf),
				IsPlayer ? Localize("Are you sure that you want to remove the player '%s' from your friends list?") : Localize("Are you sure that you want to remove the clan '%s' from your friends list?"),
				IsPlayer ? pRemoveFriend->m_aName : pRemoveFriend->m_aClan);
			PopupConfirm(Localize("Remove friend"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmRemoveFriend);
		}
	}

	// add friend
	if(m_pClient->Friends()->NumFriends() < IFriends::MAX_FRIENDS)
	{
		ServerFriends.HSplitTop(10.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(19.0f, &Button, &ServerFriends);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
		UI()->DoLabel(&Button, aBuf, FontSize + 2, TEXTALIGN_LEFT);
		Button.VSplitLeft(80.0f, 0, &Button);
		static char s_aName[MAX_NAME_LENGTH] = {0};
		static float s_OffsetName = 0.0f;
		UI()->DoEditBox(&s_aName, &Button, s_aName, sizeof(s_aName), FontSize + 2, &s_OffsetName);

		ServerFriends.HSplitTop(3.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(19.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
		UI()->DoLabel(&Button, aBuf, FontSize + 2, TEXTALIGN_LEFT);
		Button.VSplitLeft(80.0f, 0, &Button);
		static char s_aClan[MAX_CLAN_LENGTH] = {0};
		static float s_OffsetClan = 0.0f;
		UI()->DoEditBox(&s_aClan, &Button, s_aClan, sizeof(s_aClan), FontSize + 2, &s_OffsetClan);

		ServerFriends.HSplitTop(3.0f, 0, &ServerFriends);
		ServerFriends.HSplitTop(20.0f, &Button, &ServerFriends);
		static CButtonContainer s_AddButton;
		if(DoButton_Menu(&s_AddButton, Localize("Add Friend"), 0, &Button))
		{
			m_pClient->Friends()->AddFriend(s_aName, s_aClan);
			FriendlistOnUpdate();
			Client()->ServerBrowserUpdate();
		}
	}
}

void CMenus::PopupConfirmRemoveFriend()
{
	const CFriendInfo *pRemoveFriend = m_vFriends[m_FriendlistSelectedIndex].m_pFriendInfo;
	m_pClient->Friends()->RemoveFriend(pRemoveFriend->m_aName, pRemoveFriend->m_aClan);
	FriendlistOnUpdate();
	Client()->ServerBrowserUpdate();
}

void CMenus::RenderServerbrowser(CUIRect MainView)
{
	/*
		+-----------------+	+-------+
		|				  |	|		|
		|				  |	| tool	|
		|   server list	  |	| box 	|
		|				  |	|	  	|
		|				  |	|		|
		+-----------------+	|	 	|
			status box	tab	+-------+
	*/

	CUIRect ServerList, ToolBox;

	// background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

	// create server list, status box, tab bar and tool box area
	MainView.VSplitRight(205.0f, &ServerList, &ToolBox);
	ServerList.VSplitRight(5.0f, &ServerList, 0);

	// server list
	{
		RenderServerbrowserServerList(ServerList);
	}

	int ToolboxPage = g_Config.m_UiToolboxPage;

	// tool box
	{
		ToolBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_ALL, 4.0f);

		if(ToolboxPage == 0)
			RenderServerbrowserFilters(ToolBox);
		else if(ToolboxPage == 1)
			RenderServerbrowserServerDetail(ToolBox);
		else if(ToolboxPage == 2)
			RenderServerbrowserFriends(ToolBox);
	}

	// tab bar
	{
		CUIRect TabBar;
		ToolBox.HSplitBottom(18, &ToolBox, &TabBar);
		CUIRect TabButton0, TabButton1, TabButton2;
		float CurTabBarWidth = ToolBox.w;
		TabBar.VSplitLeft(0.333f * CurTabBarWidth, &TabButton0, &TabBar);
		TabBar.VSplitLeft(0.333f * CurTabBarWidth, &TabButton1, &TabBar);
		TabBar.VSplitLeft(0.333f * CurTabBarWidth, &TabButton2, &TabBar);
		ColorRGBA Active = ms_ColorTabbarActive;
		ColorRGBA InActive = ms_ColorTabbarInactive;
		ms_ColorTabbarActive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f);
		ms_ColorTabbarInactive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f);

		static CButtonContainer s_FiltersTab;
		if(DoButton_MenuTab(&s_FiltersTab, Localize("Filter"), ToolboxPage == 0, &TabButton0, IGraphics::CORNER_BL, NULL, NULL, NULL, NULL, 4.0f))
			ToolboxPage = 0;

		static CButtonContainer s_InfoTab;
		if(DoButton_MenuTab(&s_InfoTab, Localize("Info"), ToolboxPage == 1, &TabButton1, 0, NULL, NULL, NULL, NULL, 4.0f))
			ToolboxPage = 1;

		static CButtonContainer s_FriendsTab;
		if(DoButton_MenuTab(&s_FriendsTab, Localize("Friends"), ToolboxPage == 2, &TabButton2, IGraphics::CORNER_BR, NULL, NULL, NULL, NULL, 4.0f))
			ToolboxPage = 2;

		ms_ColorTabbarActive = Active;
		ms_ColorTabbarInactive = InActive;
		g_Config.m_UiToolboxPage = ToolboxPage;
	}
}

void CMenus::ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 2 && ((CMenus *)pUserData)->Client()->State() == IClient::STATE_OFFLINE)
	{
		((CMenus *)pUserData)->FriendlistOnUpdate();
		((CMenus *)pUserData)->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainServerbrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && g_Config.m_UiPage == PAGE_FAVORITES)
		((CMenus *)pUserData)->ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
}
