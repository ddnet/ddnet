/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>

#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include "menus.h"

using namespace FontIcons;

static const ColorRGBA gs_HighlightedTextColor = ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f);

static ColorRGBA PlayerBackgroundColor(bool Friend, bool Clan, bool Afk, bool Inside)
{
	static const ColorRGBA COLORS[] = {ColorRGBA(0.5f, 1.0f, 0.5f), ColorRGBA(0.4f, 0.4f, 1.0f), ColorRGBA(0.75f, 0.75f, 0.75f)};
	static const ColorRGBA COLORS_AFK[] = {ColorRGBA(1.0f, 1.0f, 0.5f), ColorRGBA(0.4f, 0.75f, 1.0f), ColorRGBA(0.6f, 0.6f, 0.6f)};
	int i;
	if(Friend)
		i = 0;
	else if(Clan)
		i = 1;
	else
		i = 2;
	return (Afk ? COLORS_AFK[i] : COLORS[i]).WithAlpha(Inside ? 0.45f : 0.3f);
}

template<size_t N>
static void FormatServerbrowserPing(char (&aBuffer)[N], const CServerInfo *pInfo)
{
	if(!pInfo->m_LatencyIsEstimated)
	{
		str_format(aBuffer, sizeof(aBuffer), "%d", pInfo->m_Latency);
		return;
	}
	static const char *const LOCATION_NAMES[CServerInfo::NUM_LOCS] = {
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
	str_copy(aBuffer, Localize(LOCATION_NAMES[pInfo->m_Location]));
}

static ColorRGBA GetPingTextColor(int Latency)
{
	return color_cast<ColorRGBA>(ColorHSLA((300.0f - clamp(Latency, 0, 300)) / 1000.0f, 1.0f, 0.5f));
}

static ColorRGBA GetGametypeTextColor(const char *pGametype)
{
	ColorHSLA HslaColor;
	if(str_comp(pGametype, "DM") == 0 || str_comp(pGametype, "TDM") == 0 || str_comp(pGametype, "CTF") == 0 || str_comp(pGametype, "LMS") == 0 || str_comp(pGametype, "LTS") == 0)
		HslaColor = ColorHSLA(0.33f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "catch"))
		HslaColor = ColorHSLA(0.17f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "dm") || str_find_nocase(pGametype, "tdm") || str_find_nocase(pGametype, "ctf") || str_find_nocase(pGametype, "lms") || str_find_nocase(pGametype, "lts"))
	{
		if(pGametype[0] == 'i' || pGametype[0] == 'g')
			HslaColor = ColorHSLA(0.0f, 1.0f, 0.75f);
		else
			HslaColor = ColorHSLA(0.40f, 1.0f, 0.75f);
	}
	else if(str_find_nocase(pGametype, "f-ddrace") || str_find_nocase(pGametype, "freeze"))
		HslaColor = ColorHSLA(0.0f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "fng"))
		HslaColor = ColorHSLA(0.83f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "gores"))
		HslaColor = ColorHSLA(0.525f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "BW"))
		HslaColor = ColorHSLA(0.05f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "ddracenet") || str_find_nocase(pGametype, "ddnet") || str_find_nocase(pGametype, "0xf"))
		HslaColor = ColorHSLA(0.58f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "ddrace") || str_find_nocase(pGametype, "mkrace"))
		HslaColor = ColorHSLA(0.75f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "race") || str_find_nocase(pGametype, "fastcap"))
		HslaColor = ColorHSLA(0.46f, 1.0f, 0.75f);
	else if(str_find_nocase(pGametype, "s-ddr"))
		HslaColor = ColorHSLA(1.0f, 1.0f, 0.7f);
	else
		HslaColor = ColorHSLA(1.0f, 1.0f, 1.0f);
	return color_cast<ColorRGBA>(HslaColor);
}

void CMenus::RenderServerbrowserServerList(CUIRect View, bool &WasListboxItemActivated)
{
	static CListBox s_ListBox;

	CUIRect Headers;
	View.HSplitTop(ms_ListheaderHeight, &Headers, &View);
	Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(s_ListBox.ScrollbarWidthMax(), &Headers, nullptr);
	View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);

	struct SColumn
	{
		int m_Id;
		int m_Sort;
		const char *m_pCaption;
		int m_Direction;
		float m_Width;
		CUIRect m_Rect;
	};

	enum
	{
		COL_FLAG_LOCK = 0,
		COL_FLAG_FAV,
		COL_COMMUNITY,
		COL_NAME,
		COL_GAMETYPE,
		COL_MAP,
		COL_FRIENDS,
		COL_PLAYERS,
		COL_PING,

		UI_ELEM_LOCK_ICON = 0,
		UI_ELEM_FAVORITE_ICON,
		UI_ELEM_NAME_1,
		UI_ELEM_NAME_2,
		UI_ELEM_NAME_3,
		UI_ELEM_GAMETYPE,
		UI_ELEM_MAP_1,
		UI_ELEM_MAP_2,
		UI_ELEM_MAP_3,
		UI_ELEM_FINISH_ICON,
		UI_ELEM_PLAYERS,
		UI_ELEM_FRIEND_ICON,
		UI_ELEM_PING,
		NUM_UI_ELEMS,
	};

	static SColumn s_aCols[] = {
		{-1, -1, "", -1, 2.0f, {0}},
		{COL_FLAG_LOCK, -1, "", -1, 14.0f, {0}},
		{COL_FLAG_FAV, -1, "", -1, 14.0f, {0}},
		{COL_COMMUNITY, -1, "", -1, 28.0f, {0}},
		{COL_NAME, IServerBrowser::SORT_NAME, Localizable("Name"), 0, 50.0f, {0}},
		{COL_GAMETYPE, IServerBrowser::SORT_GAMETYPE, Localizable("Type"), 1, 50.0f, {0}},
		{COL_MAP, IServerBrowser::SORT_MAP, Localizable("Map"), 1, 120.0f + (Headers.w - 480) / 8, {0}},
		{COL_FRIENDS, IServerBrowser::SORT_NUMFRIENDS, "", 1, 20.0f, {0}},
		{COL_PLAYERS, IServerBrowser::SORT_NUMPLAYERS, Localizable("Players"), 1, 60.0f, {0}},
		{-1, -1, "", 1, 4.0f, {0}},
		{COL_PING, IServerBrowser::SORT_PING, Localizable("Ping"), 1, 40.0f, {0}},
	};

	const int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				Headers.VSplitLeft(2.0f, nullptr, &Headers);
			}
		}
	}

	for(int i = NumCols - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
			Headers.VSplitRight(2.0f, &Headers, nullptr);
		}
	}

	for(auto &Col : s_aCols)
	{
		if(Col.m_Direction == 0)
			Col.m_Rect = Headers;
	}

	const bool PlayersOrPing = (g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS || g_Config.m_BrSort == IServerBrowser::SORT_PING);

	// do headers
	for(const auto &Col : s_aCols)
	{
		int Checked = g_Config.m_BrSort == Col.m_Sort;
		if(PlayersOrPing && g_Config.m_BrSortOrder == 2 && (Col.m_Sort == IServerBrowser::SORT_NUMPLAYERS || Col.m_Sort == IServerBrowser::SORT_PING))
			Checked = 2;

		if(DoButton_GridHeader(&Col.m_Id, Localize(Col.m_pCaption), Checked, &Col.m_Rect))
		{
			if(Col.m_Sort != -1)
			{
				if(g_Config.m_BrSort == Col.m_Sort)
					g_Config.m_BrSortOrder = (g_Config.m_BrSortOrder + 1) % (PlayersOrPing ? 3 : 2);
				else
					g_Config.m_BrSortOrder = 0;
				g_Config.m_BrSort = Col.m_Sort;
			}
		}

		if(Col.m_Id == COL_FRIENDS)
		{
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
			Ui()->DoLabel(&Col.m_Rect, FONT_ICON_HEART, 14.0f, TEXTALIGN_MC);
			TextRender()->SetRenderFlags(0);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
	}

	const int NumServers = ServerBrowser()->NumSortedServers();

	// display important messages in the middle of the screen so no
	// users misses it
	{
		if(!ServerBrowser()->NumServers() && ServerBrowser()->IsGettingServerlist())
		{
			Ui()->DoLabel(&View, Localize("Getting server list from master server"), 16.0f, TEXTALIGN_MC);
		}
		else if(!ServerBrowser()->NumServers())
		{
			if(ServerBrowser()->GetCurrentType() == IServerBrowser::TYPE_LAN)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), Localize("No local servers found (ports %d-%d)"), IServerBrowser::LAN_PORT_BEGIN, IServerBrowser::LAN_PORT_END);
				Ui()->DoLabel(&View, aBuf, 16.0f, TEXTALIGN_MC);
			}
			else
			{
				Ui()->DoLabel(&View, Localize("No servers found"), 16.0f, TEXTALIGN_MC);
			}
		}
		else if(ServerBrowser()->NumServers() && !NumServers)
		{
			CUIRect Label, ResetButton;
			View.HMargin((View.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &Label);
			Label.HSplitTop(16.0f, &Label, &ResetButton);
			ResetButton.HSplitTop(8.0f, nullptr, &ResetButton);
			ResetButton.VMargin((ResetButton.w - 200.0f) / 2.0f, &ResetButton);
			Ui()->DoLabel(&Label, Localize("No servers match your filter criteria"), 16.0f, TEXTALIGN_MC);
			static CButtonContainer s_ResetButton;
			if(DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &ResetButton))
			{
				ResetServerbrowserFilters();
			}
		}
	}

	s_ListBox.SetActive(!Ui()->IsPopupOpen());
	s_ListBox.DoStart(ms_ListheaderHeight, NumServers, 1, 3, -1, &View, false);

	if(m_ServerBrowserShouldRevealSelection)
	{
		s_ListBox.ScrollToSelected();
		m_ServerBrowserShouldRevealSelection = false;
	}
	m_SelectedIndex = -1;

	const auto &&RenderBrowserIcons = [this](CUIElement::SUIElementRect &UIRect, CUIRect *pRect, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor, const char *pText, int TextAlign, bool SmallFont = false) {
		const float FontSize = SmallFont ? 6.0f : 14.0f;
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		TextRender()->TextColor(TextColor);
		TextRender()->TextOutlineColor(TextOutlineColor);
		Ui()->DoLabelStreamed(UIRect, pRect, pText, FontSize, TextAlign);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	};

	std::vector<CUIElement *> &vpServerBrowserUiElements = m_avpServerBrowserUiElements[ServerBrowser()->GetCurrentType()];
	if(vpServerBrowserUiElements.size() < (size_t)NumServers)
		vpServerBrowserUiElements.resize(NumServers, nullptr);

	for(int i = 0; i < NumServers; i++)
	{
		const CServerInfo *pItem = ServerBrowser()->SortedGet(i);
		const CCommunity *pCommunity = ServerBrowser()->Community(pItem->m_aCommunityId);

		if(vpServerBrowserUiElements[i] == nullptr)
		{
			vpServerBrowserUiElements[i] = Ui()->GetNewUIElement(NUM_UI_ELEMS);
		}
		CUIElement *pUiElement = vpServerBrowserUiElements[i];

		const CListboxItem ListItem = s_ListBox.DoNextItem(pItem, str_comp(pItem->m_aAddress, g_Config.m_UiServerAddress) == 0);
		if(ListItem.m_Selected)
			m_SelectedIndex = i;

		if(!ListItem.m_Visible)
		{
			// reset active item, if not visible
			if(Ui()->CheckActiveItem(pItem))
				Ui()->SetActiveItem(nullptr);

			// don't render invisible items
			continue;
		}

		const float FontSize = 12.0f;
		char aTemp[64];
		for(const auto &Col : s_aCols)
		{
			CUIRect Button;
			Button.x = Col.m_Rect.x;
			Button.y = ListItem.m_Rect.y;
			Button.h = ListItem.m_Rect.h;
			Button.w = Col.m_Rect.w;

			const int Id = Col.m_Id;
			if(Id == COL_FLAG_LOCK)
			{
				if(pItem->m_Flags & SERVER_FLAG_PASSWORD)
				{
					RenderBrowserIcons(*pUiElement->Rect(UI_ELEM_LOCK_ICON), &Button, ColorRGBA(0.75f, 0.75f, 0.75f, 1.0f), TextRender()->DefaultTextOutlineColor(), FONT_ICON_LOCK, TEXTALIGN_MC);
				}
			}
			else if(Id == COL_FLAG_FAV)
			{
				if(pItem->m_Favorite != TRISTATE::NONE)
				{
					RenderBrowserIcons(*pUiElement->Rect(UI_ELEM_FAVORITE_ICON), &Button, ColorRGBA(1.0f, 0.85f, 0.3f, 1.0f), TextRender()->DefaultTextOutlineColor(), FONT_ICON_STAR, TEXTALIGN_MC);
				}
			}
			else if(Id == COL_COMMUNITY)
			{
				if(pCommunity != nullptr)
				{
					const SCommunityIcon *pIcon = FindCommunityIcon(pCommunity->Id());
					if(pIcon != nullptr)
					{
						CUIRect CommunityIcon;
						Button.Margin(2.0f, &CommunityIcon);
						RenderCommunityIcon(pIcon, CommunityIcon, true);
						Ui()->DoButtonLogic(&pItem->m_aCommunityId, 0, &CommunityIcon);
						GameClient()->m_Tooltips.DoToolTip(&pItem->m_aCommunityId, &CommunityIcon, pCommunity->Name());
					}
				}
			}
			else if(Id == COL_NAME)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
				bool Printed = false;
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_SERVERNAME))
					Printed = PrintHighlighted(pItem->m_aName, [&](const char *pFilteredStr, const int FilterLen) {
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_NAME_1), &Button, pItem->m_aName, FontSize, TEXTALIGN_ML, Props, (int)(pFilteredStr - pItem->m_aName));
						TextRender()->TextColor(gs_HighlightedTextColor);
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_NAME_2), &Button, pFilteredStr, FontSize, TEXTALIGN_ML, Props, FilterLen, &pUiElement->Rect(UI_ELEM_NAME_1)->m_Cursor);
						TextRender()->TextColor(TextRender()->DefaultTextColor());
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_NAME_3), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_ML, Props, -1, &pUiElement->Rect(UI_ELEM_NAME_2)->m_Cursor);
					});
				if(!Printed)
					Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_NAME_1), &Button, pItem->m_aName, FontSize, TEXTALIGN_ML, Props);
			}
			else if(Id == COL_GAMETYPE)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
				if(g_Config.m_UiColorizeGametype)
				{
					TextRender()->TextColor(GetGametypeTextColor(pItem->m_aGameType));
				}
				Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_GAMETYPE), &Button, pItem->m_aGameType, FontSize, TEXTALIGN_ML, Props);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
			else if(Id == COL_MAP)
			{
				{
					CUIRect Icon;
					Button.VMargin(4.0f, &Button);
					Button.VSplitLeft(Button.h, &Icon, &Button);
					if(g_Config.m_BrIndicateFinished && pItem->m_HasRank == CServerInfo::RANK_RANKED)
					{
						Icon.Margin(2.0f, &Icon);
						RenderBrowserIcons(*pUiElement->Rect(UI_ELEM_FINISH_ICON), &Icon, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), FONT_ICON_FLAG_CHECKERED, TEXTALIGN_MC);
					}
				}

				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_StopAtEnd = true;
				Props.m_EnableWidthCheck = false;
				bool Printed = false;
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_MAPNAME))
					Printed = PrintHighlighted(pItem->m_aMap, [&](const char *pFilteredStr, const int FilterLen) {
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_MAP_1), &Button, pItem->m_aMap, FontSize, TEXTALIGN_ML, Props, (int)(pFilteredStr - pItem->m_aMap));
						TextRender()->TextColor(gs_HighlightedTextColor);
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_MAP_2), &Button, pFilteredStr, FontSize, TEXTALIGN_ML, Props, FilterLen, &pUiElement->Rect(UI_ELEM_MAP_1)->m_Cursor);
						TextRender()->TextColor(TextRender()->DefaultTextColor());
						Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_MAP_3), &Button, pFilteredStr + FilterLen, FontSize, TEXTALIGN_ML, Props, -1, &pUiElement->Rect(UI_ELEM_MAP_2)->m_Cursor);
					});
				if(!Printed)
					Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_MAP_1), &Button, pItem->m_aMap, FontSize, TEXTALIGN_ML, Props);
			}
			else if(Id == COL_FRIENDS)
			{
				if(pItem->m_FriendState != IFriends::FRIEND_NO)
				{
					RenderBrowserIcons(*pUiElement->Rect(UI_ELEM_FRIEND_ICON), &Button, ColorRGBA(0.94f, 0.4f, 0.4f, 1.0f), TextRender()->DefaultTextOutlineColor(), FONT_ICON_HEART, TEXTALIGN_MC);

					if(pItem->m_FriendNum > 1)
					{
						str_format(aTemp, sizeof(aTemp), "%d", pItem->m_FriendNum);
						TextRender()->TextColor(0.94f, 0.8f, 0.8f, 1.0f);
						Ui()->DoLabel(&Button, aTemp, 9.0f, TEXTALIGN_MC);
						TextRender()->TextColor(TextRender()->DefaultTextColor());
					}
				}
			}
			else if(Id == COL_PLAYERS)
			{
				str_format(aTemp, sizeof(aTemp), "%i/%i", pItem->m_NumFilteredPlayers, ServerBrowser()->Max(*pItem));
				if(g_Config.m_BrFilterString[0] && (pItem->m_QuickSearchHit & IServerBrowser::QUICK_PLAYER))
				{
					TextRender()->TextColor(gs_HighlightedTextColor);
				}
				Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_PLAYERS), &Button, aTemp, FontSize, TEXTALIGN_MR);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
			}
			else if(Id == COL_PING)
			{
				Button.VMargin(4.0f, &Button);
				FormatServerbrowserPing(aTemp, pItem);
				if(g_Config.m_UiColorizePing)
				{
					TextRender()->TextColor(GetPingTextColor(pItem->m_Latency));
				}
				Ui()->DoLabelStreamed(*pUiElement->Rect(UI_ELEM_PING), &Button, aTemp, FontSize, TEXTALIGN_MR);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
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

	WasListboxItemActivated = s_ListBox.WasItemActivated();
}

void CMenus::RenderServerbrowserStatusBox(CUIRect StatusBox, bool WasListboxItemActivated)
{
	// Render bar that shows the loading progression.
	// The bar is only shown while loading and fades out when it's done.
	CUIRect RefreshBar;
	StatusBox.HSplitTop(5.0f, &RefreshBar, &StatusBox);
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

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	const float SearchExcludeAddrStrMax = 130.0f;
	const float SearchIconWidth = TextRender()->TextWidth(16.0f, FONT_ICON_MAGNIFYING_GLASS);
	const float ExcludeIconWidth = TextRender()->TextWidth(16.0f, FONT_ICON_BAN);
	const float ExcludeSearchIconMax = maximum(SearchIconWidth, ExcludeIconWidth);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	CUIRect SearchInfoAndAddr, ServersAndConnect, ServersPlayersOnline, SearchAndInfo, ServerAddr, ConnectButtons;
	StatusBox.VSplitRight(135.0f, &SearchInfoAndAddr, &ServersAndConnect);
	if(SearchInfoAndAddr.w > 350.0f)
		SearchInfoAndAddr.VSplitLeft(350.0f, &SearchInfoAndAddr, nullptr);
	SearchInfoAndAddr.HSplitTop(40.0f, &SearchAndInfo, &ServerAddr);
	ServersAndConnect.HSplitTop(35.0f, &ServersPlayersOnline, &ConnectButtons);
	ConnectButtons.HSplitTop(5.0f, nullptr, &ConnectButtons);

	CUIRect QuickSearch, QuickExclude;
	SearchAndInfo.HSplitTop(20.0f, &QuickSearch, &QuickExclude);
	QuickSearch.Margin(2.0f, &QuickSearch);
	QuickExclude.Margin(2.0f, &QuickExclude);

	// render quick search
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&QuickSearch, FONT_ICON_MAGNIFYING_GLASS, 16.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		QuickSearch.VSplitLeft(ExcludeSearchIconMax, nullptr, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, nullptr, &QuickSearch);

		char aBufSearch[64];
		str_format(aBufSearch, sizeof(aBufSearch), "%s:", Localize("Search"));
		Ui()->DoLabel(&QuickSearch, aBufSearch, 14.0f, TEXTALIGN_ML);
		QuickSearch.VSplitLeft(SearchExcludeAddrStrMax, nullptr, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, nullptr, &QuickSearch);

		static CLineInput s_FilterInput(g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString));
		static char s_aTooltipText[64];
		str_format(s_aTooltipText, sizeof(s_aTooltipText), "%s: \"solo; nameless tee; kobra 2\"", Localize("Example of usage"));
		GameClient()->m_Tooltips.DoToolTip(&s_FilterInput, &QuickSearch, s_aTooltipText);
		if(!Ui()->IsPopupOpen() && Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			Ui()->SetActiveItem(&s_FilterInput);
			s_FilterInput.SelectAll();
		}
		if(Ui()->DoClearableEditBox(&s_FilterInput, &QuickSearch, 12.0f))
			Client()->ServerBrowserUpdate();
	}

	// render quick exclude
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		Ui()->DoLabel(&QuickExclude, FONT_ICON_BAN, 16.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		QuickExclude.VSplitLeft(ExcludeSearchIconMax, nullptr, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, nullptr, &QuickExclude);

		char aBufExclude[64];
		str_format(aBufExclude, sizeof(aBufExclude), "%s:", Localize("Exclude"));
		Ui()->DoLabel(&QuickExclude, aBufExclude, 14.0f, TEXTALIGN_ML);
		QuickExclude.VSplitLeft(SearchExcludeAddrStrMax, nullptr, &QuickExclude);
		QuickExclude.VSplitLeft(5.0f, nullptr, &QuickExclude);

		static CLineInput s_ExcludeInput(g_Config.m_BrExcludeString, sizeof(g_Config.m_BrExcludeString));
		static char s_aTooltipText[64];
		str_format(s_aTooltipText, sizeof(s_aTooltipText), "%s: \"CHN; [A]\"", Localize("Example of usage"));
		GameClient()->m_Tooltips.DoToolTip(&s_ExcludeInput, &QuickSearch, s_aTooltipText);
		if(!Ui()->IsPopupOpen() && Input()->KeyPress(KEY_X) && Input()->ShiftIsPressed() && Input()->ModifierIsPressed())
		{
			Ui()->SetActiveItem(&s_ExcludeInput);
			s_ExcludeInput.SelectAll();
		}
		if(Ui()->DoClearableEditBox(&s_ExcludeInput, &QuickExclude, 12.0f))
			Client()->ServerBrowserUpdate();
	}

	// render status
	{
		CUIRect ServersOnline, PlayersOnline;
		ServersPlayersOnline.HSplitMid(&PlayersOnline, &ServersOnline);

		char aBuf[128];
		if(ServerBrowser()->NumServers() != 1)
			str_format(aBuf, sizeof(aBuf), Localize("%d of %d servers"), ServerBrowser()->NumSortedServers(), ServerBrowser()->NumServers());
		else
			str_format(aBuf, sizeof(aBuf), Localize("%d of %d server"), ServerBrowser()->NumSortedServers(), ServerBrowser()->NumServers());
		Ui()->DoLabel(&ServersOnline, aBuf, 12.0f, TEXTALIGN_MR);

		if(ServerBrowser()->NumSortedPlayers() != 1)
			str_format(aBuf, sizeof(aBuf), Localize("%d players"), ServerBrowser()->NumSortedPlayers());
		else
			str_format(aBuf, sizeof(aBuf), Localize("%d player"), ServerBrowser()->NumSortedPlayers());
		Ui()->DoLabel(&PlayersOnline, aBuf, 12.0f, TEXTALIGN_MR);
	}

	// address info
	{
		CUIRect ServerAddrLabel, ServerAddrEditBox;
		ServerAddr.Margin(2.0f, &ServerAddr);
		ServerAddr.VSplitLeft(SearchExcludeAddrStrMax + 5.0f + ExcludeSearchIconMax + 5.0f, &ServerAddrLabel, &ServerAddrEditBox);

		Ui()->DoLabel(&ServerAddrLabel, Localize("Server address:"), 14.0f, TEXTALIGN_ML);
		static CLineInput s_ServerAddressInput(g_Config.m_UiServerAddress, sizeof(g_Config.m_UiServerAddress));
		if(Ui()->DoClearableEditBox(&s_ServerAddressInput, &ServerAddrEditBox, 12.0f))
			m_ServerBrowserShouldRevealSelection = true;
	}

	// buttons
	{
		CUIRect ButtonRefresh, ButtonConnect;
		ConnectButtons.VSplitMid(&ButtonRefresh, &ButtonConnect, 5.0f);

		// refresh button
		{
			char aLabelBuf[32] = {0};
			const auto &&RefreshLabelFunc = [this, aLabelBuf]() mutable {
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
			if(Ui()->DoButton_Menu(m_RefreshButton, &s_RefreshButton, RefreshLabelFunc, &ButtonRefresh, Props) || (!Ui()->IsPopupOpen() && (Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))))
			{
				RefreshBrowserTab(true);
			}
		}

		// connect button
		{
			const auto &&ConnectLabelFunc = []() { return FONT_ICON_RIGHT_TO_BRACKET; };

			SMenuButtonProperties Props;
			Props.m_UseIconFont = true;
			Props.m_Color = ColorRGBA(0.5f, 1.0f, 0.5f, 0.5f);

			static CButtonContainer s_ConnectButton;
			if(Ui()->DoButton_Menu(m_ConnectButton, &s_ConnectButton, ConnectLabelFunc, &ButtonConnect, Props) || WasListboxItemActivated || (!Ui()->IsPopupOpen() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
			{
				Connect(g_Config.m_UiServerAddress);
			}
		}
	}
}

void CMenus::Connect(const char *pAddress)
{
	if(Client()->State() == IClient::STATE_ONLINE && GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
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
	const float RowHeight = 18.0f;
	const float FontSize = (RowHeight - 4.0f) * CUi::ms_FontmodHeight; // based on DoButton_CheckBox

	View.Margin(5.0f, &View);

	CUIRect Button, ResetButton;
	View.HSplitBottom(RowHeight, &View, &ResetButton);
	View.HSplitBottom(3.0f, &View, nullptr);

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterEmpty, Localize("Has people playing"), g_Config.m_BrFilterEmpty, &Button))
		g_Config.m_BrFilterEmpty ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterSpectators, Localize("Count players only"), g_Config.m_BrFilterSpectators, &Button))
		g_Config.m_BrFilterSpectators ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterFull, Localize("Server not full"), g_Config.m_BrFilterFull, &Button))
		g_Config.m_BrFilterFull ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterFriends, Localize("Show friends only"), g_Config.m_BrFilterFriends, &Button))
		g_Config.m_BrFilterFriends ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterPw, Localize("No password"), g_Config.m_BrFilterPw, &Button))
		g_Config.m_BrFilterPw ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterLogin, Localize("No login required"), g_Config.m_BrFilterLogin, &Button))
		g_Config.m_BrFilterLogin ^= 1;

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterGametypeStrict, Localize("Strict gametype filter"), g_Config.m_BrFilterGametypeStrict, &Button))
		g_Config.m_BrFilterGametypeStrict ^= 1;

	View.HSplitTop(3.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Button, &View);
	Ui()->DoLabel(&Button, Localize("Game types:"), FontSize, TEXTALIGN_ML);
	Button.VSplitRight(60.0f, nullptr, &Button);
	static CLineInput s_GametypeInput(g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype));
	if(Ui()->DoEditBox(&s_GametypeInput, &Button, FontSize))
		Client()->ServerBrowserUpdate();

	// server address
	View.HSplitTop(6.0f, nullptr, &View);
	View.HSplitTop(RowHeight, &Button, &View);
	View.HSplitTop(6.0f, nullptr, &View);
	Ui()->DoLabel(&Button, Localize("Server address:"), FontSize, TEXTALIGN_ML);
	Button.VSplitRight(60.0f, nullptr, &Button);
	static CLineInput s_FilterServerAddressInput(g_Config.m_BrFilterServerAddress, sizeof(g_Config.m_BrFilterServerAddress));
	if(Ui()->DoEditBox(&s_FilterServerAddressInput, &Button, FontSize))
		Client()->ServerBrowserUpdate();

	// player country
	{
		CUIRect Flag;
		View.HSplitTop(RowHeight, &Button, &View);
		Button.VSplitRight(60.0f, &Button, &Flag);
		if(DoButton_CheckBox(&g_Config.m_BrFilterCountry, Localize("Player country:"), g_Config.m_BrFilterCountry, &Button))
			g_Config.m_BrFilterCountry ^= 1;

		const float OldWidth = Flag.w;
		Flag.w = Flag.h * 2.0f;
		Flag.x += (OldWidth - Flag.w) / 2.0f;
		m_pClient->m_CountryFlags.Render(g_Config.m_BrFilterCountryIndex, ColorRGBA(1.0f, 1.0f, 1.0f, Ui()->HotItem() == &g_Config.m_BrFilterCountryIndex ? 1.0f : g_Config.m_BrFilterCountry ? 0.9f : 0.5f), Flag.x, Flag.y, Flag.w, Flag.h);

		if(Ui()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, 0, &Flag))
		{
			static SPopupMenuId s_PopupCountryId;
			static SPopupCountrySelectionContext s_PopupCountryContext;
			s_PopupCountryContext.m_pMenus = this;
			s_PopupCountryContext.m_Selection = g_Config.m_BrFilterCountryIndex;
			s_PopupCountryContext.m_New = true;
			Ui()->DoPopupMenu(&s_PopupCountryId, Flag.x, Flag.y + Flag.h, 490, 210, &s_PopupCountryContext, PopupCountrySelection);
		}
	}

	View.HSplitTop(RowHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_BrFilterConnectingPlayers, Localize("Filter connecting players"), g_Config.m_BrFilterConnectingPlayers, &Button))
		g_Config.m_BrFilterConnectingPlayers ^= 1;

	// map finish filters
	if(ServerBrowser()->CommunityCache().AnyRanksAvailable())
	{
		View.HSplitTop(RowHeight, &Button, &View);
		if(DoButton_CheckBox(&g_Config.m_BrIndicateFinished, Localize("Indicate map finish"), g_Config.m_BrIndicateFinished, &Button))
		{
			g_Config.m_BrIndicateFinished ^= 1;
			if(g_Config.m_BrIndicateFinished)
				ServerBrowser()->Refresh(ServerBrowser()->GetCurrentType());
		}

		if(g_Config.m_BrIndicateFinished)
		{
			View.HSplitTop(RowHeight, &Button, &View);
			if(DoButton_CheckBox(&g_Config.m_BrFilterUnfinishedMap, Localize("Unfinished map"), g_Config.m_BrFilterUnfinishedMap, &Button))
				g_Config.m_BrFilterUnfinishedMap ^= 1;
		}
		else
		{
			g_Config.m_BrFilterUnfinishedMap = 0;
		}
	}

	// countries and types filters
	if(ServerBrowser()->CommunityCache().CountriesTypesFilterAvailable())
	{
		const ColorRGBA ColorActive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f);
		const ColorRGBA ColorInactive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f);

		CUIRect TabContents, CountriesTab, TypesTab;
		View.HSplitTop(6.0f, nullptr, &View);
		View.HSplitTop(19.0f, &Button, &View);
		View.HSplitTop(minimum(4.0f * 22.0f + CScrollRegion::HEIGHT_MAGIC_FIX, View.h), &TabContents, &View);
		Button.VSplitMid(&CountriesTab, &TypesTab);
		TabContents.Draw(ColorActive, IGraphics::CORNER_B, 4.0f);

		enum EFilterTab
		{
			FILTERTAB_COUNTRIES = 0,
			FILTERTAB_TYPES,
		};
		static EFilterTab s_ActiveTab = FILTERTAB_COUNTRIES;

		static CButtonContainer s_CountriesButton;
		if(DoButton_MenuTab(&s_CountriesButton, Localize("Countries"), s_ActiveTab == FILTERTAB_COUNTRIES, &CountriesTab, IGraphics::CORNER_TL, nullptr, &ColorInactive, &ColorActive, nullptr, 4.0f))
		{
			s_ActiveTab = FILTERTAB_COUNTRIES;
		}

		static CButtonContainer s_TypesButton;
		if(DoButton_MenuTab(&s_TypesButton, Localize("Types"), s_ActiveTab == FILTERTAB_TYPES, &TypesTab, IGraphics::CORNER_TR, nullptr, &ColorInactive, &ColorActive, nullptr, 4.0f))
		{
			s_ActiveTab = FILTERTAB_TYPES;
		}

		if(s_ActiveTab == FILTERTAB_COUNTRIES)
		{
			RenderServerbrowserCountriesFilter(TabContents);
		}
		else if(s_ActiveTab == FILTERTAB_TYPES)
		{
			RenderServerbrowserTypesFilter(TabContents);
		}
	}

	static CButtonContainer s_ResetButton;
	if(DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &ResetButton))
	{
		ResetServerbrowserFilters();
	}
}

void CMenus::ResetServerbrowserFilters()
{
	g_Config.m_BrFilterString[0] = '\0';
	g_Config.m_BrExcludeString[0] = '\0';
	g_Config.m_BrFilterFull = 0;
	g_Config.m_BrFilterEmpty = 0;
	g_Config.m_BrFilterSpectators = 0;
	g_Config.m_BrFilterFriends = 0;
	g_Config.m_BrFilterCountry = 0;
	g_Config.m_BrFilterCountryIndex = -1;
	g_Config.m_BrFilterPw = 0;
	g_Config.m_BrFilterGametype[0] = '\0';
	g_Config.m_BrFilterGametypeStrict = 0;
	g_Config.m_BrFilterConnectingPlayers = 1;
	g_Config.m_BrFilterServerAddress[0] = '\0';
	g_Config.m_BrFilterLogin = true;

	if(g_Config.m_UiPage != PAGE_LAN)
	{
		if(ServerBrowser()->CommunityCache().AnyRanksAvailable())
		{
			g_Config.m_BrFilterUnfinishedMap = 0;
		}
		if(g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_FAVORITES)
		{
			ServerBrowser()->CommunitiesFilter().Clear();
		}
		ServerBrowser()->CountriesFilter().Clear();
		ServerBrowser()->TypesFilter().Clear();
		UpdateCommunityCache(true);
	}

	Client()->ServerBrowserUpdate();
}

void CMenus::RenderServerbrowserDDNetFilter(CUIRect View,
	IFilterList &Filter,
	float ItemHeight, int MaxItems, int ItemsPerRow,
	CScrollRegion &ScrollRegion, std::vector<unsigned char> &vItemIds,
	bool UpdateCommunityCacheOnChange,
	const std::function<const char *(int ItemIndex)> &GetItemName,
	const std::function<void(int ItemIndex, CUIRect Item, const void *pItemId, bool Active)> &RenderItem)
{
	vItemIds.resize(MaxItems);

	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 10.0f;
	ScrollParams.m_ScrollbarMargin = 3.0f;
	ScrollParams.m_ScrollUnit = 2.0f * ItemHeight;
	ScrollRegion.Begin(&View, &ScrollOffset, &ScrollParams);
	View.y += ScrollOffset.y;

	CUIRect Row;
	int ColumnIndex = 0;
	for(int ItemIndex = 0; ItemIndex < MaxItems; ++ItemIndex)
	{
		CUIRect Item;
		if(ColumnIndex == 0)
			View.HSplitTop(ItemHeight, &Row, &View);
		Row.VSplitLeft(View.w / ItemsPerRow, &Item, &Row);
		ColumnIndex = (ColumnIndex + 1) % ItemsPerRow;
		if(!ScrollRegion.AddRect(Item))
			continue;

		const void *pItemId = &vItemIds[ItemIndex];
		const char *pName = GetItemName(ItemIndex);
		const bool Active = !Filter.Filtered(pName);

		const int Click = Ui()->DoButtonLogic(pItemId, 0, &Item);
		if(Click == 1 || Click == 2)
		{
			// left/right click to toggle filter
			if(Filter.Empty())
			{
				if(Click == 1)
				{
					// Left click: when all are active, only activate one and none
					for(int j = 0; j < MaxItems; ++j)
					{
						if(const char *pItemName = GetItemName(j);
							j != ItemIndex &&
							!((&Filter == &ServerBrowser()->CountriesFilter() && str_comp(pItemName, IServerBrowser::COMMUNITY_COUNTRY_NONE) == 0) ||
								(&Filter == &ServerBrowser()->TypesFilter() && str_comp(pItemName, IServerBrowser::COMMUNITY_TYPE_NONE) == 0)))
							Filter.Add(pItemName);
					}
				}
				else if(Click == 2)
				{
					// Right click: when all are active, only deactivate one
					if(MaxItems >= 2)
					{
						Filter.Add(GetItemName(ItemIndex));
					}
				}
			}
			else
			{
				bool AllFilteredExceptUs = true;
				for(int j = 0; j < MaxItems; ++j)
				{
					if(const char *pItemName = GetItemName(j);
						j != ItemIndex && !Filter.Filtered(pItemName) &&
						!((&Filter == &ServerBrowser()->CountriesFilter() && str_comp(pItemName, IServerBrowser::COMMUNITY_COUNTRY_NONE) == 0) ||
							(&Filter == &ServerBrowser()->TypesFilter() && str_comp(pItemName, IServerBrowser::COMMUNITY_TYPE_NONE) == 0)))
					{
						AllFilteredExceptUs = false;
						break;
					}
				}
				// When last one is removed, re-enable all currently selectable items.
				// Don't use Clear, to avoid enabling also currently unselectable items.
				if(AllFilteredExceptUs && Active)
				{
					for(int j = 0; j < MaxItems; ++j)
					{
						Filter.Remove(GetItemName(j));
					}
				}
				else if(Active)
				{
					Filter.Add(pName);
				}
				else
				{
					Filter.Remove(pName);
				}
			}

			Client()->ServerBrowserUpdate();
			if(UpdateCommunityCacheOnChange)
				UpdateCommunityCache(true);
		}
		else if(Click == 3)
		{
			// middle click to reset (re-enable all currently selectable items)
			for(int j = 0; j < MaxItems; ++j)
			{
				Filter.Remove(GetItemName(j));
			}
			Client()->ServerBrowserUpdate();
			if(UpdateCommunityCacheOnChange)
				UpdateCommunityCache(true);
		}

		if(Ui()->HotItem() == pItemId && !ScrollRegion.Animating())
			Item.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f), IGraphics::CORNER_ALL, 2.0f);
		RenderItem(ItemIndex, Item, pItemId, Active);
	}

	ScrollRegion.End();
}

void CMenus::RenderServerbrowserCommunitiesFilter(CUIRect View)
{
	CUIRect Tab;
	View.HSplitTop(19.0f, &Tab, &View);
	Tab.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_T, 4.0f);
	Ui()->DoLabel(&Tab, Localize("Communities"), 12.0f, TEXTALIGN_MC);
	View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 4.0f);

	const int MaxEntries = ServerBrowser()->Communities().size();
	const int EntriesPerRow = 1;

	static CScrollRegion s_ScrollRegion;
	static std::vector<unsigned char> s_vItemIds;
	static std::vector<unsigned char> s_vFavoriteButtonIds;

	const float ItemHeight = 13.0f;
	const float Spacing = 2.0f;

	const auto &&GetItemName = [&](int ItemIndex) {
		return ServerBrowser()->Communities()[ItemIndex].Id();
	};
	const auto &&GetItemDisplayName = [&](int ItemIndex) {
		return ServerBrowser()->Communities()[ItemIndex].Name();
	};
	const auto &&RenderItem = [&](int ItemIndex, CUIRect Item, const void *pItemId, bool Active) {
		const float Alpha = (Active ? 0.9f : 0.2f) + (Ui()->HotItem() == pItemId ? 0.1f : 0.0f);

		CUIRect Icon, Label, FavoriteButton;
		Item.VSplitRight(Item.h, &Item, &FavoriteButton);
		Item.Margin(Spacing, &Item);
		Item.VSplitLeft(Item.h * 2.0f, &Icon, &Label);
		Label.VSplitLeft(Spacing, nullptr, &Label);

		const char *pItemName = GetItemName(ItemIndex);
		const SCommunityIcon *pIcon = FindCommunityIcon(pItemName);
		if(pIcon != nullptr)
		{
			RenderCommunityIcon(pIcon, Icon, Active);
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha);
		Ui()->DoLabel(&Label, GetItemDisplayName(ItemIndex), Label.h * CUi::ms_FontmodHeight, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		const bool Favorite = ServerBrowser()->FavoriteCommunitiesFilter().Filtered(pItemName);
		if(DoButton_Favorite(&s_vFavoriteButtonIds[ItemIndex], pItemId, Favorite, &FavoriteButton))
		{
			if(Favorite)
			{
				ServerBrowser()->FavoriteCommunitiesFilter().Remove(pItemName);
			}
			else
			{
				ServerBrowser()->FavoriteCommunitiesFilter().Add(pItemName);
			}
		}
	};

	s_vFavoriteButtonIds.resize(MaxEntries);
	RenderServerbrowserDDNetFilter(View, ServerBrowser()->CommunitiesFilter(), ItemHeight + 2.0f * Spacing, MaxEntries, EntriesPerRow, s_ScrollRegion, s_vItemIds, true, GetItemName, RenderItem);
}

void CMenus::RenderServerbrowserCountriesFilter(CUIRect View)
{
	const int MaxEntries = ServerBrowser()->CommunityCache().SelectableCountries().size();
	const int EntriesPerRow = MaxEntries > 8 ? 5 : 4;

	static CScrollRegion s_ScrollRegion;
	static std::vector<unsigned char> s_vItemIds;

	const float ItemHeight = 18.0f;
	const float Spacing = 2.0f;

	const auto &&GetItemName = [&](int ItemIndex) {
		return ServerBrowser()->CommunityCache().SelectableCountries()[ItemIndex]->Name();
	};
	const auto &&RenderItem = [&](int ItemIndex, CUIRect Item, const void *pItemId, bool Active) {
		Item.Margin(Spacing, &Item);
		const float OldWidth = Item.w;
		Item.w = Item.h * 2.0f;
		Item.x += (OldWidth - Item.w) / 2.0f;
		m_pClient->m_CountryFlags.Render(ServerBrowser()->CommunityCache().SelectableCountries()[ItemIndex]->FlagId(), ColorRGBA(1.0f, 1.0f, 1.0f, (Active ? 0.9f : 0.2f) + (Ui()->HotItem() == pItemId ? 0.1f : 0.0f)), Item.x, Item.y, Item.w, Item.h);
	};

	RenderServerbrowserDDNetFilter(View, ServerBrowser()->CountriesFilter(), ItemHeight + 2.0f * Spacing, MaxEntries, EntriesPerRow, s_ScrollRegion, s_vItemIds, false, GetItemName, RenderItem);
}

void CMenus::RenderServerbrowserTypesFilter(CUIRect View)
{
	const int MaxEntries = ServerBrowser()->CommunityCache().SelectableTypes().size();
	const int EntriesPerRow = 3;

	static CScrollRegion s_ScrollRegion;
	static std::vector<unsigned char> s_vItemIds;

	const float ItemHeight = 13.0f;
	const float Spacing = 2.0f;

	const auto &&GetItemName = [&](int ItemIndex) {
		return ServerBrowser()->CommunityCache().SelectableTypes()[ItemIndex]->Name();
	};
	const auto &&RenderItem = [&](int ItemIndex, CUIRect Item, const void *pItemId, bool Active) {
		Item.Margin(Spacing, &Item);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, (Active ? 0.9f : 0.2f) + (Ui()->HotItem() == pItemId ? 0.1f : 0.0f));
		Ui()->DoLabel(&Item, GetItemName(ItemIndex), Item.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};

	RenderServerbrowserDDNetFilter(View, ServerBrowser()->TypesFilter(), ItemHeight + 2.0f * Spacing, MaxEntries, EntriesPerRow, s_ScrollRegion, s_vItemIds, false, GetItemName, RenderItem);
}

CUi::EPopupMenuFunctionResult CMenus::PopupCountrySelection(void *pContext, CUIRect View, bool Active)
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

		pMenus->Ui()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, TEXTALIGN_MC);
	}

	const int NewSelected = s_ListBox.DoEnd();
	pPopupContext->m_Selection = NewSelected >= 0 ? pMenus->m_pClient->m_CountryFlags.GetByIndex(NewSelected)->m_CountryCode : -1;
	if(s_ListBox.WasItemSelected() || s_ListBox.WasItemActivated())
	{
		g_Config.m_BrFilterCountry = 1;
		g_Config.m_BrFilterCountryIndex = pPopupContext->m_Selection;
		pMenus->Client()->ServerBrowserUpdate();
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CMenus::RenderServerbrowserInfo(CUIRect View)
{
	const CServerInfo *pSelectedServer = ServerBrowser()->SortedGet(m_SelectedIndex);

	const float RowHeight = 18.0f;
	const float FontSize = (RowHeight - 4.0f) * CUi::ms_FontmodHeight; // based on DoButton_CheckBox

	CUIRect ServerDetails, Scoreboard;
	View.HSplitTop(4.0f * 15.0f + RowHeight + 2.0f * 5.0f + 2.0f * 2.0f, &ServerDetails, &Scoreboard);

	if(pSelectedServer)
	{
		ServerDetails.Margin(5.0f, &ServerDetails);

		// copy info button
		{
			CUIRect Button;
			ServerDetails.HSplitBottom(15.0f, &ServerDetails, &Button);
			static CButtonContainer s_CopyButton;
			if(DoButton_Menu(&s_CopyButton, Localize("Copy info"), 0, &Button))
			{
				char aInfo[256];
				str_format(
					aInfo,
					sizeof(aInfo),
					"%s\n"
					"Address: ddnet://%s\n",
					pSelectedServer->m_aName,
					pSelectedServer->m_aAddress);
				Input()->SetClipboardText(aInfo);
			}
		}

		// favorite checkbox
		{
			CUIRect ButtonAddFav, ButtonLeakIp;
			ServerDetails.HSplitBottom(2.0f, &ServerDetails, nullptr);
			ServerDetails.HSplitBottom(RowHeight, &ServerDetails, &ButtonAddFav);
			ServerDetails.HSplitBottom(2.0f, &ServerDetails, nullptr);
			ButtonAddFav.VSplitMid(&ButtonAddFav, &ButtonLeakIp);
			static int s_AddFavButton = 0;
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
				static int s_LeakIpButton = 0;
				if(DoButton_CheckBox_Tristate(&s_LeakIpButton, Localize("Leak IP"), pSelectedServer->m_FavoriteAllowPing, &ButtonLeakIp))
				{
					Favorites()->AllowPing(pSelectedServer->m_aAddresses, pSelectedServer->m_NumAddresses, pSelectedServer->m_FavoriteAllowPing == TRISTATE::NONE);
					Client()->ServerBrowserUpdate();
				}
			}
		}

		CUIRect LeftColumn, RightColumn, Row;
		ServerDetails.VSplitLeft(80.0f, &LeftColumn, &RightColumn);

		LeftColumn.HSplitTop(15.0f, &Row, &LeftColumn);
		Ui()->DoLabel(&Row, Localize("Version"), FontSize, TEXTALIGN_ML);

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		Ui()->DoLabel(&Row, pSelectedServer->m_aVersion, FontSize, TEXTALIGN_ML);

		LeftColumn.HSplitTop(15.0f, &Row, &LeftColumn);
		Ui()->DoLabel(&Row, Localize("Game type"), FontSize, TEXTALIGN_ML);

		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		Ui()->DoLabel(&Row, pSelectedServer->m_aGameType, FontSize, TEXTALIGN_ML);

		LeftColumn.HSplitTop(15.0f, &Row, &LeftColumn);
		Ui()->DoLabel(&Row, Localize("Ping"), FontSize, TEXTALIGN_ML);

		if(g_Config.m_UiColorizePing)
			TextRender()->TextColor(GetPingTextColor(pSelectedServer->m_Latency));
		char aTemp[16];
		FormatServerbrowserPing(aTemp, pSelectedServer);
		RightColumn.HSplitTop(15.0f, &Row, &RightColumn);
		Ui()->DoLabel(&Row, aTemp, FontSize, TEXTALIGN_ML);
		if(g_Config.m_UiColorizePing)
			TextRender()->TextColor(TextRender()->DefaultTextColor());

		RenderServerbrowserInfoScoreboard(Scoreboard, pSelectedServer);
	}
	else
	{
		Ui()->DoLabel(&ServerDetails, Localize("No server selected"), FontSize, TEXTALIGN_MC);
	}
}

void CMenus::RenderServerbrowserInfoScoreboard(CUIRect View, const CServerInfo *pSelectedServer)
{
	const float FontSize = 10.0f;

	static CListBox s_ListBox;
	View.VSplitLeft(5.0f, nullptr, &View);
	s_ListBox.DoAutoSpacing(2.0f);
	s_ListBox.SetScrollbarWidth(16.0f);
	s_ListBox.SetScrollbarMargin(5.0f);
	s_ListBox.DoStart(25.0f, pSelectedServer->m_NumReceivedClients, 1, 3, -1, &View, false, IGraphics::CORNER_NONE, true);

	for(int i = 0; i < pSelectedServer->m_NumReceivedClients; i++)
	{
		const CServerInfo::CClient &CurrentClient = pSelectedServer->m_aClients[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&CurrentClient);
		if(!Item.m_Visible)
			continue;

		CUIRect Skin, Name, Clan, Score, Flag;
		Name = Item.m_Rect;

		const ColorRGBA Color = PlayerBackgroundColor(CurrentClient.m_FriendState == IFriends::FRIEND_PLAYER, CurrentClient.m_FriendState == IFriends::FRIEND_CLAN, CurrentClient.m_Afk, false);
		Name.Draw(Color, IGraphics::CORNER_ALL, 4.0f);
		Name.VSplitLeft(1.0f, nullptr, &Name);
		Name.VSplitLeft(34.0f, &Score, &Name);
		Name.VSplitLeft(18.0f, &Skin, &Name);
		Name.VSplitRight(26.0f, &Name, &Flag);
		Flag.HMargin(6.0f, &Flag);
		Name.HSplitTop(12.0f, &Name, &Clan);

		// score
		char aTemp[16];
		if(!CurrentClient.m_Player)
		{
			str_copy(aTemp, "SPEC");
		}
		else if(pSelectedServer->m_ClientScoreKind == CServerInfo::CLIENT_SCORE_KIND_POINTS)
		{
			str_format(aTemp, sizeof(aTemp), "%d", CurrentClient.m_Score);
		}
		else
		{
			std::optional<int> Time = {};

			if(pSelectedServer->m_ClientScoreKind == CServerInfo::CLIENT_SCORE_KIND_TIME_BACKCOMPAT)
			{
				const int TempTime = absolute(CurrentClient.m_Score);
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
				aTemp[0] = '\0';
			}
		}

		Ui()->DoLabel(&Score, aTemp, FontSize, TEXTALIGN_ML);

		// render tee if available
		if(CurrentClient.m_aSkin[0] != '\0')
		{
			const CTeeRenderInfo TeeInfo = GetTeeRenderInfo(vec2(Skin.w, Skin.h), CurrentClient.m_aSkin, CurrentClient.m_CustomSkinColors, CurrentClient.m_CustomSkinColorBody, CurrentClient.m_CustomSkinColorFeet);
			const CAnimState *pIdleState = CAnimState::GetIdle();
			vec2 OffsetToMid;
			CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
			const vec2 TeeRenderPos = vec2(Skin.x + TeeInfo.m_Size / 2.0f, Skin.y + Skin.h / 2.0f + OffsetToMid.y);
			RenderTools()->RenderTee(pIdleState, &TeeInfo, CurrentClient.m_Afk ? EMOTE_BLINK : EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
			Ui()->DoButtonLogic(&CurrentClient.m_aSkin, 0, &Skin);
			GameClient()->m_Tooltips.DoToolTip(&CurrentClient.m_aSkin, &Skin, CurrentClient.m_aSkin);
		}

		// name
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, Name.x, Name.y + (Name.h - (FontSize - 1.0f)) / 2.0f, FontSize - 1.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Name.w;
		const char *pName = CurrentClient.m_aName;
		bool Printed = false;
		if(g_Config.m_BrFilterString[0])
			Printed = PrintHighlighted(pName, [&](const char *pFilteredStr, const int FilterLen) {
				TextRender()->TextEx(&Cursor, pName, (int)(pFilteredStr - pName));
				TextRender()->TextColor(gs_HighlightedTextColor);
				TextRender()->TextEx(&Cursor, pFilteredStr, FilterLen);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->TextEx(&Cursor, pFilteredStr + FilterLen, -1);
			});
		if(!Printed)
			TextRender()->TextEx(&Cursor, pName, -1);

		// clan
		TextRender()->SetCursor(&Cursor, Clan.x, Clan.y + (Clan.h - (FontSize - 2.0f)) / 2.0f, FontSize - 2.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Clan.w;
		const char *pClan = CurrentClient.m_aClan;
		Printed = false;
		if(g_Config.m_BrFilterString[0])
			Printed = PrintHighlighted(pClan, [&](const char *pFilteredStr, const int FilterLen) {
				TextRender()->TextEx(&Cursor, pClan, (int)(pFilteredStr - pClan));
				TextRender()->TextColor(0.4f, 0.4f, 1.0f, 1.0f);
				TextRender()->TextEx(&Cursor, pFilteredStr, FilterLen);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->TextEx(&Cursor, pFilteredStr + FilterLen, -1);
			});
		if(!Printed)
			TextRender()->TextEx(&Cursor, pClan, -1);

		// flag
		m_pClient->m_CountryFlags.Render(CurrentClient.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f), Flag.x, Flag.y, Flag.w, Flag.h);
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

void CMenus::RenderServerbrowserFriends(CUIRect View)
{
	const float FontSize = 10.0f;
	static bool s_aListExtended[NUM_FRIEND_TYPES] = {true, true, false};
	const float SpacingH = 2.0f;

	CUIRect List, ServerFriends;
	View.HSplitBottom(70.0f, &List, &ServerFriends);
	List.HSplitTop(5.0f, nullptr, &List);
	List.VSplitLeft(5.0f, nullptr, &List);

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
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollbarWidth = 16.0f;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	ScrollParams.m_ScrollUnit = 80.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	s_ScrollRegion.Begin(&List, &ScrollOffset, &ScrollParams);
	List.y += ScrollOffset.y;

	char aBuf[256];
	for(size_t FriendType = 0; FriendType < NUM_FRIEND_TYPES; ++FriendType)
	{
		// header
		CUIRect Header, GroupIcon, GroupLabel;
		List.HSplitTop(ms_ListheaderHeight, &Header, &List);
		s_ScrollRegion.AddRect(Header);
		Header.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Ui()->HotItem() == &s_aListExtended[FriendType] ? 0.4f : 0.25f), IGraphics::CORNER_ALL, 5.0f);
		Header.VSplitLeft(Header.h, &GroupIcon, &GroupLabel);
		GroupIcon.Margin(2.0f, &GroupIcon);
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->TextColor(Ui()->HotItem() == &s_aListExtended[FriendType] ? TextRender()->DefaultTextColor() : ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f));
		Ui()->DoLabel(&GroupIcon, s_aListExtended[FriendType] ? FONT_ICON_SQUARE_MINUS : FONT_ICON_SQUARE_PLUS, GroupIcon.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		switch(FriendType)
		{
		case FRIEND_PLAYER_ON:
			str_format(aBuf, sizeof(aBuf), Localize("Online friends (%d)"), (int)m_avFriends[FriendType].size());
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
		Ui()->DoLabel(&GroupLabel, aBuf, FontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_aListExtended[FriendType], 0, &Header))
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
				if(s_ScrollRegion.RectClipped(Rect))
					continue;

				const bool Inside = Ui()->HotItem() == Friend.ListItemId() || Ui()->HotItem() == Friend.RemoveButtonId() || Ui()->HotItem() == Friend.CommunityTooltipId() || Ui()->HotItem() == Friend.SkinTooltipId();
				int ButtonResult = Ui()->DoButtonLogic(Friend.ListItemId(), 0, &Rect);

				if(Friend.ServerInfo())
				{
					GameClient()->m_Tooltips.DoToolTip(Friend.ListItemId(), &Rect, Localize("Click to select server. Double click to join your friend."));
				}
				const ColorRGBA Color = PlayerBackgroundColor(FriendType == FRIEND_PLAYER_ON, FriendType == FRIEND_CLAN_ON, FriendType == FRIEND_OFF ? true : Friend.IsAfk(), Inside);
				Rect.Draw(Color, IGraphics::CORNER_ALL, 5.0f);
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
				CUIRect Skin;
				Rect.VSplitLeft(Rect.h, &Skin, &Rect);
				Rect.VSplitLeft(2.0f, nullptr, &Rect);
				if(Friend.Skin()[0] != '\0')
				{
					const CTeeRenderInfo TeeInfo = GetTeeRenderInfo(vec2(Skin.w, Skin.h), Friend.Skin(), Friend.CustomSkinColors(), Friend.CustomSkinColorBody(), Friend.CustomSkinColorFeet());
					const CAnimState *pIdleState = CAnimState::GetIdle();
					vec2 OffsetToMid;
					CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
					const vec2 TeeRenderPos = vec2(Skin.x + Skin.w / 2.0f, Skin.y + Skin.h * 0.55f + OffsetToMid.y);
					RenderTools()->RenderTee(pIdleState, &TeeInfo, Friend.IsAfk() ? EMOTE_BLINK : EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
					Ui()->DoButtonLogic(Friend.SkinTooltipId(), 0, &Skin);
					GameClient()->m_Tooltips.DoToolTip(Friend.SkinTooltipId(), &Skin, Friend.Skin());
				}
				Rect.HSplitTop(11.0f, &NameLabel, &ClanLabel);

				// name
				Ui()->DoLabel(&NameLabel, Friend.Name(), FontSize - 1.0f, TEXTALIGN_ML);

				// clan
				Ui()->DoLabel(&ClanLabel, Friend.Clan(), FontSize - 2.0f, TEXTALIGN_ML);

				// server info
				if(Friend.ServerInfo())
				{
					// community icon
					const CCommunity *pCommunity = ServerBrowser()->Community(Friend.ServerInfo()->m_aCommunityId);
					if(pCommunity != nullptr)
					{
						const SCommunityIcon *pIcon = FindCommunityIcon(pCommunity->Id());
						if(pIcon != nullptr)
						{
							CUIRect CommunityIcon;
							InfoLabel.VSplitLeft(21.0f, &CommunityIcon, &InfoLabel);
							InfoLabel.VSplitLeft(2.0f, nullptr, &InfoLabel);
							RenderCommunityIcon(pIcon, CommunityIcon, true);
							Ui()->DoButtonLogic(Friend.CommunityTooltipId(), 0, &CommunityIcon);
							GameClient()->m_Tooltips.DoToolTip(Friend.CommunityTooltipId(), &CommunityIcon, pCommunity->Name());
						}
					}

					// server info text
					char aLatency[16];
					FormatServerbrowserPing(aLatency, Friend.ServerInfo());
					if(aLatency[0] != '\0')
						str_format(aBuf, sizeof(aBuf), "%s | %s | %s", Friend.ServerInfo()->m_aMap, Friend.ServerInfo()->m_aGameType, aLatency);
					else
						str_format(aBuf, sizeof(aBuf), "%s | %s", Friend.ServerInfo()->m_aMap, Friend.ServerInfo()->m_aGameType);
					Ui()->DoLabel(&InfoLabel, aBuf, FontSize - 2.0f, TEXTALIGN_ML);
				}

				// remove button
				if(Inside)
				{
					TextRender()->TextColor(Ui()->HotItem() == Friend.RemoveButtonId() ? TextRender()->DefaultTextColor() : ColorRGBA(0.4f, 0.4f, 0.4f, 1.0f));
					TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
					TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
					Ui()->DoLabel(&RemoveButton, FONT_ICON_TRASH, RemoveButton.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
					TextRender()->SetRenderFlags(0);
					TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					if(Ui()->DoButtonLogic(Friend.RemoveButtonId(), 0, &RemoveButton))
					{
						m_pRemoveFriend = &Friend;
						ButtonResult = 0;
					}
					GameClient()->m_Tooltips.DoToolTip(Friend.RemoveButtonId(), &RemoveButton, Friend.FriendState() == IFriends::FRIEND_PLAYER ? Localize("Click to remove this player from your friends list.") : Localize("Click to remove this clan from your friends list."));
				}

				// handle click and double click on item
				if(ButtonResult && Friend.ServerInfo())
				{
					str_copy(g_Config.m_UiServerAddress, Friend.ServerInfo()->m_aAddress);
					m_ServerBrowserShouldRevealSelection = true;
					if(ButtonResult == 1 && Ui()->DoDoubleClickLogic(Friend.ListItemId()))
					{
						Connect(g_Config.m_UiServerAddress);
					}
				}
			}

			// Render empty description
			if(m_avFriends[FriendType].empty())
			{
				const char *pText;
				switch(FriendType)
				{
				case FRIEND_PLAYER_ON:
					pText = Localize("Add friends by entering their name below or by clicking their name in the player list.");
					break;
				case FRIEND_CLAN_ON:
					pText = Localize("Add clanmates by entering their clan below and leaving the name blank.");
					break;
				case FRIEND_OFF:
					pText = Localize("Offline friends and clanmates will appear here.");
					break;
				default:
					dbg_assert(false, "Invalid friend state");
					dbg_break();
				}
				const float DescriptionMargin = 2.0f;
				const STextBoundingBox BoundingBox = TextRender()->TextBoundingBox(FontSize, pText, -1, List.w - 2 * DescriptionMargin);
				CUIRect EmptyDescription;
				List.HSplitTop(BoundingBox.m_H + 2 * DescriptionMargin, &EmptyDescription, &List);
				s_ScrollRegion.AddRect(EmptyDescription);
				EmptyDescription.Margin(DescriptionMargin, &EmptyDescription);
				SLabelProperties DescriptionProps;
				DescriptionProps.m_MaxWidth = EmptyDescription.w;
				Ui()->DoLabel(&EmptyDescription, pText, FontSize, TEXTALIGN_ML, DescriptionProps);
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
		ServerFriends.Margin(5.0f, &ServerFriends);

		ServerFriends.HSplitTop(18.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
		Ui()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
		Button.VSplitLeft(80.0f, nullptr, &Button);
		static CLineInputBuffered<MAX_NAME_LENGTH> s_NameInput;
		Ui()->DoEditBox(&s_NameInput, &Button, FontSize + 2.0f);

		ServerFriends.HSplitTop(3.0f, nullptr, &ServerFriends);
		ServerFriends.HSplitTop(18.0f, &Button, &ServerFriends);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
		Ui()->DoLabel(&Button, aBuf, FontSize + 2.0f, TEXTALIGN_ML);
		Button.VSplitLeft(80.0f, nullptr, &Button);
		static CLineInputBuffered<MAX_CLAN_LENGTH> s_ClanInput;
		Ui()->DoEditBox(&s_ClanInput, &Button, FontSize + 2.0f);

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

void CMenus::FriendlistOnUpdate()
{
	// TODO: friends are currently updated every frame; optimize and only update friends when necessary
}

void CMenus::PopupConfirmRemoveFriend()
{
	m_pClient->Friends()->RemoveFriend(m_pRemoveFriend->FriendState() == IFriends::FRIEND_PLAYER ? m_pRemoveFriend->Name() : "", m_pRemoveFriend->Clan());
	FriendlistOnUpdate();
	Client()->ServerBrowserUpdate();
	m_pRemoveFriend = nullptr;
}

enum
{
	UI_TOOLBOX_PAGE_FILTERS = 0,
	UI_TOOLBOX_PAGE_INFO,
	UI_TOOLBOX_PAGE_FRIENDS,
	NUM_UI_TOOLBOX_PAGES,
};

void CMenus::RenderServerbrowserTabBar(CUIRect TabBar)
{
	CUIRect FilterTabButton, InfoTabButton, FriendsTabButton;
	TabBar.VSplitLeft(TabBar.w / 3.0f, &FilterTabButton, &TabBar);
	TabBar.VSplitMid(&InfoTabButton, &FriendsTabButton);

	const ColorRGBA ColorActive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f);
	const ColorRGBA ColorInactive = ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f);

	if(!Ui()->IsPopupOpen() && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))
	{
		const int Direction = Input()->ShiftIsPressed() ? -1 : 1;
		g_Config.m_UiToolboxPage = (g_Config.m_UiToolboxPage + NUM_UI_TOOLBOX_PAGES + Direction) % NUM_UI_TOOLBOX_PAGES;
	}

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	static CButtonContainer s_FilterTabButton;
	if(DoButton_MenuTab(&s_FilterTabButton, FONT_ICON_LIST_UL, g_Config.m_UiToolboxPage == UI_TOOLBOX_PAGE_FILTERS, &FilterTabButton, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_BROWSER_FILTER], &ColorInactive, &ColorActive))
	{
		g_Config.m_UiToolboxPage = UI_TOOLBOX_PAGE_FILTERS;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_FilterTabButton, &FilterTabButton, Localize("Server filter"));

	static CButtonContainer s_InfoTabButton;
	if(DoButton_MenuTab(&s_InfoTabButton, FONT_ICON_INFO, g_Config.m_UiToolboxPage == UI_TOOLBOX_PAGE_INFO, &InfoTabButton, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_BROWSER_INFO], &ColorInactive, &ColorActive))
	{
		g_Config.m_UiToolboxPage = UI_TOOLBOX_PAGE_INFO;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_InfoTabButton, &InfoTabButton, Localize("Server info"));

	static CButtonContainer s_FriendsTabButton;
	if(DoButton_MenuTab(&s_FriendsTabButton, FONT_ICON_HEART, g_Config.m_UiToolboxPage == UI_TOOLBOX_PAGE_FRIENDS, &FriendsTabButton, IGraphics::CORNER_T, &m_aAnimatorsSmallPage[SMALL_TAB_BROWSER_FRIENDS], &ColorInactive, &ColorActive))
	{
		g_Config.m_UiToolboxPage = UI_TOOLBOX_PAGE_FRIENDS;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_FriendsTabButton, &FriendsTabButton, Localize("Friends"));

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

void CMenus::RenderServerbrowserToolBox(CUIRect ToolBox)
{
	ToolBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_B, 4.0f);

	switch(g_Config.m_UiToolboxPage)
	{
	case UI_TOOLBOX_PAGE_FILTERS:
		RenderServerbrowserFilters(ToolBox);
		return;
	case UI_TOOLBOX_PAGE_INFO:
		RenderServerbrowserInfo(ToolBox);
		return;
	case UI_TOOLBOX_PAGE_FRIENDS:
		RenderServerbrowserFriends(ToolBox);
		return;
	default:
		dbg_assert(false, "ui_toolbox_page invalid");
		return;
	}
}

void CMenus::RenderServerbrowser(CUIRect MainView)
{
	UpdateCommunityCache(false);

	switch(g_Config.m_UiPage)
	{
	case PAGE_INTERNET:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_BROWSER_INTERNET);
		break;
	case PAGE_LAN:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_BROWSER_LAN);
		break;
	case PAGE_FAVORITES:
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_BROWSER_FAVORITES);
		break;
	case PAGE_FAVORITE_COMMUNITY_1:
	case PAGE_FAVORITE_COMMUNITY_2:
	case PAGE_FAVORITE_COMMUNITY_3:
	case PAGE_FAVORITE_COMMUNITY_4:
	case PAGE_FAVORITE_COMMUNITY_5:
		GameClient()->m_MenuBackground.ChangePosition(g_Config.m_UiPage - PAGE_FAVORITE_COMMUNITY_1 + CMenuBackground::POS_BROWSER_CUSTOM0);
		break;
	default:
		dbg_assert(false, "ui_page invalid for RenderServerbrowser");
	}

	/*
		+---------------------------+ +---communities---+
		|                           | |                 |
		|                           | +------tabs-------+
		|       server list         | |                 |
		|                           | |      tool       |
		|                           | |      box        |
		+---------------------------+ |                 |
		        status box            +-----------------+
	*/

	CUIRect ServerList, StatusBox, ToolBox, TabBar;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.VSplitRight(205.0f, &ServerList, &ToolBox);
	ServerList.VSplitRight(5.0f, &ServerList, nullptr);

	if((g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_FAVORITES) && !ServerBrowser()->Communities().empty())
	{
		CUIRect CommunityFilter;
		ToolBox.HSplitTop(19.0f + 4.0f * 17.0f + CScrollRegion::HEIGHT_MAGIC_FIX, &CommunityFilter, &ToolBox);
		ToolBox.HSplitTop(8.0f, nullptr, &ToolBox);
		RenderServerbrowserCommunitiesFilter(CommunityFilter);
	}

	ToolBox.HSplitTop(24.0f, &TabBar, &ToolBox);
	ServerList.HSplitBottom(65.0f, &ServerList, &StatusBox);

	bool WasListboxItemActivated;
	RenderServerbrowserServerList(ServerList, WasListboxItemActivated);
	RenderServerbrowserStatusBox(StatusBox, WasListboxItemActivated);

	RenderServerbrowserTabBar(TabBar);
	RenderServerbrowserToolBox(ToolBox);
}

template<typename F>
bool CMenus::PrintHighlighted(const char *pName, F &&PrintFn)
{
	const char *pStr = g_Config.m_BrFilterString;
	char aFilterStr[sizeof(g_Config.m_BrFilterString)];
	char aFilterStrTrimmed[sizeof(g_Config.m_BrFilterString)];
	while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
	{
		str_copy(aFilterStrTrimmed, str_utf8_skip_whitespaces(aFilterStr));
		str_utf8_trim_right(aFilterStrTrimmed);
		// highlight the parts that matches
		const char *pFilteredStr;
		int FilterLen = str_length(aFilterStrTrimmed);
		if(aFilterStrTrimmed[0] == '"' && aFilterStrTrimmed[FilterLen - 1] == '"')
		{
			aFilterStrTrimmed[FilterLen - 1] = '\0';
			pFilteredStr = str_comp(pName, &aFilterStrTrimmed[1]) == 0 ? pName : nullptr;
			FilterLen -= 2;
		}
		else
		{
			const char *pFilteredStrEnd;
			pFilteredStr = str_utf8_find_nocase(pName, aFilterStrTrimmed, &pFilteredStrEnd);
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

CTeeRenderInfo CMenus::GetTeeRenderInfo(vec2 Size, const char *pSkinName, bool CustomSkinColors, int CustomSkinColorBody, int CustomSkinColorFeet) const
{
	CTeeRenderInfo TeeInfo;
	TeeInfo.Apply(m_pClient->m_Skins.Find(pSkinName));
	TeeInfo.ApplyColors(CustomSkinColors, CustomSkinColorBody, CustomSkinColorFeet);
	TeeInfo.m_Size = minimum(Size.x, Size.y);
	return TeeInfo;
}

void CMenus::ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = ((CMenus *)pUserData);
	if(pResult->NumArguments() >= 1 && (pThis->Client()->State() == IClient::STATE_OFFLINE || pThis->Client()->State() == IClient::STATE_ONLINE))
	{
		pThis->FriendlistOnUpdate();
		pThis->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainFavoritesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1 && g_Config.m_UiPage == PAGE_FAVORITES)
		((CMenus *)pUserData)->ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
}

void CMenus::ConchainCommunitiesUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	if(pResult->NumArguments() >= 1 && (g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_FAVORITES || (g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5)))
	{
		pThis->UpdateCommunityCache(true);
		pThis->Client()->ServerBrowserUpdate();
	}
}

void CMenus::ConchainUiPageUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CMenus *pThis = static_cast<CMenus *>(pUserData);
	if(pResult->NumArguments() >= 1)
	{
		if(g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5 &&
			(size_t)(g_Config.m_UiPage - PAGE_FAVORITE_COMMUNITY_1) >= pThis->ServerBrowser()->FavoriteCommunities().size())
		{
			// Reset page to internet when there is no favorite community for this page.
			g_Config.m_UiPage = PAGE_INTERNET;
		}

		pThis->SetMenuPage(g_Config.m_UiPage);
	}
}

void CMenus::UpdateCommunityCache(bool Force)
{
	if(g_Config.m_UiPage >= PAGE_FAVORITE_COMMUNITY_1 && g_Config.m_UiPage <= PAGE_FAVORITE_COMMUNITY_5 &&
		(size_t)(g_Config.m_UiPage - PAGE_FAVORITE_COMMUNITY_1) >= ServerBrowser()->FavoriteCommunities().size())
	{
		// Reset page to internet when there is no favorite community for this page,
		// i.e. when favorite community is removed via console while the page is open.
		// This also updates the community cache because the page is changed.
		SetMenuPage(PAGE_INTERNET);
	}
	else
	{
		ServerBrowser()->CommunityCache().Update(Force);
	}
}

CMenus::CAbstractCommunityIconJob::CAbstractCommunityIconJob(CMenus *pMenus, const char *pCommunityId, int StorageType) :
	m_pMenus(pMenus),
	m_StorageType(StorageType)
{
	str_copy(m_aCommunityId, pCommunityId);
	str_format(m_aPath, sizeof(m_aPath), "communityicons/%s.png", pCommunityId);
}

CMenus::CCommunityIconDownloadJob::CCommunityIconDownloadJob(CMenus *pMenus, const char *pCommunityId, const char *pUrl, const SHA256_DIGEST &Sha256) :
	CHttpRequest(pUrl),
	CAbstractCommunityIconJob(pMenus, pCommunityId, IStorage::TYPE_SAVE)
{
	WriteToFile(pMenus->Storage(), m_aPath, IStorage::TYPE_SAVE);
	ExpectSha256(Sha256);
	Timeout(CTimeout{0, 0, 0, 0});
	LogProgress(HTTPLOG::FAILURE);
}

void CMenus::CCommunityIconLoadJob::Run()
{
	m_Success = m_pMenus->LoadCommunityIconFile(m_aPath, m_StorageType, m_ImageInfo, m_Sha256);
}

CMenus::CCommunityIconLoadJob::CCommunityIconLoadJob(CMenus *pMenus, const char *pCommunityId, int StorageType) :
	CAbstractCommunityIconJob(pMenus, pCommunityId, StorageType)
{
	Abortable(true);
}

CMenus::CCommunityIconLoadJob::~CCommunityIconLoadJob()
{
	m_ImageInfo.Free();
}

int CMenus::CommunityIconScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	const char *pExtension = ".png";
	CMenus *pSelf = static_cast<CMenus *>(pUser);
	if(IsDir || !str_endswith(pName, pExtension) || str_length(pName) - str_length(pExtension) >= (int)CServerInfo::MAX_COMMUNITY_ID_LENGTH)
		return 0;

	char aCommunityId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	str_truncate(aCommunityId, sizeof(aCommunityId), pName, str_length(pName) - str_length(pExtension));

	std::shared_ptr<CCommunityIconLoadJob> pJob = std::make_shared<CCommunityIconLoadJob>(pSelf, aCommunityId, DirType);
	pSelf->Engine()->AddJob(pJob);
	pSelf->m_CommunityIconLoadJobs.push_back(pJob);
	return 0;
}

const SCommunityIcon *CMenus::FindCommunityIcon(const char *pCommunityId)
{
	auto Icon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [pCommunityId](const SCommunityIcon &Element) {
		return str_comp(Element.m_aCommunityId, pCommunityId) == 0;
	});
	return Icon == m_vCommunityIcons.end() ? nullptr : &(*Icon);
}

bool CMenus::LoadCommunityIconFile(const char *pPath, int DirType, CImageInfo &Info, SHA256_DIGEST &Sha256)
{
	char aError[IO_MAX_PATH_LENGTH + 128];
	if(!Graphics()->LoadPng(Info, pPath, DirType))
	{
		str_format(aError, sizeof(aError), "Failed to load community icon from '%s'", pPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "menus/browser", aError);
		return false;
	}
	if(Info.m_Format != CImageInfo::FORMAT_RGBA)
	{
		Info.Free();
		str_format(aError, sizeof(aError), "Failed to load community icon from '%s': must be an RGBA image", pPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "menus/browser", aError);
		return false;
	}
	if(!Storage()->CalculateHashes(pPath, DirType, &Sha256))
	{
		Info.Free();
		str_format(aError, sizeof(aError), "Failed to load community icon from '%s': could not calculate hash", pPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "menus/browser", aError);
		return false;
	}
	return true;
}

void CMenus::LoadCommunityIconFinish(const char *pCommunityId, CImageInfo &Info, const SHA256_DIGEST &Sha256)
{
	SCommunityIcon CommunityIcon;
	str_copy(CommunityIcon.m_aCommunityId, pCommunityId);
	CommunityIcon.m_Sha256 = Sha256;
	CommunityIcon.m_OrgTexture = Graphics()->LoadTextureRaw(Info, 0, pCommunityId);

	ConvertToGrayscale(Info);
	CommunityIcon.m_GreyTexture = Graphics()->LoadTextureRawMove(Info, 0, pCommunityId);

	auto ExistingIcon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [pCommunityId](const SCommunityIcon &Element) {
		return str_comp(Element.m_aCommunityId, pCommunityId) == 0;
	});
	if(ExistingIcon == m_vCommunityIcons.end())
	{
		m_vCommunityIcons.push_back(CommunityIcon);
	}
	else
	{
		Graphics()->UnloadTexture(&ExistingIcon->m_OrgTexture);
		Graphics()->UnloadTexture(&ExistingIcon->m_GreyTexture);
		*ExistingIcon = CommunityIcon;
	}

	char aBuf[CServerInfo::MAX_COMMUNITY_ID_LENGTH + 32];
	str_format(aBuf, sizeof(aBuf), "Loaded community icon '%s'", pCommunityId);
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "menus/browser", aBuf);
}

void CMenus::RenderCommunityIcon(const SCommunityIcon *pIcon, CUIRect Rect, bool Active)
{
	Rect.VMargin(Rect.w / 2.0f - Rect.h, &Rect);

	Graphics()->TextureSet(Active ? pIcon->m_OrgTexture : pIcon->m_GreyTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Active ? 1.0f : 0.5f);
	IGraphics::CQuadItem QuadItem(Rect.x, Rect.y, Rect.w, Rect.h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CMenus::UpdateCommunityIcons()
{
	// Update load jobs (icon is loaded from existing file)
	if(!m_CommunityIconLoadJobs.empty())
	{
		std::shared_ptr<CCommunityIconLoadJob> pJob = m_CommunityIconLoadJobs.front();
		if(pJob->Done())
		{
			if(pJob->Success())
				LoadCommunityIconFinish(pJob->CommunityId(), pJob->ImageInfo(), pJob->Sha256());
			m_CommunityIconLoadJobs.pop_front();
		}

		// Don't start download jobs until all load jobs are done
		if(!m_CommunityIconLoadJobs.empty())
			return;
	}

	// Update download jobs (icon is downloaded and loaded from new file)
	if(!m_CommunityIconDownloadJobs.empty())
	{
		std::shared_ptr<CCommunityIconDownloadJob> pJob = m_CommunityIconDownloadJobs.front();
		if(pJob->Done())
		{
			if(pJob->State() == EHttpState::DONE)
			{
				std::shared_ptr<CCommunityIconLoadJob> pLoadJob = std::make_shared<CCommunityIconLoadJob>(this, pJob->CommunityId(), IStorage::TYPE_SAVE);
				Engine()->AddJob(pLoadJob);
				m_CommunityIconLoadJobs.push_back(pLoadJob);
			}
			m_CommunityIconDownloadJobs.pop_front();
		}
	}

	// Rescan for changed communities only when necessary
	if(!ServerBrowser()->DDNetInfoAvailable() || (m_CommunityIconsInfoSha256 != SHA256_ZEROED && m_CommunityIconsInfoSha256 == ServerBrowser()->DDNetInfoSha256()))
		return;
	m_CommunityIconsInfoSha256 = ServerBrowser()->DDNetInfoSha256();

	// Remove icons for removed communities
	auto RemovalIterator = m_vCommunityIcons.begin();
	while(RemovalIterator != m_vCommunityIcons.end())
	{
		if(ServerBrowser()->Community(RemovalIterator->m_aCommunityId) == nullptr)
		{
			Graphics()->UnloadTexture(&RemovalIterator->m_OrgTexture);
			Graphics()->UnloadTexture(&RemovalIterator->m_GreyTexture);
			RemovalIterator = m_vCommunityIcons.erase(RemovalIterator);
		}
		else
		{
			++RemovalIterator;
		}
	}

	// Find added and updated community icons
	for(const auto &Community : ServerBrowser()->Communities())
	{
		if(str_comp(Community.Id(), IServerBrowser::COMMUNITY_NONE) == 0)
			continue;
		auto ExistingIcon = std::find_if(m_vCommunityIcons.begin(), m_vCommunityIcons.end(), [Community](const auto &Element) {
			return str_comp(Element.m_aCommunityId, Community.Id()) == 0;
		});
		auto pExistingDownload = std::find_if(m_CommunityIconDownloadJobs.begin(), m_CommunityIconDownloadJobs.end(), [Community](const auto &Element) {
			return str_comp(Element->CommunityId(), Community.Id()) == 0;
		});
		if(pExistingDownload == m_CommunityIconDownloadJobs.end() && (ExistingIcon == m_vCommunityIcons.end() || ExistingIcon->m_Sha256 != Community.IconSha256()))
		{
			std::shared_ptr<CCommunityIconDownloadJob> pJob = std::make_shared<CCommunityIconDownloadJob>(this, Community.Id(), Community.IconUrl(), Community.IconSha256());
			Http()->Run(pJob);
			m_CommunityIconDownloadJobs.push_back(pJob);
		}
	}
}
