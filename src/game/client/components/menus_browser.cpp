/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>

#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/console.h>
#include <game/client/components/countryflags.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <game/client/gameclient.h>

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
		"AFR", // LOC_AFRICA // Localize("AFR")
		"ASI", // LOC_ASIA // Localize("ASI")
		"AUS", // LOC_AUSTRALIA // Localize("AUS")
		"EUR", // LOC_EUROPE // Localize("EUR")
		"NA", // LOC_NORTH_AMERICA // Localize("NA")
		"SA", // LOC_SOUTH_AMERICA // Localize("SA")
		"CHN", // LOC_CHINA // Localize("CHN")
	};
	dbg_assert(0 <= pInfo->m_Location && pInfo->m_Location < CServerInfo::NUM_LOCS, "location out of range");
	str_format(pBuffer, BufferLength, "%s", Localize(LOCATION_NAMES[pInfo->m_Location]));
}

void CMenus::RenderServerbrowserServerList(CUIRect View)
{
	CUIRect Headers;
	CUIRect Status;

	View.HSplitTop(ms_ListheaderHeight, &Headers, &View);
	View.HSplitBottom(70.0f, &View, &Status);

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
		int m_Flags;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		FIXED = 1,
		SPACER = 2,

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
		{-1, -1, " ", -1, 2.0f, 0, {0}, {0}},
		{COL_FLAG_LOCK, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_FLAG_FAV, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_FLAG_OFFICIAL, -1, " ", -1, 14.0f, 0, {0}, {0}},
		{COL_NAME, IServerBrowser::SORT_NAME, "Name", 0, 50.0f, 0, {0}, {0}}, // Localize - these strings are localized within CLocConstString
		{COL_GAMETYPE, IServerBrowser::SORT_GAMETYPE, "Type", 1, 50.0f, 0, {0}, {0}},
		{COL_MAP, IServerBrowser::SORT_MAP, "Map", 1, 120.0f + (Headers.w - 480) / 8, 0, {0}, {0}},
		{COL_PLAYERS, IServerBrowser::SORT_NUMPLAYERS, "Players", 1, 75.0f, 0, {0}, {0}},
		{-1, -1, " ", 1, 10.0f, 0, {0}, {0}},
		{COL_PING, IServerBrowser::SORT_PING, "Ping", 1, 40.0f, FIXED, {0}, {0}},
	};
	// This is just for scripts/update_localization.py to work correctly (all other strings are already Localize()'d somewhere else). Don't remove!
	// Localize("Type");

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				//Cols[i].flags |= SPACER;
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

	CUIRect Scroll;
	View.VSplitRight(20.0f, &View, &Scroll);

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

	static float s_ScrollValue = 0;

	s_ScrollValue = UI()->DoScrollbarV(&s_ScrollValue, &Scroll, s_ScrollValue);

	if(UI()->ConsumeHotkey(CUI::HOTKEY_TAB))
	{
		const int Direction = Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT) ? -1 : 1;
		g_Config.m_UiToolboxPage = (g_Config.m_UiToolboxPage + 3 + Direction) % 3;
	}

	if(HandleListInputs(View, s_ScrollValue, 3.0f, &m_ScrollOffset, s_aCols[0].m_Rect.h, m_SelectedIndex, NumServers))
	{
		const CServerInfo *pItem = ServerBrowser()->SortedGet(m_SelectedIndex);
		str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress);
	}

	// set clipping
	UI()->ClipEnable(&View);

	CUIRect OriginalView = View;
	int Num = (int)(View.h / s_aCols[0].m_Rect.h) + 1;
	int ScrollNum = maximum(NumServers - Num + 1, 0);
	View.y -= s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h;

	int NewSelected = -1;
	bool DoubleClicked = false;
	int NumPlayers = 0;

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
		int ItemIndex = i;
		const CServerInfo *pItem = ServerBrowser()->SortedGet(ItemIndex);
		NumPlayers += pItem->m_NumFilteredPlayers;
		CUIRect Row;

		const int UIRectCount = 2 + (COL_VERSION + 1) * 3;
		//initialize
		if(pItem->m_pUIElement == NULL)
		{
			pItem->m_pUIElement = UI()->GetNewUIElement(UIRectCount);
		}

		int Selected = str_comp(pItem->m_aAddress, g_Config.m_UiServerAddress) == 0; //selected_index==ItemIndex;

		View.HSplitTop(ms_ListheaderHeight, &Row, &View);

		if(Selected)
			m_SelectedIndex = i;

		// update friend counter
		if(pItem->m_FriendState != IFriends::FRIEND_NO)
		{
			for(int j = 0; j < pItem->m_NumReceivedClients; ++j)
			{
				if(pItem->m_aClients[j].m_FriendState != IFriends::FRIEND_NO)
				{
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

		// make sure that only those in view can be selected
		if(Row.y + Row.h > OriginalView.y && Row.y < OriginalView.y + OriginalView.h)
		{
			if(Selected)
			{
				CUIRect r = Row;
				r.Margin(0.5f, &r);
				pItem->m_pUIElement->Rect(0)->Draw(&r, ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_ALL, 4.0f);
			}
			else if(UI()->MouseHovered(&Row))
			{
				CUIRect r = Row;
				r.Margin(0.5f, &r);
				pItem->m_pUIElement->Rect(0)->Draw(&r, ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 4.0f);
			}

			if(UI()->DoButtonLogic(pItem, Selected, &Row))
			{
				NewSelected = ItemIndex;
				if(NewSelected == m_DoubleClickIndex)
					DoubleClicked = true;
				m_DoubleClickIndex = NewSelected;
			}
		}
		else
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
			Button.y = Row.y;
			Button.h = Row.h;
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
				{
					const char *pStrToken = g_Config.m_BrFilterString;
					char aFilterStr[sizeof(g_Config.m_BrFilterString)];
					while((pStrToken = str_next_token(pStrToken, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
					{
						if(aFilterStr[0] == '\0')
						{
							continue;
						}

						// highlight the parts that matches
						const char *pStr = str_utf8_find_nocase(pItem->m_aName, aFilterStr);
						if(pStr)
						{
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 0), &Button, pItem->m_aName, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)(pStr - pItem->m_aName));
							TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 1), &Button, pStr, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)str_length(aFilterStr), &pItem->m_pUIElement->Rect(gs_OffsetColName + 0)->m_Cursor);
							TextRender()->TextColor(1, 1, 1, 1);
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColName + 2), &Button, pStr + str_length(aFilterStr), FontSize, TEXTALIGN_LEFT, Button.w, 1, true, -1, &pItem->m_pUIElement->Rect(gs_OffsetColName + 1)->m_Cursor);
							Printed = true;
							break;
						}
					}
				}
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
				{
					const char *pStrToken = g_Config.m_BrFilterString;
					char aFilterStr[sizeof(g_Config.m_BrFilterString)];
					while((pStrToken = str_next_token(pStrToken, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
					{
						// highlight the parts that matches
						const char *pStr = str_utf8_find_nocase(pItem->m_aMap, aFilterStr);
						if(pStr)
						{
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 0), &Button, pItem->m_aMap, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)(pStr - pItem->m_aMap));
							TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1);
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 1), &Button, pStr, FontSize, TEXTALIGN_LEFT, Button.w, 1, true, (int)str_length(aFilterStr), &pItem->m_pUIElement->Rect(gs_OffsetColMap + 0)->m_Cursor);
							TextRender()->TextColor(1, 1, 1, 1);
							UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap + 2), &Button, pStr + str_length(aFilterStr), FontSize, TEXTALIGN_LEFT, Button.w, 1, true, -1, &pItem->m_pUIElement->Rect(gs_OffsetColMap + 1)->m_Cursor);
							Printed = true;
							break;
						}
					}
				}
				if(!Printed)
					UI()->DoLabelStreamed(*pItem->m_pUIElement->Rect(gs_OffsetColMap), &Button, pItem->m_aMap, FontSize, TEXTALIGN_LEFT, Button.w, 1, true);
			}
			else if(ID == COL_PLAYERS)
			{
				CUIRect Icon;
				Button.VMargin(4.0f, &Button);
				if(pItem->m_FriendState != IFriends::FRIEND_NO)
				{
					Button.VSplitLeft(Button.h, &Icon, &Button);
					Icon.Margin(2.0f, &Icon);
					RenderBrowserIcons(*pItem->m_pUIElement->Rect(gs_OffsetColFav + 1), &Icon, {0.94f, 0.4f, 0.4f, 1}, TextRender()->DefaultTextOutlineColor(), "\xEF\x80\x84", TEXTALIGN_LEFT);
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
					else if(str_find_nocase(pItem->m_aGameType, "idm") || str_find_nocase(pItem->m_aGameType, "itdm") || str_find_nocase(pItem->m_aGameType, "ictf"))
						hsl = ColorHSLA(0.00f, 1.0f, 0.75f);
					else if(str_find_nocase(pItem->m_aGameType, "fng"))
						hsl = ColorHSLA(0.83f, 1.0f, 0.75f);
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

	UI()->ClipDisable();

	if(NewSelected != -1)
	{
		// select the new server
		const CServerInfo *pItem = ServerBrowser()->SortedGet(NewSelected);
		str_copy(g_Config.m_UiServerAddress, pItem->m_aAddress);
		if(DoubleClicked && Input()->MouseDoubleClick())
		{
			if(Client()->State() == IClient::STATE_ONLINE && Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				m_Popup = POPUP_SWITCH_SERVER;
				str_copy(m_aNextServer, g_Config.m_UiServerAddress);
			}
			else
				Client()->Connect(g_Config.m_UiServerAddress);
		}
	}

	//Status.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 5.0f);
	Status.Margin(5.0f, &Status);

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
		if(Input()->KeyPress(KEY_X) && (Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && Input()->ModifierIsPressed())
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
			if(ServerBrowser()->IsRefreshing())
				str_format(m_aLocalStringHelper, sizeof(m_aLocalStringHelper), "%s (%d%%)", Localize("Refresh"), ServerBrowser()->LoadingProgression());
			else if(ServerBrowser()->IsGettingServerlist())
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
			if(Client()->State() == IClient::STATE_ONLINE && Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				m_Popup = POPUP_SWITCH_SERVER;
				str_copy(m_aNextServer, g_Config.m_UiServerAddress);
			}
			else
				Client()->Connect(g_Config.m_UiServerAddress);
		}
	}
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
	ServerDetails.HSplitTop(90.0f, &ServerDetails, &ServerScoreBoard);
	ServerDetails.HSplitBottom(2.5f, &ServerDetails, 0x0);

	// server details
	CTextCursor Cursor;
	const float FontSize = 12.0f;
	ServerDetails.HSplitTop(ms_ListheaderHeight, &ServerHeader, &ServerDetails);
	ServerHeader.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 4.0f);
	ServerDetails.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	UI()->DoLabel(&ServerHeader, Localize("Server details"), FontSize + 2.0f, TEXTALIGN_CENTER);

	if(pSelectedServer)
	{
		ServerDetails.VSplitLeft(5.0f, 0, &ServerDetails);
		ServerDetails.Margin(3.0f, &ServerDetails);

		CUIRect Row;
		static CLocConstString s_aLabels[] = {
			"Version", // Localize - these strings are localized within CLocConstString
			"Game type",
			"Ping"};

		CUIRect LeftColumn;
		CUIRect RightColumn;

		//
		{
			CUIRect Button;
			ServerDetails.HSplitBottom(20.0f, &ServerDetails, &Button);
			CUIRect ButtonAddFav;
			CUIRect ButtonLeakIp;
			Button.VSplitMid(&ButtonAddFav, &ButtonLeakIp);
			ButtonAddFav.VSplitLeft(5.0f, 0, &ButtonAddFav);
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

		ServerDetails.VSplitLeft(5.0f, 0x0, &ServerDetails);
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
		static int s_VoteList = 0;
		static float s_ScrollValue = 0;
		UiDoListboxStart(&s_VoteList, &ServerScoreBoard, 26.0f, Localize("Scoreboard"), "", pSelectedServer->m_NumReceivedClients, 1, -1, s_ScrollValue);

		for(int i = 0; i < pSelectedServer->m_NumReceivedClients; i++)
		{
			CListboxItem Item = UiDoListboxNextItem(&pSelectedServer->m_aClients[i]);

			if(!Item.m_Visible)
				continue;

			CUIRect Name, Clan, Score, Flag;
			Item.m_Rect.HSplitTop(25.0f, &Name, &Item.m_Rect);
			if(UiLogicGetCurrentClickedItem() == i)
			{
				if(pSelectedServer->m_aClients[i].m_FriendState == IFriends::FRIEND_PLAYER)
					m_pClient->Friends()->RemoveFriend(pSelectedServer->m_aClients[i].m_aName, pSelectedServer->m_aClients[i].m_aClan);
				else
					m_pClient->Friends()->AddFriend(pSelectedServer->m_aClients[i].m_aName, pSelectedServer->m_aClients[i].m_aClan);
				FriendlistOnUpdate();
				Client()->ServerBrowserUpdate();
			}

			ColorRGBA Color = pSelectedServer->m_aClients[i].m_FriendState == IFriends::FRIEND_NO ?
						  ColorRGBA(1.0f, 1.0f, 1.0f, (i % 2 + 1) * 0.05f) :
						  ColorRGBA(0.5f, 1.0f, 0.5f, 0.15f + (i % 2 + 1) * 0.05f);
			Name.Draw(Color, IGraphics::CORNER_ALL, 4.0f);
			Name.VSplitLeft(5.0f, 0, &Name);
			Name.VSplitLeft(34.0f, &Score, &Name);
			Name.VSplitRight(34.0f, &Name, &Flag);
			Flag.HMargin(4.0f, &Flag);
			Name.HSplitTop(12.0f, &Name, &Clan);

			// score
			char aTemp[16];

			if(!pSelectedServer->m_aClients[i].m_Player)
				str_copy(aTemp, "SPEC");
			else if((str_find_nocase(pSelectedServer->m_aGameType, "race") || str_find_nocase(pSelectedServer->m_aGameType, "fastcap")) && g_Config.m_ClDDRaceScoreBoard)
			{
				if(pSelectedServer->m_aClients[i].m_Score == -9999 || pSelectedServer->m_aClients[i].m_Score == 0)
					aTemp[0] = 0;
				else
					str_time((int64_t)abs(pSelectedServer->m_aClients[i].m_Score) * 100, TIME_HOURS, aTemp, sizeof(aTemp));
			}
			else
				str_format(aTemp, sizeof(aTemp), "%d", pSelectedServer->m_aClients[i].m_Score);

			float ScoreFontSize = 12.0f;
			while(ScoreFontSize >= 4.0f && TextRender()->TextWidth(0, ScoreFontSize, aTemp, -1, -1.0f) > Score.w)
				ScoreFontSize--;

			TextRender()->SetCursor(&Cursor, Score.x, Score.y + (Score.h - ScoreFontSize) / 2.0f, ScoreFontSize, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Score.w;
			TextRender()->TextEx(&Cursor, aTemp, -1);

			// name
			TextRender()->SetCursor(&Cursor, Name.x, Name.y + (Name.h - (FontSize - 2)) / 2.f, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Name.w;
			const char *pName = pSelectedServer->m_aClients[i].m_aName;
			bool Printed = false;
			if(g_Config.m_BrFilterString[0])
			{
				const char *pStr = g_Config.m_BrFilterString;
				char aFilterStr[sizeof(g_Config.m_BrFilterString)];
				while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
				{
					// highlight the parts that matches
					const char *pFilteredStr = str_utf8_find_nocase(pName, aFilterStr);
					if(pFilteredStr)
					{
						TextRender()->TextEx(&Cursor, pName, (int)(pFilteredStr - pName));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1.0f);
						TextRender()->TextEx(&Cursor, pFilteredStr, str_length(aFilterStr));
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
						TextRender()->TextEx(&Cursor, pFilteredStr + str_length(aFilterStr), -1);
						Printed = true;
						break;
					}
				}
			}
			if(!Printed)
				TextRender()->TextEx(&Cursor, pName, -1);

			// clan
			TextRender()->SetCursor(&Cursor, Clan.x, Clan.y + (Clan.h - (FontSize - 2)) / 2.f, FontSize - 2, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = Clan.w;
			const char *pClan = pSelectedServer->m_aClients[i].m_aClan;
			Printed = false;
			if(g_Config.m_BrFilterString[0])
			{
				const char *pStr = g_Config.m_BrFilterString;
				char aFilterStr[sizeof(g_Config.m_BrFilterString)];
				while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
				{
					// highlight the parts that matches
					const char *pFilteredString = str_utf8_find_nocase(pClan, aFilterStr);
					if(pFilteredString)
					{
						TextRender()->TextEx(&Cursor, pClan, (int)(pFilteredString - pClan));
						TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1.0f);
						TextRender()->TextEx(&Cursor, pFilteredString, str_length(aFilterStr));
						TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
						TextRender()->TextEx(&Cursor, pFilteredString + str_length(aFilterStr), -1);
						Printed = true;
						break;
					}
				}
			}
			if(!Printed)
				TextRender()->TextEx(&Cursor, pClan, -1);

			// flag
			ColorRGBA FColor(1.0f, 1.0f, 1.0f, 0.5f);
			m_pClient->m_CountryFlags.Render(pSelectedServer->m_aClients[i].m_Country, &FColor, Flag.x, Flag.y, Flag.w, Flag.h);
		}

		UiDoListboxEnd(&s_ScrollValue, 0);
	}
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
	CUIRect Button, List;

	ServerFriends.Margin(3.0f, &ServerFriends);
	ServerFriends.VMargin(3.0f, &ServerFriends);
	ServerFriends.HSplitBottom(100.0f, &List, &ServerFriends);

	// friends list(remove friend)
	static float s_ScrollValue = 0;
	if(m_FriendlistSelectedIndex >= (int)m_vFriends.size())
		m_FriendlistSelectedIndex = m_vFriends.size() - 1;
	UiDoListboxStart(&m_vFriends, &List, 30.0f, "", "", m_vFriends.size(), 1, m_FriendlistSelectedIndex, s_ScrollValue);

	std::sort(m_vFriends.begin(), m_vFriends.end());
	for(auto &Friend : m_vFriends)
	{
		CListboxItem Item = UiDoListboxNextItem(&Friend.m_NumFound, false, false);

		if(Item.m_Visible)
		{
			Item.m_Rect.Margin(1.5f, &Item.m_Rect);
			CUIRect OnState;
			Item.m_Rect.VSplitRight(30.0f, &Item.m_Rect, &OnState);
			Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f), IGraphics::CORNER_L, 4.0f);

			Item.m_Rect.VMargin(2.5f, &Item.m_Rect);
			Item.m_Rect.HSplitTop(12.0f, &Item.m_Rect, &Button);
			UI()->DoLabel(&Item.m_Rect, Friend.m_pFriendInfo->m_aName, FontSize, TEXTALIGN_LEFT);
			UI()->DoLabel(&Button, Friend.m_pFriendInfo->m_aClan, FontSize, TEXTALIGN_LEFT);

			OnState.Draw(Friend.m_NumFound ? ColorRGBA(0.0f, 1.0f, 0.0f, 0.25f) : ColorRGBA(1.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_R, 4.0f);
			OnState.HMargin((OnState.h - FontSize) / 3, &OnState);
			OnState.VMargin(5.0f, &OnState);
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%i", Friend.m_NumFound);
			UI()->DoLabel(&OnState, aBuf, FontSize + 2, TEXTALIGN_RIGHT);
		}
	}

	bool Activated = false;
	m_FriendlistSelectedIndex = UiDoListboxEnd(&s_ScrollValue, &Activated);

	// activate found server with friend
	if(Activated && m_vFriends[m_FriendlistSelectedIndex].m_NumFound)
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
						m_ScrollOffset = ItemIndex;
						m_SelectedIndex = ItemIndex;
						Found = true;
					}
				}
			}
		}
	}

	ServerFriends.HSplitTop(2.5f, 0, &ServerFriends);
	ServerFriends.HSplitTop(20.0f, &Button, &ServerFriends);
	if(m_FriendlistSelectedIndex != -1)
	{
		static CButtonContainer s_RemoveButton;
		if(DoButton_Menu(&s_RemoveButton, Localize("Remove"), 0, &Button))
			m_Popup = POPUP_REMOVE_FRIEND;
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
