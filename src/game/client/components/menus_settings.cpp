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
	static int s_ClShowConsole = Config()->m_ClShowConsole;
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
		bool IsDyncam = Config()->m_ClDyncam || Config()->m_ClMouseFollowfactor > 0;
		if(DoButton_CheckBox(&Config()->m_ClDyncam, Localize("Dynamic Camera"), IsDyncam, &Button))
		{
			if(IsDyncam)
			{
				Config()->m_ClDyncam = 0;
				Config()->m_ClMouseFollowfactor = 0;
			}
			else
			{
				Config()->m_ClDyncam = 1;
			}
		}

		// smooth dynamic camera
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(Config()->m_ClDyncam)
		{
			if(DoButton_CheckBox(&Config()->m_ClDyncamSmoothness, Localize("Smooth Dynamic Camera"), Config()->m_ClDyncamSmoothness, &Button))
			{
				if(Config()->m_ClDyncamSmoothness)
				{
					Config()->m_ClDyncamSmoothness = 0;
				}
				else
				{
					Config()->m_ClDyncamSmoothness = 50;
					Config()->m_ClDyncamStabilizing = 50;
				}
			}
		}

		// weapon pickup
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&Config()->m_ClAutoswitchWeapons, Localize("Switch weapon on pickup"), Config()->m_ClAutoswitchWeapons, &Button))
			Config()->m_ClAutoswitchWeapons ^= 1;

		// weapon out of ammo autoswitch
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&Config()->m_ClAutoswitchWeaponsOutOfAmmo, Localize("Switch weapon when out of ammo"), Config()->m_ClAutoswitchWeaponsOutOfAmmo, &Button))
			Config()->m_ClAutoswitchWeaponsOutOfAmmo ^= 1;

		// weapon reset on death
		Left.HSplitTop(5.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&Config()->m_ClResetWantedWeaponOnDeath, Localize("Reset wanted weapon on death"), Config()->m_ClResetWantedWeaponOnDeath, &Button))
			Config()->m_ClResetWantedWeaponOnDeath ^= 1;

		// chat messages
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClShowChatFriends, Localize("Show only chat messages from friends"), Config()->m_ClShowChatFriends, &Button))
			Config()->m_ClShowChatFriends ^= 1;

		// name plates
		Right.HSplitTop(5.0f, 0, &Right);
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClNameplates, Localize("Show name plates"), Config()->m_ClNameplates, &Button))
			Config()->m_ClNameplates ^= 1;

		if(Config()->m_ClNameplates)
		{
			Right.HSplitTop(2.5f, 0, &Right);
			Right.HSplitTop(20.0f, &Label, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Name plates size"), Config()->m_ClNameplatesSize);
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Button.HMargin(2.0f, &Button);
			Config()->m_ClNameplatesSize = (int)(DoScrollbarH(&Config()->m_ClNameplatesSize, &Button, Config()->m_ClNameplatesSize / 100.0f) * 100.0f + 0.1f);

			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&Config()->m_ClNameplatesTeamcolors, Localize("Use team colors for name plates"), Config()->m_ClNameplatesTeamcolors, &Button))
				Config()->m_ClNameplatesTeamcolors ^= 1;

			Right.HSplitTop(5.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&Config()->m_ClNameplatesClan, Localize("Show clan above name plates"), Config()->m_ClNameplatesClan, &Button))
				Config()->m_ClNameplatesClan ^= 1;

			if(Config()->m_ClNameplatesClan)
			{
				Right.HSplitTop(2.5f, 0, &Right);
				Right.HSplitTop(20.0f, &Label, &Right);
				Right.HSplitTop(20.0f, &Button, &Right);
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Clan plates size"), Config()->m_ClNameplatesClanSize);
				UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
				Button.HMargin(2.0f, &Button);
				Config()->m_ClNameplatesClanSize = (int)(DoScrollbarH(&Config()->m_ClNameplatesClanSize, &Button, Config()->m_ClNameplatesClanSize / 100.0f) * 100.0f + 0.1f);
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
		if(DoButton_CheckBox(&Config()->m_ClSkipStartMenu, Localize("Skip the main menu"), Config()->m_ClSkipStartMenu, &Button))
			Config()->m_ClSkipStartMenu ^= 1;

		float SliderGroupMargin = 10.0f;

		// auto demo settings
		{
			Right.HSplitTop(40.0f, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&Config()->m_ClAutoDemoRecord, Localize("Automatically record demos"), Config()->m_ClAutoDemoRecord, &Button))
				Config()->m_ClAutoDemoRecord ^= 1;

			Right.HSplitTop(20.0f, &Label, &Right);
			char aBuf[64];
			if(Config()->m_ClAutoDemoMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max demos"), Config()->m_ClAutoDemoMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max demos"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			Config()->m_ClAutoDemoMax = static_cast<int>(DoScrollbarH(&Config()->m_ClAutoDemoMax, &Button, Config()->m_ClAutoDemoMax / 1000.0f) * 1000.0f + 0.1f);

			Right.HSplitTop(SliderGroupMargin, 0, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&Config()->m_ClAutoScreenshot, Localize("Automatically take game over screenshot"), Config()->m_ClAutoScreenshot, &Button))
				Config()->m_ClAutoScreenshot ^= 1;

			Right.HSplitTop(20.0f, &Label, &Right);
			if(Config()->m_ClAutoScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), Config()->m_ClAutoScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			Config()->m_ClAutoScreenshotMax = static_cast<int>(DoScrollbarH(&Config()->m_ClAutoScreenshotMax, &Button, Config()->m_ClAutoScreenshotMax / 1000.0f) * 1000.0f + 0.1f);
		}

		Left.HSplitTop(10.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Label, &Left);
		char aBuf[64];
		if(Config()->m_ClRefreshRate)
			str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), Config()->m_ClRefreshRate);
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Refresh Rate"), "∞");
		UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.HMargin(2.0f, &Button);
		Config()->m_ClRefreshRate = static_cast<int>(DoScrollbarH(&Config()->m_ClRefreshRate, &Button, Config()->m_ClRefreshRate / 10000.0f) * 10000.0f + 0.1f);

#if defined(CONF_FAMILY_WINDOWS)
		Left.HSplitTop(10.0f, 0, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&Config()->m_ClShowConsole, Localize("Show console window"), Config()->m_ClShowConsole, &Button))
		{
			Config()->m_ClShowConsole ^= 1;
			CheckSettings = true;
		}

		if(CheckSettings)
			m_NeedRestartGeneral = s_ClShowConsole != Config()->m_ClShowConsole;
#endif

		Left.HSplitTop(15.0f, 0, &Left);
		CUIRect DirectoryButton;
		Left.HSplitBottom(25.0f, &Left, &DirectoryButton);
		RenderThemeSelection(Left);

		DirectoryButton.HSplitTop(5.0f, 0, &DirectoryButton);
		static int s_ThemesButtonID = 0;
		if(DoButton_Menu(&s_ThemesButtonID, Localize("Themes directory"), 0, &DirectoryButton))
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
			if(DoButton_CheckBox(&Config()->m_ClAutoStatboardScreenshot,
				   Localize("Automatically take statboard screenshot"),
				   Config()->m_ClAutoStatboardScreenshot, &Button))
			{
				Config()->m_ClAutoStatboardScreenshot ^= 1;
			}

			Right.HSplitTop(20.0f, &Label, &Right);
			if(Config()->m_ClAutoStatboardScreenshotMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), Config()->m_ClAutoStatboardScreenshotMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			Config()->m_ClAutoStatboardScreenshotMax =
				static_cast<int>(DoScrollbarH(&Config()->m_ClAutoStatboardScreenshotMax,
							 &Button,
							 Config()->m_ClAutoStatboardScreenshotMax / 1000.0f) *
							 1000.0f +
						 0.1f);
		}

		// auto statboard csv
		{
			Right.HSplitTop(SliderGroupMargin, 0, &Right); //
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&Config()->m_ClAutoCSV,
				   Localize("Automatically create statboard csv"),
				   Config()->m_ClAutoCSV, &Button))
			{
				Config()->m_ClAutoCSV ^= 1;
			}

			Right.HSplitTop(20.0f, &Label, &Right);
			if(Config()->m_ClAutoCSVMax)
				str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max CSVs"), Config()->m_ClAutoCSVMax);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max CSVs"), "∞");
			UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
			Right.HSplitTop(20.0f, &Button, &Right);
			Button.HMargin(2.0f, &Button);
			Config()->m_ClAutoCSVMax =
				static_cast<int>(DoScrollbarH(&Config()->m_ClAutoCSVMax,
							 &Button,
							 Config()->m_ClAutoCSVMax / 1000.0f) *
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

	char *pName = Config()->m_PlayerName;
	const char *pNameFallback = Client()->PlayerName();
	char *pClan = Config()->m_PlayerClan;
	int *pCountry = &Config()->m_PlayerCountry;

	if(m_Dummy)
	{
		pName = Config()->m_ClDummyName;
		pNameFallback = Client()->DummyName();
		pClan = Config()->m_ClDummyClan;
		pCountry = &Config()->m_ClDummyCountry;
	}

	// player name
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	static float s_OffsetName = 0.0f;
	if(DoEditBox(pName, &Button, pName, sizeof(Config()->m_PlayerName), 14.0f, &s_OffsetName, false, CUI::CORNER_ALL, pNameFallback))
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
	if(DoEditBox(pClan, &Button, pClan, sizeof(Config()->m_PlayerClan), 14.0f, &s_OffsetClan))
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
	UiDoListboxStart(&s_ScrollValue, &MainView, 50.0f, Localize("Country / Region"), "", m_pClient->m_CountryFlags.Num(), 6, OldSelected, s_ScrollValue);

	for(int i = 0; i < m_pClient->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_CountryFlags.GetByIndex(i);
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
			m_pClient->m_CountryFlags.Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
			if(pEntry->m_Texture.IsValid())
				UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
		}
	}

	bool Clicked = false;
	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0, &Clicked);
	if(Clicked)
		s_ListBoxUsed = true;

	if(OldSelected != NewSelected)
	{
		*pCountry = m_pClient->m_CountryFlags.GetByIndex(NewSelected)->m_CountryCode;
		SetNeedSendInfo();
	}
}

struct CUISkin
{
	const CSkin *m_pSkin;

	CUISkin() :
		m_pSkin(nullptr) {}
	CUISkin(const CSkin *pSkin) :
		m_pSkin(pSkin) {}

	bool operator<(const CUISkin &Other) const { return str_comp_nocase(m_pSkin->m_aName, Other.m_pSkin->m_aName) < 0; }

	bool operator<(const char *pOther) const { return str_comp_nocase(m_pSkin->m_aName, pOther) < 0; }
	bool operator==(const char *pOther) const { return !str_comp_nocase(m_pSkin->m_aName, pOther); }
};

void CMenus::RenderSettingsTee(CUIRect MainView)
{
	CUIRect Button, Label, Button2, Dummy, DummyLabel, SkinList, QuickSearch, QuickSearchClearButton, SkinDB, SkinPrefix, SkinPrefixLabel, DirectoryButton, RefreshButton;

	static float s_ClSkinPrefix = 0.0f;

	static bool s_InitSkinlist = true;
	MainView.HSplitTop(10.0f, 0, &MainView);

	char *Skin = Config()->m_ClPlayerSkin;
	int *UseCustomColor = &Config()->m_ClPlayerUseCustomColor;
	unsigned *ColorBody = &Config()->m_ClPlayerColorBody;
	unsigned *ColorFeet = &Config()->m_ClPlayerColorFeet;

	if(m_Dummy)
	{
		Skin = Config()->m_ClDummySkin;
		UseCustomColor = &Config()->m_ClDummyUseCustomColor;
		ColorBody = &Config()->m_ClDummyColorBody;
		ColorFeet = &Config()->m_ClDummyColorFeet;
	}

	// skin info
	CTeeRenderInfo OwnSkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Get(m_pClient->m_Skins.Find(Skin));
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

	if(DoButton_CheckBox(&Config()->m_ClDownloadSkins, Localize("Download skins"), Config()->m_ClDownloadSkins, &DummyLabel))
	{
		Config()->m_ClDownloadSkins ^= 1;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&Config()->m_ClVanillaSkinsOnly, Localize("Vanilla skins only"), Config()->m_ClVanillaSkinsOnly, &DummyLabel))
	{
		Config()->m_ClVanillaSkinsOnly ^= 1;
		s_InitSkinlist = true;
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	if(DoButton_CheckBox(&Config()->m_ClFatSkins, Localize("Fat skins (DDFat)"), Config()->m_ClFatSkins, &DummyLabel))
	{
		Config()->m_ClFatSkins ^= 1;
	}

	SkinPrefix.HSplitTop(20.0f, &SkinPrefixLabel, &SkinPrefix);
	UI()->DoLabelScaled(&SkinPrefixLabel, Localize("Skin prefix"), 14.0f, -1);

	SkinPrefix.HSplitTop(20.0f, &SkinPrefixLabel, &SkinPrefix);
	{
		static int s_ClearButton = 0;
		DoClearableEditBox(Config()->m_ClSkinPrefix, &s_ClearButton, &SkinPrefixLabel, Config()->m_ClSkinPrefix, sizeof(Config()->m_ClSkinPrefix), 14.0f, &s_ClSkinPrefix);
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
				str_copy(Config()->m_ClSkinPrefix, pPrefix, sizeof(Config()->m_ClSkinPrefix));
			}
		}
	}

	Dummy.HSplitTop(20.0f, &DummyLabel, &Dummy);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(230.0f, &Label, 0);
	CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
	vec2 TeeRenderPos(Label.x + 30.0f, Label.y + Label.h / 2.0f + OffsetToMid.y);
	RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, 0, vec2(1, 0), TeeRenderPos);
	Label.VSplitLeft(70.0f, 0, &Label);
	Label.HMargin(15.0f, &Label);
	// UI()->DoLabelScaled(&Label, Skin, 14.0f, -1, 150.0f);
	static float s_OffsetSkin = 0.0f;
	static int s_ClearButton = 0;
	if(DoClearableEditBox(Skin, &s_ClearButton, &Label, Skin, sizeof(Config()->m_ClPlayerSkin), 14.0f, &s_OffsetSkin, false, CUI::CORNER_ALL, "default"))
	{
		SetNeedSendInfo();
	}

	// custom color selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitMid(&Button, &Button2);
	static int s_CustomColorID = 0;
	if(DoButton_CheckBox(&s_CustomColorID, Localize("Custom colors"), *UseCustomColor, &Button))
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
			RenderHSLScrollbars(&aRects[i], paColors[i], false, true);

			if(PrevColor != *paColors[i])
			{
				SetNeedSendInfo();
			}
		}
	}

	// skin selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(230.0f, &SkinList, &MainView);
	static sorted_array<CUISkin> s_paSkinList;
	static int s_SkinCount = 0;
	static float s_ScrollValue = 0.0f;
	if(s_InitSkinlist || m_pClient->m_Skins.Num() != s_SkinCount)
	{
		s_paSkinList.clear();
		for(int i = 0; i < m_pClient->m_Skins.Num(); ++i)
		{
			const CSkin *s = m_pClient->m_Skins.Get(i);

			// filter quick search
			if(Config()->m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(s->m_aName, Config()->m_ClSkinFilterString))
				continue;

			// no special skins
			if((s->m_aName[0] == 'x' && s->m_aName[1] == '_'))
				continue;

			// vanilla skins only
			if(Config()->m_ClVanillaSkinsOnly && !s->m_IsVanilla)
				continue;

			if(s == 0)
				continue;

			s_paSkinList.add(CUISkin(s));
		}
		s_InitSkinlist = false;
		s_SkinCount = m_pClient->m_Skins.Num();
	}

	int OldSelected = -1;
	UiDoListboxStart(&s_InitSkinlist, &SkinList, 50.0f, Localize("Skins"), "", s_paSkinList.size(), 4, OldSelected, s_ScrollValue);
	for(int i = 0; i < s_paSkinList.size(); ++i)
	{
		const CSkin *s = s_paSkinList[i].m_pSkin;

		if(str_comp(s->m_aName, Skin) == 0)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(s_paSkinList[i].m_pSkin, OldSelected == i);
		char aBuf[128];
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info = OwnSkinInfo;
			Info.m_CustomColoredSkin = *UseCustomColor;

			Info.m_OriginalRenderSkin = s->m_OriginalSkin;
			Info.m_ColorableRenderSkin = s->m_ColorableSkin;
			Info.m_SkinMetrics = s->m_Metrics;

			RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &Info, OffsetToMid);
			TeeRenderPos = vec2(Item.m_Rect.x + 30, Item.m_Rect.y + Item.m_Rect.h / 2 + OffsetToMid.y);
			RenderTools()->RenderTee(pIdleState, &Info, 0, vec2(1.0f, 0.0f), TeeRenderPos);

			Item.m_Rect.VSplitLeft(60.0f, 0, &Item.m_Rect);
			str_format(aBuf, sizeof(aBuf), "%s", s->m_aName);
			RenderTools()->UI()->DoLabelScaled(&Item.m_Rect, aBuf, 12.0f, -1, Item.m_Rect.w);
			if(Config()->m_Debug)
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
		mem_copy(Skin, s_paSkinList[NewSelected].m_pSkin->m_aName, sizeof(Config()->m_ClPlayerSkin));
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
			UI()->SetActiveItem(&Config()->m_ClSkinFilterString);
		if(DoClearableEditBox(&Config()->m_ClSkinFilterString, &s_ClearButton, &QuickSearch, Config()->m_ClSkinFilterString, sizeof(Config()->m_ClSkinFilterString), 14.0f, &Offset, false, CUI::CORNER_ALL, Localize("Search")))
			s_InitSkinlist = true;
	}

	SkinDB.VSplitLeft(150.0f, &SkinDB, &DirectoryButton);
	SkinDB.HSplitTop(5.0f, 0, &SkinDB);
	static int s_SkinDBDirID = 0;
	if(DoButton_Menu(&s_SkinDBDirID, Localize("Skin Database"), 0, &SkinDB))
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
	static int s_DirectoryButtonID = 0;
	if(DoButton_Menu(&s_DirectoryButtonID, Localize("Skins directory"), 0, &DirectoryButton))
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
	static int s_SkinRefreshButtonID = 0;
	if(DoButton_Menu(&s_SkinRefreshButtonID, "\xEE\x97\x95", 0, &RefreshButton, NULL, 15, 5, 0, vec4(1.0f, 1.0f, 1.0f, 0.75f), vec4(1, 1, 1, 0.5f), 0))
	{
		m_pClient->m_Skins.Refresh();
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
	Localize("Lock team");Localize("Show entities");Localize("Show HUD");Localize("Chat command");
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
					m_pClient->m_Binds.Bind(OldId, "", false, OldModifier);
				if(NewId != 0)
					m_pClient->m_Binds.Bind(NewId, gs_aKeys[i].m_pCommand, false, NewModifier);
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
			const char *pBind = m_pClient->m_Binds.Get(KeyId, Mod);
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
	static int s_OldSelected = 0;
	// Hacky values: Size of 10.0f per item for smoother scrolling, 72 elements
	// fits the current size of controls settings
	UiDoListboxStart(&s_ControlsList, &MainView, 10.0f, Localize("Controls"), "", 72, 1, s_SelectedControl, s_ScrollValue);

	CUIRect MovementSettings, WeaponSettings, VotingSettings, ChatSettings, DummySettings, MiscSettings, ResetButton;
	CListboxItem Item = UiDoListboxNextItem(&s_OldSelected, false, false, true);
	Item.m_Rect.HSplitTop(10.0f, 0, &Item.m_Rect);
	Item.m_Rect.VSplitMid(&MovementSettings, &VotingSettings);

	// movement settings
	{
		MovementSettings.VMargin(5.0f, &MovementSettings);
		MovementSettings.HSplitTop(445.0f, &MovementSettings, &WeaponSettings);
		RenderTools()->DrawUIRect(&MovementSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		MovementSettings.VMargin(10.0f, &MovementSettings);

		TextRender()->Text(0, MovementSettings.x, MovementSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Movement"), -1.0f);

		MovementSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &MovementSettings);

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Mouse sens."), Config()->m_InpMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f * UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&Config()->m_InpMousesens, &Button, (minimum(Config()->m_InpMousesens, 500) - 1) / 500.0f) * 500.0f) + 1;
			if(Config()->m_InpMousesens < 500 || NewValue < 500)
				Config()->m_InpMousesens = minimum(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		{
			CUIRect Button, Label;
			MovementSettings.HSplitTop(20.0f, &Button, &MovementSettings);
			Button.VSplitLeft(160.0f, &Label, &Button);
			str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("UI mouse s."), Config()->m_UiMousesens);
			UI()->DoLabel(&Label, aBuf, 14.0f * UI()->Scale(), -1);
			Button.HMargin(2.0f, &Button);
			int NewValue = (int)(DoScrollbarH(&Config()->m_UiMousesens, &Button, (minimum(Config()->m_UiMousesens, 500) - 1) / 500.0f) * 500.0f) + 1;
			if(Config()->m_UiMousesens < 500 || NewValue < 500)
				Config()->m_UiMousesens = minimum(NewValue, 500);
			MovementSettings.HSplitTop(20.0f, 0, &MovementSettings);
		}

		UiDoGetButtons(0, 15, MovementSettings, MainView);
	}

	// weapon settings
	{
		WeaponSettings.HSplitTop(10.0f, 0, &WeaponSettings);
		WeaponSettings.HSplitTop(190.0f, &WeaponSettings, &ResetButton);
		RenderTools()->DrawUIRect(&WeaponSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		WeaponSettings.VMargin(10.0f, &WeaponSettings);

		TextRender()->Text(0, WeaponSettings.x, WeaponSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Weapon"), -1.0f);

		WeaponSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &WeaponSettings);
		UiDoGetButtons(15, 22, WeaponSettings, MainView);
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
			m_pClient->m_Binds.SetDefaults();
	}

	// voting settings
	{
		VotingSettings.VMargin(5.0f, &VotingSettings);
		VotingSettings.HSplitTop(80.0f, &VotingSettings, &ChatSettings);
		RenderTools()->DrawUIRect(&VotingSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		VotingSettings.VMargin(10.0f, &VotingSettings);

		TextRender()->Text(0, VotingSettings.x, VotingSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Voting"), -1.0f);

		VotingSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &VotingSettings);
		UiDoGetButtons(22, 24, VotingSettings, MainView);
	}

	// chat settings
	{
		ChatSettings.HSplitTop(10.0f, 0, &ChatSettings);
		ChatSettings.HSplitTop(145.0f, &ChatSettings, &DummySettings);
		RenderTools()->DrawUIRect(&ChatSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		ChatSettings.VMargin(10.0f, &ChatSettings);

		TextRender()->Text(0, ChatSettings.x, ChatSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Chat"), -1.0f);

		ChatSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &ChatSettings);
		UiDoGetButtons(24, 29, ChatSettings, MainView);
	}

	// dummy settings
	{
		DummySettings.HSplitTop(10.0f, 0, &DummySettings);
		DummySettings.HSplitTop(100.0f, &DummySettings, &MiscSettings);
		RenderTools()->DrawUIRect(&DummySettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		DummySettings.VMargin(10.0f, &DummySettings);

		TextRender()->Text(0, DummySettings.x, DummySettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Dummy"), -1.0f);

		DummySettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &DummySettings);
		UiDoGetButtons(29, 32, DummySettings, MainView);
	}

	// misc settings
	{
		MiscSettings.HSplitTop(10.0f, 0, &MiscSettings);
		MiscSettings.HSplitTop(300.0f, &MiscSettings, 0);
		RenderTools()->DrawUIRect(&MiscSettings, ColorRGBA(1, 1, 1, 0.25f), CUI::CORNER_ALL, 10.0f);
		MiscSettings.VMargin(10.0f, &MiscSettings);

		TextRender()->Text(0, MiscSettings.x, MiscSettings.y + (14.0f + 5.0f + 10.0f - 14.0f * UI()->Scale()) / 2.f, 14.0f * UI()->Scale(), Localize("Miscellaneous"), -1.0f);

		MiscSettings.HSplitTop(14.0f + 5.0f + 10.0f, 0, &MiscSettings);
		UiDoGetButtons(32, 44, MiscSettings, MainView);
	}

	UiDoListboxEnd(&s_ScrollValue, 0);
}

int CMenus::RenderDropDown(int &CurDropDownState, CUIRect *pRect, int CurSelection, const void **pIDs, const char **pStr, int PickNum, const void *pID, float &ScrollVal)
{
	if(CurDropDownState != 0)
	{
		CUIRect ListRect;
		pRect->HSplitTop(24.0f * PickNum, &ListRect, pRect);
		char aBuf[1024];
		UiDoListboxStart(&pID, &ListRect, 24.0f, "", aBuf, PickNum, 1, CurSelection, ScrollVal);
		for(int i = 0; i < PickNum; ++i)
		{
			CListboxItem Item = UiDoListboxNextItem(pIDs[i], CurSelection == i);
			if(Item.m_Visible)
			{
				str_format(aBuf, sizeof(aBuf), "%s", pStr[i]);
				UI()->DoLabelScaled(&Item.m_Rect, aBuf, 16.0f, 0);
			}
		}
		bool ClickedItem = false;
		int NewIndex = UiDoListboxEnd(&ScrollVal, NULL, &ClickedItem);
		if(ClickedItem)
		{
			CurDropDownState = 0;
			return NewIndex;
		}
		else
			return CurSelection;
	}
	else
	{
		CUIRect Button;
		pRect->HSplitTop(24.0f, &Button, pRect);
		if(DoButton_MenuTab(pID, CurSelection > -1 ? pStr[CurSelection] : "", 0, &Button, CUI::CORNER_ALL, NULL, NULL, NULL, NULL, 4.0f))
			CurDropDownState = 1;
		return CurSelection;
	}
}

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button, Label;
	char aBuf[128];
	bool CheckSettings = false;

	static const int MAX_RESOLUTIONS = 256;
	static CVideoMode s_aModes[MAX_RESOLUTIONS];
	static int s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, Config()->m_GfxScreen);
	static int s_GfxScreenWidth = Config()->m_GfxScreenWidth;
	static int s_GfxScreenHeight = Config()->m_GfxScreenHeight;
	static int s_GfxColorDepth = Config()->m_GfxColorDepth;
	static int s_GfxVsync = Config()->m_GfxVsync;
	static int s_GfxFsaaSamples = Config()->m_GfxFsaaSamples;
	static int s_GfxOpenGLVersion = Graphics()->IsConfigModernAPI();
	static int s_GfxEnableTextureUnitOptimization = Config()->m_GfxEnableTextureUnitOptimization;
	static int s_GfxUsePreinitBuffer = Config()->m_GfxUsePreinitBuffer;
	static int s_GfxHighdpi = Config()->m_GfxHighdpi;

	CUIRect ModeList;
	MainView.VSplitLeft(350.0f, &MainView, &ModeList);
	MainView.VSplitLeft(340.0f, &MainView, 0);

	// display mode list
	static float s_ScrollValue = 0;
	int OldSelected = -1;
	int G = gcd(s_GfxScreenWidth, s_GfxScreenHeight);
	str_format(aBuf, sizeof(aBuf), "%s: %dx%d %d bit (%d:%d)", Localize("Current"), s_GfxScreenWidth, s_GfxScreenHeight, s_GfxColorDepth, s_GfxScreenWidth / G, s_GfxScreenHeight / G);
	UiDoListboxStart(&s_NumNodes, &ModeList, 24.0f, Localize("Display Modes"), aBuf, s_NumNodes, 1, OldSelected, s_ScrollValue);

	for(int i = 0; i < s_NumNodes; ++i)
	{
		const int Depth = s_aModes[i].m_Red + s_aModes[i].m_Green + s_aModes[i].m_Blue > 16 ? 24 : 16;
		if(Config()->m_GfxColorDepth == Depth &&
			Config()->m_GfxScreenWidth == s_aModes[i].m_WindowWidth &&
			Config()->m_GfxScreenHeight == s_aModes[i].m_WindowHeight)
		{
			OldSelected = i;
		}

		CListboxItem Item = UiDoListboxNextItem(&s_aModes[i], OldSelected == i);
		if(Item.m_Visible)
		{
			int G = gcd(s_aModes[i].m_CanvasWidth, s_aModes[i].m_CanvasHeight);
			str_format(aBuf, sizeof(aBuf), " %dx%d %d bit (%d:%d)", s_aModes[i].m_CanvasWidth, s_aModes[i].m_CanvasHeight, Depth, s_aModes[i].m_CanvasWidth / G, s_aModes[i].m_CanvasHeight / G);
			UI()->DoLabelScaled(&Item.m_Rect, aBuf, 16.0f, -1);
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		const int Depth = s_aModes[NewSelected].m_Red + s_aModes[NewSelected].m_Green + s_aModes[NewSelected].m_Blue > 16 ? 24 : 16;
		Config()->m_GfxColorDepth = Depth;
		Config()->m_GfxScreenWidth = s_aModes[NewSelected].m_WindowWidth;
		Config()->m_GfxScreenHeight = s_aModes[NewSelected].m_WindowHeight;
		Graphics()->Resize(Config()->m_GfxScreenWidth, Config()->m_GfxScreenHeight, true);
	}

	// switches
	static float s_ScrollValueDrop = 0;
	static const int s_NumWindowMode = 4;
	static int s_aWindowModeIDs[s_NumWindowMode];
	const void *aWindowModeIDs[s_NumWindowMode];
	for(int i = 0; i < s_NumWindowMode; ++i)
		aWindowModeIDs[i] = &s_aWindowModeIDs[i];
	static int s_WindowModeDropDownState = 0;
	const char *pWindowModes[] = {Localize("Windowed"), Localize("Windowed borderless"), Localize("Desktop fullscreen"), Localize("Fullscreen")};

	OldSelected = (Config()->m_GfxFullscreen ? (Config()->m_GfxFullscreen == 1 ? 3 : 2) : (Config()->m_GfxBorderless ? 1 : 0));

	const int NewWindowMode = RenderDropDown(s_WindowModeDropDownState, &MainView, OldSelected, aWindowModeIDs, pWindowModes, s_NumWindowMode, &s_NumWindowMode, s_ScrollValueDrop);
	if(OldSelected != NewWindowMode)
	{
		if(NewWindowMode == 0)
			Client()->SetWindowParams(0, false);
		else if(NewWindowMode == 1)
			Client()->SetWindowParams(0, true);
		else if(NewWindowMode == 2)
			Client()->SetWindowParams(2, false);
		else if(NewWindowMode == 3)
			Client()->SetWindowParams(1, false);
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("V-Sync"), Localize("may cause delay"));
	if(DoButton_CheckBox(&Config()->m_GfxVsync, aBuf, Config()->m_GfxVsync, &Button))
	{
		Client()->ToggleWindowVSync();
	}

	if(Graphics()->GetNumScreens() > 1)
	{
		int NumScreens = Graphics()->GetNumScreens();
		MainView.HSplitTop(20.0f, &Button, &MainView);
		int Screen_MouseButton = DoButton_CheckBox_Number(&Config()->m_GfxScreen, Localize("Screen"), Config()->m_GfxScreen, &Button);
		if(Screen_MouseButton == 1) //inc
		{
			Client()->SwitchWindowScreen((Config()->m_GfxScreen + 1) % NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, Config()->m_GfxScreen);
		}
		else if(Screen_MouseButton == 2) //dec
		{
			Client()->SwitchWindowScreen((Config()->m_GfxScreen - 1 + NumScreens) % NumScreens);
			s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, Config()->m_GfxScreen);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("FSAA samples"), Localize("may cause delay"));
	int GfxFsaaSamples_MouseButton = DoButton_CheckBox_Number(&Config()->m_GfxFsaaSamples, aBuf, Config()->m_GfxFsaaSamples, &Button);
	if(GfxFsaaSamples_MouseButton == 1) //inc
	{
		Config()->m_GfxFsaaSamples = (Config()->m_GfxFsaaSamples + 1) % 17;
		CheckSettings = true;
	}
	else if(GfxFsaaSamples_MouseButton == 2) //dec
	{
		Config()->m_GfxFsaaSamples = (Config()->m_GfxFsaaSamples - 1 + 17) % 17;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_GfxHighDetail, Localize("High Detail"), Config()->m_GfxHighDetail, &Button))
		Config()->m_GfxHighDetail ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	bool IsNewOpenGL = Graphics()->IsConfigModernAPI();

	if(DoButton_CheckBox(&Config()->m_GfxOpenGLMajor, Localize("Use modern OpenGL"), IsNewOpenGL, &Button))
	{
		CheckSettings = true;
		if(IsNewOpenGL)
		{
			Graphics()->GetDriverVersion(GRAPHICS_DRIVER_AGE_TYPE_DEFAULT, Config()->m_GfxOpenGLMajor, Config()->m_GfxOpenGLMinor, Config()->m_GfxOpenGLPatch);
			IsNewOpenGL = false;
		}
		else
		{
			Graphics()->GetDriverVersion(GRAPHICS_DRIVER_AGE_TYPE_MODERN, Config()->m_GfxOpenGLMajor, Config()->m_GfxOpenGLMinor, Config()->m_GfxOpenGLPatch);
			IsNewOpenGL = true;
		}
	}

	if(IsNewOpenGL)
	{
		MainView.HSplitTop(20.0f, &Button, &MainView);
		if(DoButton_CheckBox(&Config()->m_GfxUsePreinitBuffer, Localize("Preinit VBO (iGPUs only)"), Config()->m_GfxUsePreinitBuffer, &Button))
		{
			CheckSettings = true;
			Config()->m_GfxUsePreinitBuffer ^= 1;
		}

		MainView.HSplitTop(20.0f, &Button, &MainView);
		if(DoButton_CheckBox(&Config()->m_GfxEnableTextureUnitOptimization, Localize("Multiple texture units (disable for macOS)"), Config()->m_GfxEnableTextureUnitOptimization, &Button))
		{
			CheckSettings = true;
			Config()->m_GfxEnableTextureUnitOptimization ^= 1;
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_GfxHighdpi, Localize("Use high DPI"), Config()->m_GfxHighdpi, &Button))
	{
		CheckSettings = true;
		Config()->m_GfxHighdpi ^= 1;
	}

	// check if the new settings require a restart
	if(CheckSettings)
	{
		if(s_GfxScreenWidth == Config()->m_GfxScreenWidth &&
			s_GfxScreenHeight == Config()->m_GfxScreenHeight &&
			s_GfxColorDepth == Config()->m_GfxColorDepth &&
			s_GfxVsync == Config()->m_GfxVsync &&
			s_GfxFsaaSamples == Config()->m_GfxFsaaSamples &&
			s_GfxOpenGLVersion == (int)IsNewOpenGL &&
			s_GfxUsePreinitBuffer == Config()->m_GfxUsePreinitBuffer &&
			s_GfxEnableTextureUnitOptimization == Config()->m_GfxEnableTextureUnitOptimization &&
			s_GfxHighdpi == Config()->m_GfxHighdpi)
			m_NeedRestartGraphics = false;
		else
			m_NeedRestartGraphics = true;
	}

	MainView.HSplitTop(20.0f, &Label, &MainView);
	Label.VSplitLeft(160.0f, &Label, &Button);
	if(Config()->m_GfxRefreshRate)
		str_format(aBuf, sizeof(aBuf), "%s: %i Hz", Localize("Refresh Rate"), Config()->m_GfxRefreshRate);
	else
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Refresh Rate"), "∞");
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	Button.HMargin(2.0f, &Button);
	int NewRefreshRate = static_cast<int>(DoScrollbarH(&Config()->m_GfxRefreshRate, &Button, (minimum(Config()->m_GfxRefreshRate, 1000)) / 1000.0f) * 1000.0f + 0.1f);
	if(Config()->m_GfxRefreshRate <= 1000 || NewRefreshRate < 1000)
		Config()->m_GfxRefreshRate = NewRefreshRate;

	CUIRect Text;
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Text, &MainView);
	//text.VSplitLeft(15.0f, 0, &text);
	UI()->DoLabelScaled(&Text, Localize("UI Color"), 14.0f, -1);
	RenderHSLScrollbars(&MainView, &Config()->m_UiColor, true);
}

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	CUIRect Button;
	MainView.VSplitMid(&MainView, 0);
	static int s_SndEnable = Config()->m_SndEnable;
	static int s_SndRate = Config()->m_SndRate;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndEnable, Localize("Use sounds"), Config()->m_SndEnable, &Button))
	{
		Config()->m_SndEnable ^= 1;
		if(Config()->m_SndEnable)
		{
			if(Config()->m_SndMusic && Client()->State() == IClient::STATE_OFFLINE)
				m_pClient->m_Sounds.Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		}
		else
			m_pClient->m_Sounds.Stop(SOUND_MENU);
		m_NeedRestartSound = Config()->m_SndEnable && (!s_SndEnable || s_SndRate != Config()->m_SndRate);
	}

	if(!Config()->m_SndEnable)
		return;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndMusic, Localize("Play background music"), Config()->m_SndMusic, &Button))
	{
		Config()->m_SndMusic ^= 1;
		if(Client()->State() == IClient::STATE_OFFLINE)
		{
			if(Config()->m_SndMusic)
				m_pClient->m_Sounds.Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
			else
				m_pClient->m_Sounds.Stop(SOUND_MENU);
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndNonactiveMute, Localize("Mute when not active"), Config()->m_SndNonactiveMute, &Button))
		Config()->m_SndNonactiveMute ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndGame, Localize("Enable game sounds"), Config()->m_SndGame, &Button))
		Config()->m_SndGame ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndGun, Localize("Enable gun sound"), Config()->m_SndGun, &Button))
		Config()->m_SndGun ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndLongPain, Localize("Enable long pain sound (used when shooting in freeze)"), Config()->m_SndLongPain, &Button))
		Config()->m_SndLongPain ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndServerMessage, Localize("Enable server message sound"), Config()->m_SndServerMessage, &Button))
		Config()->m_SndServerMessage ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndChat, Localize("Enable regular chat sound"), Config()->m_SndChat, &Button))
		Config()->m_SndChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndTeamChat, Localize("Enable team chat sound"), Config()->m_SndTeamChat, &Button))
		Config()->m_SndTeamChat ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_SndHighlight, Localize("Enable highlighted chat sound"), Config()->m_SndHighlight, &Button))
		Config()->m_SndHighlight ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&Config()->m_ClThreadsoundloading, Localize("Threaded sound loading"), Config()->m_ClThreadsoundloading, &Button))
		Config()->m_ClThreadsoundloading ^= 1;

	// sample rate box
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", Config()->m_SndRate);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		UI()->DoLabelScaled(&Button, Localize("Sample rate"), 14.0f, -1);
		Button.VSplitLeft(190.0f, 0, &Button);
		static float Offset = 0.0f;
		DoEditBox(&Config()->m_SndRate, &Button, aBuf, sizeof(aBuf), 14.0f, &Offset);
		Config()->m_SndRate = maximum(1, str_toint(aBuf));
		m_NeedRestartSound = !s_SndEnable || s_SndRate != Config()->m_SndRate;
	}

	// volume slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sound volume"), 14.0f, -1);
		Config()->m_SndVolume = (int)(DoScrollbarH(&Config()->m_SndVolume, &Button, Config()->m_SndVolume / 100.0f) * 100.0f);
	}

	// volume slider game sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Game sound volume"), 14.0f, -1);
		Config()->m_SndGameSoundVolume = (int)(DoScrollbarH(&Config()->m_SndGameSoundVolume, &Button, Config()->m_SndGameSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider gui sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Chat sound volume"), 14.0f, -1);
		Config()->m_SndChatSoundVolume = (int)(DoScrollbarH(&Config()->m_SndChatSoundVolume, &Button, Config()->m_SndChatSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider map sounds
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Map sound volume"), 14.0f, -1);
		Config()->m_SndMapSoundVolume = (int)(DoScrollbarH(&Config()->m_SndMapSoundVolume, &Button, Config()->m_SndMapSoundVolume / 100.0f) * 100.0f);
	}

	// volume slider background music
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Background music volume"), 14.0f, -1);
		Config()->m_SndBackgroundMusicVolume = (int)(DoScrollbarH(&Config()->m_SndBackgroundMusicVolume, &Button, Config()->m_SndBackgroundMusicVolume / 100.0f) * 100.0f);
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
			if(str_comp(s_Languages[i].m_FileName, Config()->m_ClLanguagefile) == 0)
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
			m_pClient->m_CountryFlags.Render(r.front().m_CountryCode, &Color, Rect.x, Rect.y, Rect.w, Rect.h);
			Item.m_Rect.HSplitTop(2.0f, 0, &Item.m_Rect);
			UI()->DoLabelScaled(&Item.m_Rect, r.front().m_Name, 16.0f, -1);
		}
	}

	s_SelectedLanguage = UiDoListboxEnd(&s_ScrollValue, 0);

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(Config()->m_ClLanguagefile, s_Languages[s_SelectedLanguage].m_FileName, sizeof(Config()->m_ClLanguagefile));
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
	int PreviousPage = Config()->m_UiSettingsPage;

	for(int i = 0; i < NumTabs; i++)
	{
		TabBar.HSplitTop(10, &Button, &TabBar);
		TabBar.HSplitTop(26, &Button, &TabBar);
		if(DoButton_MenuTab(aTabs[i], aTabs[i], Config()->m_UiSettingsPage == i, &Button, CUI::CORNER_R, &m_aAnimatorsSettingsTab[i]))
			Config()->m_UiSettingsPage = i;
	}

	if(PreviousPage != Config()->m_UiSettingsPage)
		ms_ColorPicker.m_Active = false;

	MainView.Margin(10.0f, &MainView);

	if(Config()->m_UiSettingsPage == SETTINGS_LANGUAGE)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderLanguageSelection(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_GENERAL)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
		RenderSettingsGeneral(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_PLAYER)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
		RenderSettingsPlayer(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_TEE)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_TEE);
		RenderSettingsTee(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_HUD)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_HUD);
		RenderSettingsHUD(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_CONTROLS)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_CONTROLS);
		RenderSettingsControls(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_GRAPHICS)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_GRAPHICS);
		RenderSettingsGraphics(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_SOUND)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_SOUND);
		RenderSettingsSound(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_DDNET)
	{
		m_pBackground->ChangePosition(CMenuBackground::POS_SETTINGS_DDNET);
		RenderSettingsDDNet(MainView);
	}
	else if(Config()->m_UiSettingsPage == SETTINGS_ASSETS)
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

ColorHSLA CMenus::RenderHSLScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha, bool ClampedLight)
{
	ColorHSLA Color(*pColor, Alpha);
	CUIRect Preview, Button, Label;
	char aBuf[32];
	float *paComponent[] = {&Color.h, &Color.s, &Color.l, &Color.a};
	const char *aLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};

	float SizePerEntry = 20.0f;
	float MarginPerEntry = 5.0f;

	float OffY = (SizePerEntry + MarginPerEntry) * (3 + (Alpha ? 1 : 0)) - 40.0f;
	pRect->VSplitLeft(40.0f, &Preview, pRect);
	Preview.HSplitTop(OffY / 2.0f, NULL, &Preview);
	Preview.HSplitTop(40.0f, &Preview, NULL);

	Graphics()->TextureClear();
	{
		const float SizeBorder = 5.0f;
		ColorRGBA SetColorRGBA{0.15f, 0.15f, 0.15f, 1};
		Graphics()->SetColor(SetColorRGBA);
		int TmpCont = RenderTools()->CreateRoundRectQuadContainer(Preview.x - SizeBorder / 2.0f, Preview.y - SizeBorder / 2.0f, Preview.w + SizeBorder, Preview.h + SizeBorder, 4.0f + SizeBorder / 2.0f, CUI::CORNER_ALL);
		Graphics()->RenderQuadContainer(TmpCont, -1);
		Graphics()->DeleteQuadContainer(TmpCont);
	}
	ColorHSLA RenderColorHSLA{Color.r, Color.g, Color.b, Color.a};
	if(ClampedLight)
		RenderColorHSLA = RenderColorHSLA.UnclampLighting();
	ColorRGBA SetColorRGBA = color_cast<ColorRGBA>(RenderColorHSLA);
	Graphics()->SetColor(SetColorRGBA);
	int TmpCont = RenderTools()->CreateRoundRectQuadContainer(Preview.x, Preview.y, Preview.w, Preview.h, 4.0f, CUI::CORNER_ALL);
	Graphics()->RenderQuadContainer(TmpCont, -1);
	Graphics()->DeleteQuadContainer(TmpCont);

	auto &&RenderHSLColorsRect = [&](CUIRect *pColorRect) {
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();

		float CurXOff = pColorRect->x;
		float SizeColor = pColorRect->w / 6;

		// red to yellow
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 1, 0, 0, 1),
				IGraphics::CColorVertex(1, 1, 1, 0, 1),
				IGraphics::CColorVertex(2, 1, 0, 0, 1),
				IGraphics::CColorVertex(3, 1, 1, 0, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		// yellow to green
		CurXOff += SizeColor;
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 1, 1, 0, 1),
				IGraphics::CColorVertex(1, 0, 1, 0, 1),
				IGraphics::CColorVertex(2, 1, 1, 0, 1),
				IGraphics::CColorVertex(3, 0, 1, 0, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// green to turquoise
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 0, 1, 0, 1),
				IGraphics::CColorVertex(1, 0, 1, 1, 1),
				IGraphics::CColorVertex(2, 0, 1, 0, 1),
				IGraphics::CColorVertex(3, 0, 1, 1, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// turquoise to blue
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 0, 1, 1, 1),
				IGraphics::CColorVertex(1, 0, 0, 1, 1),
				IGraphics::CColorVertex(2, 0, 1, 1, 1),
				IGraphics::CColorVertex(3, 0, 0, 1, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// blue to purple
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 0, 0, 1, 1),
				IGraphics::CColorVertex(1, 1, 0, 1, 1),
				IGraphics::CColorVertex(2, 0, 0, 1, 1),
				IGraphics::CColorVertex(3, 1, 0, 1, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		CurXOff += SizeColor;
		// purple to red
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, 1, 0, 1, 1),
				IGraphics::CColorVertex(1, 1, 0, 0, 1),
				IGraphics::CColorVertex(2, 1, 0, 1, 1),
				IGraphics::CColorVertex(3, 1, 0, 0, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		Graphics()->TrianglesEnd();
	};

	auto &&RenderHSLSatRect = [&](CUIRect *pColorRect, ColorRGBA &CurColor) {
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();

		float CurXOff = pColorRect->x;
		float SizeColor = pColorRect->w;

		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.g = 0;
		RightColor.g = 1;

		ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);
		ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);

		// saturation
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(1, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1),
				IGraphics::CColorVertex(2, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(3, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		Graphics()->TrianglesEnd();
	};

	auto &&RenderHSLLightRect = [&](CUIRect *pColorRect, ColorRGBA &CurColorSat) {
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();

		float CurXOff = pColorRect->x;
		float SizeColor = pColorRect->w / (ClampedLight ? 1.0f : 2.0f);

		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColorSat);
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColorSat);

		LeftColor.b = ColorHSLA::DARKEST_LGT;
		RightColor.b = 1;

		ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);
		ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);

		if(!ClampedLight)
			CurXOff += SizeColor;

		// light
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(1, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1),
				IGraphics::CColorVertex(2, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(3, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		if(!ClampedLight)
		{
			CurXOff -= SizeColor;
			LeftColor.b = 0;
			RightColor.b = ColorHSLA::DARKEST_LGT;

			ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);
			ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);

			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(1, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1),
				IGraphics::CColorVertex(2, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, 1),
				IGraphics::CColorVertex(3, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, 1)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		Graphics()->TrianglesEnd();
	};

	auto &&RenderHSLAlphaRect = [&](CUIRect *pColorRect, ColorRGBA &CurColorFull) {
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();

		float CurXOff = pColorRect->x;
		float SizeColor = pColorRect->w;

		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColorFull);
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColorFull);

		LeftColor.a = 0;
		RightColor.a = 1;

		ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);
		ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);

		// alpha
		{
			IGraphics::CColorVertex Array[4] = {
				IGraphics::CColorVertex(0, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, LeftColorRGBA.a),
				IGraphics::CColorVertex(1, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, RightColorRGBA.a),
				IGraphics::CColorVertex(2, LeftColorRGBA.r, LeftColorRGBA.g, LeftColorRGBA.b, LeftColorRGBA.a),
				IGraphics::CColorVertex(3, RightColorRGBA.r, RightColorRGBA.g, RightColorRGBA.b, RightColorRGBA.a)};
			Graphics()->SetColorVertex(Array, 4);

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}

		Graphics()->TrianglesEnd();
	};

	for(int i = 0; i < 3 + Alpha; i++)
	{
		pRect->HSplitTop(SizePerEntry, &Button, pRect);
		pRect->HSplitTop(MarginPerEntry, NULL, pRect);
		Button.VSplitLeft(10.0f, 0, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);

		RenderTools()->DrawUIRect(&Button, ColorRGBA{0.15f, 0.15f, 0.15f, 1.0f}, CUI::CORNER_ALL, 1.0f);

		Button.Margin(2.0f, &Button);
		str_format(aBuf, sizeof(aBuf), "%s: %03d", aLabels[i], (int)(*paComponent[i] * 255));
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);

		ColorHSLA CurColorPureHSLA{RenderColorHSLA.r, 1, 0.5f, 1};
		ColorRGBA CurColorPure = color_cast<ColorRGBA>(CurColorPureHSLA);
		ColorRGBA ColorInner{1, 1, 1, 0.25f};

		if(i == 0)
		{
			ColorInner = CurColorPure;
			RenderHSLColorsRect(&Button);
		}
		else if(i == 1)
		{
			RenderHSLSatRect(&Button, CurColorPure);
			ColorInner = color_cast<ColorRGBA>(ColorHSLA{CurColorPureHSLA.r, *paComponent[1], CurColorPureHSLA.b, 1});
		}
		else if(i == 2)
		{
			ColorRGBA CurColorSat = color_cast<ColorRGBA>(ColorHSLA{CurColorPureHSLA.r, *paComponent[1], 0.5f, 1});
			RenderHSLLightRect(&Button, CurColorSat);
			float LightVal = *paComponent[2];
			if(ClampedLight)
				LightVal = ColorHSLA::DARKEST_LGT + LightVal * (1.0f - ColorHSLA::DARKEST_LGT);
			ColorInner = color_cast<ColorRGBA>(ColorHSLA{CurColorPureHSLA.r, *paComponent[1], LightVal, 1});
		}
		else if(i == 3)
		{
			ColorRGBA CurColorFull = color_cast<ColorRGBA>(ColorHSLA{CurColorPureHSLA.r, *paComponent[1], *paComponent[2], 1});
			RenderHSLAlphaRect(&Button, CurColorFull);
			float LightVal = *paComponent[2];
			if(ClampedLight)
				LightVal = ColorHSLA::DARKEST_LGT + LightVal * (1.0f - ColorHSLA::DARKEST_LGT);
			ColorInner = color_cast<ColorRGBA>(ColorHSLA{CurColorPureHSLA.r, *paComponent[1], LightVal, *paComponent[3]});
		}

		*paComponent[i] = DoScrollbarH(&((char *)pColor)[i], &Button, *paComponent[i], true, &ColorInner);
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

	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowhud, Localize("Show ingame HUD"), &Config()->m_ClShowhud, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClDDRaceScoreBoard, Localize("Use DDRace Scoreboard"), &Config()->m_ClDDRaceScoreBoard, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowIDs, Localize("Show client IDs"), &Config()->m_ClShowIDs, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowhudScore, Localize("Show score"), &Config()->m_ClShowhudScore, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowhudHealthAmmo, Localize("Show health + ammo"), &Config()->m_ClShowhudHealthAmmo, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowChat, Localize("Show chat"), &Config()->m_ClShowChat, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClChatTeamColors, Localize("Show names in chat in team colors"), &Config()->m_ClChatTeamColors, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowKillMessages, Localize("Show kill messages"), &Config()->m_ClShowKillMessages, &HUD, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClShowVotesAfterVoting, Localize("Show votes window after voting"), &Config()->m_ClShowVotesAfterVoting, &HUD, LineMargin);

	// Laser

	HUD.HSplitTop(15.0f, 0x0, &HUD);
	HUD.HSplitTop(50.0f, &Section, &HUD);
	Section.VSplitRight(110.0f, &Section, 0x0);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	static int LasterOutResetID, LaserInResetID;

	ColorHSLA LaserOutlineColor = DoLine_ColorPicker(&LasterOutResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Laser Outline Color"), &Config()->m_ClLaserOutlineColor, ColorRGBA(0.074402f, 0.074402f, 0.247166f, 1.0f), false);

	HUD.HSplitTop(5.0f, 0x0, &HUD);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	ColorHSLA LaserInnerColor = DoLine_ColorPicker(&LaserInResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Laser Inner Color"), &Config()->m_ClLaserInnerColor, ColorRGBA(0.498039f, 0.498039f, 1.0f, 1.0f), false);

	Section.VSplitLeft(30.0f, 0, &Section);

	DoLaserPreview(&Section, LaserOutlineColor, LaserInnerColor);

	HUD.HSplitTop(25.0f, 0x0, &HUD);
	HUD.HSplitTop(20.0f, &SectionTwo, &HUD);

	UI()->DoLabelScaled(&SectionTwo, Localize("Hookline"), 20.0f, -1);

	HUD.HSplitTop(5.0f, 0x0, &HUD);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	static int HookCollNoCollResetID, HookCollHookableCollResetID, HookCollTeeCollResetID;
	DoLine_ColorPicker(&HookCollNoCollResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("No hit"), &Config()->m_ClHookCollColorNoColl, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), false);

	HUD.HSplitTop(5.0f, 0x0, &HUD);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	DoLine_ColorPicker(&HookCollHookableCollResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Hookable"), &Config()->m_ClHookCollColorHookableColl, ColorRGBA(130.0f / 255.0f, 232.0f / 255.0f, 160.0f / 255.0f, 1.0f), false);

	HUD.HSplitTop(5.0f, 0x0, &HUD);
	HUD.HSplitTop(25.0f, &SectionTwo, &HUD);

	DoLine_ColorPicker(&HookCollTeeCollResetID, 25.0f, 194.0f, 13.0f, 5.0f, &SectionTwo, Localize("Tee"), &Config()->m_ClHookCollColorTeeColl, ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f), false);

	// ***** Chat ***** //

	if(DoButton_CheckBoxAutoVMarginAndSet(&Config()->m_ClChatOld, Localize("Use old chat style"), &Config()->m_ClChatOld, &Chat, LineMargin))
		GameClient()->m_Chat.RebuildChat();

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

	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("System message"), &Config()->m_ClMessageSystemColor, ColorRGBA(1.0f, 1.0f, 0.5f), true, true, &Config()->m_ClShowChatSystem);
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Highlighted message"), &Config()->m_ClMessageHighlightColor, ColorRGBA(1.0f, 0.5f, 0.5f));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Team message"), &Config()->m_ClMessageTeamColor, ColorRGBA(0.65f, 1.0f, 0.65f));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Friend message"), &Config()->m_ClMessageFriendColor, ColorRGBA(1.0f, 0.137f, 0.137f), true, true, &Config()->m_ClMessageFriend);
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, Localize("Normal message"), &Config()->m_ClMessageColor, ColorRGBA(1.0f, 1.0f, 1.0f));

	str_format(aBuf, sizeof(aBuf), "%s (echo)", Localize("Client message"));
	DoLine_ColorPicker(&ResetIDs[i++], LineSize, WantedPickerPosition, LabelSize, LineSpacing, &Chat, aBuf, &Config()->m_ClMessageClientColor, ColorRGBA(0.5f, 0.78f, 1.0f));

	// ***** Chat Preview ***** //

	Chat.HSplitTop(10.0f, 0x0, &Chat);
	Chat.HSplitTop(20.0f, &Section, &Chat);

	UI()->DoLabelScaled(&Section, Localize("Preview"), 20.0f, -1);

	Chat.HSplitTop(10.0f, 0x0, &Chat);
	RenderTools()->DrawUIRect(&Chat, ColorRGBA(1, 1, 1, 0.1f), CUI::CORNER_ALL, 8.0f);
	Chat.HSplitTop(10.0f, 0x0, &Chat);

	ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageSystemColor));
	ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageHighlightColor));
	ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageTeamColor));
	ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageFriendColor));
	ColorRGBA NormalColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageColor));
	ColorRGBA ClientColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(Config()->m_ClMessageClientColor));
	ColorRGBA DefaultNameColor(0.8f, 0.8f, 0.8f, 1.0f);

	constexpr float RealFontSize = CChat::FONT_SIZE * 2;
	const float RealMsgPaddingX = (!Config()->m_ClChatOld ? CChat::MESSAGE_PADDING_X : 0) * 2;
	const float RealMsgPaddingY = (!Config()->m_ClChatOld ? CChat::MESSAGE_PADDING_Y : 0) * 2;
	const float RealMsgPaddingTee = (!Config()->m_ClChatOld ? CChat::MESSAGE_TEE_SIZE + CChat::MESSAGE_TEE_PADDING_RIGHT : 0) * 2;
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
	if(!Config()->m_ClChatOld)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.12f);

		char LineBuilder[128];
		float Width;
		float TempY = Y;
		constexpr float RealBackgroundRounding = CChat::MESSAGE_ROUNDING * 2.0f;

		if(Config()->m_ClShowChatSystem)
		{
			str_format(LineBuilder, sizeof(LineBuilder), "*** '%s' entered and joined the game", aBuf);
			Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
			RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
			TempY += RealOffsetY;
		}

		str_format(LineBuilder, sizeof(LineBuilder), "%sRandom Tee: Hey, how are you %s?", Config()->m_ClShowIDs ? " 7: " : "", aBuf);
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%sYour Teammate: Let's speedrun this!", Config()->m_ClShowIDs ? "11: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%s%sFriend: Hello there", Config()->m_ClMessageFriend ? "♥ " : "", Config()->m_ClShowIDs ? " 8: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		str_format(LineBuilder, sizeof(LineBuilder), "%sSpammer [6]: Hey fools, I'm spamming here!", Config()->m_ClShowIDs ? " 9: " : "");
		Width = TextRender()->TextWidth(0, RealFontSize, LineBuilder, -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX + RealMsgPaddingTee, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);
		TempY += RealOffsetY;

		Width = TextRender()->TextWidth(0, RealFontSize, "*** Echo command executed", -1, -1);
		RenderTools()->DrawRoundRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Width + RealMsgPaddingX, RealFontSize + RealMsgPaddingY, RealBackgroundRounding, CUI::CORNER_ALL);

		Graphics()->QuadsEnd();

		// Load skins

		int DefaultInd = GameClient()->m_Skins.Find("default");

		for(auto &i : RenderInfo)
		{
			i.m_Size = RealTeeSize;
			i.m_CustomColoredSkin = false;
		}

		int ind = -1;
		int i = 0;

		RenderInfo[i++].m_OriginalRenderSkin = GameClient()->m_Skins.Get(DefaultInd)->m_OriginalSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_Skins.Find("pinky")) != -1 ? GameClient()->m_Skins.Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_Skins.Find("cammostripes")) != -1 ? GameClient()->m_Skins.Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
		RenderInfo[i++].m_OriginalRenderSkin = (ind = GameClient()->m_Skins.Find("beast")) != -1 ? GameClient()->m_Skins.Get(ind)->m_OriginalSkin : RenderInfo[0].m_OriginalRenderSkin;
	}

	// System
	if(Config()->m_ClShowChatSystem)
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
	if(Config()->m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 7: ", -1);
	TextRender()->TextEx(&Cursor, "Random Tee: ", -1);
	TextRender()->TextColor(HighlightedColor);
	TextRender()->TextEx(&Cursor, "Hey, how are you ", -1);
	TextRender()->TextEx(&Cursor, aBuf, -1);
	TextRender()->TextEx(&Cursor, "?", -1);
	if(!Config()->m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[1], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Team
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	TextRender()->TextColor(TeamColor);
	if(Config()->m_ClShowIDs)
		TextRender()->TextEx(&Cursor, "11: ", -1);
	TextRender()->TextEx(&Cursor, "Your Teammate: Let's speedrun this!", -1);
	if(!Config()->m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[0], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Friend
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	if(Config()->m_ClMessageFriend)
	{
		TextRender()->TextColor(FriendColor);
		TextRender()->TextEx(&Cursor, "♥ ", -1);
	}
	TextRender()->TextColor(DefaultNameColor);
	if(Config()->m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 8: ", -1);
	TextRender()->TextEx(&Cursor, "Friend: ", -1);
	TextRender()->TextColor(NormalColor);
	TextRender()->TextEx(&Cursor, "Hello there", -1);
	if(!Config()->m_ClChatOld)
		RenderTools()->RenderTee(pIdleState, &RenderInfo[2], EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
	TextRender()->SetCursorPosition(&Cursor, X, Y += RealOffsetY);

	// Normal
	TextRender()->MoveCursor(&Cursor, RealMsgPaddingTee, 0);
	TextRender()->TextColor(DefaultNameColor);
	if(Config()->m_ClShowIDs)
		TextRender()->TextEx(&Cursor, " 9: ", -1);
	TextRender()->TextEx(&Cursor, "Spammer ", -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
	TextRender()->TextEx(&Cursor, "[6]", -1);
	TextRender()->TextColor(NormalColor);
	TextRender()->TextEx(&Cursor, ": Hey fools, I'm spamming here!", -1);
	if(!Config()->m_ClChatOld)
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
	static int s_InpMouseOld = Config()->m_InpMouseOld;

	MainView.HSplitTop(100.0f, &Demo, &MainView);

	Demo.HSplitTop(30.0f, &Label, &Demo);
	UI()->DoLabelScaled(&Label, Localize("Demo"), 20.0f, -1);
	Demo.Margin(5.0f, &Demo);
	Demo.VSplitMid(&Left, &Right);
	Left.VSplitRight(5.0f, &Left, 0);
	Right.VMargin(5.0f, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_ClAutoRaceRecord, Localize("Save the best demo of each race"), Config()->m_ClAutoRaceRecord, &Button))
	{
		Config()->m_ClAutoRaceRecord ^= 1;
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitLeft(160.0f, &LeftLeft, &Button);

		Button.VSplitLeft(140.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), Localize("Default length: %d"), Config()->m_ClReplayLength);
		UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);

		int NewValue = (int)(DoScrollbarH(&Config()->m_ClReplayLength, &Button, (minimum(Config()->m_ClReplayLength, 600) - 10) / 590.0f) * 590.0f) + 10;
		if(Config()->m_ClReplayLength < 600 || NewValue < 600)
			Config()->m_ClReplayLength = minimum(NewValue, 600);

		if(DoButton_CheckBox(&Config()->m_ClReplays, Localize("Enable replays"), Config()->m_ClReplays, &LeftLeft))
		{
			Config()->m_ClReplays ^= 1;
			if(!Config()->m_ClReplays)
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
	if(DoButton_CheckBox(&Config()->m_ClRaceGhost, Localize("Ghost"), Config()->m_ClRaceGhost, &Button))
	{
		Config()->m_ClRaceGhost ^= 1;
	}

	if(Config()->m_ClRaceGhost)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClRaceShowGhost, Localize("Show ghost"), Config()->m_ClRaceShowGhost, &Button))
		{
			Config()->m_ClRaceShowGhost ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClRaceSaveGhost, Localize("Save ghost"), Config()->m_ClRaceSaveGhost, &Button))
		{
			Config()->m_ClRaceSaveGhost ^= 1;
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
		Config()->m_ClOverlayEntities = (int)(DoScrollbarH(&Config()->m_ClOverlayEntities, &Button, Config()->m_ClOverlayEntities / 100.0f) * 100.0f);
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitMid(&LeftLeft, &Button);

		Button.VSplitLeft(50.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Size"), 14.0f, -1);
		Config()->m_ClTextEntitiesSize = (int)(DoScrollbarH(&Config()->m_ClTextEntitiesSize, &Button, Config()->m_ClTextEntitiesSize / 100.0f) * 100.0f);

		if(DoButton_CheckBox(&Config()->m_ClTextEntities, Localize("Show text entities"), Config()->m_ClTextEntities, &LeftLeft))
		{
			Config()->m_ClTextEntities ^= 1;
		}
	}

	{
		CUIRect Button, Label;
		Left.HSplitTop(20.0f, &Button, &Left);
		Button.VSplitMid(&LeftLeft, &Button);

		Button.VSplitLeft(50.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Alpha"), 14.0f, -1);
		Config()->m_ClShowOthersAlpha = (int)(DoScrollbarH(&Config()->m_ClShowOthersAlpha, &Button, Config()->m_ClShowOthersAlpha / 100.0f) * 100.0f);

		if(DoButton_CheckBox(&Config()->m_ClShowOthers, Localize("Show others"), Config()->m_ClShowOthers == 1, &LeftLeft))
		{
			Config()->m_ClShowOthers = Config()->m_ClShowOthers != 1 ? 1 : 0;
		}
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	bool ShowOwnTeam = Config()->m_ClShowOthers == 2;
	static int s_ShowOwnTeamID = 0;
	if(DoButton_CheckBox(&s_ShowOwnTeamID, Localize("Show others (own team only)"), ShowOwnTeam, &Button))
	{
		Config()->m_ClShowOthers = Config()->m_ClShowOthers != 2 ? 2 : 0;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_ClShowQuads, Localize("Show quads"), Config()->m_ClShowQuads, &Button))
	{
		Config()->m_ClShowQuads ^= 1;
	}

	Right.HSplitTop(20.0f, &Label, &Right);
	Label.VSplitLeft(130.0f, &Label, &Button);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Default zoom"), Config()->m_ClDefaultZoom);
	UI()->DoLabelScaled(&Label, aBuf, 14.0f, -1);
	//Right.HSplitTop(20.0f, &Button, 0);
	Button.HMargin(2.0f, &Button);
	Config()->m_ClDefaultZoom = static_cast<int>(DoScrollbarH(&Config()->m_ClDefaultZoom, &Button, Config()->m_ClDefaultZoom / 20.0f) * 20.0f + 0.1f);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&Config()->m_ClAntiPing, Localize("AntiPing"), Config()->m_ClAntiPing, &Button))
	{
		Config()->m_ClAntiPing ^= 1;
	}

	if(Config()->m_ClAntiPing)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClAntiPingPlayers, Localize("AntiPing: predict other players"), Config()->m_ClAntiPingPlayers, &Button))
		{
			Config()->m_ClAntiPingPlayers ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClAntiPingWeapons, Localize("AntiPing: predict weapons"), Config()->m_ClAntiPingWeapons, &Button))
		{
			Config()->m_ClAntiPingWeapons ^= 1;
		}

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&Config()->m_ClAntiPingGrenade, Localize("AntiPing: predict grenade paths"), Config()->m_ClAntiPingGrenade, &Button))
		{
			Config()->m_ClAntiPingGrenade ^= 1;
		}
	}
	else
	{
		Right.HSplitTop(60.0f, 0, &Right);
	}

	Right.HSplitTop(40.0f, 0, &Right);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_ClShowHookCollOther, Localize("Show other players' hook collision lines"), Config()->m_ClShowHookCollOther, &Button))
	{
		Config()->m_ClShowHookCollOther ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_ClShowDirection, Localize("Show other players' key presses"), Config()->m_ClShowDirection, &Button))
	{
		Config()->m_ClShowDirection ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_InpMouseOld, Localize("Old mouse mode"), Config()->m_InpMouseOld, &Button))
	{
		Config()->m_InpMouseOld ^= 1;
		CheckSettings = true;
	}

	if(CheckSettings)
		m_NeedRestartDDNet = s_InpMouseOld != Config()->m_InpMouseOld;

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
	DoLine_ColorPicker(&ResetID2, 25.0f, 194.0f, 13.0f, 5.0f, &Left, Localize("Entities Background color"), &Config()->m_ClBackgroundEntitiesColor, GreyDefault, false);

	static float s_Map = 0.0f;
	Left.VSplitLeft(5.0f, 0x0, &Left);
	Left.HSplitTop(25.0f, &Background, &Left);
	Background.HSplitTop(20.0f, &Background, 0);
	Background.VSplitLeft(100.0f, &Label, &TempLabel);
	UI()->DoLabelScaled(&Label, Localize("Map"), 14.0f, -1);
	DoEditBox(Config()->m_ClBackgroundEntities, &TempLabel, Config()->m_ClBackgroundEntities, sizeof(Config()->m_ClBackgroundEntities), 14.0f, &s_Map);

	Left.HSplitTop(20.0f, &Button, &Left);
	bool UseCurrentMap = str_comp(Config()->m_ClBackgroundEntities, CURRENT_MAP) == 0;
	static int s_UseCurrentMapID = 0;
	if(DoButton_CheckBox(&s_UseCurrentMapID, Localize("Use current map as background"), UseCurrentMap, &Button))
	{
		if(UseCurrentMap)
			Config()->m_ClBackgroundEntities[0] = '\0';
		else
			str_copy(Config()->m_ClBackgroundEntities, CURRENT_MAP, sizeof(Config()->m_ClBackgroundEntities));
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&Config()->m_ClBackgroundShowTilesLayers, Localize("Show tiles layers from BG map"), Config()->m_ClBackgroundShowTilesLayers, &Button))
		Config()->m_ClBackgroundShowTilesLayers ^= 1;

	Miscellaneous.HSplitTop(25.0f, &Button, &Right);
	DoLine_ColorPicker(&ResetID1, 25.0f, 194.0f, 13.0f, 5.0f, &Button, Localize("Regular Background Color"), &Config()->m_ClBackgroundColor, GreyDefault, false);
	Right.HSplitTop(5.0f, 0x0, &Right);
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&Config()->m_ClHttpMapDownload, Localize("Try fast HTTP map download first"), Config()->m_ClHttpMapDownload, &Button))
		Config()->m_ClHttpMapDownload ^= 1;

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
