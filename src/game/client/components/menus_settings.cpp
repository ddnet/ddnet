/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SDL.h" // SDL_VIDEO_DRIVER_X11

#include <base/tl/string.h>

#include <base/math.h>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include "binds.h"
#include "camera.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

#include <utility>

CMenusKeyBinder CMenus::m_Binder;

CMenusKeyBinder::CMenusKeyBinder()
{
	m_TakeKey = false;
	m_GotKey = false;
	m_Modifier = 0;
}

bool CMenusKeyBinder::OnInput(IInput::CEvent Event)
{
	if(m_TakeKey)
	{
		int TriggeringEvent = (Event.m_Key == KEY_MOUSE_1) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
		if(Event.m_Flags & TriggeringEvent)
		{
			m_Key = Event;
			m_GotKey = true;
			m_TakeKey = false;

			int Mask = CBinds::GetModifierMask(Input());
			m_Modifier = 0;
			while(!(Mask & 1))
			{
				Mask >>= 1;
				m_Modifier++;
			}
			if(CBinds::ModifierMatchesKey(m_Modifier, Event.m_Key))
				m_Modifier = 0;
		}
		return true;
	}

	return false;
}

void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
#if defined(CONF_FAMILY_WINDOWS)
	bool CheckSettings = false;
	static int s_ClShowConsole = g_Config.m_ClShowConsole;
#endif

	char aBuf[128];
	CUIRect Label, Button, Left, Right, Game, Client;
	MainView.HSplitTop(150.0f, &Game, &Client);

	// game
	{
		// headline
		Game.HSplitTop(20.0f, &Label, &Game);
		UI()->DoLabelScaled(&Label, Localize("Game"), 20.0f, -1);
		Game.Margin(5.0f, &Game);
		Game.VSplitMid(&Left, &Right);
		Left.VSplitRight(5.0f, &Left, 0);
		Right.VMargin(5.0f, &Right);

		// dynamic camera
		Left.HSplitTop(20.0f, &Button, &Left);
		bool IsDyncam = g_Config.m_ClDyncam || g_Config.m_ClMouseFollowfactor > 0;
		if(DoButton_CheckBox(&g_Config.m_ClDyncam, Localize("Dynamic Camera"), IsDyncam, &Button))
		{
			if(IsDyncam)
			{
				g_Config.m_ClDyncam = 0;
				g_Config.m_ClMouseFollowfactor = 0;
			}
			else
			{
				g_Config.m_ClDyncam = 1;
			}
		}

		// smooth dynamic camera
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(g_Config.m_ClDyncam)
		{
			if(DoButton_CheckBox(&g_Config.m_ClDyncamSmoothness, Localize("Smooth Dynamic Camera"), g_Config.m_ClDyncamSmoothness, &Button))
			{
				if(g_Config.m_ClDyncamSmoothness)
				{
					g_Config.m_ClDyncamSmoothness = 0;
				}
				else
				{
					g_Config.m_ClDyncamSmoothness = 50;
					g_Config.m_ClDyncamStabilizing = 50;
				}
			}
		}

		// weapon pickup
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeapons, Localize("Switch weapon on pickup"), g_Config.m_ClAutoswitchWeapons, &Button))
			g_Config.m_ClAutoswitchWeapons ^= 1;

		// weapon out of ammo autoswitch
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeaponsOutOfAmmo, Localize("Switch weapon when out of ammo"), g_Config.m_ClAutoswitchWeaponsOutOfAmmo, &Button))
			g_Config.m_ClAutoswitchWeaponsOutOfAmmo ^= 1;

		// weapon reset on death
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClResetWantedWeaponOnDeath, Localize("Reset wanted weapon on death"), g_Config.m_ClResetWantedWeaponOnDeath, &Button))
			g_Config.m_ClResetWantedWeaponOnDeath ^= 1;

		// chat messages
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClShowChatFriends, Localize("Show only chat messages from friends"), g_Config.m_ClShowChatFriends, &Button))
			g_Config.m_ClShowChatFriends ^= 1;

		// name plates
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClNameplates, Localize("Show name plates"), g_Config.m_ClNameplates, &Button))
			g_Config.m_ClNameplates ^= 1;

		if(g_Config.m_ClNameplates)
		{
			Right.HSplitTop(2.5f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Name plates size"), g_Config.m_ClNameplatesSize);
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClNameplatesSize = (int)(DoScrollbarH(&g_Config.m_ClNameplatesSize, &Button, g_Config.m_ClNameplatesSize / 100.0f) * 100.0f + 0.1f);

			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClNameplatesTeamcolors, Localize("Use team colors for name plates"), g_Config.m_ClNameplatesTeamcolors, &Button))
				g_Config.m_ClNameplatesTeamcolors ^= 1;

			Right.HSplitTop(5.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClNameplatesClan, Localize("Show clan above name plates"), g_Config.m_ClNameplatesClan, &Button))
				g_Config.m_ClNameplatesClan ^= 1;

			if(g_Config.m_ClNameplatesClan)
			{
				Right.HSplitTop(2.5f, 0, &Right);
				Right.HSplitTop(20.0f, &Label, &Right);
				Right.HSplitTop(20.0f, &Button, &Right);
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Clan plates size"), g_Config.m_ClNameplatesClanSize);
				UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
				Button.HMargin(2.0f, &Button);
				g_Config.m_ClNameplatesClanSize = (int)(DoScrollbarH(&g_Config.m_ClNameplatesClanSize, &Button, g_Config.m_ClNameplatesClanSize / 100.0f) * 100.0f + 0.1f);
			}
		}
	}

	// client
	{
		// headline
		Client.HSplitTop(20.0f, &Label, &Client);
		UI()->DoLabelScaled(&Label, Localize("Client"), 20.0f, -1);
		Client.Margin(5.0f, &Client);
		Client.VSplitMid(&Left, &Right);
		Left.VSplitRight(5.0f, &Left, 0);
		Right.VMargin(5.0f, &Right);

		// skip main menu
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClSkipStartMenu, Localize("Skip the main menu"), g_Config.m_ClSkipStartMenu, &Button))
			g_Config.m_ClSkipStartMenu ^= 1;

		float SliderGroupMargin = 10.0f;

		// auto demo settings
		{
			Right.HSplitTop(40.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoDemoRecord, Localize("Automatically record demos"), g_Config.m_ClAutoDemoRecord, &Button))
				g_Config.m_ClAutoDemoRecord ^= 1;

			Right.HSplitTop(20.0f, &Label, &Right);
			char aBuf[64];
			if(g_Config.m_ClAutoDemoMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max demos"), g_Config.m_ClAutoDemoMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max demos"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoDemoMax = static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoDemoMax, &Button, g_Config.m_ClAutoDemoMax / 1000.0f) * 1000.0f + 0.1f);

			Right.HSplitTop(SliderGroupMargin, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoScreenshot, Localize("Automatically take game over screenshot"), g_Config.m_ClAutoScreenshot, &Button))
				g_Config.m_ClAutoScreenshot ^= 1;

			Right.HSplitTop(20.0f, &Label, &Right);
			if(g_Config.m_ClAutoScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), g_Config.m_ClAutoScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoScreenshotMax = static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoScreenshotMax, &Button, g_Config.m_ClAutoScreenshotMax / 1000.0f) * 1000.0f + 0.1f);
		}

		Left.HSplitTop(10.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Label, &Left);
		char aBuf[64];
		if(g_Config.m_ClRefreshRate)
			str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), g_Config.m_ClRefreshRate);
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Refresh Rate"), "∞");
		UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.HMargin(2.0f, &Button);
		g_Config.m_ClRefreshRate = static_cast<int>(DoScrollbarH(&g_Config.m_ClRefreshRate, &Button, g_Config.m_ClRefreshRate / 10000.0f) * 10000.0f + 0.1f);

#if defined(CONF_FAMILY_WINDOWS)
		Left.HSplitTop(10.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowConsole, Localize("Show console window"), g_Config.m_ClShowConsole, &Button))
		{
			g_Config.m_ClShowConsole ^= 1;
			CheckSettings = true;
		}

		if(CheckSettings)
			m_NeedRestartGeneral = s_ClShowConsole != g_Config.m_ClShowConsole;
#endif

		Left.HSplitTop(15.0f, 0, &Left);
		CUIRect DirectoryButton;
		Left.HSplitBottom(25.0f, &Left, &DirectoryButton);
		RenderThemeSelection(Left);

		DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
		if(DoButton_Menu(&DirectoryButton, Localize("Themes directory"), 0, &DirectoryButton))
		{
			char aBuf[MAX_PATH_LENGTH];
			char aBufFull[MAX_PATH_LENGTH + 7];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "themes", aBuf, sizeof(aBuf));
			Storage()->CreateFolder("themes", IStorage::TYPE_SAVE);
			str_format(aBufFull, sizeof(aBufFull), "file://%s", aBuf);
			if(!open_link(aBufFull))
			{
				dbg_msg("menus", "couldn't open link");
			}
		}

		// auto statboard screenshot
		{
			Right.HSplitTop(SliderGroupMargin, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoStatboardScreenshot,
				   Localize("Automatically take statboard screenshot"),
				   g_Config.m_ClAutoStatboardScreenshot, &Button))
			{
				g_Config.m_ClAutoStatboardScreenshot ^= 1;
			}

			Right.HSplitTop(20.0f, &Label, &Right);
			if(g_Config.m_ClAutoStatboardScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), g_Config.m_ClAutoStatboardScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoStatboardScreenshotMax =
				static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoStatboardScreenshotMax,
							 &Button,
							 g_Config.m_ClAutoStatboardScreenshotMax / 1000.0f) *
							 1000.0f +
						 0.1f);
		}

		// auto statboard csv
		{
			Right.HSplitTop(SliderGroupMargin, 0, &Right); //
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoCSV,
				   Localize("Automatically create statboard csv"),
				   g_Config.m_ClAutoCSV, &Button))
			{
				g_Config.m_ClAutoCSV ^= 1;
			}

			Right.HSplitTop(20.0f, &Label, &Right);
			if(g_Config.m_ClAutoCSVMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max CSVs"), g_Config.m_ClAutoCSVMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max CSVs"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			g_Config.m_ClAutoCSVMax =
				static_cast<int>(DoScrollbarH(&g_Config.m_ClAutoCSVMax,
							 &Button,
							 g_Config.m_ClAutoCSVMax / 1000.0f) *
							 1000.0f +
						 0.1f);
		}
	}
}

void CMenus::SetNeedSendInfo()
{
	if(m_Dummy)
		m_NeedSendDummyinfo = true;
	else
		m_NeedSendinfo = true;
}

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	CUIRect Button, Label, Dummy;
	MainView.HSplitTop(10.0f, 0, &MainView);

	char *pName = g_Config.m_PlayerName;
	const char *pNameFallback = Client()->PlayerName();
	char *pClan = g_Config.m_PlayerClan;
	int *pCountry = &g_Config.m_PlayerCountry;

	if(m_Dummy)
	{
		pName = g_Config.m_ClDummyName;
		pNameFallback = Client()->DummyName();
		pClan = g_Config.m_ClDummyClan;
		pCountry = &g_Config.m_ClDummyCountry;
	}

	// player name
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	static float s_OffsetName = 0.0f;
	if(DoEditBox(pName, &Button, pName, sizeof(g_Config.m_PlayerName), 14.0f, &s_OffsetName, false, CUI::CORNER_ALL, pNameFallback))
	{
		SetNeedSendInfo();
	}

	// player clan
	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(200.0f, &Button, &Dummy);
	Button.VSplitLeft(150.0f, &Button, 0);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	static float s_OffsetClan = 0.0f;
	if(DoEditBox(pClan, &Button, pClan, sizeof(g_Config.m_PlayerClan), 14.0f, &s_OffsetClan))
	{
		SetNeedSendInfo();
	}

	if(DoButton_CheckBox(&m_Dummy, Localize("Dummy settings"), m_Dummy, &Dummy))
	{
		m_Dummy ^= 1;
	}

	static bool s_ListBoxUsed = false;
	if(UI()->ActiveItem() == pClan || UI()->ActiveItem() == pName)
		s_ListBoxUsed = false;

	// country flag selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	static float s_ScrollValue = 0.0f;
	int OldSelected = -1;
	UiDoListboxStart(&s_ScrollValue, &MainView, 50.0f, Localize("Country / Region"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

	for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
	{
		const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
		if(pEntry->m_CountryCode == *pCountry)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i, s_ListBoxUsed);
		if(Item.m_Visible)
		{
			CUIRect Label;
			Item.m_Rect.Margin(5.0f, &Item.m_Rect);
			Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
			float OldWidth = Item.m_Rect.w;
			Item.m_Rect.w = Item.m_Rect.h * 2;
			Item.m_Rect.x += (OldWidth - Item.m_Rect.w) / 2.0f;
			ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
			m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
			if(pEntry->m_Texture != -1)
				UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
		}
	}

	bool Clicked = false;
	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0, &Clicked);
	if(Clicked)
		s_ListBoxUsed = true;

	if(OldSelected != NewSelected)
	{
		*pCountry = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;
		SetNeedSendInfo();
	}
}

void CMenus::RenderSettingsTee(CUIRect MainView)
{
	CUIRect Button, Label, Button2, Dummy, DummyLabel, SkinList, QuickSearch, QuickSearchClearButton, SkinDB, SkinPrefix, SkinPrefixLabel, DirectoryButton, RefreshButton;

	static float s_ClSkinPrefix = 0.0f;

	static bool s_InitSkinlist = true;
	MainView.HSplitTop(10.0f, 0, &MainView);

	char *Skin = g_Config.m_ClPlayerSkin;
	int *UseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
	unsigned *ColorBody = &g_Config.m_ClPlayerColorBody;
	unsigned *ColorFeet = &g_Config.m_ClPlayerColorFeet;

	if(m_Dummy)
	{
		Skin = g_Config.m_ClDummySkin;
		UseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		ColorBody = &g_Config.m_ClDummyColorBody;
		ColorFeet = &g_Config.m_ClDummyColorFeet;
	}

	// skin info
	CTeeRenderInfo OwnSkinInfo;
	const CSkin *pSkin = m_pClient->m_pSkins->Get(m_pClient->m_pSkins->Find(Skin));
	OwnSkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	OwnSkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	OwnSkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	OwnSkinInfo.m_CustomColoredSkin = *UseCustomColor;
	if(*UseCustomColor)
	{
		OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(*ColorBody).UnclampLighting());
		OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(*ColorFeet).UnclampLighting());
	}
	else
	{
		OwnSkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		OwnSkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	OwnSkinInfo.m_Size = 50.0f * UI()->Scale();

	MainView.HSplitTop(20.0f, &Label, &MainView);
	Label.VSplitLeft(280.0f, &Label, &Dummy);
	Label.VSplitLeft(230.0f, &Label, 0);
	Dummy.VSplitLeft(170.0f, &Dummy, &SkinPrefix);
	SkinPrefix.VSplitLeft(120.0f, &SkinPrefix, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Your skin"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&m_Dummy, Localize("Dummy settings"), m_Dummy, &DummyLabel))
	{
		m_Dummy ^= 1;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&g_Config.m_ClDownloadSkins, Localize("Download skins"), g_Config.m_ClDownloadSkins, &DummyLabel))
	{
		g_Config.m_ClDownloadSkins ^= 1;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&g_Config.m_ClVanillaSkinsOnly, Localize("Vanilla skins only"), g_Config.m_ClVanillaSkinsOnly, &DummyLabel))
	{
		g_Config.m_ClVanillaSkinsOnly ^= 1;
		s_InitSkinlist = true;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&g_Config.m_ClFatSkins, Localize("Fat skins (DDFat)"), g_Config.m_ClFatSkins, &DummyLabel))
	{
		g_Config.m_ClFatSkins ^= 1;
	}

	SkinPrefix.HSplitTop(20.0f, &SkinPrefixLabel, &SkinPrefix);
	UI()->DoLabelScaled(&SkinPrefixLabel, Localize("Skin prefix"), 14.0f, -1);

	SkinPrefix.HSplitTop(20.0f, &SkinPrefixLabel, &SkinPrefix);
	{
		static int s_ClearButton = 0;
		DoClearableEditBox(g_Config.m_ClSkinPrefix, &s_ClearButton, &SkinPrefixLabel, g_Config.m_ClSkinPrefix, sizeof(g_Config.m_ClSkinPrefix), 14.0f, &s_ClSkinPrefix);
	}

	SkinPrefix.HSplitTop(2.0f, 0, &SkinPrefix);
	{
		static const char *s_aSkinPrefixes[] = {"kitty", "santa"};
		for(auto &pPrefix : s_aSkinPrefixes)
		{
			CUIRect Button;
			SkinPrefix.HSplitTop(20.0f, &Button, &SkinPrefix);
			Button.HMargin(2.0f, &Button);
			if(DoButton_Menu(&pPrefix, pPrefix, 0, &Button))
			{
				str_copy(g_Config.m_ClSkinPrefix, pPrefix, sizeof(g_Config.m_ClSkinPrefix));
			}
		}
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(230.0f, &Label, 0);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1, 0), vec2(Label.x + 30.0f, Label.y + 28.0f));
	Label.VSplitLeft(70.0f, 0, &Label);
	Label.HMargin(15.0f, &Label);
	//UI()->DoLabelScaled(&Label, Skin, 14.0f, -1, 150.0f);
	static float s_OffsetSkin = 0.0f;
	static int s_ClearButton = 0;
	if(DoClearableEditBox(Skin, &s_ClearButton, &Label, Skin, sizeof(g_Config.m_ClPlayerSkin), 14.0f, &s_OffsetSkin, false, CUI::CORNER_ALL, "default"))
	{
		SetNeedSendInfo();
	}

	// custom color selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitMid(&Button, &Button2);
	if(DoButton_CheckBox(&ColorBody, Localize("Custom colors"), *UseCustomColor, &Button))
	{
		*UseCustomColor = *UseCustomColor ? 0 : 1;
		SetNeedSendInfo();
	}

	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(82.5f, &Label, &MainView);
	if(*UseCustomColor)
	{
		CUIRect aRects[2];
		Label.VSplitMid(&aRects[0], &aRects[1]);
		aRects[0].VSplitRight(10.0f, &aRects[0], 0);
		aRects[1].VSplitLeft(10.0f, 0, &aRects[1]);

		unsigned *paColors[2] = {ColorBody, ColorFeet};
		const char *paParts[] = {Localize("Body"), Localize("Feet")};

		for(int i = 0; i < 2; i++)
		{
			aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
			UI()->DoLabelScaled(&Label, paParts[i], 14.0f, -1);
			aRects[i].VSplitLeft(10.0f, 0, &aRects[i]);
			aRects[i].HSplitTop(2.5f, 0, &aRects[i]);

			unsigned PrevColor = *paColors[i];
			RenderHSLScrollbars(&aRects[i], paColors[i]);

			if(PrevColor != *paColors[i])
			{
				SetNeedSendInfo();
			}
		}
	}

	// skin selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(230.0f, &SkinList, &MainView);
	static sorted_array<const CSkin *> s_paSkinList;
	static int s_SkinCount = 0;
	static float s_ScrollValue = 0.0f;
	if(s_InitSkinlist || m_pClient->m_pSkins->Num() != s_SkinCount)
	{
		s_paSkinList.clear();
		for(int i = 0; i < m_pClient->m_pSkins->Num(); ++i)
		{
			const CSkin *s = m_pClient->m_pSkins->Get(i);

			// filter quick search
			if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_find_nocase(s->m_aName, g_Config.m_ClSkinFilterString))
				continue;

			// no special skins
			if((s->m_aName[0] == 'x' && s->m_aName[1] == '_'))
				continue;

			// vanilla skins only
			if(g_Config.m_ClVanillaSkinsOnly && !s->m_IsVanilla)
				continue;

			if(s == 0)
				continue;

			s_paSkinList.add_unsorted(s);
		}
		s_InitSkinlist = false;
		s_SkinCount = m_pClient->m_pSkins->Num();
	}

	int OldSelected = -1;
	UiDoListboxStart(&s_InitSkinlist, &SkinList, 50.0f, Localize("Skins"), "", s_paSkinList.size(), 4, OldSelected, s_ScrollValue);
	for(int i = 0; i < s_paSkinList.size(); ++i)
	{
		const CSkin *s = s_paSkinList[i];

		if(str_comp(s->m_aName, Skin) == 0)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(s_paSkinList[i], OldSelected == i);
		char aBuf[128];
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info = OwnSkinInfo;
			Info.m_CustomColoredSkin = *UseCustomColor;

			Info.m_OriginalRenderSkin = s->m_OriginalSkin;
			Info.m_ColorableRenderSkin = s->m_ColorableSkin;
			Info.m_SkinMetrics = s->m_Metrics;

			Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top
			RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, 0, vec2(1.0f, 0.0f), vec2(Item.m_Rect.x + 30, Item.m_Rect.y + Item.m_Rect.h / 2));

			Item.m_Rect.VSplitLeft(60.0f, 0, &Item.m_Rect);
			str_format(aBuf, sizeof(aBuf), "%s", s->m_aName);
			RenderTools()->UI()->DoLabelScaled(&Item.m_Rect, aBuf, 12.0f, -1, Item.m_Rect.w);
			if(g_Config.m_Debug)
			{
				ColorRGBA BloodColor = *UseCustomColor ? color_cast<ColorRGBA>(ColorHSLA(*ColorBody)) : s->m_BloodColor;
				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BloodColor.r, BloodColor.g, BloodColor.b, 1.0f);
				IGraphics::CQuadItem QuadItem(Item.m_Rect.x, Item.m_Rect.y, 12.0f, 12.0f);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		mem_copy(Skin, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_ClPlayerSkin));
		SetNeedSendInfo();
	}

	// render quick search
	{
		MainView.HSplitBottom(ms_ButtonHeight, &MainView, &QuickSearch);
		QuickSearch.VSplitLeft(240.0f, &QuickSearch, &SkinDB);
		QuickSearch.HSplitTop(5.0f, 0, &QuickSearch);
		const char *pSearchLabel = "\xEE\xA2\xB6";
		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		UI()->DoLabelScaled(&QuickSearch, pSearchLabel, 14.0f, -1, -1, 0);
		float wSearch = TextRender()->TextWidth(0, 14.0f, pSearchLabel, -1, -1.0f);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetCurFont(NULL);
		QuickSearch.VSplitLeft(wSearch, 0, &QuickSearch);
		QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);
		QuickSearch.VSplitLeft(QuickSearch.w - 15.0f, &QuickSearch, &QuickSearchClearButton);
		static int s_ClearButton = 0;
		static float Offset = 0.0f;
		if(Input()->KeyPress(KEY_F) && (Input()->KeyIsPressed(KEY_LCTRL) || Input()->KeyIsPressed(KEY_RCTRL)))
			UI()->SetActiveItem(&g_Config.m_ClSkinFilterString);
		if(DoClearableEditBox(&g_Config.m_ClSkinFilterString, &s_ClearButton, &QuickSearch, g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString), 14.0f, &Offset, false, CUI::CORNER_ALL, Localize("Search")))
			s_InitSkinlist = true;
	}

	SkinDB.VSplitLeft(150.0f, &SkinDB, &DirectoryButton);
	SkinDB.HSplitTop(5.0f, 0, &SkinDB);
	if(DoButton_Menu(&SkinDB, Localize("Skin Database"), 0, &SkinDB))
	{
		if(!open_link("https://ddnet.tw/skins/"))
		{
			dbg_msg("menus", "couldn't open link");
		}
	}

	DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, 0, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &RefreshButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, 0);
	if(DoButton_Menu(&DirectoryButton, Localize("Skins directory"), 0, &DirectoryButton))
	{
		char aBuf[MAX_PATH_LENGTH];
		char aBufFull[MAX_PATH_LENGTH + 7];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
		str_format(aBufFull, sizeof(aBufFull), "file://%s", aBuf);
		if(!open_link(aBufFull))
		{
			dbg_msg("menus", "couldn't open link");
		}
	}

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(DoButton_Menu(&RefreshButton, "\xEE\x97\x95", 0, &RefreshButton, NULL, 15, 5, 0, vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4(1, 1, 1, 0.5f), 0))
	{
		m_pClient->m_pSkins->Refresh();
		s_InitSkinlist = true;
		if(Client()->State() >= IClient::STATE_ONLINE)
		{
			m_pClient->RefindSkins();
		}
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetCurFont(NULL);
}

typedef struct
{
	CLocConstString m_Name;
	const char *m_pCommand;
	int m_KeyId;
	int m_Modifier;
} CKeyInfo;

static CKeyInfo gs_aKeys[] =
	{
		{"Move left", "+left", 0, 0}, // Localize - these strings are localized within CLocConstString
		{"Move right", "+right", 0, 0},
		{"Jump", "+jump", 0, 0},
		{"Fire", "+fire", 0, 0},
		{"Hook", "+hook", 0, 0},
		{"Hook collisions", "+showhookcoll", 0, 0},
		{"45° aim", "+toggle cl_mouse_max_distance 2 400; +toggle inp_mousesens 1 200", 0, 0},
		{"Pause", "say /pause", 0, 0},
		{"Kill", "kill", 0, 0},
		{"Zoom in", "zoom+", 0, 0},
		{"Zoom out", "zoom-", 0, 0},
		{"Default zoom", "zoom", 0, 0},
		{"Show others", "say /showothers", 0, 0},
		{"Show all", "say /showall", 0, 0},
		{"Toggle dyncam", "toggle cl_dyncam 0 1", 0, 0},
		{"Toggle ghost", "toggle cl_race_show_ghost 0 1", 0, 0},

		{"Hammer", "+weapon1", 0, 0},
		{"Pistol", "+weapon2", 0, 0},
		{"Shotgun", "+weapon3", 0, 0},
		{"Grenade", "+weapon4", 0, 0},
		{"Laser", "+weapon5", 0, 0},
		{"Next weapon", "+nextweapon", 0, 0},
		{"Prev. weapon", "+prevweapon", 0, 0},

		{"Vote yes", "vote yes", 0, 0},
		{"Vote no", "vote no", 0, 0},

		{"Chat", "+show_chat; chat all", 0, 0},
		{"Team chat", "+show_chat; chat team", 0, 0},
		{"Converse", "+show_chat; chat all /c ", 0, 0},
		{"Chat command", "+show_chat; chat all /", 0, 0},
		{"Show chat", "+show_chat", 0, 0},

		{"Toggle dummy", "toggle cl_dummy 0 1", 0, 0},
		{"Dummy copy", "toggle cl_dummy_copy_moves 0 1", 0, 0},
		{"Hammerfly dummy", "toggle cl_dummy_hammer 0 1", 0, 0},
		{"Deepfly on", "bind mouse1 \"+fire; +toggle cl_dummy_hammer 1 0\"", 0, 0},
		{"Deepfly off", "bind mouse1 \"+fire\"", 0, 0},

		{"Emoticon", "+emote", 0, 0},
		{"Spectator mode", "+spectate", 0, 0},
		{"Spectate next", "spectate_next", 0, 0},
		{"Spectate previous", "spectate_previous", 0, 0},
		{"Console", "toggle_local_console", 0, 0},
		{"Remote console", "toggle_remote_console", 0, 0},
		{"Screenshot", "screenshot", 0, 0},
		{"Scoreboard", "+scoreboard", 0, 0},
		{"Statboard", "+statboard", 0, 0},
		{"Lock team", "say /lock", 0, 0},
		{"Show entities", "toggle cl_overlay_entities 0 100", 0, 0},
		{"Show HUD", "toggle cl_showhud 0 1", 0, 0},
};

/*	This is for scripts/languages to work, don't remove!
	Localize("Move left");Localize("Move right");Localize("Jump");Localize("Fire");Localize("Hook");
	Localize("Hook collisions");Localize("Pause");Localize("Kill");Localize("Zoom in");Localize("Zoom out");
	Localize("Default zoom");Localize("Show others");Localize("Show all");Localize("Toggle dyncam");
	Localize("Toggle dummy");Localize("Toggle ghost");Localize("Dummy copy");Localize("Hammerfly dummy");
	Localize("Hammer");Localize("Pistol");Localize("Shotgun");Localize("Grenade");Localize("Laser");
	Localize("Next weapon");Localize("Prev. weapon");Localize("Vote yes");Localize("Vote no");
	Localize("Chat");Localize("Team chat");Localize("Converse");Localize("Show chat");Localize("Emoticon");
	Localize("Spectator mode");Localize("Spectate next");Localize("Spectate previous");Localize("Console");
	Localize("Remote console");Localize("Screenshot");Localize("Scoreboard");Localize("Statboard");
	Localize("Lock team");Localize("Show entities");Localize("Show HUD");Localize("45° aim");
	Localize("Chat command";Localize("Deepfly on");Localize("Deepfly off");
*/

void CMenus::UiDoGetButtons(int Start, int Stop, CUIRect View, CUIRect ScopeView)
{
	for(int i = Start; i < Stop; i++)
	{
		CKeyInfo &Key = gs_aKeys[i];
		CUIRect Button, Label;
		View.HSplitTop(20.0f, &Button, &View);
		Button.VSplitLeft(135.0f, &Label, &Button);

		if(Button.y >= ScopeView.y && Button.y + Button.h <= ScopeView.y + ScopeView.h)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "%s:", Localize((const char *)Key.m_Name));

			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			int OldId = Key.m_KeyId, OldModifier = Key.m_Modifier, NewModifier;
			int NewId = DoKeyReader((void *)&gs_aKeys[i].m_Name, &Button, OldId, OldModifier, &NewModifier);
			if(NewId != OldId || NewModifier != OldModifier)
			{
				if(OldId != 0 || NewId == 0)
					m_pClient->m_pBinds->Bind(OldId, "", false, OldModifier);
				if(NewId != 0)
					m_pClient->m_pBinds->Bind(NewId, gs_aKeys[i].m_pCommand, false, NewModifier);
			}
		}

		View.HSplitTop(2.0f, 0, &View);
	}
}

void CMenus::RenderSettingsControls(CUIRect MainView)
{
	char aBuf[128];

	// this is kinda slow, but whatever
	for(auto &Key : gs_aKeys)
		Key.m_KeyId = Key.m_Modifier = 0;

	for(int Mod = 0; Mod < CBinds::MODIFIER_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = m_pClient->m_pBinds->Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			for(auto &Key : gs_aKeys)
				if(str_comp(pBind, Key.m_pCommand) == 0)
				{
					Key.m_KeyId = KeyId;
					Key.m_Modifier = Mod;
					break;
				}
		}
	}

	// controls in a scrollable listbox
	static int s_ControlsList = 0;
	static int s_SelectedControl = -1;
	static float s_ScrollValue = 0;
	int OldSelected = s_SelectedControl;
	UiDoListboxStart(&s_ControlsList, &MainView, 500.0f, Localize("Controls"), "", 1, 1, s_SelectedControl, s_ScrollValue);

	CUIRect MovementSettings, WeaponSettings, VotingSettings, ChatSettings, DummySettings, MiscSettings, ResetButton;
	CListboxItem Item = UiDoListboxNextItem(&OldSelected, false, false, true);
	Item.m_Rect.HSplitTop(10.0f, 0, &Item.m_Rect);
	Item.m_Rect.VSplitMid(&MovementSettings, &VotingSettings);

	// movement settings
	{
		MovementSettings.VMargin(5.0f, &MovementSettings);
		MovementSettings.HSplitTop(510.0f, &MovementSettings, &WeaponSettings);
		RenderTools()->DrawUIRect(&MovementSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		MovementSettings.VMargin(10.0f, &MovementSettings);

		TextRender()->Text(0, MovementSettings.x, MovementSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Movement"), -1.0f);

		MovementSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &MovementSettings);

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Mouse sens."), g_Config.m_InpMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f * UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&g_Config.m_InpMousesens, &Button, (minimum(g_Config.m_InpMousesens, 500) - 1) / 500.0f) * 500.0f) + 1;
			if(g_Config.m_InpMousesens < 500 || NewValue < 500)
				g_Config.m_InpMousesens = minimum(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("UI mouse s."), g_Config.m_UiMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f * UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&g_Config.m_UiMousesens, &Button, (minimum(g_Config.m_UiMousesens, 500) - 1) / 500.0f) * 500.0f) + 1;
			if(g_Config.m_UiMousesens < 500 || NewValue < 500)
				g_Config.m_UiMousesens = minimum(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		UiDoGetButtons(0, 18, MovementSettings, MainView);
	}

	// weapon settings
	{
		WeaponSettings.HSplitTop(10.0f, 0, &WeaponSettings);
		WeaponSettings.HSplitTop(145.0f, &WeaponSettings, &ResetButton);
		RenderTools()->DrawUIRect(&WeaponSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		WeaponSettings.VMargin(10.0f, &WeaponSettings);

		TextRender()->Text(0, WeaponSettings.x, WeaponSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Weapon"), -1.0f);

		WeaponSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &WeaponSettings);
		UiDoGetButtons(18, 23, WeaponSettings, MainView);
	}

	// defaults
	{
		ResetButton.HSplitTop(10.0f, 0, &ResetButton);
		ResetButton.HSplitTop(40.0f, &ResetButton, 0);
		RenderTools()->DrawUIRect(&ResetButton, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		ResetButton.HMargin(10.0f, &ResetButton);
		ResetButton.VMargin(30.0f, &ResetButton);
		ResetButton.HSplitTop(20.0f, &ResetButton, 0);
		static int s_DefaultButton = 0;
		if(DoButton_Menu((void *)&s_DefaultButton, Localize("Reset to defaults"), 0, &ResetButton))
			m_pClient->m_pBinds->SetDefaults();
	}

	// voting settings
	{
		VotingSettings.VMargin(5.0f, &VotingSettings);
		VotingSettings.HSplitTop(80.0f, &VotingSettings, &ChatSettings);
		RenderTools()->DrawUIRect(&VotingSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		VotingSettings.VMargin(10.0f, &VotingSettings);

		TextRender()->Text(0, VotingSettings.x, VotingSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Voting"), -1.0f);

		VotingSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &VotingSettings);
		UiDoGetButtons(23, 25, VotingSettings, MainView);
	}

	// chat settings
	{
		ChatSettings.HSplitTop(10.0f, 0, &ChatSettings);
		ChatSettings.HSplitTop(145.0f, &ChatSettings, &DummySettings);
		RenderTools()->DrawUIRect(&ChatSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		ChatSettings.VMargin(10.0f, &ChatSettings);

		TextRender()->Text(0, ChatSettings.x, ChatSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Chat"), -1.0f);

		ChatSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &ChatSettings);
		UiDoGetButtons(25, 30, ChatSettings, MainView);
	}

	// dummy settings
	{
		DummySettings.HSplitTop(10.0f, 0, &DummySettings);
		DummySettings.HSplitTop(145.0f, &DummySettings, &MiscSettings);
		RenderTools()->DrawUIRect(&DummySettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		DummySettings.VMargin(10.0f, &DummySettings);

		TextRender()->Text(0, DummySettings.x, DummySettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Dummy"), -1.0f);

		DummySettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &DummySettings);
		UiDoGetButtons(30, 35, DummySettings, MainView);
	}

	// misc settings
	{
		MiscSettings.HSplitTop(10.0f, 0, &MiscSettings);
		MiscSettings.HSplitTop(300.0f, &MiscSettings, 0);
		RenderTools()->DrawUIRect(&MiscSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		MiscSettings.VMargin(10.0f, &MiscSettings);

		TextRender()->Text(0, MiscSettings.x, MiscSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Miscellaneous"), -1.0f);

		MiscSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &MiscSettings);
		UiDoGetButtons(35, 47, MiscSettings, MainView);
	}

	UiDoListboxEnd(&s_ScrollValue, 0);
}

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button, Label;
	char aBuf[128];
	bool CheckSettings = false;

	static const int MAX_RESOLUTIONS = 256;
	static CVideoMode s_aModes[MAX_RESOLUTIONS];
	static int s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	static int s_GfxScreenWidth = g_Config.m_GfxScreenWidth;
	static int s_GfxScreenHeight = g_Config.m_GfxScreenHeight;
	static int s_GfxColorDepth = g_Config.m_GfxColorDepth;
	static int s_GfxVsync = g_Config.m_GfxVsync;
	static int s_GfxFsaaSamples = g_Config.m_GfxFsaaSamples;
	static int s_GfxOpenGLVersion = (g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 3) || g_Config.m_GfxOpenGLMajor >= 4;
	static int s_GfxEnableTextureUnitOptimization = g_Config.m_GfxEnableTextureUnitOptimization;
	static int s_GfxUsePreinitBuffer = g_Config.m_GfxUsePreinitBuffer;
	static int s_GfxHighdpi = g_Config.m_GfxHighdpi;

	CUIRect ModeList;
	MainView.VSplitLeft(350.0f, &MainView, &ModeList);
	MainView.VSplitLeft(340.0f, &MainView, 0);

	// draw allmodes switch
	ModeList.HSplitTop(20, &Button, &ModeList);
	if(DoButton_CheckBox(&g_Config.m_GfxDisplayAllModes, Localize("Show only supported"), g_Config.m_GfxDisplayAllModes ^ 1, &Button))
	{
		g_Config.m_GfxDisplayAllModes ^= 1;
		s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	}

	// display mode list
	static float s_ScrollValue = 0;
	int OldSelected = -1;
	int G = gcd(s_GfxScreenWidth, s_GfxScreenHeight);
	str_format(aBuf, sizeof(aBuf), "%s: %dx%d %d bit (%d:%d)", Localize("Current"), s_GfxScreenWidth, s_GfxScreenHeight, s_GfxColorDepth, s_GfxScreenWidth / G, s_GfxScreenHeight / G);
	UiDoListboxStart(&s_NumNodes, &ModeList, 24.0f, Localize("Display Modes"), aBuf, s_NumNodes, 1, OldSelected, s_ScrollValue);

	for(int i = 0; i < s_NumNodes; ++i)
	{
		const int Depth = s_aModes[i].m_Red + s_aModes[i].m_Green + s_aModes[i].m_Blue > 16 ? 24 : 16;
		if(g_Config.m_GfxColorDepth == Depth &&
			g_Config.m_GfxScreenWidth == s_aModes[i].m_Width &&
			g_Config.m_GfxScreenHeight == s_aModes[i].m_Height)
		{
			OldSelected = i;
		}

		CListboxItem Item = UiDoListboxNextItem(&s_aModes[i], OldSelected == i);
		if(Item.m_Visible)
		{
			int G = gcd(s_aModes[i].m_Width, s_aModes[i].m_Height);
			str_format(aBuf, sizeof(aBuf), " %dx%d %d bit (%d:%d)", s_aModes[i].m_Width, s_aModes[i].m_Height, Depth, s_aModes[i].m_Width / G, s_aModes[i].m_Height / G);
			UI()->DoLabelScaled(&Item.m_Rect, aBuf, 16.0f, -1);
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		const int Depth = s_aModes[NewSelected].m_Red + s_aModes[NewSelected].m_Green + s_aModes[NewSelected].m_Blue > 16 ? 24 : 16;
		g_Config.m_GfxColorDepth = Depth;
		g_Config.m_GfxScreenWidth = s_aModes[NewSelected].m_Width;
		g_Config.m_GfxScreenHeight = s_aModes[NewSelected].m_Height;
		Graphics()->Resize(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight, true);
	}

	// switches
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxBorderless, Localize("Borderless window"), g_Config.m_GfxBorderless, &Button))
	{
		Client()->ToggleWindowBordered();
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxFullscreen, Localize("Fullscreen"), g_Config.m_GfxFullscreen, &Button))
	{
		Client()->ToggleFullscreen();
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("V-Sync"), Localize("may cause delay"));
	if(DoButton_CheckBox(&g_Config.m_GfxVsync, aBuf, g_Config.m_GfxVsync, &Button))
	{
		Client()->ToggleWindowVSync();
	}

	if(Graphics()->GetNumScreens() > 1)
	{
		int NumScreens = Graphics()->GetNumScreens();
		MainView.HSplitTop(20.0f, &Button, &MainView);
		int Screen_MouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxScreen, Localize("Screen"), g_Config.m_GfxScreen, &Button);
		if(Screen_MouseButton == 1) //inc
		{
			Client()->SwitchWindowScreen((g_Config.m_GfxScreen + 1) % NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		}
		else if(Screen_MouseButton == 2) //dec
		{
			Client()->SwitchWindowScreen((g_Config.m_GfxScreen - 1 + NumScreens) % NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("FSAA samples"), Localize("may cause delay"));
	int GfxFsaaSamples_MouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxFsaaSamples, aBuf, g_Config.m_GfxFsaaSamples, &Button);
	if(GfxFsaaSamples_MouseButton == 1) //inc
	{
		g_Config.m_GfxFsaaSamples = (g_Config.m_GfxFsaaSamples + 1) % 17;
		CheckSettings = true;
	}
	else if(GfxFsaaSamples_MouseButton == 2) //dec
	{
		g_Config.m_GfxFsaaSamples = (g_Config.m_GfxFsaaSamples - 1 + 17) % 17;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighDetail, Localize("High Detail"), g_Config.m_GfxHighDetail, &Button))
		g_Config.m_GfxHighDetail ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	bool IsNewOpenGL = (g_Config.m_GfxOpenGLMajor == 3 && g_Config.m_GfxOpenGLMinor == 3) || g_Config.m_GfxOpenGLMajor >= 4;
	if(DoButton_CheckBox(&g_Config.m_GfxOpenGLMajor, Localize("Use OpenGL 3.3 (experimental)"), IsNewOpenGL, &Button))
	{
		CheckSettings = true;
		if(IsNewOpenGL)
		{
			g_Config.m_GfxOpenGLMajor = 3;
			g_Config.m_GfxOpenGLMinor = 0;
			g_Config.m_GfxOpenGLPatch = 0;
			IsNewOpenGL = false;
		}
		else
		{
			g_Config.m_GfxOpenGLMajor = 3;
			g_Config.m_GfxOpenGLMinor = 3;
			g_Config.m_GfxOpenGLPatch = 0;
			IsNewOpenGL = true;
		}
	}

	if(IsNewOpenGL)
	{
		MainView.HSplitTop(20.0f, &Button, &MainView);
		if(DoButton_CheckBox(&g_Config.m_GfxUsePreinitBuffer, Localize("Preinit VBO (iGPUs only)"), g_Config.m_GfxUsePreinitBuffer, &Button))
		{
			CheckSettings = true;
			g_Config.m_GfxUsePreinitBuffer ^= 1;
		}

		MainView.HSplitTop(20.0f, &Button, &MainView);
		if(DoButton_CheckBox(&g_Config.m_GfxEnableTextureUnitOptimization, Localize("Multiple texture units (disable for MacOS)"), g_Config.m_GfxEnableTextureUnitOptimization, &Button))
		{
			CheckSettings = true;
			g_Config.m_GfxEnableTextureUnitOptimization ^= 1;
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighdpi, Localize("Use high DPI"), g_Config.m_GfxHighdpi, &Button))
	{
		CheckSettings = true;
		g_Config.m_GfxHighdpi ^= 1;
	}

	// check if the new settings require a restart
	if(CheckSettings)
	{
		if(s_GfxScreenWidth == g_Config.m_GfxScreenWidth &&
			s_GfxScreenHeight == g_Config.m_GfxScreenHeight &&
			s_GfxColorDepth == g_Config.m_GfxColorDepth &&
			s_GfxVsync == g_Config.m_GfxVsync &&
			s_GfxFsaaSamples == g_Config.m_GfxFsaaSamples &&
			s_GfxOpenGLVersion == (int)IsNewOpenGL &&
			s_GfxUsePreinitBuffer == g_Config.m_GfxUsePreinitBuffer &&
			s_GfxEnableTextureUnitOptimization == g_Config.m_GfxEnableTextureUnitOptimization &&
			s_GfxHighdpi == g_Config.m_GfxHighdpi)
			m_NeedRestartGraphics = false;
		else
			m_NeedRestartGraphics = true;
	}

	MainView.HSplitTop(20.0f, &Label, &MainView);
	Label.VSplitLeft(160.0f, &Label, &Button);
	if(g_Config.m_GfxRefreshRate)
		str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), g_Config.m_GfxRefreshRate);
	else
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Refresh Rate"), "∞");
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	Button.HMargin(2.0f, &Button);
	int NewRefreshRate = static_cast<int>(DoScrollbarH(&g_Config.m_GfxRefreshRate, &Button, (minimum(g_Config.m_GfxRefreshRate, 1000)) / 1000.0f) * 1000.0f + 0.1f);
	if(g_Config.m_GfxRefreshRate <= 1000 || NewRefreshRate < 1000)
		g_Config.m_GfxRefreshRate = NewRefreshRate;

	CUIRect Text;
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Text, &MainView);
	//text.VSplitLeft(15.0f, 0, &text);
	UI()->DoLabelScaled(&Text, Localize("UI Color"), 14.0f, -1);
	RenderHSLScrollbars(&MainView, &g_Config.m_UiColor, true);
}

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	CUIRect Button;
	MainView.VSplitMid(&MainView, 0);
	static int s_SndEnable = g_Config.m_SndEnable;
	static int s_SndRate = g_Config.m_SndRate;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndEnable, Localize("Use sounds"), g_Config.m_SndEnable, &Button))
	{
		g_Config.m_SndEnable ^= 1;
		if(g_Config.m_SndEnable)
		{
			if(g_Config.m_SndMusic && Client()->State() == IClient::STATE_OFFLINE)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		}
		else
			m_pClient->m_pSounds->Stop(SOUND_MENU);
		m_NeedRestartSound = g_Config.m_SndEnable && (!s_SndEnable || s_SndRate != g_Config.m_SndRate);
	}

	if(!g_Config.m_SndEnable)
		return;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndMusic, Localize("Play background music"), g_Config.m_SndMusic, &Button))
	{
		g_Config.m_SndMusic ^= 1;
		if(Client()->State() == IClient::STATE_OFFLINE)
		{
			if(g_Config.m_SndMusic)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
			else
				m_pClient->m_pSounds->Stop(SOUND_MENU);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndNonactiveMute, Localize("Mute when not active"), g_Config.m_SndNonactiveMute, &Button))
		g_Config.m_SndNonactiveMute ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndGame, Localize("Enable game sounds"), g_Config.m_SndGame, &Button))
		g_Config.m_SndGame ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndGun, Localize("Enable gun sound"), g_Config.m_SndGun, &Button))
		g_Config.m_SndGun ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndLongPain, Localize("Enable long pain sound (used when shooting in freeze)"), g_Config.m_SndLongPain, &Button))
		g_Config.m_SndLongPain ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndServerMessage, Localize("Enable server message sound"), g_Config.m_SndServerMessage, &Button))
		g_Config.m_SndServerMessage ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndChat, Localize("Enable regular chat sound"), g_Config.m_SndChat, &Button))
		g_Config.m_SndChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndTeamChat, Localize("Enable team chat sound"), g_Config.m_SndTeamChat, &Button))
		g_Config.m_SndTeamChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndHighlight, Localize("Enable highlighted chat sound"), g_Config.m_SndHighlight, &Button))
		g_Config.m_SndHighlight ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClThreadsoundloading, Localize("Threaded sound loading"), g_Config.m_ClThreadsoundloading, &Button))
		g_Config.m_ClThreadsoundloading ^= 1;

	// sample rate box
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", g_Config.m_SndRate);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		UI()->DoLabelScaled(&Button, Localize("Sample rate"), 14.0f, -1);
		Button.VSplitLeft(190.0f, 0, &Button);
		static float Offset = 0.0f;
		DoEditBox(&g_Config.m_SndRate, &Button, aBuf, sizeof(aBuf), 14.0f, &Offset);
		g_Config.m_SndRate = maximum(1, str_toint(aBuf));
		m_NeedRestartSound = !s_SndEnable || s_SndRate != g_Config.m_SndRate;
	}

	// volume slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sound volume"), 14.0f, -1);
		g_Config.m_SndVolume = (int)(DoScrollbarH(&g_Config.m_SndVolume, &Button, g_Config.m_SndVolume / 100.0f) * 100.0f);
	}

	// volume slider game sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Game sound volume"), 14.0f, -1);
		g_Config.m_SndGameSoundVolume = (int)(DoScrollbarH(&g_Config.m_SndGameSoundVolume, &Button, g_Config.m_SndGameSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider gui sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Chat sound volume"), 14.0f, -1);
		g_Config.m_SndChatSoundVolume = (int)(DoScrollbarH(&g_Config.m_SndChatSoundVolume, &Button, g_Config.m_SndChatSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider map sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Map sound volume"), 14.0f, -1);
		g_Config.m_SndMapSoundVolume = (int)(DoScrollbarH(&g_Config.m_SndMapSoundVolume, &Button, g_Config.m_SndMapSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider background music
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Background music volume"), 14.0f, -1);
		g_Config.m_SndBackgroundMusicVolume = (int)(DoScrollbarH(&g_Config.m_SndBackgroundMusicVolume, &Button, g_Config.m_SndBackgroundMusicVolume / 100.0f) * 100.0f);
	}
}

class CLanguage
{
public:
	CLanguage() {}
	CLanguage(const char *n, const char *f, int Code) :
		m_Name(n), m_FileName(f), m_CountryCode(Code) {}

	string m_Name;
	string m_FileName;
	int m_CountryCode;

	bool operator<(const CLanguage &Other) const { return m_Name < Other.m_Name; }
};

void LoadLanguageIndexfile(IStorage *pStorage, IConsole *pConsole, sorted_array<CLanguage> *pLanguages)
{
	IOHANDLE File = pStorage->OpenFile("languages/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "couldn't open index file");
		return;
	}

	char aOrigin[128];
	char aReplacement[128];
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aOrigin, pLine, sizeof(aOrigin));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aReplacement, pLine + 3, sizeof(aReplacement));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		char aFileName[128];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aOrigin);
		pLanguages->add(CLanguage(aReplacement, aFileName, str_toint(pLine + 3)));
	}
	io_close(File);
}

void CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_LanguageList = 0;
	static int s_SelectedLanguage = 0;
	static sorted_array<CLanguage> s_Languages;
	static float s_ScrollValue = 0;

	if(s_Languages.size() == 0)
	{
		s_Languages.add(CLanguage("English", "", 826));
		LoadLanguageIndexfile(Storage(), Console(), &s_Languages);
		for(int i = 0; i < s_Languages.size(); i++)
			if(str_comp(s_Languages[i].m_FileName, g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				break;
			}
	}

	int OldSelected = s_SelectedLanguage;

	UiDoListboxStart(&s_LanguageList, &MainView, 24.0f, Localize("Language"), "", s_Languages.size(), 1, s_SelectedLanguage, s_ScrollValue);

	for(sorted_array<CLanguage>::range r = s_Languages.all(); !r.empty(); r.pop_front())
	{
		CListboxItem Item = UiDoListboxNextItem(&r.front());

		if(Item.m_Visible)
		{
			CUIRect Rect;
			Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Rect, &Item.m_Rect);
			Rect.VMargin(6.0f, &Rect);
			Rect.HMargin(3.0f, &Rect);
			ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
			m_pClient->m_pCountryFlags->Render(r.front().m_CountryCode, &Color, Rect.x, Rect.y, Rect.w, Rect.h);
			Item.m_Rect.HSplitTop(2.0f, 0, &Item.m_Rect);
			UI()->DoLabelScaled(&Item.m_Rect, r.front().m_Name, 16.0f, -1);
		}
	}

	s_SelectedLanguage = UiDoListboxEnd(&s_ScrollValue, 0);

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(g_Config.m_ClLanguagefile, s_Languages[s_SelectedLanguage].m_FileName, sizeof(g_Config.m_ClLanguagefile));
		g_Localization.Load(s_Languages[s_SelectedLanguage].m_FileName, Storage(), Console());
		GameClient()->OnLanguageChange();
	}
}

void CMenus::RenderSettings(CUIRect MainView)
{
	// render background
	CUIRect Temp, TabBar, RestartWarning;
	MainView.VSplitRight(120.0f, &MainView, &TabBar);
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitBottom(15.0f, &MainView, &RestartWarning);
	TabBar.HSplitTop(50.0f, &Temp, &TabBar);
	RenderTools()->DrawUIRect(&Temp, ms_ColorTabbarActive, CUI::CORNER_BR, 10.0f);

	MainView.HSplitTop(10.0f, 0, &MainView);

	CUIRect Button;

	const char *aTabs[] = {
		Localize("Language"),
		Localize("General"),
		Localize("Player"),
		("Tee"),
		Localize("HUD"),
		Localize("Controls"),
		Localize("Graphics"),
		Localize("Sound"),
		Localize("DDNet"),
		Localize("Assets")};

	int NumTabs = (int)(sizeof(aTabs) / sizeof(*aTabs));
	int PreviousPage = g_Config.m_UiSettingsPage;

	for(int i = 0; i < NumTabs; i++)
	{
		TabBar.HSplitTop(10, &Button, &TabBar);
		TabBar.HSplitTop(26, &Button, &TabBar);
		if(DoButton_MenuTab(aTabs[i], aTabs[i], g_Config.m_UiSettingsPage == i, &Button, CUI::CORNER_R, &m_aAnimatorsSettingsTab[i]))
			g_Config.m_UiSettingsPage = i;
	}

	if(PreviousPage != g_Config.m_UiSettingsPage)
		ms_ColorPicker.m_Active = false;

	MainView.Margin(10.0f, &MainView);

	if(g_Config.m_UiSettingsPage == SETTINGS_LANGUAGE)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderLanguageSelection(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_GENERAL)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
		RenderSettingsGeneral(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_PLAYER)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
		RenderSettingsPlayer(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_TEE)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_TEE);
		RenderSettingsTee(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_HUD)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_HUD);
		RenderSettingsHUD(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_CONTROLS)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_CONTROLS);
		RenderSettingsControls(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_GRAPHICS)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_GRAPHICS);
		RenderSettingsGraphics(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_SOUND)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_SOUND);
		RenderSettingsSound(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_DDNET)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_DDNET);
		RenderSettingsDDNet(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_ASSETS)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_ASSETS);
		RenderSettingsCustom(MainView);
	}

	if(m_NeedRestartUpdate)
	{
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
		UI()->DoLabelScaled(&RestartWarning, Localize("DDNet Client needs to be restarted to complete update!"), 14.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else if(m_NeedRestartGeneral || m_NeedRestartSkins || m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartDDNet)
		UI()->DoLabelScaled(&RestartWarning, Localize("You must restart the game for all settings to take effect."), 14.0f, -1);

	RenderColorPicker();
}

ColorHSLA CMenus::RenderHSLColorPicker(const CUIRect *pRect, unsigned int *pColor, bool Alpha)
{
	ColorHSLA HSLColor(*pColor, false);
	ColorRGBA RGBColor = color_cast<ColorRGBA>(HSLColor);

	ColorRGBA Outline(1, 1, 1, 0.25f);
	const float OutlineSize = 3.0f;
	Outline.a *= ButtonColorMul(pColor);

	CUIRect Rect;
	pRect->Margin(OutlineSize, &Rect);

	RenderTools()->DrawUIRect(pRect, Outline, CUI::CORNER_ALL, 4.0f);
	RenderTools()->DrawUIRect(&Rect, RGBColor, CUI::CORNER_ALL, 4.0f);

	if(UI()->DoButtonLogic(pColor, 0, pRect))
	{
		if(ms_ColorPicker.m_Active)
		{
			CUIRect PickerRect;
			PickerRect.x = ms_ColorPicker.m_X;
			PickerRect.y = ms_ColorPicker.m_Y;
			PickerRect.w = ms_ColorPicker.ms_Width;
			PickerRect.h = ms_ColorPicker.ms_Height;

			if(ms_ColorPicker.m_pColor == pColor || UI()->MouseInside(&PickerRect))
				return HSLColor;
		}

		CUIRect *pScreen = UI()->Screen();
		ms_ColorPicker.m_X = minimum(UI()->MouseX(), pScreen->w - ms_ColorPicker.ms_Width);
		ms_ColorPicker.m_Y = minimum(UI()->MouseY(), pScreen->h - ms_ColorPicker.ms_Height);
		ms_ColorPicker.m_pColor = pColor;
		ms_ColorPicker.m_Active = true;
		ms_ColorPicker.m_AttachedRect = *pRect;
		ms_ColorPicker.m_HSVColor = color_cast<ColorHSVA, ColorHSLA>(HSLColor).Pack(false);
	}

	return HSLColor;
}

ColorHSLA CMenus::RenderHSLScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha)
{
	ColorHSLA Color(*pColor, Alpha);
	CUIRect Button, Label;
	char aBuf[32];
	float *paComponent[] = {&Color.h, &Color.s, &Color.l, &Color.a};
	const char *aLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};

	for(int i = 0; i < 3 + Alpha; i++)
	{
		pRect->HSplitTop(20.0f, &Button, pRect);
		Button.VSplitLeft(10.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		str_format(aBuf, sizeof(aBuf), "%s: %03d", aLabels[i], (int)(*paComponent[i] * 255));
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
		*paComponent[i] = DoScrollbarH(&((char *)pColor)[i], &Button, *paComponent[i]);
	}

	*pColor = Color.Pack(Alpha);
	return Color;
}

void CMenus::RenderSettingsHUD(CUIRect MainView)
{
	CUIRect HUD, Chat, Section, SectionTwo;

	MainView.VSplitMid(&HUD, &Chat);

	HUD.HSplitTop(30.0f, &Section, &HUD);
	UI()->DoLabelScaled(&Section, Localize("HUD"), 20.0f, -1);
	HUD.VSplitLeft(5.0f, 0x0, &HUD);
	HUD.HSplitTop(5.0f, 0x0, &HUD);

	Chat.HSplitTop(30.0f, &Section, &Chat);
	UI()->DoLabelScaled(&Section, Localize("Chat"), 20.0f, -1);
	Chat.VSplitLeft(5.0f, 0x0, &Chat);
	Chat.HSplitTop(5.0f, 0x0, &Chat);

	const float LineMargin = 20.0f;

	// ***** HUD ***** //

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhud, Localize("Show ingame HUD"), &g_Config.m_ClShowhud, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDDRaceScoreBoard, Localize("Use DDRace Scoreboard"), &g_Config.m_ClDDRaceScoreBoard, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowIDs, Localize("Show client IDs"), &g_Config.m_ClShowIDs, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudScore, Localize("Show score"), &g_Config.m_ClShowhudScore, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudHealthAmmo, Localize("Show health + ammo"), &g_Config.m_ClShowhudHealthAmmo, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowChat, Localize("Show chat"), &g_Config.m_ClShowChat, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatTeamColors, Localize("Show names in chat in team colors"), &g_Config.m_ClChatTeamColors, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowKillMessages, Localize("Show kill messages"), &g_Config.m_ClShowKillMessages, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowVotesAfterVoting, Localize("Show votes window after voting"), &g_Config.m_ClShowVotesAfterVoting, &HUD, LineMargin);

	// Laser

	HUD.HSplitTop(15.0f, 0x0, &HUD);
	HUD.HSplitTop(50.0f, &Section, &HUD);
	Section.VSplitRight(110.0f, &Section, 0x0);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	static int LasterOutResetID, LaserInResetID;

	ColorHSLA LaserOutlineColor = DoLine_ColorPicker(&LasterOutResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Laser Outline Color"), &g_Config.m_ClLaserOutlineColor, ColorRGBA(0.074402f, 0.074402f, 0.247166f, 1.0f), false);

	HUD.HSplitTop(5.0f, 0x0, &HUD);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	ColorHSLA LaserInnerColor = DoLine_ColorPicker(&LaserInResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Laser Inner Color"), &g_Config.m_ClLaserInnerColor, ColorRGBA(0.498039f, 0.498039f, 1.0f, 1.0f), false);

	Section.VSplitLeft(30.0f, 0, &Section);

	DoLaserPreview(&Section, LaserOutlineColor, LaserInnerColor);

	// ***** Chat ***** //

	if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatOld, Localize("Use old chat style"), &g_Config.m_ClChatOld, &Chat, LineMargin))
		GameClient()->m_pChat->RebuildChat();

	Chat.HSplitTop(30.0f, 0x0, &Chat);

	// Message Colors and extra

	Chat.HSplitTop(20.0f, &Section, &Chat);
	Chat.HSplitTop(10.0f, 0x0, &Chat);

	UI()->DoLabelScaled(&Section, Localize("Messages"), 20.0f, -1);

	const float LineSize = 25.0f;
	const float WantedPickerPosition = 194.0f;
	const float LabelSize = 13.0f;
	const float LineSpacing = 5.0f;

	char aBuf[64];

	int i = 0;
	static int ResetIDs[24];

	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("System message"), &g_Config.m_ClMessageSystemColor, ColorRGBA(1.0f, 1.0f, 0.5f), true, true, &g_Config.m_ClShowChatSystem);
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Highlighted message"), &g_Config.m_ClMessageHighlightColor, ColorRGBA(1.0f, 0.5f, 0.5f));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Team message"), &g_Config.m_ClMessageTeamColor, ColorRGBA(0.65f, 1.0f, 0.65f));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Friend message"), &g_Config.m_ClMessageFriendColor, ColorRGBA(1.0f, 0.137f, 0.137f), true, true, &g_Config.m_ClMessageFriend);
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Normal message"), &g_Config.m_ClMessageColor, ColorRGBA(1.0f, 1.0f, 1.0f));

	str_format(aBuf, sizeof(aBuf), "%s (echo)", Localize("Client message"));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, aBuf, &g_Config.m_ClMessageClientColor, ColorRGBA(0.5f, 0.78f, 1.0f));

	// ***** Chat Preview ***** //

	Chat.HSplitTop(10.0f, 0x0, &Chat);
	Chat.HSplitTop(20.0f, &Section, &Chat);

	UI()->DoLabelScaled(&Section, Localize("Preview"), 20.0f, -1);

	Chat.HSplitTop(10.0f, 0x0, &Chat);
	RenderTools()->DrawUIRect(&Chat, ColorRGBA(1, 1, 1, 0.1f), CUI::CORNER_ALL, 8.0f);
	Chat.HSplitTop(10.0f, 0x0, &Chat);

	ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
	ColorRGBA NormalColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageColor));
	ColorRGBA ClientColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	ColorRGBA DefaultNameColor(0.8f, 0.8f, 0.8f, 1.0f);

	constexpr float RealFontSize = CChat::FONT_SIZE * 2;
	const float RealMsgPaddingX = (!g_Config.m_ClChatOld ? CChat::MESSAGE_PADDING_X : 0) * 2;
	const float RealMsgPaddingY = (!g_Config.m_ClChatOld ? CChat::MESSAGE_PADDING_Y : 0) * 2;
	const float RealMsgPaddingTee = (!g_Config.m_ClChatOld ? CChat::MESSAGE_TEE_SIZE + CChat::MESSAGE_TEE_PADDING_RIGHT : 0) * 2;
	const float RealOffsetY = RealFontSize + RealMsgPaddingY;

	const float X = 5.0f + RealMsgPaddingX / 2.0f + Chat.x;
	float Y = Chat.y;

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, X, Y, RealFontSize, TEXTFLAG_RENDER);

	str_copy(aBuf, Client()->PlayerName(), sizeof(aBuf));

	CAnimState *pIdleState = CAnimState::GetIdle();
	constexpr int PreviewTeeCount = 4;
	constexpr float RealTeeSize = CChat::MESSAGE_TEE_SIZE * 2;
	constexpr float RealTeeSizeHalved = CChat::MESSAGE_TEE_SIZE;
	constexpr float TWSkinUnreliableOffset = -0.25f;
	constexpr float OffsetTeeY = RealTeeSizeHalved;
	const float FullHeightMinusTee = RealOffsetY - RealTeeSize;

	CTeeRenderInfo RenderInfo[PreviewTeeCount];

	// Backgrounds first
	if(!g_Config.m_ClChatOld)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.12f);

		char LineBuilder[128];
		float Width;
		float TempY = Y;
		constexpr float RealBackgroundRounding = CChat::MESSAGE_ROUNDING * 2.0f;

		if(g_Config.m_ClShowChatSystem)
		{
			str_format(LineBuilder, sizeof(LineBuilder), "*** '%s' entered and joined the game", aBuf);
			Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
			RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
			TempY += RealOffsetY;
		}

		str_format(LineBuilder, sizeof(LineBuilder), "%sRandom Tee: Hey, how are you %s?", g_Config.m_ClShowIDs ? " 7: " : "", aBuf);
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%sYour Teammate: Let's speedrun this!", g_Config.m_ClShowIDs ? "11: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%s%sFriend: Hello there", g_Config.m_ClMessageFriend ? "♥ " : "", g_Config.m_ClShowIDs ? " 8: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%sSpammer [6]: Hey fools, I'm spamming here!", g_Config.m_ClShowIDs ? " 9: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		Width = TextRender()->TextWidth(0, RealFontSize, "*** Echo command executed", -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);

		Graphics()->QuadsEnd();

		// Load skins

		int DefaultInd = GameClient()->m_pSkins->Find("default");

		for(auto &i : RenderInfo)
		{
			i.m_Size = RealTeeSize;
			i.m_CustomColoredSkin = false;
		}

		int ind = -1;
		int i = 0;

		RenderInfo[i++].m_OriginalRenderSkin = GameClient()->m_pSkins->Get(DefaultInd)->m_OriginalSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_pSkins->Find("pinky")) != -1 ? GameClient()->m_pSkins->Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_pSkins->Find("cammostripes")) != -1 ? GameClient()->m_pSkins->Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_pSkins->Find("beast")) != -1 ? GameClient()->m_pSkins->Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
	}

	// System
	if(g_Config.m_ClShowChatSystem)
	{
		TextRender()->TextColor(SystemColor);
		TextRender()->TextEx(&Cursor, "*** '", -1);
		TextRender()->TextEx(&Cursor, aBuf, -1);
		TextRender()->TextEx(&Cursor, "' entered and joined the game", -1);
		TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);
	}

	// Highlighted
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	TextRender()->TextColor(DefaultNameColor);
	if(g_Config.m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 7: ", -1);
	TextRender()->TextEx(&Cursor, "Random Tee: ", -1);
	TextRender()->TextColor(HighlightedColor);
	TextRender()->TextEx(&Cursor, "Hey, how are you ", -1);
	TextRender()->TextEx(&Cursor, aBuf, -1);
	TextRender()->TextEx(&Cursor, "?", -1);
	if(!g_Config.m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[1], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Team
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	TextRender()->TextColor(TeamColor);
	if(g_Config.m_ClShowIDs)
		TextRender()->TextEx(&Cursor, "11: ", -1);
	TextRender()->TextEx(&Cursor, "Your Teammate: Let's speedrun this!", -1);
	if(!g_Config.m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[0], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Friend
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	if(g_Config.m_ClMessageFriend)
	{
		TextRender()->TextColor(FriendColor);
		TextRender()->TextEx(&Cursor, "♥ ", -1);
	}
	TextRender()->TextColor(DefaultNameColor);
	if(g_Config.m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 8: ", -1);
	TextRender()->TextEx(&Cursor, "Friend: ", -1);
	TextRender()->TextColor(NormalColor);
	TextRender()->TextEx(&Cursor, "Hello there", -1);
	if(!g_Config.m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[2], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Normal
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	TextRender()->TextColor(DefaultNameColor);
	if(g_Config.m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 9: ", -1);
	TextRender()->TextEx(&Cursor, "Spammer ", -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
	TextRender()->TextEx(&Cursor, "[6]", -1);
	TextRender()->TextColor(NormalColor);
	TextRender()->TextEx(&Cursor, ": Hey fools, I'm spamming here!", -1);
	if(!g_Config.m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[3], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Client
	TextRender()->TextColor(ClientColor);
	TextRender()->TextEx(&Cursor, "*** Echo command executed", -1);
	TextRender()->SetCursorPosition(&Cursor, X, Y);

	TextRender()->TextColor(1, 1, 1, 1);
}

void CMenus::RenderSettingsDDNet(CUIRect MainView)
{
	CUIRect Button, Left, Right, LeftLeft, Demo, Gameplay, Miscellaneous, Label, Background;

	bool CheckSettings = false;
	static int s_InpMouseOld = g_Config.m_InpMouseOld;

	MainView.HSplitTop(100.0f, &Demo, &MainView);

	Demo.HSplitTop(30.0f, &Label, &Demo);
	UI()->DoLabelScaled(&Label, Localize("Demo"), 20.0f, -1);
	Demo.Margin(5.0f, &Demo);
	Demo.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClAutoRaceRecord, Localize("Save the best demo of each race"), g_Config.m_ClAutoRaceRecord, &Button))
	{
		g_Config.m_ClAutoRaceRecord ^= 1;
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitLeft(160.0f, &LeftLeft, &Button);

		Button.VSplitLeft(140.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), Localize("Default length: %d"), g_Config.m_ClReplayLength);
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);

		int NewValue = (int)(DoScrollbarH(&g_Config.m_ClReplayLength, &Button, (minimum(g_Config.m_ClReplayLength, 600) - 10) / 590.0f) * 590.0f) + 10;
		if(g_Config.m_ClReplayLength < 600 || NewValue < 600)
			g_Config.m_ClReplayLength = minimum(NewValue, 600);

		if(DoButton_CheckBox(&g_Config.m_ClReplays, Localize("Enable replays"), g_Config.m_ClReplays, &LeftLeft))
		{
			g_Config.m_ClReplays ^= 1;
			if(!g_Config.m_ClReplays)
			{
				// stop recording and remove the tmp demo file
				Client()->DemoRecorder_Stop(RECORDER_REPLAYS, true);
			}
			else
			{
				// start recording
				Client()->DemoRecorder_HandleAutoStart();
			}
		}
	}

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClRaceGhost, Localize("Ghost"), g_Config.m_ClRaceGhost, &Button))
	{
		g_Config.m_ClRaceGhost ^= 1;
	}

	if(g_Config.m_ClRaceGhost)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClRaceShowGhost, Localize("Show ghost"), g_Config.m_ClRaceShowGhost, &Button))
		{
			g_Config.m_ClRaceShowGhost ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClRaceSaveGhost, Localize("Save ghost"), g_Config.m_ClRaceSaveGhost, &Button))
		{
			g_Config.m_ClRaceSaveGhost ^= 1;
		}
	}

	MainView.HSplitTop(330.0f, &Gameplay, &MainView);

	Gameplay.HSplitTop(30.0f, &Label, &Gameplay);
	UI()->DoLabelScaled(&Label, Localize("Gameplay"), 20.0f, -1);
	Gameplay.Margin(5.0f, &Gameplay);
	Gameplay.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitLeft(120.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Overlay entities"), 14.0f, -1);
		g_Config.m_ClOverlayEntities = (int)(DoScrollbarH(&g_Config.m_ClOverlayEntities, &Button, g_Config.m_ClOverlayEntities / 100.0f) * 100.0f);
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitMid(&LeftLeft, &Button);

		Button.VSplitLeft(50.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Size"), 14.0f, -1);
		g_Config.m_ClTextEntitiesSize = (int)(DoScrollbarH(&g_Config.m_ClTextEntitiesSize, &Button, g_Config.m_ClTextEntitiesSize / 100.0f) * 100.0f);

		if(DoButton_CheckBox(&g_Config.m_ClTextEntities, Localize("Show text entities"), g_Config.m_ClTextEntities, &LeftLeft))
		{
			g_Config.m_ClTextEntities ^= 1;
		}
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitMid(&LeftLeft, &Button);

		Button.VSplitLeft(50.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Alpha"), 14.0f, -1);
		g_Config.m_ClShowOthersAlpha = (int)(DoScrollbarH(&g_Config.m_ClShowOthersAlpha, &Button, g_Config.m_ClShowOthersAlpha / 100.0f) * 100.0f);

		if(DoButton_CheckBox(&g_Config.m_ClShowOthers, Localize("Show others"), g_Config.m_ClShowOthers == 1, &LeftLeft))
		{
			g_Config.m_ClShowOthers = g_Config.m_ClShowOthers != 1 ? 1 : 0;
		}
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	bool ShowOwnTeam = g_Config.m_ClShowOthers == 2;
	if(DoButton_CheckBox(&ShowOwnTeam, Localize("Show others (own team only)"), ShowOwnTeam, &Button))
	{
		g_Config.m_ClShowOthers = g_Config.m_ClShowOthers != 2 ? 2 : 0;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowQuads, Localize("Show quads"), g_Config.m_ClShowQuads, &Button))
	{
		g_Config.m_ClShowQuads ^= 1;
	}

	Right.HSplitTop(20.0f, &Label, &Right);
	Label.VSplitLeft(130.0f, &Label, &Button);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Default zoom"), g_Config.m_ClDefaultZoom);
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	//Right.HSplitTop(20.0f, &Button, 0);
	Button.HMargin(2.0f, &Button);
	g_Config.m_ClDefaultZoom = static_cast<int>(DoScrollbarH(&g_Config.m_ClDefaultZoom, &Button, g_Config.m_ClDefaultZoom / 20.0f) * 20.0f + 0.1f);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClAntiPing, Localize("AntiPing"), g_Config.m_ClAntiPing, &Button))
	{
		g_Config.m_ClAntiPing ^= 1;
	}

	if(g_Config.m_ClAntiPing)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingPlayers, Localize("AntiPing: predict other players"), g_Config.m_ClAntiPingPlayers, &Button))
		{
			g_Config.m_ClAntiPingPlayers ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingWeapons, Localize("AntiPing: predict weapons"), g_Config.m_ClAntiPingWeapons, &Button))
		{
			g_Config.m_ClAntiPingWeapons ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingGrenade, Localize("AntiPing: predict grenade paths"), g_Config.m_ClAntiPingGrenade, &Button))
		{
			g_Config.m_ClAntiPingGrenade ^= 1;
		}
	}
	else
	{
		Right.HSplitTop(60.0f, 0, &Right);
	}

	Right.HSplitTop(40.0f, 0, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowHookCollOther, Localize("Show other players' hook collision lines"), g_Config.m_ClShowHookCollOther, &Button))
	{
		g_Config.m_ClShowHookCollOther ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowDirection, Localize("Show other players' key presses"), g_Config.m_ClShowDirection, &Button))
	{
		g_Config.m_ClShowDirection ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_InpMouseOld, Localize("Old mouse mode"), g_Config.m_InpMouseOld, &Button))
	{
		g_Config.m_InpMouseOld ^= 1;
		CheckSettings = true;
	}

	if(CheckSettings)
		m_NeedRestartDDNet = s_InpMouseOld != g_Config.m_InpMouseOld;

	Left.HSplitTop(5.0f, &Button, &Left);
	Right.HSplitTop(25.0f, &Button, &Right);
	Left.VSplitRight(10.0f, &Left, 0x0);
	Right.VSplitLeft(10.0f, 0x0, &Right);
	Left.HSplitTop(25.0f, 0x0, &Left);
	CUIRect TempLabel;
	Left.HSplitTop(25.0f, &TempLabel, &Left);
	Left.HSplitTop(5.0f, 0x0, &Left);

	UI()->DoLabelScaled(&TempLabel, Localize("Background"), 20.0f, -1);

	Right.HSplitTop(25.0f, 0x0, &Right);
	Right.HSplitTop(25.0f, &TempLabel, &Right);
	Right.HSplitTop(5.0f, 0x0, &Miscellaneous);

	UI()->DoLabelScaled(&TempLabel, Localize("Miscellaneous"), 20.0f, -1);

	static int ResetID1 = 0;
	static int ResetID2 = 0;
	ColorRGBA GreyDefault(0.5f, 0.5f, 0.5f, 1);
	DoLine_ColorPicker(&ResetID2, 25.0f, 194.0f, 13.0f, 5.0f, &Left, Localize("Entities Background color"), &g_Config.m_ClBackgroundEntitiesColor, GreyDefault, false);

	static float s_Map = 0.0f;
	Left.VSplitLeft(5.0f, 0x0, &Left);
	Left.HSplitTop(25.0f, &Background, &Left);
	Background.HSplitTop(20.0f, &Background, 0);
	Background.VSplitLeft(100.0f, &Label, &TempLabel);
	UI()->DoLabelScaled(&Label, Localize("Map"), 14.0f, -1);
	DoEditBox(g_Config.m_ClBackgroundEntities, &TempLabel, g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities), 14.0f, &s_Map);

	Left.HSplitTop(20.0f, &Button, &Left);
	bool UseCurrentMap = str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0;
	if(DoButton_CheckBox(&UseCurrentMap, Localize("Use current map as background"), UseCurrentMap, &Button))
	{
		if(UseCurrentMap)
			g_Config.m_ClBackgroundEntities[0] = '\0';
		else
			str_copy(g_Config.m_ClBackgroundEntities, CURRENT_MAP, sizeof(g_Config.m_ClBackgroundEntities));
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClBackgroundShowTilesLayers, Localize("Show tiles layers from BG map"), g_Config.m_ClBackgroundShowTilesLayers, &Button))
		g_Config.m_ClBackgroundShowTilesLayers ^= 1;

	Miscellaneous.HSplitTop(25.0f, &Button, &Right);
	DoLine_ColorPicker(&ResetID1, 25.0f, 194.0f, 13.0f, 5.0f, &Button, Localize("Regular Background Color"), &g_Config.m_ClBackgroundColor, GreyDefault, false);
	Right.HSplitTop(5.0f, 0x0, &Right);
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClHttpMapDownload, Localize("Try fast HTTP map download first"), g_Config.m_ClHttpMapDownload, &Button))
		g_Config.m_ClHttpMapDownload ^= 1;

	static int s_ButtonTimeout = 0;
	Right.HSplitTop(10.0f, 0x0, &Right);
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_Menu(&s_ButtonTimeout, Localize("New random timeout code"), 0, &Button))
	{
		Client()->GenerateTimeoutSeed();
	}
	// Updater
#if defined(CONF_AUTOUPDATE)
	{
		MainView.VSplitMid(&Left, &Right);
		Left.w += 20.0f;
		Left.HSplitBottom(25.0f, 0x0, &Label);
		bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");
		char aBuf[256];
		int State = Updater()->GetCurrentState();

		// Update Button
		if(NeedUpdate && State <= IUpdater::CLEAN)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is available:"), Client()->LatestVersion());
			Label.VSplitLeft(TextRender()->TextWidth(0, 14.0f, aBuf, -1, -1.0f) + 10.0f, &Label, &Button);
			Button.VSplitLeft(100.0f, &Button, 0);
			static int s_ButtonUpdate = 0;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Update now"), 0, &Button))
			{
				Updater()->InitiateUpdate();
			}
		}
		else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
			str_format(aBuf, sizeof(aBuf), Localize("Updating..."));
		else if(State == IUpdater::NEED_RESTART)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet Client updated!"));
			m_NeedRestartUpdate = true;
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), Localize("No updates available"));
			Label.VSplitLeft(TextRender()->TextWidth(0, 14.0f, aBuf, -1, -1.0f) + 10.0f, &Label, &Button);
			Button.VSplitLeft(100.0f, &Button, 0);
			static int s_ButtonUpdate = 0;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Check now"), 0, &Button))
			{
				Client()->RequestDDNetInfo();
			}
		}
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
#endif
}
