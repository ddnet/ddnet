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

using namespace FontIcons;

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
			UI()->DoLabel(&MsgBox, Localize("Getting server list from master server"), 16.0f, TEXTALIGN_MC);
		else if(!ServerBrowser()->NumServers())
			UI()->DoLabel(&MsgBox, Localize("No servers found"), 16.0f, TEXTALIGN_MC);
		else if(ServerBrowser()->NumServers() && !NumServers)
			UI()->DoLabel(&MsgBox, Localize("No servers match your filter criteria"), 16.0f, TEXTALIGN_MC);
	}

	if(UI()->ConsumeHotkey(CUI::HOTKEY_TAB))
	{
		const int Direction = Input()->ShiftIsPressed() ? -1 : 1;
		g_Config.m_UiToolboxPage = (g_Config.m_UiToolboxPage + 3 + Direction) % 3;
	}

	static CListBox s_ListBox;
	s_ListBox.SetActive(!UI()->IsPopupOpen());
	s_ListBox.DoStart(ms_ListheaderHeight, NumServers, 1, 3, -1, &View, false);

	if(m_ServerBrowserShouldRevealSelection)
	{
		s_ListBox.ScrollToSelected();
		m_ServerBrowserShouldRevealSelection = false;
	}
	m_SelectedIndex = -1;

	auto RenderBrowserIcons = [this](CUIElement::SUIElementRect &UIRect, CUIRect *pRect, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor, const char *pText, int TextAlign, bool SmallFont = false) {
		float FontSize = 14.0f;
		if(SmallFont)
			FontSize = 6.0f;
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		TextRender()->TextColor(TextColor);
		TextRender()->TextOutlineColor(TextOutlineColor);
		UI()->DoLabelStreamed(UIRect, pRect, pText, FontSize, TextAlign);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	};

	int NumPlayers = 0;
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

		const float FontSize = 12.0f;
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
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFlagLock + 0), &Button, {0.75f, 0.75f, 0.75f, 1}, TextRender()->DefaultTextOutlineColor(), FONT_ICON_LOCK, TEXTALIGN_MC);
				}
			}
			else if(ID == COL_FLAG_FAV)
			{
				if(pItem->m_Favorite != TRISTATE::NONE)
				{
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFav + 0), &Button, {0.94f, 0.4f, 0.4f, 1}, TextRender()->DefaultTextOutlineColor(), FONT_ICON_HEART, TEXTALIGN_MC);
				}
			}
			else if(ID == COL_FLAG_OFFICIAL)
			{
				if(pItem->m_Official && g_Config.m_UiPage != PAGE_DDNET && g_Config.m_UiPage != PAGE_KOG)
				{
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColOff + 0), &Button, {0.4f, 0.7f, 0.94f, 1}, {0.0f, 0.0f, 0.0f, 1.0f}, FONT_ICON_CERTIFICATE, TEXTALIGN_MC);
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColOff + 1), &Button, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, FONT_ICON_CHECK, TEXTALIGN_MC, true);
				}
			}
			else if(ID == COL_NAME)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
				bool Printed = false;
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_SERVERNAME))
					Printed = PrintHighlighted(pItem->m_aName, [this, pItem, FontSize, Props, &Button](const char *pFilteredStr, const int FilterLen) {
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 0), &Button, pItem->m_aName, FontSize, TEXTALIGN_ML, Props, (int)(pFilteredStr - pItem->m_aName));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 1), &Button, pFilteredStr, FontSize, TEXTALIGN_ML, Props, FilterLen, &pItem->m_pUIElement->Rect(gs_OffsetColName + 0)->m_Cursor);
						TextRender()->TextColor(1, 1, 1, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 2), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_ML, Props, -1, &pItem->m_pUIElement->Rect(gs_OffsetColName + 1)->m_Cursor);
					});
				if(!Printed)
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName), &Button, pItem->m_aName, FontSize, TEXTALIGN_ML, Props);
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
						RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFlagLock + 1), &Icon, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), FONT_ICON_FLAG_CHECKERED, TEXTALIGN_MC);
					}
				}

				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
				bool Printed = false;
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_MAPNAME))
					Printed = PrintHighlighted(pItem->m_aMap, [this, pItem, FontSize, Props, &Button](const char *pFilteredStr, const int FilterLen) {
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 0), &Button, pItem->m_aMap, FontSize, TEXTALIGN_ML, Props, (int)(pFilteredStr - pItem->m_aMap));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 1), &Button, pFilteredStr, FontSize, TEXTALIGN_ML, Props, FilterLen, &pItem->m_pUIElement->Rect(gs_OffsetColMap + 0)->m_Cursor);
						TextRender()->TextColor(1, 1, 1, 1);
						UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 2), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_ML, Props, -1, &pItem->m_pUIElement->Rect(gs_OffsetColMap + 1)->m_Cursor);
					});
				if(!Printed)
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap), &Button, pItem->m_aMap, FontSize, TEXTALIGN_ML, Props);
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
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFav + 1), &Icon, {0.94f, 0.4f, 0.4f, 1}, TextRender()->DefaultTextOutlineColor(), FONT_ICON_HEART, TEXTALIGN_MC);
					if(FriendsOnServer > 1)
					{
						char aBufFriendsOnServer[64];
						str_format(aBufFriendsOnServer, sizeof(aBufFriendsOnServer), "%i", FriendsOnServer);
						TextRender()->TextColor(0.94f, 0.8f, 0.8f, 1);
						UI()->DoLabel(&IconText, aBufFriendsOnServer, 10.0f, TEXTALIGN_MC);
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1);
					}
				}

				str_format(aTemp, sizeof(aTemp), "%i/%i", pItem->m_NumFilteredPlayers, ServerBrowser()->Max(*pItem));
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_PLAYER))
					TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColPlayers), &Button, aTemp, FontSize, TEXTALIGN_MR);
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

				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColPing), &Button, aTemp, FontSize, TEXTALIGN_MR);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else if(ID == COL_VERSION)
			{
				const char *pVersion = pItem->m_aVersion;
				UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColVersion), &Button, pVersion, FontSize, TEXTALIGN_MR);
			}
			else if(ID == COL_GAMETYPE)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
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
					else if(str_find_nocase(pItem->m_aGameType, "ddracenet") || str_find_nocase(pItem->m_aGameType, "ddnet") || str_find_nocase(pItem->m_aGameType, "0xf"))
						hsl = ColorHSLA(0.58f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "ddrace") || str_find_nocase(pItem->m_aGameType, "mkrace"))
						hsl = ColorHSLA(0.75f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "race") || str_find_nocase(pItem->m_aGameType, "fastcap"))
						hsl = ColorHSLA(0.46f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "s-ddr"))
						hsl = ColorHSLA(1.0f, 1.0f, 0.70f);

					ColorRGBA rgb = color_cast<ColorRGBA>(hsl);
					TextRender()->TextColor(rgb);
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColGameType), &Button, pItem->m_aGameType, FontSize, TEXTALIGN_ML, Props);
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				}
				else
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColGameType), &Button, pItem->m_aGameType, FontSize, TEXTALIGN_ML, Props);
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
				m_ServerBrowserShouldRevealSelection = true;
			}
		}
	}

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
	Status.VSplitRight(125.0f, &SearchInfoAndAddr, &ServersAndConnect);
	if(SearchInfoAndAddr.w > 350.0f)
		SearchInfoAndAddr.VSplitLeft(350.0f, &SearchInfoAndAddr, NULL);
	CUIRect SearchAndInfo, ServerAddr, ConnectButtons;
	SearchInfoAndAddr.HSplitTop(40.0f, &SearchAndInfo, &ServerAddr);
	ServersAndConnect.HSplitTop(35.0f, &Status3, &ConnectButtons);
	ConnectButtons.HSplitTop(5.0f, nullptr, &ConnectButtons);
	CUIRect QuickSearch, QuickExclude;

	SearchAndInfo.HSplitTop(20.f, &QuickSearch, &QuickExclude);
	QuickSearch.Margin(2.f, &QuickSearch);
	QuickExclude.Margin(2.f, &QuickExclude);

	float SearchExcludeAddrStrMax = 130.0f;

	float SearchIconWidth = 0;
	float ExcludeIconWidth = 0;
	float ExcludeSearchIconMax = 0;

	// render quick search
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		UI()->DoLabel(&QuickSearch, FONT_ICON_MAGNIFYING_GLASS, 16.0f, TEXTALIGN_ML);
		SearchIconWidth = TextRender()->TextWidth(16.0f, FONT_ICON_MAGNIFYING_GLASS, -1, -1.0f);
		ExcludeIconWidth = TextRender()->TextWidth(16.0f, FONT_ICON_BAN, -1, -1.0f);
		ExcludeSearchIconMax = maximum(SearchIconWidth, ExcludeIconWidth);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		QuickSearch.VSplitLeft(ExcludeSearchIconMax, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);

		char aBufSearch[64];
		str_format(aBufSearch, sizeof(aBufSearch), "%s:", Localize("Search"));
		UI()->DoLabel(&QuickSearch, aBufSearch, 14.0f, TEXTALIGN_ML);
		QuickSearch.VSplitLeft(SearchExcludeAddrStrMax, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);

		static CLineInput s_FilterInput(g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString));
		if(Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			UI()->SetActiveItem(&s_FilterInput);
			s_FilterInput.SelectAll();
		}
		if(UI()->DoClearableEditBox(&s_FilterInput, &QuickSearch, 12.0f))
			Client()->ServerBrowserUpdate();
	}

	// render quick exclude
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

		UI()->DoLabel(&QuickExclude, FONT_ICON_BAN, 16.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		QuickExclude.VSplitLeft(ExcludeSearchIconMax, 0, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, 0, &QuickExclude);

		char aBufExclude[64];
		str_format(aBufExclude, sizeof(aBufExclude), "%s:", Localize("Exclude"));
		UI()->DoLabel(&QuickExclude, aBufExclude, 14.0f, TEXTALIGN_ML);
		QuickExclude.VSplitLeft(SearchExcludeAddrStrMax, 0, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, 0, &QuickExclude);

		static CLineInput s_ExcludeInput(g_Config.m_BrExcludeString, sizeof(g_Config.m_BrExcludeString));
		if(Input()->KeyPress(KEY_X) && Input()->ShiftIsPressed() && Input()->ModifierIsPressed())
			UI()->SetActiveItem(&s_ExcludeInput);
		if(UI()->DoClearableEditBox(&s_ExcludeInput, &QuickExclude, 12.0f))
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
	PlysOnline.VSplitRight(TextRender()->TextWidth(12.0f, aBufPyr, -1, -1.0f), 0, &PlysOnline);
	UI()->DoLabel(&PlysOnline, aBufPyr, 12.0f, TEXTALIGN_ML);
	SvrsOnline.VSplitRight(TextRender()->TextWidth(12.0f, aBufSvr, -1, -1.0f), 0, &SvrsOnline);
	UI()->DoLabel(&SvrsOnline, aBufSvr, 12.0f, TEXTALIGN_ML);

	// status box
	{
		ServerAddr.Margin(2.0f, &ServerAddr);

		// address info
		UI()->DoLabel(&ServerAddr, Localize("Server address:"), 14.0f, TEXTALIGN_ML);
		ServerAddr.VSplitLeft(SearchExcludeAddrStrMax + 5.0f + ExcludeSearchIconMax + 5.0f, NULL, &ServerAddr);
		static CLineInput s_ServerAddressInput(g_Config.m_UiServerAddress, sizeof(g_Config.m_UiServerAddress));
		if(UI()->DoClearableEditBox(&s_ServerAddressInput, &ServerAddr, 12.0f))
			m_ServerBrowserShouldRevealSelection = true;

		// button area
		CUIRect ButtonRefresh, ButtonConnect;
		ConnectButtons.VSplitMid(&ButtonRefresh, &ButtonConnect, 5.0f);

		// refresh button
		{
			char aLabelBuf[32] = {0};
			const auto RefreshLabelFunc = [this, aLabelBuf]() mutable {
				if(ServerBrowser()->IsRefreshing() || ServerBrowser()->IsGettingServerlist())
					str_format(aLabelBuf, sizeof(aLabelBuf), "%s%s", FONT_ICON_ARROW_ROTATE_RIGHT, FONT_ICON_ELLIPSIS);
				else
					str_copy(aLabelBuf, FONT_ICON_ARROW_ROTATE_RIGHT);
				return aLabelBuf;
			};

			SMenuButtonProperties Props;
			Props.m_HintRequiresStringCheck = true;
			Props.m_UseIconFont = true;

			static CButtonContainer s_RefreshButton;
			if(UI()->DoButton_Menu(m_RefreshButton, &s_RefreshButton, RefreshLabelFunc, &ButtonRefresh, Props) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
			{
				RefreshBrowserTab(g_Config.m_UiPage);
			}
		}

		// connect button
		{
			const auto ConnectLabelFunc = []() { return FONT_ICON_RIGHT_TO_BRACKET; };

			SMenuButtonProperties Props;
			Props.m_UseIconFont = true;
			Props.m_Color = ColorRGBA(0.5f, 1.0f, 0.5f, 0.5f);

			static CButtonContainer s_ConnectButton;
			if(UI()->DoButton_Menu(m_ConnectButton, &s_ConnectButton, ConnectLabelFunc, &ButtonConnect, Props) || s_ListBox.WasItemActivated() || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
			{
				Connect(g_Config.m_UiServerAddress);
			}
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

	// server filter
	ServerFilter.HSplitTop(ms_ListheaderHeight, &FilterHeader, &ServerFilter);
	FilterHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);

	ServerFilter.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	UI()->DoLabel(&FilterHeader, Localize("Server filter"), FontSize + 2.0f, TEXTALIGN_MC);
	CUIRect Button, Button2;

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
	UI()->DoLabel(&Button, Localize("Game types:"), FontSize, TEXTALIGN_ML);
	Button.VSplitRight(60.0f, 0, &Button);
	ServerFilter.HSplitTop(3.0f, 0, &ServerFilter);
	static CLineInput s_GametypeInput(g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype));
	if(UI()->DoEditBox(&s_GametypeInput, &Button, FontSize))
		Client()->ServerBrowserUpdate();

	// server address
	ServerFilter.HSplitTop(3.0f, 0, &ServerFilter);
	ServerFilter.HSplitTop(19.0f, &Button, &ServerFilter);
	UI()->DoLabel(&Button, Localize("Server address:"), FontSize, TEXTALIGN_ML);
	Button.VSplitRight(60.0f, 0, &Button);
	static CLineInput s_FilterServerAddressInput(g_Config.m_BrFilterServerAddress, sizeof(g_Config.m_BrFilterServerAddress));
	if(UI()->DoEditBox(&s_FilterServerAddressInput, &Button, FontSize))
		Client()->ServerBrowserUpdate();

	// player country
	{
		CUIRect Rect;
		ServerFilter.HSplitTop(3.0f, nullptr, &ServerFilter);
		ServerFilter.HSplitTop(26.0f, &Button, &ServerFilter);
		Button.HMargin(3.0f, &Button);
		Button.VSplitRight(60.0f, &Button, &Rect);
		if(DoButton_CheckBox(&g_Config.m_BrFilterCountry, Localize("Player country:"), g_Config.m_BrFilterCountry, &Button))
			g_Config.m_BrFilterCountry ^= 1;

		float OldWidth = Rect.w;
		Rect.w = Rect.h * 2;
		Rect.x += (OldWidth - Rect.w) / 2.0f;
		m_pClient->m_CountryFlags.Render(g_Config.m_BrFilterCountryIndex, ColorRGBA(1.0f, 1.0f, 1.0f, UI()->MouseHovered(&Rect) ? 1.0f : g_Config.m_BrFilterCountry ? 0.9f : 0.5f), Rect.x, Rect.y, Rect.w, Rect.h);

		if(UI()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, 0, &Rect))
		{
			static SPopupMenuId s_PopupCountryId;
			static SPopupCountrySelectionContext s_PopupCountryContext;
			s_PopupCountryContext.m_pMenus = this;
			s_PopupCountryContext.m_Selection = g_Config.m_BrFilterCountryIndex;
			s_PopupCountryContext.m_New = true;
			UI()->DoPopupMenu(&s_PopupCountryId, Rect.x, Rect.y + Rect.h, 490, 210, &s_PopupCountryContext, PopupCountrySelection);
		}
	}

	ServerFilter.HSplitTop(20.0f, &Button, &ServerFilter);
	if(DoButton_CheckBox(&g_Config.m_BrFilterConnectingPlayers, Localize("Filter connecting players"), g_Config.m_BrFilterConnectingPlayers, &Button))
		g_Config.m_BrFilterConnectingPlayers ^= 1;

	CUIRect FilterTabs;
	ServerFilter.HSplitBottom(23, &ServerFilter, &FilterTabs);

	CUIRect ResetButton;

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
			const int FilterExcludeTypesSize = Network == IServerBrowser::NETWORK_DDNET ? sizeof(g_Config.m_BrFilterExcludeTypes) : sizeof(g_Config.m_BrFilterExcludeTypesKoG);
			int MaxTypes = ServerBrowser()->NumTypes(Network);
			int NumTypes = ServerBrowser()->NumTypes(Network);
			int PerLine = 3;

			ServerFilter.HSplitTop(4.0f, 0, &ServerFilter);
			ServerFilter.HSplitBottom(4.0f, &ServerFilter, 0);

			const float TypesWidth = 40.0f;
			const float TypesHeight = ServerFilter.h / std::ceil(MaxTypes / (float)PerLine);

			CUIRect TypesRect, Left, Right;

			static std::vector<unsigned char> s_vTypeButtons;
			s_vTypeButtons.resize(MaxTypes);

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

					int Click = UI()->DoButtonLogic(&s_vTypeButtons[TypeIndex], 0, &Rect);
					if(Click == 1 || Click == 2)
					{
						// left/right click to toggle filter
						if(pFilterExcludeTypes[0] == '\0')
						{
							if(Click == 1)
							{
								// Left click: when all are active, only activate one
								for(int j = 0; j < MaxTypes; ++j)
								{
									if(j != TypeIndex)
										ServerBrowser()->DDNetFilterAdd(pFilterExcludeTypes, FilterExcludeTypesSize, ServerBrowser()->GetType(Network, j));
								}
							}
							else if(Click == 2)
							{
								// Right click: when all are active, only deactivate one
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeTypes, FilterExcludeTypesSize, ServerBrowser()->GetType(Network, TypeIndex));
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
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeTypes, FilterExcludeTypesSize, pName);
							}
							else
							{
								ServerBrowser()->DDNetFilterRem(pFilterExcludeTypes, FilterExcludeTypesSize, pName);
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

					TextRender()->TextColor(1.0f, 1.0f, 1.0f, (Active ? 0.9f : 0.2f) + (UI()->HotItem() == &s_vTypeButtons[TypeIndex] ? 0.1f : 0.0f));
					UI()->DoLabel(&Rect, pName, FontSize, TEXTALIGN_MC);
					TextRender()->TextColor(1.0, 1.0, 1.0, 1.0f);
				}
			}
		}
		else
		{
			char *pFilterExcludeCountries = Network == IServerBrowser::NETWORK_DDNET ? g_Config.m_BrFilterExcludeCountries : g_Config.m_BrFilterExcludeCountriesKoG;
			const int FilterExcludeCountriesSize = Network == IServerBrowser::NETWORK_DDNET ? sizeof(g_Config.m_BrFilterExcludeCountries) : sizeof(g_Config.m_BrFilterExcludeCountriesKoG);
			ServerFilter.HSplitTop(15.0f, &ServerFilter, &ServerFilter);

			const float FlagWidth = 40.0f;
			const float FlagHeight = 20.0f;

			int MaxFlags = ServerBrowser()->NumCountries(Network);
			int NumFlags = ServerBrowser()->NumCountries(Network);
			int PerLine = MaxFlags > 8 ? 5 : 4;

			CUIRect FlagsRect;

			static std::vector<unsigned char> s_vFlagButtons;
			s_vFlagButtons.resize(MaxFlags);

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

					int Click = UI()->DoButtonLogic(&s_vFlagButtons[CountryIndex], 0, &Rect);
					if(Click == 1 || Click == 2)
					{
						// left/right click to toggle filter
						if(pFilterExcludeCountries[0] == '\0')
						{
							if(Click == 1)
							{
								// Left click: when all are active, only activate one
								for(int j = 0; j < MaxFlags; ++j)
								{
									if(j != CountryIndex)
										ServerBrowser()->DDNetFilterAdd(pFilterExcludeCountries, FilterExcludeCountriesSize, ServerBrowser()->GetCountryName(Network, j));
								}
							}
							else if(Click == 2)
							{
								// Right click: when all are active, only deactivate one
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeCountries, FilterExcludeCountriesSize, ServerBrowser()->GetCountryName(Network, CountryIndex));
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
								ServerBrowser()->DDNetFilterAdd(pFilterExcludeCountries, FilterExcludeCountriesSize, pName);
							}
							else
							{
								ServerBrowser()->DDNetFilterRem(pFilterExcludeCountries, FilterExcludeCountriesSize, pName);
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

					m_pClient->m_CountryFlags.Render(FlagID, ColorRGBA(1.0f, 1.0f, 1.0f, (Active ? 0.9f : 0.2f) + (UI()->HotItem() == &s_vFlagButtons[CountryIndex] ? 0.1f : 0.0f)), Pos.x, Pos.y, FlagWidth, FlagHeight);
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

CUI::EPopupMenuFunctionResult CMenus::PopupCountrySelection(void *pContext, CUIRect View, bool Active)
{
	SPopupCountrySelectionContext *pPopupContext = static_cast<SPopupCountrySelectionContext *>(pContext);
	CMenus *pMenus = pPopupContext->m_pMenus;

	static CListBox s_ListBox;
	s_ListBox.SetActive(Active);
	s_ListBox.DoStart(50.0f, pMenus->m_pClient->m_CountryFlags.Num(), 8, 1, -1, &View, false);

	if(pPopupContext->m_New)
	{
		pPopupContext->m_New = false;
		s_ListBox.ScrollToSelected();
	}

	for(size_t i = 0; i < pMenus->m_pClient->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag *pEntry = pMenus->m_pClient->m_CountryFlags.GetByIndex(i);

		const CListboxItem Item = s_ListBox.DoNextItem(pEntry, pEntry->m_CountryCode == pPopupContext->m_Selection);
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.Margin(5.0f, &FlagRect);
		FlagRect.HSplitBottom(12.0f, &FlagRect, &Label);
		Label.HSplitTop(2.0f, nullptr, &Label);
		const float OldWidth = FlagRect.w;
		FlagRect.w = FlagRect.h * 2.0f;
		FlagRect.x += (OldWidth - FlagRect.w) / 2.0f;
		pMenus->m_pClient->m_CountryFlags.Render(pEntry->m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		pMenus->UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, TEXTALIGN_MC);
	}

	const int NewSelected = s_ListBox.DoEnd();
	pPopupContext->m_Selection = NewSelected >= 0 ? pMenus->m_pClient->m_CountryFlags.GetByIndex(NewSelected)->m_CountryCode : -1;
	if(s_ListBox.WasItemSelected() || s_ListBox.WasItemActivated())
	{
		g_Config.m_BrFilterCountry = 1;
		g_Config.m_BrFilterCountryIndex = pPopupContext->m_Selection;
		pMenus->Client()->ServerBrowserUpdate();
		return CUI::POPUP_CLOSE_CURRENT;
	}

	return CUI::POPUP_KEEP_OPEN;
}

void CMenus::RenderServerbrowserServerDetail(CUIRect View)
{
	CUIRect ServerDetails = View;
	CUIRect ServerScoreBoard, ServerHeader;

	const CServerInfo *pSelectedServer = ServerBrowser()->SortedGet(m_SelectedIndex);

	// split off a piece to use for scoreboard
	ServerDetails.HSplitTop(110.0f, &ServerDetails, &ServerScoreBoard);

	// server details
	const float FontSize = 12.0f;
	ServerDetails.HSplitTop(ms_ListheaderHeight, &ServerHeader, &ServerDetails);
	ServerHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);
	ServerDetails.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	UI()->DoLabel(&ServerHeader, Localize("Server details"), FontSize + 2.0f, TEXTALIGN_MC);

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
			UI()->DoLabel(&Row, Label, FontSize, TEXTALIGN_ML);
		}

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		UI()->DoLabel(&Row, pSelectedServer->m_aVersion, FontSize, TEXTALIGN_ML);

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		UI()->DoLabel(&Row, pSelectedServer->m_aGameType, FontSize, TEXTALIGN_ML);

		char aTemp[16];
		FormatServerbrowserPing(aTemp, sizeof(aTemp), pSelectedServer);
		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		UI()->DoLabel(&Row, aTemp, FontSize, TEXTALIGN_ML);
	}
	else
	{
		UI()->DoLabel(&ServerDetails, Localize("No server selected"), FontSize, TEXTALIGN_MC);
	}

	// server scoreboard
	ServerScoreBoard.HSplitBottom(23.0f, &ServerScoreBoard, 0x0);

	CTextCursor Cursor;
	if(pSelectedServer)
	{
		static CListBox s_ListBox;
		s_ListBox.DoAutoSpacing(1.0f);
		s_ListBox.DoStart(25.0f, pSelectedServer->m_NumReceivedClients, 1, 3, -1, &ServerScoreBoard);

		int ClientScoreKind = pSelectedServer->m_ClientScoreKind;

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
			{
				str_copy(aTemp, "SPEC");
			}
			else if(ClientScoreKind == CServerInfo::CLIENT_SCORE_KIND_POINTS)
			{
				str_format(aTemp, sizeof(aTemp), "%d", CurrentClient.m_Score);
			}
			else
			{
				std::optional<int> Time = {};

				if(ClientScoreKind == CServerInfo::CLIENT_SCORE_KIND_TIME_BACKCOMPAT)
				{
					int TempTime = absolute(CurrentClient.m_Score);
					if(TempTime != 0 && TempTime != 9999)
						Time = TempTime;
				}
				else
				{
					// CServerInfo::CLIENT_SCORE_KIND_POINTS
					if(CurrentClient.m_Score >= 0)
						Time = CurrentClient.m_Score;
				}

				if(Time.has_value())
				{
					str_time((int64_t)Time.value() * 100, TIME_HOURS, aTemp, sizeof(aTemp));
				}
				else
				{
					aTemp[0] = 0;
				}
			}

			UI()->DoLabel(&Score, aTemp, FontSize, TEXTALIGN_ML);

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

				const CAnimState *pIdleState = CAnimState::GetIdle();
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
			m_pClient->m_CountryFlags.Render(CurrentClient.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f),
				Flag.x, Flag.y + ((Flag.h - Flag.w / 2) / 2), Flag.w, Flag.w / 2);
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
			const char *pFilteredStrEnd;
			pFilteredStr = str_utf8_find_nocase(pName, aFilterStr, &pFilteredStrEnd);
			if(pFilteredStr != nullptr && pFilteredStrEnd != nullptr)
				FilterLen = pFilteredStrEnd - pFilteredStr;
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
	// TODO: friends are currently updated every frame; optimize and only update friends when necessary
}

void CMenus::RenderServerbrowserFriends(CUIRect View)
{
	const float FontSize = 10.0f;
	static bool s_aListExtended[NUM_FRIEND_TYPES] = {true, true, false};
	static const ColorRGBA s_aListColors[NUM_FRIEND_TYPES] = {ColorRGBA(0.5f, 1.0f, 0.5f, 1.0f), ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f), ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f)};
	const ColorRGBA OfflineClanColor = ColorRGBA(0.7f, 0.45f, 0.75f, 1.0f);
	const float SpacingH = 2.0f;

	char aBuf[256];
	CUIRect ServerFriends, FilterHeader, List;

	// header
	View.HSplitTop(ms_ListheaderHeight, &FilterHeader, &View);
	FilterHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);
	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 4.0f);
	UI()->DoLabel(&FilterHeader, Localize("Friends"), FontSize + 4.0f, TEXTALIGN_MC);

	View.HSplitBottom(84.0f, &List, &ServerFriends);
	List.HSplitTop(3.0f, nullptr, &List);
	List.VSplitLeft(3.0f, nullptr, &List);

	// calculate friends
	// TODO: optimize this
	m_pRemoveFriend = nullptr;
	for(auto &vFriends : m_avFriends)
		vFriends.clear();

	for(int FriendIndex = 0; FriendIndex < m_pClient->Friends()->NumFriends(); ++FriendIndex)
	{
		m_avFriends[FRIEND_OFF].emplace_back(m_pClient->Friends()->GetFriend(FriendIndex));
	}

	for(int ServerIndex = 0; ServerIndex < ServerBrowser()->NumSortedServers(); ++ServerIndex)
	{
		const CServerInfo *pEntry = ServerBrowser()->SortedGet(ServerIndex);
		if(pEntry->m_FriendState == IFriends::FRIEND_NO)
			continue;

		for(int ClientIndex = 0; ClientIndex < pEntry->m_NumClients; ++ClientIndex)
		{
			const CServerInfo::CClient &CurrentClient = pEntry->m_aClients[ClientIndex];
			if(CurrentClient.m_FriendState == IFriends::FRIEND_NO)
				continue;

			const int FriendIndex = CurrentClient.m_FriendState == IFriends::FRIEND_PLAYER ? FRIEND_PLAYER_ON : FRIEND_CLAN_ON;
			m_avFriends[FriendIndex].emplace_back(CurrentClient, pEntry);
			const auto &&RemovalPredicate = [CurrentClient](const CFriendItem &Friend) {
				return (Friend.Name()[0] == '\0' || str_comp(Friend.Name(), CurrentClient.m_aName) == 0) && ((Friend.Name()[0] != '\0' && g_Config.m_ClFriendsIgnoreClan) || str_comp(Friend.Clan(), CurrentClient.m_aClan) == 0);
			};
			m_avFriends[FRIEND_OFF].erase(std::remove_if(m_avFriends[FRIEND_OFF].begin(), m_avFriends[FRIEND_OFF].end(), RemovalPredicate), m_avFriends[FRIEND_OFF].end());
		}
	}
	for(auto &vFriends : m_avFriends)
		std::sort(vFriends.begin(), vFriends.end());

	// friends list
	static CScrollRegion s_ScrollRegion;
	if(!s_ScrollRegion.IsScrollbarShown())
		List.VSplitRight(3.0f, &List, nullptr);
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 14.0f;
	ScrollParams.m_ScrollbarMargin = 4.0f;
	ScrollParams.m_ScrollUnit = 80.0f;
	s_ScrollRegion.Begin(&List, &ScrollOffset, &ScrollParams);
	List.y += ScrollOffset.y;

	for(size_t FriendType = 0; FriendType < NUM_FRIEND_TYPES; ++FriendType)
	{
		// header
		CUIRect Header, GroupIcon, GroupLabel;
		List.HSplitTop(ms_ListheaderHeight, &Header, &List);
		s_ScrollRegion.AddRect(Header);
		Header.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, UI()->MouseHovered(&Header) ? 0.4f : 0.25f), IGraphics::CORNER_ALL, 5.0f);
		Header.VSplitLeft(Header.h, &GroupIcon, &GroupLabel);
		GroupIcon.Margin(2.0f, &GroupIcon);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->TextColor(UI()->MouseHovered(&Header) ? TextRender()->DefaultTextColor() : ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
		UI()->DoLabel(&GroupIcon, s_aListExtended[FriendType] ? FONT_ICON_SQUARE_MINUS : FONT_ICON_SQUARE_PLUS, GroupIcon.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		switch(FriendType)
		{
		case FRIEND_PLAYER_ON:
			str_format(aBuf, sizeof(aBuf), Localize("Online players (%d)"), (int)m_avFriends[FriendType].size());
			break;
		case FRIEND_CLAN_ON:
			str_format(aBuf, sizeof(aBuf), Localize("Online clanmates (%d)"), (int)m_avFriends[FriendType].size());
			break;
		case FRIEND_OFF:
			str_format(aBuf, sizeof(aBuf), Localize("Offline (%d)", "friends (server browser)"), (int)m_avFriends[FriendType].size());
			break;
		default:
			dbg_assert(false, "FriendType invalid");
			break;
		}
		UI()->DoLabel(&GroupLabel, aBuf, FontSize, TEXTALIGN_ML);
		if(UI()->DoButtonLogic(&s_aListExtended[FriendType], 0, &Header))
		{
			s_aListExtended[FriendType] = !s_aListExtended[FriendType];
		}

		// entries
		if(s_aListExtended[FriendType])
		{
			for(size_t FriendIndex = 0; FriendIndex < m_avFriends[FriendType].size(); ++FriendIndex)
			{
				// space
				{
					CUIRect Space;
					List.HSplitTop(SpacingH, &Space, &List);
					s_ScrollRegion.AddRect(Space);
				}

				CUIRect Rect;
				const auto &Friend = m_avFriends[FriendType][FriendIndex];
				List.HSplitTop(11.0f + 10.0f + 2 * 2.0f + 1.0f + (Friend.ServerInfo() == nullptr ? 0.0f : 10.0f), &Rect, &List);
				s_ScrollRegion.AddRect(Rect);
				if(s_ScrollRegion.IsRectClipped(Rect))
					continue;

				const bool Inside = UI()->MouseHovered(&Rect);
				bool ButtonResult = false;
				if(Friend.ServerInfo())
				{
					ButtonResult = UI()->DoButtonLogic(Friend.ListItemId(), 0, &Rect);
					GameClient()->m_Tooltips.DoToolTip(Friend.ListItemId(), &Rect, Localize("Click to select server. Double click to join your friend."));
				}
				const bool OfflineClan = Friend.FriendState() == IFriends::FRIEND_CLAN && FriendType == FRIEND_OFF;
				Rect.Draw((OfflineClan ? OfflineClanColor : s_aListColors[FriendType]).WithAlpha(Inside ? 0.5f : 0.3f), IGraphics::CORNER_ALL, 5.0f);
				Rect.Margin(2.0f, &Rect);

				CUIRect RemoveButton, NameLabel, ClanLabel, InfoLabel;
				Rect.HSplitTop(16.0f, &RemoveButton, nullptr);
				RemoveButton.VSplitRight(13.0f, nullptr, &RemoveButton);
				RemoveButton.HMargin((RemoveButton.h - RemoveButton.w) / 2.0f, &RemoveButton);
				Rect.VSplitLeft(2.0f, nullptr, &Rect);

				if(Friend.ServerInfo())
					Rect.HSplitBottom(10.0f, &Rect, &InfoLabel);
				Rect.HSplitTop(11.0f + 10.0f, &Rect, nullptr);

				// tee
				if(Friend.Skin()[0] != '\0')
				{
					CUIRect Skin;
					Rect.VSplitLeft(Rect.h, &Skin, &Rect);
					Rect.VSplitLeft(2.0f, nullptr, &Rect);

					CTeeRenderInfo TeeInfo;
					const CSkin *pSkin = m_pClient->m_Skins.Find(Friend.Skin());
					TeeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
					TeeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
					TeeInfo.m_SkinMetrics = pSkin->m_Metrics;
					TeeInfo.m_CustomColoredSkin = Friend.CustomSkinColors();
					if(Friend.CustomSkinColors())
					{
						TeeInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(Friend.CustomSkinColorBody()).UnclampLighting());
						TeeInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(Friend.CustomSkinColorFeet()).UnclampLighting());
					}
					else
					{
						TeeInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
						TeeInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
					}
					TeeInfo.m_Size = minimum(Skin.w, Skin.h);

					const CAnimState *pIdleState = CAnimState::GetIdle();
					vec2 OffsetToMid;
					RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
					vec2 TeeRenderPos(Skin.x + Skin.w / 2.0f, Skin.y + Skin.h * 0.55f + OffsetToMid.y);

					RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
				}
				Rect.HSplitTop(11.0f, &NameLabel, &ClanLabel);

				// name
				UI()->DoLabel(&NameLabel, Friend.Name(), FontSize - 1.0f, TEXTALIGN_ML);

				// clan
				UI()->DoLabel(&ClanLabel, Friend.Clan(), FontSize - 2.0f, TEXTALIGN_ML);

				// server info
				if(Friend.ServerInfo())
				{
					// official server icon
					if(Friend.ServerInfo()->m_Official)
					{
						CUIRect OfficialIcon;
						InfoLabel.VSplitLeft(InfoLabel.h, &OfficialIcon, &InfoLabel);
						InfoLabel.VSplitLeft(1.0f, nullptr, &InfoLabel); // spacing
						OfficialIcon.HSplitTop(1.0f, nullptr, &OfficialIcon); // alignment

						SLabelProperties Props;
						Props.m_EnableWidthCheck = false;
						TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
						TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
						TextRender()->TextColor(0.4f, 0.7f, 0.94f, 1.0f);
						TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 1.0f);
						UI()->DoLabel(&OfficialIcon, FONT_ICON_CERTIFICATE, OfficialIcon.h, TEXTALIGN_MC, Props);
						TextRender()->TextColor(0.0f, 0.0f, 0.0f, 1.0f);
						TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f);
						UI()->DoLabel(&OfficialIcon, FONT_ICON_CHECK, OfficialIcon.h * 0.5f, TEXTALIGN_MC, Props);
						TextRender()->TextColor(TextRender()->DefaultTextColor());
						TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
						TextRender()->SetRenderFlags(0);
						TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
					}

					// server info text
					char aLatency[16];
					FormatServerbrowserPing(aLatency, sizeof(aLatency), Friend.ServerInfo());
					if(aLatency[0] != '\0')
						str_format(aBuf, sizeof(aBuf), "%s | %s | %s", Friend.ServerInfo()->m_aMap, Friend.ServerInfo()->m_aGameType, aLatency);
					else
						str_format(aBuf, sizeof(aBuf), "%s | %s", Friend.ServerInfo()->m_aMap, Friend.ServerInfo()->m_aGameType);
					UI()->DoLabel(&InfoLabel, aBuf, FontSize - 2.0f, TEXTALIGN_ML);
				}

				// remove button
				TextRender()->TextColor(UI()->MouseHovered(&RemoveButton) ? TextRender()->DefaultTextColor() : ColorRGBA(0.4f, 0.4f, 0.4f, 1.0f));
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
				UI()->DoLabel(&RemoveButton, "×", RemoveButton.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);
				TextRender()->SetRenderFlags(0);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				if(UI()->DoButtonLogic(Friend.RemoveButtonId(), 0, &RemoveButton))
				{
					m_pRemoveFriend = &Friend;
					ButtonResult = false;
				}
				GameClient()->m_Tooltips.DoToolTip(Friend.RemoveButtonId(), &RemoveButton, Friend.FriendState() == IFriends::FRIEND_PLAYER ? Localize("Click to remove this player from your friends list.") : Localize("Click to remove this clan from your friends list."));

				// handle click and double click on item
				if(ButtonResult && Friend.ServerInfo())
				{
					str_copy(g_Config.m_UiServerAddress, Friend.ServerInfo()->m_aAddress);
					m_ServerBrowserShouldRevealSelection = true;
					if(Input()->MouseDoubleClick())
					{
						Connect(g_Config.m_UiServerAddress);
					}
				}
			}

			if(m_avFriends[FriendType].empty())
			{
				CUIRect Label;
				List.HSplitTop(12.0f, &Label, &List);
				s_ScrollRegion.AddRect(Label);
				UI()->DoLabel(&Label, Localize("None"), Label.h * CUI::ms_FontmodHeight, TEXTALIGN_ML);
			}
		}

		// space
		{
			CUIRect Space;
			List.HSplitTop(SpacingH, &Space, &List);
			s_ScrollRegion.AddRect(Space);
		}
	}
	s_ScrollRegion.End();

	if(m_pRemoveFriend != nullptr)
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage),
			m_pRemoveFriend->FriendState() == IFriends::FRIEND_PLAYER ? Localize("Are you sure that you want to remove the player '%s' from your friends list?") : Localize("Are you sure that you want to remove the clan '%s' from your friends list?"),
			m_pRemoveFriend->FriendState() == IFriends::FRIEND_PLAYER ? m_pRemoveFriend->Name() : m_pRemoveFriend->Clan());
		PopupConfirm(Localize("Remove friend"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmRemoveFriend);
	}

	// add friend
	if(m_pClient->Friends()->NumFriends() < IFriends::MAX_FRIENDS)
	{
		CUIRect Button;
		ServerFriends.Margin(3.0f, &ServerFriends);

		ServerFriends.HSplitTop(18.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
		UI()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
		Button.VSplitLeft(80.0f, nullptr, &Button);
		static CLineInputBuffered<MAX_NAME_LENGTH> s_NameInput;
		UI()->DoEditBox(&s_NameInput, &Button, FontSize + 2.0f);

		ServerFriends.HSplitTop(3.0f, nullptr, &ServerFriends);
		ServerFriends.HSplitTop(18.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
		UI()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
		Button.VSplitLeft(80.0f, nullptr, &Button);
		static CLineInputBuffered<MAX_CLAN_LENGTH> s_ClanInput;
		UI()->DoEditBox(&s_ClanInput, &Button, FontSize + 2.0f);

		ServerFriends.HSplitTop(3.0f, nullptr, &ServerFriends);
		ServerFriends.HSplitTop(18.0f, &Button, &ServerFriends);
		static CButtonContainer s_AddButton;
		if(DoButton_Menu(&s_AddButton, s_NameInput.IsEmpty() && !s_ClanInput.IsEmpty() ? Localize("Add Clan") : Localize("Add Friend"), 0, &Button))
		{
			m_pClient->Friends()->AddFriend(s_NameInput.GetString(), s_ClanInput.GetString());
			s_NameInput.Clear();
			s_ClanInput.Clear();
			FriendlistOnUpdate();
			Client()->ServerBrowserUpdate();
		}
	}
}

void CMenus::PopupConfirmRemoveFriend()
{
	m_pClient->Friends()->RemoveFriend(m_pRemoveFriend->FriendState() == IFriends::FRIEND_PLAYER ? m_pRemoveFriend->Name() : "", m_pRemoveFriend->Clan());
	FriendlistOnUpdate();
	Client()->ServerBrowserUpdate();
	m_pRemoveFriend = nullptr;
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
		if(DoButton_MenuTab(&s_InfoTab, Localize("Info"), ToolboxPage == 1, &TabButton1, IGraphics::CORNER_NONE, NULL, NULL, NULL, NULL, 4.0f))
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
