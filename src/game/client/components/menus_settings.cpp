/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "binds.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

#include <array>
#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace FontIcons;
using namespace std::chrono_literals;

CMenusKeyBinder CMenus::m_Binder;

CMenusKeyBinder::CMenusKeyBinder()
{
	m_pKeyReaderId = nullptr;
	m_TakeKey = false;
	m_GotKey = false;
	m_ModifierCombination = CBinds::MODIFIER_NONE;
}

bool CMenusKeyBinder::OnInput(const IInput::CEvent &Event)
{
	if(m_TakeKey)
	{
		int TriggeringEvent = (Event.m_Key == KEY_MOUSE_1) ? IInput::FLAG_PRESS : IInput::FLAG_RELEASE;
		if(Event.m_Flags & TriggeringEvent)
		{
			m_Key = Event;
			m_GotKey = true;
			m_TakeKey = false;

			m_ModifierCombination = CBinds::GetModifierMask(Input());
			if(m_ModifierCombination == CBinds::GetModifierMaskOfKey(Event.m_Key))
			{
				m_ModifierCombination = CBinds::MODIFIER_NONE;
			}
		}
		return true;
	}

	return false;
}

void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
	char aBuf[128 + IO_MAX_PATH_LENGTH];
	CUIRect Label, Button, Left, Right, Game, ClientSettings;
	MainView.HSplitTop(150.0f, &Game, &ClientSettings);

	// game
	{
		// headline
		Game.HSplitTop(30.0f, &Label, &Game);
		Ui()->DoLabel(&Label, Localize("Game"), 20.0f, TEXTALIGN_ML);
		Game.HSplitTop(5.0f, nullptr, &Game);
		Game.VSplitMid(&Left, nullptr, 20.0f);

		// dynamic camera
		Left.HSplitTop(20.0f, &Button, &Left);
		const bool IsDyncam = g_Config.m_ClDyncam || g_Config.m_ClMouseFollowfactor > 0;
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
		Left.HSplitTop(5.0f, nullptr, &Left);
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
		Left.HSplitTop(5.0f, nullptr, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeapons, Localize("Switch weapon on pickup"), g_Config.m_ClAutoswitchWeapons, &Button))
			g_Config.m_ClAutoswitchWeapons ^= 1;

		// weapon out of ammo autoswitch
		Left.HSplitTop(5.0f, nullptr, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeaponsOutOfAmmo, Localize("Switch weapon when out of ammo"), g_Config.m_ClAutoswitchWeaponsOutOfAmmo, &Button))
			g_Config.m_ClAutoswitchWeaponsOutOfAmmo ^= 1;
	}

	// client
	{
		// headline
		ClientSettings.HSplitTop(30.0f, &Label, &ClientSettings);
		Ui()->DoLabel(&Label, Localize("Client"), 20.0f, TEXTALIGN_ML);
		ClientSettings.HSplitTop(5.0f, nullptr, &ClientSettings);
		ClientSettings.VSplitMid(&Left, &Right, 20.0f);

		// skip main menu
		Left.HSplitTop(20.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClSkipStartMenu, Localize("Skip the main menu"), g_Config.m_ClSkipStartMenu, &Button))
			g_Config.m_ClSkipStartMenu ^= 1;

		Left.HSplitTop(10.0f, nullptr, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		str_copy(aBuf, " ");
		str_append(aBuf, Localize("Hz", "Hertz"));
		Ui()->DoScrollbarOption(&g_Config.m_ClRefreshRate, &g_Config.m_ClRefreshRate, &Button, Localize("Refresh Rate"), 10, 10000, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE, aBuf);
		Left.HSplitTop(5.0f, nullptr, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		static int s_LowerRefreshRate;
		if(DoButton_CheckBox(&s_LowerRefreshRate, Localize("Save power by lowering refresh rate (higher input latency)"), g_Config.m_ClRefreshRate <= 480 && g_Config.m_ClRefreshRate != 0, &Button))
			g_Config.m_ClRefreshRate = g_Config.m_ClRefreshRate > 480 || g_Config.m_ClRefreshRate == 0 ? 480 : 0;

		CUIRect SettingsButton;
		Left.HSplitBottom(20.0f, &Left, &SettingsButton);
		Left.HSplitBottom(5.0f, &Left, nullptr);
		static CButtonContainer s_SettingsButtonId;
		if(DoButton_Menu(&s_SettingsButtonId, Localize("Settings file"), 0, &SettingsButton))
		{
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, CONFIG_FILE, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
		GameClient()->m_Tooltips.DoToolTip(&s_SettingsButtonId, &SettingsButton, Localize("Open the settings file"));

		CUIRect SavesButton;
		Left.HSplitBottom(20.0f, &Left, &SavesButton);
		Left.HSplitBottom(5.0f, &Left, nullptr);
		static CButtonContainer s_SavesButtonId;
		if(DoButton_Menu(&s_SavesButtonId, Localize("Saves file"), 0, &SavesButton))
		{
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, SAVES_FILE, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
		GameClient()->m_Tooltips.DoToolTip(&s_SavesButtonId, &SavesButton, Localize("Open the saves file"));

		CUIRect ConfigButton;
		Left.HSplitBottom(20.0f, &Left, &ConfigButton);
		Left.HSplitBottom(5.0f, &Left, nullptr);
		static CButtonContainer s_ConfigButtonId;
		if(DoButton_Menu(&s_ConfigButtonId, Localize("Config directory"), 0, &ConfigButton))
		{
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "", aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
		GameClient()->m_Tooltips.DoToolTip(&s_ConfigButtonId, &ConfigButton, Localize("Open the directory that contains the configuration and user files"));

		CUIRect DirectoryButton;
		Left.HSplitBottom(20.0f, &Left, &DirectoryButton);
		Left.HSplitBottom(5.0f, &Left, nullptr);
		static CButtonContainer s_ThemesButtonId;
		if(DoButton_Menu(&s_ThemesButtonId, Localize("Themes directory"), 0, &DirectoryButton))
		{
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "themes", aBuf, sizeof(aBuf));
			Storage()->CreateFolder("themes", IStorage::TYPE_SAVE);
			Client()->ViewFile(aBuf);
		}
		GameClient()->m_Tooltips.DoToolTip(&s_ThemesButtonId, &DirectoryButton, Localize("Open the directory to add custom themes"));

		Left.HSplitTop(20.0f, nullptr, &Left);
		RenderThemeSelection(Left);

		// auto demo settings
		{
			Right.HSplitTop(40.0f, nullptr, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoDemoRecord, Localize("Automatically record demos"), g_Config.m_ClAutoDemoRecord, &Button))
				g_Config.m_ClAutoDemoRecord ^= 1;

			Right.HSplitTop(2 * 20.0f, &Button, &Right);
			if(g_Config.m_ClAutoDemoRecord)
				Ui()->DoScrollbarOption(&g_Config.m_ClAutoDemoMax, &g_Config.m_ClAutoDemoMax, &Button, Localize("Max demos"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE);

			Right.HSplitTop(10.0f, nullptr, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoScreenshot, Localize("Automatically take game over screenshot"), g_Config.m_ClAutoScreenshot, &Button))
				g_Config.m_ClAutoScreenshot ^= 1;

			Right.HSplitTop(2 * 20.0f, &Button, &Right);
			if(g_Config.m_ClAutoScreenshot)
				Ui()->DoScrollbarOption(&g_Config.m_ClAutoScreenshotMax, &g_Config.m_ClAutoScreenshotMax, &Button, Localize("Max Screenshots"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		// auto statboard screenshot
		{
			Right.HSplitTop(10.0f, nullptr, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoStatboardScreenshot, Localize("Automatically take statboard screenshot"), g_Config.m_ClAutoStatboardScreenshot, &Button))
			{
				g_Config.m_ClAutoStatboardScreenshot ^= 1;
			}

			Right.HSplitTop(2 * 20.0f, &Button, &Right);
			if(g_Config.m_ClAutoStatboardScreenshot)
				Ui()->DoScrollbarOption(&g_Config.m_ClAutoStatboardScreenshotMax, &g_Config.m_ClAutoStatboardScreenshotMax, &Button, Localize("Max Screenshots"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		// auto statboard csv
		{
			Right.HSplitTop(10.0f, nullptr, &Right);
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClAutoCSV, Localize("Automatically create statboard csv"), g_Config.m_ClAutoCSV, &Button))
			{
				g_Config.m_ClAutoCSV ^= 1;
			}

			Right.HSplitTop(2 * 20.0f, &Button, &Right);
			if(g_Config.m_ClAutoCSV)
				Ui()->DoScrollbarOption(&g_Config.m_ClAutoCSVMax, &g_Config.m_ClAutoCSVMax, &Button, Localize("Max CSVs"), 1, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_MULTILINE);
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
	CUIRect TabBar, PlayerTab, DummyTab, ChangeInfo, QuickSearch;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	TabBar.VSplitMid(&TabBar, &ChangeInfo, 20.f);
	TabBar.VSplitMid(&PlayerTab, &DummyTab);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	static CButtonContainer s_PlayerTabButton;
	if(DoButton_MenuTab(&s_PlayerTabButton, Localize("Player"), !m_Dummy, &PlayerTab, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = false;
	}

	static CButtonContainer s_DummyTabButton;
	if(DoButton_MenuTab(&s_DummyTabButton, Localize("Dummy"), m_Dummy, &DummyTab, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = true;
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_NextChangeInfo && m_pClient->m_NextChangeInfo > Client()->GameTick(g_Config.m_ClDummy))
	{
		char aChangeInfo[128], aTimeLeft[32];
		str_format(aTimeLeft, sizeof(aTimeLeft), Localize("%ds left"), (m_pClient->m_NextChangeInfo - Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
		str_format(aChangeInfo, sizeof(aChangeInfo), "%s: %s", Localize("Player info change cooldown"), aTimeLeft);
		Ui()->DoLabel(&ChangeInfo, aChangeInfo, 10.f, TEXTALIGN_ML);
	}

	static CLineInput s_NameInput;
	static CLineInput s_ClanInput;

	int *pCountry;
	if(!m_Dummy)
	{
		pCountry = &g_Config.m_PlayerCountry;
		s_NameInput.SetBuffer(g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName));
		s_NameInput.SetEmptyText(Client()->PlayerName());
		s_ClanInput.SetBuffer(g_Config.m_PlayerClan, sizeof(g_Config.m_PlayerClan));
	}
	else
	{
		pCountry = &g_Config.m_ClDummyCountry;
		s_NameInput.SetBuffer(g_Config.m_ClDummyName, sizeof(g_Config.m_ClDummyName));
		s_NameInput.SetEmptyText(Client()->DummyName());
		s_ClanInput.SetBuffer(g_Config.m_ClDummyClan, sizeof(g_Config.m_ClDummyClan));
	}

	// player name
	CUIRect Button, Label;
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, nullptr);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_NameInput, &Button, 14.0f))
	{
		SetNeedSendInfo();
	}

	// player clan
	MainView.HSplitTop(5.0f, nullptr, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, nullptr);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);
	if(Ui()->DoEditBox(&s_ClanInput, &Button, 14.0f))
	{
		SetNeedSendInfo();
	}

	// country flag selector
	static CLineInputBuffered<25> s_FlagFilterInput;

	std::vector<const CCountryFlags::CCountryFlag *> vpFilteredFlags;
	for(size_t i = 0; i < m_pClient->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_CountryFlags.GetByIndex(i);
		if(!str_find_nocase(pEntry->m_aCountryCodeString, s_FlagFilterInput.GetString()))
			continue;
		vpFilteredFlags.push_back(pEntry);
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitBottom(20.0f, &MainView, &QuickSearch);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, nullptr);

	int OldSelected = -1;
	static CListBox s_ListBox;
	s_ListBox.DoStart(48.0f, vpFilteredFlags.size(), 10, 3, OldSelected, &MainView);

	for(size_t i = 0; i < vpFilteredFlags.size(); i++)
	{
		const CCountryFlags::CCountryFlag *pEntry = vpFilteredFlags[i];

		if(pEntry->m_CountryCode == *pCountry)
			OldSelected = i;

		const CListboxItem Item = s_ListBox.DoNextItem(&pEntry->m_CountryCode, OldSelected >= 0 && (size_t)OldSelected == i);
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect;
		Item.m_Rect.Margin(5.0f, &FlagRect);
		FlagRect.HSplitBottom(12.0f, &FlagRect, &Label);
		Label.HSplitTop(2.0f, nullptr, &Label);
		const float OldWidth = FlagRect.w;
		FlagRect.w = FlagRect.h * 2;
		FlagRect.x += (OldWidth - FlagRect.w) / 2.0f;
		m_pClient->m_CountryFlags.Render(pEntry->m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		if(pEntry->m_Texture.IsValid())
		{
			Ui()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, TEXTALIGN_MC);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		*pCountry = vpFilteredFlags[NewSelected]->m_CountryCode;
		SetNeedSendInfo();
	}

	Ui()->DoEditBox_Search(&s_FlagFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !m_pClient->m_GameConsole.IsActive());
}

struct CUISkin
{
	const CSkin *m_pSkin;

	CUISkin() :
		m_pSkin(nullptr) {}
	CUISkin(const CSkin *pSkin) :
		m_pSkin(pSkin) {}

	bool operator<(const CUISkin &Other) const { return str_comp_nocase(m_pSkin->GetName(), Other.m_pSkin->GetName()) < 0; }

	bool operator<(const char *pOther) const { return str_comp_nocase(m_pSkin->GetName(), pOther) < 0; }
	bool operator==(const char *pOther) const { return !str_comp_nocase(m_pSkin->GetName(), pOther); }
};

void CMenus::Con_AddFavoriteSkin(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CMenus *)pUserData;
	const char *pStr = pResult->GetString(0);
	if(!CSkin::IsValidName(pStr))
	{
		log_error("menus/settings", "Favorite skin name '%s' is not valid", pStr);
		log_error("menus/settings", "%s", CSkin::m_aSkinNameRestrictions);
		return;
	}
	pSelf->m_SkinFavorites.emplace(pStr);
	pSelf->m_SkinListLastRefreshTime = std::nullopt;
}

void CMenus::Con_RemFavoriteSkin(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CMenus *)pUserData;
	const auto it = pSelf->m_SkinFavorites.find(pResult->GetString(0));
	if(it != pSelf->m_SkinFavorites.end())
	{
		pSelf->m_SkinFavorites.erase(it);
		pSelf->m_SkinListLastRefreshTime = std::nullopt;
	}
}

void CMenus::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	auto *pSelf = (CMenus *)pUserData;
	pSelf->OnConfigSave(pConfigManager);
}

void CMenus::OnConfigSave(IConfigManager *pConfigManager)
{
	for(const auto &Entry : m_SkinFavorites)
	{
		char aBuffer[256];
		str_format(aBuffer, std::size(aBuffer), "add_favorite_skin \"%s\"", Entry.c_str());
		pConfigManager->WriteLine(aBuffer);
	}
}

void CMenus::RenderSettingsTee(CUIRect MainView)
{
	CUIRect TabBar, PlayerTab, DummyTab, ChangeInfo;
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	TabBar.VSplitMid(&TabBar, &ChangeInfo, 20.f);
	TabBar.VSplitMid(&PlayerTab, &DummyTab);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	static CButtonContainer s_PlayerTabButton;
	if(DoButton_MenuTab(&s_PlayerTabButton, Localize("Player"), !m_Dummy, &PlayerTab, IGraphics::CORNER_L, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = false;
		m_SkinListScrollToSelected = true;
	}

	static CButtonContainer s_DummyTabButton;
	if(DoButton_MenuTab(&s_DummyTabButton, Localize("Dummy"), m_Dummy, &DummyTab, IGraphics::CORNER_R, nullptr, nullptr, nullptr, nullptr, 4.0f))
	{
		m_Dummy = true;
		m_SkinListScrollToSelected = true;
	}

	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_NextChangeInfo && m_pClient->m_NextChangeInfo > Client()->GameTick(g_Config.m_ClDummy))
	{
		char aChangeInfo[128], aTimeLeft[32];
		str_format(aTimeLeft, sizeof(aTimeLeft), Localize("%ds left"), (m_pClient->m_NextChangeInfo - Client()->GameTick(g_Config.m_ClDummy) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
		str_format(aChangeInfo, sizeof(aChangeInfo), "%s: %s", Localize("Player info change cooldown"), aTimeLeft);
		Ui()->DoLabel(&ChangeInfo, aChangeInfo, 10.f, TEXTALIGN_ML);
	}

	char *pSkinName;
	size_t SkinNameSize;
	int *pUseCustomColor;
	unsigned *pColorBody;
	unsigned *pColorFeet;
	int *pEmote;
	if(!m_Dummy)
	{
		pSkinName = g_Config.m_ClPlayerSkin;
		SkinNameSize = sizeof(g_Config.m_ClPlayerSkin);
		pUseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
		pColorBody = &g_Config.m_ClPlayerColorBody;
		pColorFeet = &g_Config.m_ClPlayerColorFeet;
		pEmote = &g_Config.m_ClPlayerDefaultEyes;
	}
	else
	{
		pSkinName = g_Config.m_ClDummySkin;
		SkinNameSize = sizeof(g_Config.m_ClDummySkin);
		pUseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		pColorBody = &g_Config.m_ClDummyColorBody;
		pColorFeet = &g_Config.m_ClDummyColorFeet;
		pEmote = &g_Config.m_ClDummyDefaultEyes;
	}

	const float EyeButtonSize = 40.0f;
	const bool RenderEyesBelow = MainView.w < 750.0f;
	CUIRect YourSkin, Checkboxes, SkinPrefix, Eyes, Button, Label;
	MainView.HSplitTop(90.0f, &YourSkin, &MainView);
	if(RenderEyesBelow)
	{
		YourSkin.VSplitLeft(MainView.w * 0.45f, &YourSkin, &Checkboxes);
		Checkboxes.VSplitLeft(MainView.w * 0.35f, &Checkboxes, &SkinPrefix);
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(EyeButtonSize, &Eyes, &MainView);
		Eyes.VSplitRight(EyeButtonSize * NUM_EMOTES + 5.0f * (NUM_EMOTES - 1), nullptr, &Eyes);
	}
	else
	{
		YourSkin.VSplitRight(3 * EyeButtonSize + 2 * 5.0f, &YourSkin, &Eyes);
		const float RemainderWidth = YourSkin.w;
		YourSkin.VSplitLeft(RemainderWidth * 0.4f, &YourSkin, &Checkboxes);
		Checkboxes.VSplitLeft(RemainderWidth * 0.35f, &Checkboxes, &SkinPrefix);
		SkinPrefix.VSplitRight(20.0f, &SkinPrefix, nullptr);
	}
	YourSkin.VSplitRight(20.0f, &YourSkin, nullptr);
	Checkboxes.VSplitRight(20.0f, &Checkboxes, nullptr);

	// Checkboxes
	Checkboxes.HSplitTop(20.0f, &Button, &Checkboxes);
	if(DoButton_CheckBox(&g_Config.m_ClDownloadSkins, Localize("Download skins"), g_Config.m_ClDownloadSkins, &Button))
	{
		g_Config.m_ClDownloadSkins ^= 1;
		m_pClient->RefreshSkins();
	}

	Checkboxes.HSplitTop(20.0f, &Button, &Checkboxes);
	if(DoButton_CheckBox(&g_Config.m_ClDownloadCommunitySkins, Localize("Download community skins"), g_Config.m_ClDownloadCommunitySkins, &Button))
	{
		g_Config.m_ClDownloadCommunitySkins ^= 1;
		m_pClient->RefreshSkins();
	}

	Checkboxes.HSplitTop(20.0f, &Button, &Checkboxes);
	if(DoButton_CheckBox(&g_Config.m_ClVanillaSkinsOnly, Localize("Vanilla skins only"), g_Config.m_ClVanillaSkinsOnly, &Button))
	{
		g_Config.m_ClVanillaSkinsOnly ^= 1;
		m_pClient->RefreshSkins();
	}

	Checkboxes.HSplitTop(20.0f, &Button, &Checkboxes);
	if(DoButton_CheckBox(&g_Config.m_ClFatSkins, Localize("Fat skins (DDFat)"), g_Config.m_ClFatSkins, &Button))
	{
		g_Config.m_ClFatSkins ^= 1;
	}

	// Skin prefix
	{
		SkinPrefix.HSplitTop(20.0f, &Label, &SkinPrefix);
		Ui()->DoLabel(&Label, Localize("Skin prefix"), 14.0f, TEXTALIGN_ML);

		SkinPrefix.HSplitTop(20.0f, &Button, &SkinPrefix);
		static CLineInput s_SkinPrefixInput(g_Config.m_ClSkinPrefix, sizeof(g_Config.m_ClSkinPrefix));
		Ui()->DoClearableEditBox(&s_SkinPrefixInput, &Button, 14.0f);

		SkinPrefix.HSplitTop(2.0f, nullptr, &SkinPrefix);

		static const char *s_apSkinPrefixes[] = {"kitty", "santa"};
		static CButtonContainer s_aPrefixButtons[std::size(s_apSkinPrefixes)];
		for(size_t i = 0; i < std::size(s_apSkinPrefixes); i++)
		{
			SkinPrefix.HSplitTop(20.0f, &Button, &SkinPrefix);
			Button.HMargin(2.0f, &Button);
			if(DoButton_Menu(&s_aPrefixButtons[i], s_apSkinPrefixes[i], 0, &Button))
			{
				str_copy(g_Config.m_ClSkinPrefix, s_apSkinPrefixes[i]);
			}
		}
	}

	// Player skin area
	CUIRect CustomColorsButton, RandomSkinButton;
	YourSkin.HSplitTop(20.0f, &Label, &YourSkin);
	YourSkin.HSplitBottom(20.0f, &YourSkin, &CustomColorsButton);
	CustomColorsButton.VSplitRight(30.0f, &CustomColorsButton, &RandomSkinButton);
	CustomColorsButton.VSplitRight(20.0f, &CustomColorsButton, nullptr);
	YourSkin.VSplitLeft(65.0f, &YourSkin, &Button);
	Button.VSplitLeft(5.0f, nullptr, &Button);
	Button.HMargin((Button.h - 20.0f) / 2.0f, &Button);

	char aBuf[128 + IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Your skin"));
	Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);

	// Note: get the skin info after the settings buttons, because they can trigger a refresh
	// which invalidates the skin.
	CTeeRenderInfo OwnSkinInfo;
	OwnSkinInfo.Apply(m_pClient->m_Skins.Find(pSkinName));
	OwnSkinInfo.ApplyColors(*pUseCustomColor, *pColorBody, *pColorFeet);
	OwnSkinInfo.m_Size = 50.0f;

	// Tee
	{
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &OwnSkinInfo, OffsetToMid);
		const vec2 TeeRenderPos = vec2(YourSkin.x + YourSkin.w / 2.0f, YourSkin.y + YourSkin.h / 2.0f + OffsetToMid.y);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, *pEmote, vec2(1.0f, 0.0f), TeeRenderPos);
	}

	// Skin name
	static CLineInput s_SkinInput;
	s_SkinInput.SetBuffer(pSkinName, SkinNameSize);
	s_SkinInput.SetEmptyText("default");
	if(Ui()->DoClearableEditBox(&s_SkinInput, &Button, 14.0f))
	{
		SetNeedSendInfo();
		m_SkinListScrollToSelected = true;
	}

	// Random skin button
	static CButtonContainer s_RandomSkinButton;
	static const char *s_apDice[] = {FONT_ICON_DICE_ONE, FONT_ICON_DICE_TWO, FONT_ICON_DICE_THREE, FONT_ICON_DICE_FOUR, FONT_ICON_DICE_FIVE, FONT_ICON_DICE_SIX};
	static int s_CurrentDie = rand() % std::size(s_apDice);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(DoButton_Menu(&s_RandomSkinButton, s_apDice[s_CurrentDie], 0, &RandomSkinButton, nullptr, IGraphics::CORNER_ALL, 5.0f, -0.2f))
	{
		GameClient()->m_Skins.RandomizeSkin(m_Dummy);
		SetNeedSendInfo();
		m_SkinListScrollToSelected = true;
		s_CurrentDie = rand() % std::size(s_apDice);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	GameClient()->m_Tooltips.DoToolTip(&s_RandomSkinButton, &RandomSkinButton, Localize("Create a random skin"));

	// Custom colors button
	if(DoButton_CheckBox(pUseCustomColor, Localize("Custom colors"), *pUseCustomColor, &CustomColorsButton))
	{
		*pUseCustomColor = *pUseCustomColor ? 0 : 1;
		SetNeedSendInfo();
	}

	// Default eyes
	{
		CTeeRenderInfo EyeSkinInfo = OwnSkinInfo;
		EyeSkinInfo.m_Size = EyeButtonSize;
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &EyeSkinInfo, OffsetToMid);

		CUIRect EyesRow;
		Eyes.HSplitTop(EyeButtonSize, &EyesRow, &Eyes);
		static CButtonContainer s_aEyeButtons[NUM_EMOTES];
		for(int CurrentEyeEmote = 0; CurrentEyeEmote < NUM_EMOTES; CurrentEyeEmote++)
		{
			EyesRow.VSplitLeft(EyeButtonSize, &Button, &EyesRow);
			EyesRow.VSplitLeft(5.0f, nullptr, &EyesRow);
			if(!RenderEyesBelow && (CurrentEyeEmote + 1) % 3 == 0)
			{
				Eyes.HSplitTop(5.0f, nullptr, &Eyes);
				Eyes.HSplitTop(EyeButtonSize, &EyesRow, &Eyes);
			}

			const ColorRGBA EyeButtonColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f + (*pEmote == CurrentEyeEmote ? 0.25f : 0.0f));
			if(DoButton_Menu(&s_aEyeButtons[CurrentEyeEmote], "", 0, &Button, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, EyeButtonColor))
			{
				*pEmote = CurrentEyeEmote;
				if((int)m_Dummy == g_Config.m_ClDummy)
					GameClient()->m_Emoticon.EyeEmote(CurrentEyeEmote);
			}
			GameClient()->m_Tooltips.DoToolTip(&s_aEyeButtons[CurrentEyeEmote], &Button, Localize("Choose default eyes when joining a server"));
			RenderTools()->RenderTee(CAnimState::GetIdle(), &EyeSkinInfo, CurrentEyeEmote, vec2(1.0f, 0.0f), vec2(Button.x + Button.w / 2.0f, Button.y + Button.h / 2.0f + OffsetToMid.y));
		}
	}

	// Custom color pickers
	MainView.HSplitTop(5.0f, nullptr, &MainView);
	if(*pUseCustomColor)
	{
		CUIRect CustomColors;
		MainView.HSplitTop(95.0f, &CustomColors, &MainView);
		CUIRect aRects[2];
		CustomColors.VSplitMid(&aRects[0], &aRects[1], 20.0f);

		unsigned *apColors[] = {pColorBody, pColorFeet};
		const char *apParts[] = {Localize("Body"), Localize("Feet")};

		for(int i = 0; i < 2; i++)
		{
			aRects[i].HSplitTop(20.0f, &Label, &aRects[i]);
			Ui()->DoLabel(&Label, apParts[i], 14.0f, TEXTALIGN_ML);
			if(RenderHslaScrollbars(&aRects[i], apColors[i], false, ColorHSLA::DARKEST_LGT))
			{
				SetNeedSendInfo();
			}
		}
	}
	MainView.HSplitTop(5.0f, nullptr, &MainView);

	// Layout bottom controls and use remainder for skin selector
	CUIRect QuickSearch, DatabaseButton, DirectoryButton, RefreshButton;
	MainView.HSplitBottom(20.0f, &MainView, &QuickSearch);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, &DatabaseButton);
	DatabaseButton.VSplitLeft(10.0f, nullptr, &DatabaseButton);
	DatabaseButton.VSplitLeft(150.0f, &DatabaseButton, &DirectoryButton);
	DirectoryButton.VSplitRight(175.0f, nullptr, &DirectoryButton);
	DirectoryButton.VSplitRight(25.0f, &DirectoryButton, &RefreshButton);
	DirectoryButton.VSplitRight(10.0f, &DirectoryButton, nullptr);

	// Skin selector
	static std::vector<CUISkin> s_vSkinList;
	static std::vector<CUISkin> s_vSkinListHelper;
	static std::vector<CUISkin> s_vFavoriteSkinListHelper;
	static CListBox s_ListBox;

	// be nice to the CPU
	if(!m_SkinListLastRefreshTime.has_value() || m_SkinListLastRefreshTime.value() != m_pClient->m_Skins.LastRefreshTime())
	{
		m_SkinListLastRefreshTime = m_pClient->m_Skins.LastRefreshTime();
		s_vSkinList.clear();
		s_vSkinListHelper.clear();
		s_vFavoriteSkinListHelper.clear();

		auto &&SkinNotFiltered = [&](const CSkin *pSkinToBeSelected) {
			// filter quick search
			if(g_Config.m_ClSkinFilterString[0] != '\0' && !str_utf8_find_nocase(pSkinToBeSelected->GetName(), g_Config.m_ClSkinFilterString))
				return false;

			// no special skins
			if(CSkins::IsSpecialSkin(pSkinToBeSelected->GetName()))
				return false;

			return true;
		};

		for(const auto &it : m_SkinFavorites)
		{
			const CSkin *pSkinToBeSelected = m_pClient->m_Skins.FindOrNullptr(it.c_str(), true);

			if(pSkinToBeSelected == nullptr || !SkinNotFiltered(pSkinToBeSelected))
				continue;

			s_vFavoriteSkinListHelper.emplace_back(pSkinToBeSelected);
		}
		for(const auto &SkinIt : m_pClient->m_Skins.GetSkinsUnsafe())
		{
			const auto &pSkinToBeSelected = SkinIt.second;
			if(!SkinNotFiltered(pSkinToBeSelected.get()))
				continue;

			if(std::find(m_SkinFavorites.begin(), m_SkinFavorites.end(), pSkinToBeSelected->GetName()) == m_SkinFavorites.end())
				s_vSkinListHelper.emplace_back(pSkinToBeSelected.get());
		}
		std::sort(s_vSkinListHelper.begin(), s_vSkinListHelper.end());
		std::sort(s_vFavoriteSkinListHelper.begin(), s_vFavoriteSkinListHelper.end());
		s_vSkinList = s_vFavoriteSkinListHelper;
		s_vSkinList.insert(s_vSkinList.end(), s_vSkinListHelper.begin(), s_vSkinListHelper.end());
	}

	int OldSelected = -1;
	s_ListBox.DoStart(50.0f, s_vSkinList.size(), 4, 1, OldSelected, &MainView);
	for(size_t i = 0; i < s_vSkinList.size(); ++i)
	{
		const CSkin *pSkinToBeDraw = s_vSkinList[i].m_pSkin;
		if(str_comp(pSkinToBeDraw->GetName(), pSkinName) == 0)
		{
			OldSelected = i;
			if(m_SkinListScrollToSelected)
			{
				s_ListBox.ScrollToSelected();
				m_SkinListScrollToSelected = false;
			}
		}

		const CListboxItem Item = s_ListBox.DoNextItem(pSkinToBeDraw, OldSelected >= 0 && (size_t)OldSelected == i);
		if(!Item.m_Visible)
			continue;

		Item.m_Rect.VSplitLeft(60.0f, &Button, &Label);

		CTeeRenderInfo Info = OwnSkinInfo;
		Info.Apply(pSkinToBeDraw);

		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &Info, OffsetToMid);
		const vec2 TeeRenderPos = vec2(Button.x + Button.w / 2.0f, Button.y + Button.h / 2 + OffsetToMid.y);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, *pEmote, vec2(1.0f, 0.0f), TeeRenderPos);

		SLabelProperties Props;
		Props.m_MaxWidth = Label.w - 5.0f;
		Ui()->DoLabel(&Label, pSkinToBeDraw->GetName(), 12.0f, TEXTALIGN_ML, Props);

		if(g_Config.m_Debug)
		{
			const ColorRGBA BloodColor = *pUseCustomColor ? color_cast<ColorRGBA>(ColorHSLA(*pColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT)) : pSkinToBeDraw->m_BloodColor;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(BloodColor.r, BloodColor.g, BloodColor.b, 1.0f);
			IGraphics::CQuadItem QuadItem(Label.x, Label.y, 12.0f, 12.0f);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// render skin favorite icon
		{
			const auto SkinItFav = m_SkinFavorites.find(pSkinToBeDraw->GetName());
			const bool IsFav = SkinItFav != m_SkinFavorites.end();
			CUIRect FavIcon;
			Item.m_Rect.HSplitTop(20.0f, &FavIcon, nullptr);
			FavIcon.VSplitRight(20.0f, nullptr, &FavIcon);
			if(DoButton_Favorite(&pSkinToBeDraw->m_Metrics.m_Body, pSkinToBeDraw, IsFav, &FavIcon))
			{
				if(IsFav)
				{
					m_SkinFavorites.erase(SkinItFav);
				}
				else
				{
					m_SkinFavorites.emplace(pSkinToBeDraw->GetName());
				}
				m_SkinListLastRefreshTime = std::nullopt;
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		str_copy(pSkinName, s_vSkinList[NewSelected].m_pSkin->GetName(), SkinNameSize);
		SetNeedSendInfo();
	}

	static CLineInput s_SkinFilterInput(g_Config.m_ClSkinFilterString, sizeof(g_Config.m_ClSkinFilterString));
	if(Ui()->DoEditBox_Search(&s_SkinFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !m_pClient->m_GameConsole.IsActive()))
	{
		m_SkinListLastRefreshTime = std::nullopt;
	}

	static CButtonContainer s_SkinDatabaseButton;
	if(DoButton_Menu(&s_SkinDatabaseButton, Localize("Skin Database"), 0, &DatabaseButton))
	{
		Client()->ViewLink("https://ddnet.org/skins/");
	}

	static CButtonContainer s_DirectoryButton;
	if(DoButton_Menu(&s_DirectoryButton, Localize("Skins directory"), 0, &DirectoryButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "skins", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("skins", IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_DirectoryButton, &DirectoryButton, Localize("Open the directory to add custom skins"));

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	static CButtonContainer s_SkinRefreshButton;
	if(DoButton_Menu(&s_SkinRefreshButton, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &RefreshButton) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		// reset render flags for possible loading screen
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		m_pClient->RefreshSkins();
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

typedef struct
{
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
} CKeyInfo;

static CKeyInfo gs_aKeys[] =
	{
		{Localizable("Move left"), "+left", 0, 0},
		{Localizable("Move right"), "+right", 0, 0},
		{Localizable("Jump"), "+jump", 0, 0},
		{Localizable("Fire"), "+fire", 0, 0},
		{Localizable("Hook"), "+hook", 0, 0},
		{Localizable("Hook collisions"), "+showhookcoll", 0, 0},
		{Localizable("Pause"), "say /pause", 0, 0},
		{Localizable("Kill"), "kill", 0, 0},
		{Localizable("Zoom in"), "zoom+", 0, 0},
		{Localizable("Zoom out"), "zoom-", 0, 0},
		{Localizable("Default zoom"), "zoom", 0, 0},
		{Localizable("Show others"), "say /showothers", 0, 0},
		{Localizable("Show all"), "say /showall", 0, 0},
		{Localizable("Toggle dyncam"), "toggle cl_dyncam 0 1", 0, 0},
		{Localizable("Toggle ghost"), "toggle cl_race_show_ghost 0 1", 0, 0},

		{Localizable("Hammer"), "+weapon1", 0, 0},
		{Localizable("Pistol"), "+weapon2", 0, 0},
		{Localizable("Shotgun"), "+weapon3", 0, 0},
		{Localizable("Grenade"), "+weapon4", 0, 0},
		{Localizable("Laser"), "+weapon5", 0, 0},
		{Localizable("Next weapon"), "+nextweapon", 0, 0},
		{Localizable("Prev. weapon"), "+prevweapon", 0, 0},

		{Localizable("Vote yes"), "vote yes", 0, 0},
		{Localizable("Vote no"), "vote no", 0, 0},

		{Localizable("Chat"), "+show_chat; chat all", 0, 0},
		{Localizable("Team chat"), "+show_chat; chat team", 0, 0},
		{Localizable("Converse"), "+show_chat; chat all /c ", 0, 0},
		{Localizable("Chat command"), "+show_chat; chat all /", 0, 0},
		{Localizable("Show chat"), "+show_chat", 0, 0},

		{Localizable("Toggle dummy"), "toggle cl_dummy 0 1", 0, 0},
		{Localizable("Dummy copy"), "toggle cl_dummy_copy_moves 0 1", 0, 0},
		{Localizable("Hammerfly dummy"), "toggle cl_dummy_hammer 0 1", 0, 0},

		{Localizable("Emoticon"), "+emote", 0, 0},
		{Localizable("Spectator mode"), "+spectate", 0, 0},
		{Localizable("Spectate next"), "spectate_next", 0, 0},
		{Localizable("Spectate previous"), "spectate_previous", 0, 0},
		{Localizable("Console"), "toggle_local_console", 0, 0},
		{Localizable("Remote console"), "toggle_remote_console", 0, 0},
		{Localizable("Screenshot"), "screenshot", 0, 0},
		{Localizable("Scoreboard"), "+scoreboard", 0, 0},
		{Localizable("Statboard"), "+statboard", 0, 0},
		{Localizable("Lock team"), "say /lock", 0, 0},
		{Localizable("Show entities"), "toggle cl_overlay_entities 0 100", 0, 0},
		{Localizable("Show HUD"), "toggle cl_showhud 0 1", 0, 0},
};

void CMenus::DoSettingsControlsButtons(int Start, int Stop, CUIRect View)
{
	for(int i = Start; i < Stop; i++)
	{
		const CKeyInfo &Key = gs_aKeys[i];

		CUIRect Button, Label;
		View.HSplitTop(20.0f, &Button, &View);
		Button.VSplitLeft(135.0f, &Label, &Button);

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", Localize(Key.m_pName));

		Ui()->DoLabel(&Label, aBuf, 13.0f, TEXTALIGN_ML);
		int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
		int NewId = DoKeyReader(&Key.m_KeyId, &Button, OldId, OldModifierCombination, &NewModifierCombination);
		if(NewId != OldId || NewModifierCombination != OldModifierCombination)
		{
			if(OldId != 0 || NewId == 0)
				m_pClient->m_Binds.Bind(OldId, "", false, OldModifierCombination);
			if(NewId != 0)
				m_pClient->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
		}

		View.HSplitTop(2.0f, nullptr, &View);
	}
}

float CMenus::RenderSettingsControlsJoystick(CUIRect View)
{
	bool JoystickEnabled = g_Config.m_InpControllerEnable;
	int NumJoysticks = Input()->NumJoysticks();
	int NumOptions = 1; // expandable header
	if(JoystickEnabled)
	{
		NumOptions++; // message or joystick name/selection
		if(NumJoysticks > 0)
		{
			NumOptions += 3; // mode, ui sens, tolerance
			if(!g_Config.m_InpControllerAbsolute)
				NumOptions++; // ingame sens
			NumOptions += Input()->GetActiveJoystick()->GetNumAxes() + 1; // axis selection + header
		}
	}
	const float ButtonHeight = 20.0f;
	const float Spacing = 2.0f;
	const float BackgroundHeight = NumOptions * (ButtonHeight + Spacing) + (NumOptions == 1 ? 0.0f : Spacing);
	if(View.h < BackgroundHeight)
		return BackgroundHeight;

	View.HSplitTop(BackgroundHeight, &View, nullptr);

	CUIRect Button;
	View.HSplitTop(Spacing, nullptr, &View);
	View.HSplitTop(ButtonHeight, &Button, &View);
	if(DoButton_CheckBox(&g_Config.m_InpControllerEnable, Localize("Enable controller"), g_Config.m_InpControllerEnable, &Button))
	{
		g_Config.m_InpControllerEnable ^= 1;
	}
	if(JoystickEnabled)
	{
		if(NumJoysticks > 0)
		{
			// show joystick device selection if more than one available or just the joystick name if there is only one
			{
				CUIRect JoystickDropDown;
				View.HSplitTop(Spacing, nullptr, &View);
				View.HSplitTop(ButtonHeight, &JoystickDropDown, &View);
				if(NumJoysticks > 1)
				{
					static std::vector<std::string> s_vJoystickNames;
					static std::vector<const char *> s_vpJoystickNames;
					s_vJoystickNames.resize(NumJoysticks);
					s_vpJoystickNames.resize(NumJoysticks);

					for(int i = 0; i < NumJoysticks; ++i)
					{
						char aBuf[256];
						str_format(aBuf, sizeof(aBuf), "%s %d: %s", Localize("Controller"), i, Input()->GetJoystick(i)->GetName());
						s_vJoystickNames[i] = aBuf;
						s_vpJoystickNames[i] = s_vJoystickNames[i].c_str();
					}

					static CUi::SDropDownState s_JoystickDropDownState;
					static CScrollRegion s_JoystickDropDownScrollRegion;
					s_JoystickDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_JoystickDropDownScrollRegion;
					const int CurrentJoystick = Input()->GetActiveJoystick()->GetIndex();
					const int NewJoystick = Ui()->DoDropDown(&JoystickDropDown, CurrentJoystick, s_vpJoystickNames.data(), s_vpJoystickNames.size(), s_JoystickDropDownState);
					if(NewJoystick != CurrentJoystick)
					{
						Input()->SetActiveJoystick(NewJoystick);
					}
				}
				else
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "%s 0: %s", Localize("Controller"), Input()->GetJoystick(0)->GetName());
					Ui()->DoLabel(&JoystickDropDown, aBuf, 13.0f, TEXTALIGN_ML);
				}
			}

			{
				View.HSplitTop(Spacing, nullptr, &View);
				View.HSplitTop(ButtonHeight, &Button, &View);
				CUIRect Label, ButtonRelative, ButtonAbsolute;
				Button.VSplitMid(&Label, &Button, 10.0f);
				Button.HMargin(2.0f, &Button);
				Button.VSplitMid(&ButtonRelative, &ButtonAbsolute);
				Ui()->DoLabel(&Label, Localize("Ingame controller mode"), 13.0f, TEXTALIGN_ML);
				CButtonContainer s_RelativeButton;
				if(DoButton_Menu(&s_RelativeButton, Localize("Relative", "Ingame controller mode"), g_Config.m_InpControllerAbsolute == 0, &ButtonRelative, nullptr, IGraphics::CORNER_L))
				{
					g_Config.m_InpControllerAbsolute = 0;
				}
				CButtonContainer s_AbsoluteButton;
				if(DoButton_Menu(&s_AbsoluteButton, Localize("Absolute", "Ingame controller mode"), g_Config.m_InpControllerAbsolute == 1, &ButtonAbsolute, nullptr, IGraphics::CORNER_R))
				{
					g_Config.m_InpControllerAbsolute = 1;
				}
			}

			if(!g_Config.m_InpControllerAbsolute)
			{
				View.HSplitTop(Spacing, nullptr, &View);
				View.HSplitTop(ButtonHeight, &Button, &View);
				Ui()->DoScrollbarOption(&g_Config.m_InpControllerSens, &g_Config.m_InpControllerSens, &Button, Localize("Ingame controller sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);
			}

			View.HSplitTop(Spacing, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoScrollbarOption(&g_Config.m_UiControllerSens, &g_Config.m_UiControllerSens, &Button, Localize("UI controller sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

			View.HSplitTop(Spacing, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoScrollbarOption(&g_Config.m_InpControllerTolerance, &g_Config.m_InpControllerTolerance, &Button, Localize("Controller jitter tolerance"), 0, 50);

			View.HSplitTop(Spacing, nullptr, &View);
			View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.125f), IGraphics::CORNER_ALL, 5.0f);
			DoJoystickAxisPicker(View);
		}
		else
		{
			View.HSplitTop(View.h - ButtonHeight, nullptr, &View);
			View.HSplitTop(ButtonHeight, &Button, &View);
			Ui()->DoLabel(&Button, Localize("No controller found. Plug in a controller."), 13.0f, TEXTALIGN_ML);
		}
	}

	return BackgroundHeight;
}

void CMenus::DoJoystickAxisPicker(CUIRect View)
{
	const float FontSize = 13.0f;
	const float RowHeight = 20.0f;
	const float SpacingH = 2.0f;
	const float AxisWidth = 0.2f * View.w;
	const float StatusWidth = 0.4f * View.w;
	const float AimBindWidth = 90.0f;
	const float SpacingV = (View.w - AxisWidth - StatusWidth - AimBindWidth) / 2.0f;

	CUIRect Row, Axis, Status, AimBind;
	View.HSplitTop(SpacingH, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	Row.VSplitLeft(AxisWidth, &Axis, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(StatusWidth, &Status, &Row);
	Row.VSplitLeft(SpacingV, nullptr, &Row);
	Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

	Ui()->DoLabel(&Axis, Localize("Axis"), FontSize, TEXTALIGN_MC);
	Ui()->DoLabel(&Status, Localize("Status"), FontSize, TEXTALIGN_MC);
	Ui()->DoLabel(&AimBind, Localize("Aim bind"), FontSize, TEXTALIGN_MC);

	IInput::IJoystick *pJoystick = Input()->GetActiveJoystick();
	static int s_aActive[NUM_JOYSTICK_AXES][2];
	for(int i = 0; i < minimum<int>(pJoystick->GetNumAxes(), NUM_JOYSTICK_AXES); i++)
	{
		const bool Active = g_Config.m_InpControllerX == i || g_Config.m_InpControllerY == i;

		View.HSplitTop(SpacingH, nullptr, &View);
		View.HSplitTop(RowHeight, &Row, &View);
		Row.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.125f), IGraphics::CORNER_ALL, 5.0f);
		Row.VSplitLeft(AxisWidth, &Axis, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(StatusWidth, &Status, &Row);
		Row.VSplitLeft(SpacingV, nullptr, &Row);
		Row.VSplitLeft(AimBindWidth, &AimBind, &Row);

		// Axis label
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d", i + 1);
		if(Active)
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		else
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		Ui()->DoLabel(&Axis, aBuf, FontSize, TEXTALIGN_MC);

		// Axis status
		Status.HMargin(7.0f, &Status);
		DoJoystickBar(&Status, (pJoystick->GetAxisValue(i) + 1.0f) / 2.0f, g_Config.m_InpControllerTolerance / 50.0f, Active);

		// Bind to X/Y
		CUIRect AimBindX, AimBindY;
		AimBind.VSplitMid(&AimBindX, &AimBindY);
		if(DoButton_CheckBox(&s_aActive[i][0], "X", g_Config.m_InpControllerX == i, &AimBindX))
		{
			if(g_Config.m_InpControllerY == i)
				g_Config.m_InpControllerY = g_Config.m_InpControllerX;
			g_Config.m_InpControllerX = i;
		}
		if(DoButton_CheckBox(&s_aActive[i][1], "Y", g_Config.m_InpControllerY == i, &AimBindY))
		{
			if(g_Config.m_InpControllerX == i)
				g_Config.m_InpControllerX = g_Config.m_InpControllerY;
			g_Config.m_InpControllerY = i;
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CMenus::DoJoystickBar(const CUIRect *pRect, float Current, float Tolerance, bool Active)
{
	CUIRect Handle;
	pRect->VSplitLeft(pRect->h, &Handle, nullptr); // Slider size
	Handle.x += (pRect->w - Handle.w) * Current;

	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Active ? 0.25f : 0.125f), IGraphics::CORNER_ALL, pRect->h / 2.0f);

	CUIRect ToleranceArea = *pRect;
	ToleranceArea.w *= Tolerance;
	ToleranceArea.x += (pRect->w - ToleranceArea.w) / 2.0f;
	const ColorRGBA ToleranceColor = Active ? ColorRGBA(0.8f, 0.35f, 0.35f, 1.0f) : ColorRGBA(0.7f, 0.5f, 0.5f, 1.0f);
	ToleranceArea.Draw(ToleranceColor, IGraphics::CORNER_ALL, ToleranceArea.h / 2.0f);

	const ColorRGBA SliderColor = Active ? ColorRGBA(0.95f, 0.95f, 0.95f, 1.0f) : ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
	Handle.Draw(SliderColor, IGraphics::CORNER_ALL, Handle.h / 2.0f);
}

void CMenus::RenderSettingsControls(CUIRect MainView)
{
	// this is kinda slow, but whatever
	for(auto &Key : gs_aKeys)
		Key.m_KeyId = Key.m_ModifierCombination = 0;

	for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
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
					Key.m_ModifierCombination = Mod;
					break;
				}
		}
	}

	// scrollable controls
	static float s_JoystickSettingsHeight = 0.0f; // we calculate this later and don't render until enough space is available
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 120.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.y += ScrollOffset.y;

	const float FontSize = 14.0f;
	const float Margin = 10.0f;
	const float HeaderHeight = FontSize + 5.0f + Margin;

	CUIRect MouseSettings, MovementSettings, WeaponSettings, VotingSettings, ChatSettings, DummySettings, MiscSettings, JoystickSettings, ResetButton, Button;
	MainView.VSplitMid(&MouseSettings, &VotingSettings);

	// mouse settings
	{
		MouseSettings.VMargin(5.0f, &MouseSettings);
		MouseSettings.HSplitTop(80.0f, &MouseSettings, &JoystickSettings);
		if(s_ScrollRegion.AddRect(MouseSettings))
		{
			MouseSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MouseSettings.VMargin(10.0f, &MouseSettings);

			MouseSettings.HSplitTop(HeaderHeight, &Button, &MouseSettings);
			Ui()->DoLabel(&Button, Localize("Mouse"), FontSize, TEXTALIGN_ML);

			MouseSettings.HSplitTop(20.0f, &Button, &MouseSettings);
			Ui()->DoScrollbarOption(&g_Config.m_InpMousesens, &g_Config.m_InpMousesens, &Button, Localize("Ingame mouse sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

			MouseSettings.HSplitTop(2.0f, nullptr, &MouseSettings);

			MouseSettings.HSplitTop(20.0f, &Button, &MouseSettings);
			Ui()->DoScrollbarOption(&g_Config.m_UiMousesens, &g_Config.m_UiMousesens, &Button, Localize("UI mouse sens."), 1, 500, &CUi::ms_LogarithmicScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);
		}
	}

	// joystick settings
	{
		JoystickSettings.HSplitTop(Margin, nullptr, &JoystickSettings);
		JoystickSettings.HSplitTop(s_JoystickSettingsHeight + HeaderHeight + Margin, &JoystickSettings, &MovementSettings);
		if(s_ScrollRegion.AddRect(JoystickSettings))
		{
			JoystickSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			JoystickSettings.VMargin(Margin, &JoystickSettings);

			JoystickSettings.HSplitTop(HeaderHeight, &Button, &JoystickSettings);
			Ui()->DoLabel(&Button, Localize("Controller"), FontSize, TEXTALIGN_ML);

			s_JoystickSettingsHeight = RenderSettingsControlsJoystick(JoystickSettings);
		}
	}

	// movement settings
	{
		MovementSettings.HSplitTop(Margin, nullptr, &MovementSettings);
		MovementSettings.HSplitTop(365.0f, &MovementSettings, &WeaponSettings);
		if(s_ScrollRegion.AddRect(MovementSettings))
		{
			MovementSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MovementSettings.VMargin(Margin, &MovementSettings);

			MovementSettings.HSplitTop(HeaderHeight, &Button, &MovementSettings);
			Ui()->DoLabel(&Button, Localize("Movement"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(0, 15, MovementSettings);
		}
	}

	// weapon settings
	{
		WeaponSettings.HSplitTop(Margin, nullptr, &WeaponSettings);
		WeaponSettings.HSplitTop(190.0f, &WeaponSettings, &ResetButton);
		if(s_ScrollRegion.AddRect(WeaponSettings))
		{
			WeaponSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			WeaponSettings.VMargin(Margin, &WeaponSettings);

			WeaponSettings.HSplitTop(HeaderHeight, &Button, &WeaponSettings);
			Ui()->DoLabel(&Button, Localize("Weapon"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(15, 22, WeaponSettings);
		}
	}

	// defaults
	{
		ResetButton.HSplitTop(Margin, nullptr, &ResetButton);
		ResetButton.HSplitTop(40.0f, &ResetButton, nullptr);
		if(s_ScrollRegion.AddRect(ResetButton))
		{
			ResetButton.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			ResetButton.Margin(10.0f, &ResetButton);
			static CButtonContainer s_DefaultButton;
			if(DoButton_Menu(&s_DefaultButton, Localize("Reset to defaults"), 0, &ResetButton))
			{
				PopupConfirm(Localize("Reset controls"), Localize("Are you sure that you want to reset the controls to their defaults?"),
					Localize("Reset"), Localize("Cancel"), &CMenus::ResetSettingsControls);
			}
		}
	}

	// voting settings
	{
		VotingSettings.VMargin(5.0f, &VotingSettings);
		VotingSettings.HSplitTop(80.0f, &VotingSettings, &ChatSettings);
		if(s_ScrollRegion.AddRect(VotingSettings))
		{
			VotingSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			VotingSettings.VMargin(Margin, &VotingSettings);

			VotingSettings.HSplitTop(HeaderHeight, &Button, &VotingSettings);
			Ui()->DoLabel(&Button, Localize("Voting"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(22, 24, VotingSettings);
		}
	}

	// chat settings
	{
		ChatSettings.HSplitTop(Margin, nullptr, &ChatSettings);
		ChatSettings.HSplitTop(145.0f, &ChatSettings, &DummySettings);
		if(s_ScrollRegion.AddRect(ChatSettings))
		{
			ChatSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			ChatSettings.VMargin(Margin, &ChatSettings);

			ChatSettings.HSplitTop(HeaderHeight, &Button, &ChatSettings);
			Ui()->DoLabel(&Button, Localize("Chat"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(24, 29, ChatSettings);
		}
	}

	// dummy settings
	{
		DummySettings.HSplitTop(Margin, nullptr, &DummySettings);
		DummySettings.HSplitTop(100.0f, &DummySettings, &MiscSettings);
		if(s_ScrollRegion.AddRect(DummySettings))
		{
			DummySettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			DummySettings.VMargin(Margin, &DummySettings);

			DummySettings.HSplitTop(HeaderHeight, &Button, &DummySettings);
			Ui()->DoLabel(&Button, Localize("Dummy"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(29, 32, DummySettings);
		}
	}

	// misc settings
	{
		MiscSettings.HSplitTop(Margin, nullptr, &MiscSettings);
		MiscSettings.HSplitTop(300.0f, &MiscSettings, nullptr);
		if(s_ScrollRegion.AddRect(MiscSettings))
		{
			MiscSettings.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
			MiscSettings.VMargin(Margin, &MiscSettings);

			MiscSettings.HSplitTop(HeaderHeight, &Button, &MiscSettings);
			Ui()->DoLabel(&Button, Localize("Miscellaneous"), FontSize, TEXTALIGN_ML);

			DoSettingsControlsButtons(32, 44, MiscSettings);
		}
	}

	s_ScrollRegion.End();
}

void CMenus::ResetSettingsControls()
{
	m_pClient->m_Binds.SetDefaults();

	g_Config.m_InpMousesens = 200;
	g_Config.m_UiMousesens = 200;

	g_Config.m_InpControllerEnable = 0;
	g_Config.m_InpControllerGUID[0] = '\0';
	g_Config.m_InpControllerAbsolute = 0;
	g_Config.m_InpControllerSens = 100;
	g_Config.m_InpControllerX = 0;
	g_Config.m_InpControllerY = 1;
	g_Config.m_InpControllerTolerance = 5;
	g_Config.m_UiControllerSens = 100;
}

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button;
	char aBuf[128];
	bool CheckSettings = false;

	static const int MAX_RESOLUTIONS = 256;
	static CVideoMode s_aModes[MAX_RESOLUTIONS];
	static int s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	static int s_GfxFsaaSamples = g_Config.m_GfxFsaaSamples;
	static bool s_GfxBackendChanged = false;
	static bool s_GfxGpuChanged = false;
	static int s_GfxHighdpi = g_Config.m_GfxHighdpi;

	static int s_InitDisplayAllVideoModes = g_Config.m_GfxDisplayAllVideoModes;

	static bool s_WasInit = false;
	static bool s_ModesReload = false;
	if(!s_WasInit)
	{
		s_WasInit = true;

		Graphics()->AddWindowPropChangeListener([]() {
			s_ModesReload = true;
		});
	}

	if(s_ModesReload || g_Config.m_GfxDisplayAllVideoModes != s_InitDisplayAllVideoModes)
	{
		s_NumNodes = Graphics()->GetVideoModes(s_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
		s_ModesReload = false;
		s_InitDisplayAllVideoModes = g_Config.m_GfxDisplayAllVideoModes;
	}

	CUIRect ModeList, ModeLabel;
	MainView.VSplitLeft(350.0f, &MainView, &ModeList);
	ModeList.HSplitTop(24.0f, &ModeLabel, &ModeList);
	MainView.VSplitLeft(340.0f, &MainView, nullptr);

	// display mode list
	static CListBox s_ListBox;
	static const float sc_RowHeightResList = 22.0f;
	static const float sc_FontSizeResListHeader = 12.0f;
	static const float sc_FontSizeResList = 10.0f;

	{
		int G = std::gcd(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);
		str_format(aBuf, sizeof(aBuf), "%s: %dx%d @%dhz %d bit (%d:%d)", Localize("Current"), (int)(g_Config.m_GfxScreenWidth * Graphics()->ScreenHiDPIScale()), (int)(g_Config.m_GfxScreenHeight * Graphics()->ScreenHiDPIScale()), g_Config.m_GfxScreenRefreshRate, g_Config.m_GfxColorDepth, g_Config.m_GfxScreenWidth / G, g_Config.m_GfxScreenHeight / G);
		Ui()->DoLabel(&ModeLabel, aBuf, sc_FontSizeResListHeader, TEXTALIGN_MC);
	}

	int OldSelected = -1;
	s_ListBox.SetActive(!Ui()->IsPopupOpen());
	s_ListBox.DoStart(sc_RowHeightResList, s_NumNodes, 1, 3, OldSelected, &ModeList);

	for(int i = 0; i < s_NumNodes; ++i)
	{
		const int Depth = s_aModes[i].m_Red + s_aModes[i].m_Green + s_aModes[i].m_Blue > 16 ? 24 : 16;
		if(g_Config.m_GfxColorDepth == Depth &&
			g_Config.m_GfxScreenWidth == s_aModes[i].m_WindowWidth &&
			g_Config.m_GfxScreenHeight == s_aModes[i].m_WindowHeight &&
			g_Config.m_GfxScreenRefreshRate == s_aModes[i].m_RefreshRate)
		{
			OldSelected = i;
		}

		const CListboxItem Item = s_ListBox.DoNextItem(&s_aModes[i], OldSelected == i);
		if(!Item.m_Visible)
			continue;

		int G = std::gcd(s_aModes[i].m_CanvasWidth, s_aModes[i].m_CanvasHeight);
		str_format(aBuf, sizeof(aBuf), " %dx%d @%dhz %d bit (%d:%d)", s_aModes[i].m_CanvasWidth, s_aModes[i].m_CanvasHeight, s_aModes[i].m_RefreshRate, Depth, s_aModes[i].m_CanvasWidth / G, s_aModes[i].m_CanvasHeight / G);
		Ui()->DoLabel(&Item.m_Rect, aBuf, sc_FontSizeResList, TEXTALIGN_ML);
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		const int Depth = s_aModes[NewSelected].m_Red + s_aModes[NewSelected].m_Green + s_aModes[NewSelected].m_Blue > 16 ? 24 : 16;
		g_Config.m_GfxColorDepth = Depth;
		g_Config.m_GfxScreenWidth = s_aModes[NewSelected].m_WindowWidth;
		g_Config.m_GfxScreenHeight = s_aModes[NewSelected].m_WindowHeight;
		g_Config.m_GfxScreenRefreshRate = s_aModes[NewSelected].m_RefreshRate;
		Graphics()->ResizeToScreen();
	}

	// switches
	CUIRect WindowModeDropDown;
	MainView.HSplitTop(20.0f, &WindowModeDropDown, &MainView);

	const char *apWindowModes[] = {Localize("Windowed"), Localize("Windowed borderless"), Localize("Windowed fullscreen"), Localize("Desktop fullscreen"), Localize("Fullscreen")};
	static const int s_NumWindowMode = std::size(apWindowModes);

	const int OldWindowMode = (g_Config.m_GfxFullscreen ? (g_Config.m_GfxFullscreen == 1 ? 4 : (g_Config.m_GfxFullscreen == 2 ? 3 : 2)) : (g_Config.m_GfxBorderless ? 1 : 0));

	static CUi::SDropDownState s_WindowModeDropDownState;
	static CScrollRegion s_WindowModeDropDownScrollRegion;
	s_WindowModeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_WindowModeDropDownScrollRegion;
	const int NewWindowMode = Ui()->DoDropDown(&WindowModeDropDown, OldWindowMode, apWindowModes, s_NumWindowMode, s_WindowModeDropDownState);
	if(OldWindowMode != NewWindowMode)
	{
		if(NewWindowMode == 0)
			Client()->SetWindowParams(0, false);
		else if(NewWindowMode == 1)
			Client()->SetWindowParams(0, true);
		else if(NewWindowMode == 2)
			Client()->SetWindowParams(3, false);
		else if(NewWindowMode == 3)
			Client()->SetWindowParams(2, false);
		else if(NewWindowMode == 4)
			Client()->SetWindowParams(1, false);
	}

	if(Graphics()->GetNumScreens() > 1)
	{
		CUIRect ScreenDropDown;
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &ScreenDropDown, &MainView);

		const int NumScreens = Graphics()->GetNumScreens();
		static std::vector<std::string> s_vScreenNames;
		static std::vector<const char *> s_vpScreenNames;
		s_vScreenNames.resize(NumScreens);
		s_vpScreenNames.resize(NumScreens);

		for(int i = 0; i < NumScreens; ++i)
		{
			str_format(aBuf, sizeof(aBuf), "%s %d: %s", Localize("Screen"), i, Graphics()->GetScreenName(i));
			s_vScreenNames[i] = aBuf;
			s_vpScreenNames[i] = s_vScreenNames[i].c_str();
		}

		static CUi::SDropDownState s_ScreenDropDownState;
		static CScrollRegion s_ScreenDropDownScrollRegion;
		s_ScreenDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ScreenDropDownScrollRegion;
		const int NewScreen = Ui()->DoDropDown(&ScreenDropDown, g_Config.m_GfxScreen, s_vpScreenNames.data(), s_vpScreenNames.size(), s_ScreenDropDownState);
		if(NewScreen != g_Config.m_GfxScreen)
			Client()->SwitchWindowScreen(NewScreen);
	}

	MainView.HSplitTop(2.0f, nullptr, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("V-Sync"), Localize("may cause delay"));
	if(DoButton_CheckBox(&g_Config.m_GfxVsync, aBuf, g_Config.m_GfxVsync, &Button))
	{
		Client()->ToggleWindowVSync();
	}

	bool MultiSamplingChanged = false;
	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_format(aBuf, sizeof(aBuf), "%s (%s)", Localize("FSAA samples"), Localize("may cause delay"));
	int GfxFsaaSamplesMouseButton = DoButton_CheckBox_Number(&g_Config.m_GfxFsaaSamples, aBuf, g_Config.m_GfxFsaaSamples, &Button);
	int CurFSAA = g_Config.m_GfxFsaaSamples == 0 ? 1 : g_Config.m_GfxFsaaSamples;
	if(GfxFsaaSamplesMouseButton == 1) // inc
	{
		g_Config.m_GfxFsaaSamples = std::pow(2, (int)std::log2(CurFSAA) + 1);
		if(g_Config.m_GfxFsaaSamples > 64)
			g_Config.m_GfxFsaaSamples = 0;
		MultiSamplingChanged = true;
	}
	else if(GfxFsaaSamplesMouseButton == 2) // dec
	{
		if(CurFSAA == 1)
			g_Config.m_GfxFsaaSamples = 64;
		else if(CurFSAA == 2)
			g_Config.m_GfxFsaaSamples = 0;
		else
			g_Config.m_GfxFsaaSamples = std::pow(2, (int)std::log2(CurFSAA) - 1);
		MultiSamplingChanged = true;
	}

	uint32_t MultiSamplingCountBackend = 0;
	if(MultiSamplingChanged)
	{
		if(Graphics()->SetMultiSampling(g_Config.m_GfxFsaaSamples, MultiSamplingCountBackend))
		{
			// try again with 0 if mouse click was increasing multi sampling
			// else just accept the current value as is
			if((uint32_t)g_Config.m_GfxFsaaSamples > MultiSamplingCountBackend && GfxFsaaSamplesMouseButton == 1)
				Graphics()->SetMultiSampling(0, MultiSamplingCountBackend);
			g_Config.m_GfxFsaaSamples = (int)MultiSamplingCountBackend;
		}
		else
		{
			CheckSettings = true;
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighDetail, Localize("High Detail"), g_Config.m_GfxHighDetail, &Button))
		g_Config.m_GfxHighDetail ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_GfxHighDetail, &Button, Localize("Allows maps to render with more detail"));

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClShowfps, Localize("Show FPS"), g_Config.m_ClShowfps, &Button))
		g_Config.m_ClShowfps ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClShowfps, &Button, Localize("Renders your frame rate in the top right"));

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighdpi, Localize("Use high DPI"), g_Config.m_GfxHighdpi, &Button))
	{
		CheckSettings = true;
		g_Config.m_GfxHighdpi ^= 1;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	str_copy(aBuf, " ");
	str_append(aBuf, Localize("Hz", "Hertz"));
	Ui()->DoScrollbarOption(&g_Config.m_GfxRefreshRate, &g_Config.m_GfxRefreshRate, &Button, Localize("Refresh Rate"), 10, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, aBuf);

	MainView.HSplitTop(2.0f, nullptr, &MainView);
	static CButtonContainer s_UiColorResetId;
	DoLine_ColorPicker(&s_UiColorResetId, 25.0f, 13.0f, 2.0f, &MainView, Localize("UI Color"), &g_Config.m_UiColor, color_cast<ColorRGBA>(ColorHSLA(0xE4A046AFU, true)), false, nullptr, true);

	// Backend list
	struct SMenuBackendInfo
	{
		int m_Major = 0;
		int m_Minor = 0;
		int m_Patch = 0;
		const char *m_pBackendName = "";
		bool m_Found = false;
	};
	std::array<std::array<SMenuBackendInfo, EGraphicsDriverAgeType::GRAPHICS_DRIVER_AGE_TYPE_COUNT>, EBackendType::BACKEND_TYPE_COUNT> aaSupportedBackends{};
	uint32_t FoundBackendCount = 0;
	for(uint32_t i = 0; i < BACKEND_TYPE_COUNT; ++i)
	{
		if(EBackendType(i) == BACKEND_TYPE_AUTO)
			continue;
		for(uint32_t n = 0; n < GRAPHICS_DRIVER_AGE_TYPE_COUNT; ++n)
		{
			auto &Info = aaSupportedBackends[i][n];
			if(Graphics()->GetDriverVersion(EGraphicsDriverAgeType(n), Info.m_Major, Info.m_Minor, Info.m_Patch, Info.m_pBackendName, EBackendType(i)))
			{
				// don't count blocked opengl drivers
				if(EBackendType(i) != BACKEND_TYPE_OPENGL || EGraphicsDriverAgeType(n) == GRAPHICS_DRIVER_AGE_TYPE_LEGACY || g_Config.m_GfxDriverIsBlocked == 0)
				{
					Info.m_Found = true;
					++FoundBackendCount;
				}
			}
		}
	}

	if(FoundBackendCount > 1)
	{
		CUIRect Text, BackendDropDown;
		MainView.HSplitTop(10.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Text, &MainView);
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &BackendDropDown, &MainView);
		Ui()->DoLabel(&Text, Localize("Renderer"), 16.0f, TEXTALIGN_MC);

		static std::vector<std::string> s_vBackendIdNames;
		static std::vector<const char *> s_vpBackendIdNamesCStr;
		static std::vector<SMenuBackendInfo> s_vBackendInfos;

		size_t BackendCount = FoundBackendCount + 1;
		s_vBackendIdNames.resize(BackendCount);
		s_vpBackendIdNamesCStr.resize(BackendCount);
		s_vBackendInfos.resize(BackendCount);

		char aTmpBackendName[256];

		auto IsInfoDefault = [](const SMenuBackendInfo &CheckInfo) {
			return str_comp_nocase(CheckInfo.m_pBackendName, CConfig::ms_pGfxBackend) == 0 && CheckInfo.m_Major == CConfig::ms_GfxGLMajor && CheckInfo.m_Minor == CConfig::ms_GfxGLMinor && CheckInfo.m_Patch == CConfig::ms_GfxGLPatch;
		};

		int OldSelectedBackend = -1;
		uint32_t CurCounter = 0;
		for(uint32_t i = 0; i < BACKEND_TYPE_COUNT; ++i)
		{
			for(uint32_t n = 0; n < GRAPHICS_DRIVER_AGE_TYPE_COUNT; ++n)
			{
				auto &Info = aaSupportedBackends[i][n];
				if(Info.m_Found)
				{
					bool IsDefault = IsInfoDefault(Info);
					str_format(aTmpBackendName, sizeof(aTmpBackendName), "%s (%d.%d.%d)%s%s", Info.m_pBackendName, Info.m_Major, Info.m_Minor, Info.m_Patch, IsDefault ? " - " : "", IsDefault ? Localize("default") : "");
					s_vBackendIdNames[CurCounter] = aTmpBackendName;
					s_vpBackendIdNamesCStr[CurCounter] = s_vBackendIdNames[CurCounter].c_str();
					if(str_comp_nocase(Info.m_pBackendName, g_Config.m_GfxBackend) == 0 && g_Config.m_GfxGLMajor == Info.m_Major && g_Config.m_GfxGLMinor == Info.m_Minor && g_Config.m_GfxGLPatch == Info.m_Patch)
					{
						OldSelectedBackend = CurCounter;
					}

					s_vBackendInfos[CurCounter] = Info;
					++CurCounter;
				}
			}
		}

		if(OldSelectedBackend != -1)
		{
			// no custom selected
			BackendCount -= 1;
		}
		else
		{
			// custom selected one
			str_format(aTmpBackendName, sizeof(aTmpBackendName), "%s (%s %d.%d.%d)", Localize("custom"), g_Config.m_GfxBackend, g_Config.m_GfxGLMajor, g_Config.m_GfxGLMinor, g_Config.m_GfxGLPatch);
			s_vBackendIdNames[CurCounter] = aTmpBackendName;
			s_vpBackendIdNamesCStr[CurCounter] = s_vBackendIdNames[CurCounter].c_str();
			OldSelectedBackend = CurCounter;

			s_vBackendInfos[CurCounter].m_pBackendName = "custom";
			s_vBackendInfos[CurCounter].m_Major = g_Config.m_GfxGLMajor;
			s_vBackendInfos[CurCounter].m_Minor = g_Config.m_GfxGLMinor;
			s_vBackendInfos[CurCounter].m_Patch = g_Config.m_GfxGLPatch;
		}

		static int s_OldSelectedBackend = -1;
		if(s_OldSelectedBackend == -1)
			s_OldSelectedBackend = OldSelectedBackend;

		static CUi::SDropDownState s_BackendDropDownState;
		static CScrollRegion s_BackendDropDownScrollRegion;
		s_BackendDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_BackendDropDownScrollRegion;
		const int NewBackend = Ui()->DoDropDown(&BackendDropDown, OldSelectedBackend, s_vpBackendIdNamesCStr.data(), BackendCount, s_BackendDropDownState);
		if(OldSelectedBackend != NewBackend)
		{
			str_copy(g_Config.m_GfxBackend, s_vBackendInfos[NewBackend].m_pBackendName);
			g_Config.m_GfxGLMajor = s_vBackendInfos[NewBackend].m_Major;
			g_Config.m_GfxGLMinor = s_vBackendInfos[NewBackend].m_Minor;
			g_Config.m_GfxGLPatch = s_vBackendInfos[NewBackend].m_Patch;

			CheckSettings = true;
			s_GfxBackendChanged = s_OldSelectedBackend != NewBackend;
		}
	}

	// GPU list
	const auto &GpuList = Graphics()->GetGpus();
	if(GpuList.m_vGpus.size() > 1)
	{
		CUIRect Text, GpuDropDown;
		MainView.HSplitTop(10.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Text, &MainView);
		MainView.HSplitTop(2.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &GpuDropDown, &MainView);
		Ui()->DoLabel(&Text, Localize("Graphics card"), 16.0f, TEXTALIGN_MC);

		static std::vector<const char *> s_vpGpuIdNames;

		size_t GpuCount = GpuList.m_vGpus.size() + 1;
		s_vpGpuIdNames.resize(GpuCount);

		char aCurDeviceName[256 + 4];

		int OldSelectedGpu = -1;
		for(size_t i = 0; i < GpuCount; ++i)
		{
			if(i == 0)
			{
				str_format(aCurDeviceName, sizeof(aCurDeviceName), "%s (%s)", Localize("auto"), GpuList.m_AutoGpu.m_aName);
				s_vpGpuIdNames[i] = aCurDeviceName;
				if(str_comp("auto", g_Config.m_GfxGpuName) == 0)
				{
					OldSelectedGpu = 0;
				}
			}
			else
			{
				s_vpGpuIdNames[i] = GpuList.m_vGpus[i - 1].m_aName;
				if(str_comp(GpuList.m_vGpus[i - 1].m_aName, g_Config.m_GfxGpuName) == 0)
				{
					OldSelectedGpu = i;
				}
			}
		}

		static int s_OldSelectedGpu = -1;
		if(s_OldSelectedGpu == -1)
			s_OldSelectedGpu = OldSelectedGpu;

		static CUi::SDropDownState s_GpuDropDownState;
		static CScrollRegion s_GpuDropDownScrollRegion;
		s_GpuDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_GpuDropDownScrollRegion;
		const int NewGpu = Ui()->DoDropDown(&GpuDropDown, OldSelectedGpu, s_vpGpuIdNames.data(), GpuCount, s_GpuDropDownState);
		if(OldSelectedGpu != NewGpu)
		{
			if(NewGpu == 0)
				str_copy(g_Config.m_GfxGpuName, "auto");
			else
				str_copy(g_Config.m_GfxGpuName, GpuList.m_vGpus[NewGpu - 1].m_aName);
			CheckSettings = true;
			s_GfxGpuChanged = NewGpu != s_OldSelectedGpu;
		}
	}

	// check if the new settings require a restart
	if(CheckSettings)
	{
		m_NeedRestartGraphics = !(s_GfxFsaaSamples == g_Config.m_GfxFsaaSamples &&
					  !s_GfxBackendChanged &&
					  !s_GfxGpuChanged &&
					  s_GfxHighdpi == g_Config.m_GfxHighdpi);
	}
}

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	static int s_SndEnable = g_Config.m_SndEnable;

	CUIRect Button;
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndEnable, Localize("Use sounds"), g_Config.m_SndEnable, &Button))
	{
		g_Config.m_SndEnable ^= 1;
		UpdateMusicState();
		m_NeedRestartSound = g_Config.m_SndEnable && !s_SndEnable;
	}

	if(!g_Config.m_SndEnable)
		return;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndMusic, Localize("Play background music"), g_Config.m_SndMusic, &Button))
	{
		g_Config.m_SndMusic ^= 1;
		UpdateMusicState();
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

	// volume slider
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndVolume, &g_Config.m_SndVolume, &Button, Localize("Sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider game sounds
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndGameSoundVolume, &g_Config.m_SndGameSoundVolume, &Button, Localize("Game sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider gui sounds
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndChatSoundVolume, &g_Config.m_SndChatSoundVolume, &Button, Localize("Chat sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider map sounds
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndMapSoundVolume, &g_Config.m_SndMapSoundVolume, &Button, Localize("Map sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider background music
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndBackgroundMusicVolume, &g_Config.m_SndBackgroundMusicVolume, &Button, Localize("Background music volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}
}

void CMenus::RenderLanguageSettings(CUIRect MainView)
{
	const float CreditsFontSize = 14.0f;
	const float CreditsMargin = 10.0f;

	CUIRect List, CreditsScroll;
	MainView.HSplitBottom(4.0f * CreditsFontSize + 2.0f * CreditsMargin + CScrollRegion::HEIGHT_MAGIC_FIX, &List, &CreditsScroll);
	List.HSplitBottom(5.0f, &List, nullptr);

	RenderLanguageSelection(List);

	CreditsScroll.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);

	static CScrollRegion s_CreditsScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = CreditsFontSize;
	s_CreditsScrollRegion.Begin(&CreditsScroll, &ScrollOffset, &ScrollParams);
	CreditsScroll.y += ScrollOffset.y;

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, CreditsFontSize, TEXTFLAG_RENDER);
	Cursor.m_LineWidth = CreditsScroll.w - 2.0f * CreditsMargin;

	const unsigned OldRenderFlags = TextRender()->GetRenderFlags();
	TextRender()->SetRenderFlags(OldRenderFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
	STextContainerIndex CreditsTextContainer;
	TextRender()->CreateTextContainer(CreditsTextContainer, &Cursor, Localize("English translation by the DDNet Team", "Translation credits: Add your own name here when you update translations"));
	TextRender()->SetRenderFlags(OldRenderFlags);
	if(CreditsTextContainer.Valid())
	{
		CUIRect CreditsLabel;
		CreditsScroll.HSplitTop(TextRender()->GetBoundingBoxTextContainer(CreditsTextContainer).m_H + 2.0f * CreditsMargin, &CreditsLabel, &CreditsScroll);
		s_CreditsScrollRegion.AddRect(CreditsLabel);
		CreditsLabel.Margin(CreditsMargin, &CreditsLabel);
		TextRender()->RenderTextContainer(CreditsTextContainer, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), CreditsLabel.x, CreditsLabel.y);
		TextRender()->DeleteTextContainer(CreditsTextContainer);
	}

	s_CreditsScrollRegion.End();
}

bool CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_SelectedLanguage = -2; // -2 = unloaded, -1 = unset
	static CListBox s_ListBox;

	if(s_SelectedLanguage == -2)
	{
		s_SelectedLanguage = -1;
		for(size_t i = 0; i < g_Localization.Languages().size(); i++)
		{
			if(str_comp(g_Localization.Languages()[i].m_FileName.c_str(), g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				s_ListBox.ScrollToSelected();
				break;
			}
		}
	}

	const int OldSelected = s_SelectedLanguage;

	s_ListBox.DoStart(24.0f, g_Localization.Languages().size(), 1, 3, s_SelectedLanguage, &MainView);

	for(const auto &Language : g_Localization.Languages())
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&Language.m_Name, s_SelectedLanguage != -1 && !str_comp(g_Localization.Languages()[s_SelectedLanguage].m_Name.c_str(), Language.m_Name.c_str()));
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &FlagRect, &Label);
		FlagRect.VMargin(6.0f, &FlagRect);
		FlagRect.HMargin(3.0f, &FlagRect);
		m_pClient->m_CountryFlags.Render(Language.m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		Ui()->DoLabel(&Label, Language.m_Name.c_str(), 16.0f, TEXTALIGN_ML);
	}

	s_SelectedLanguage = s_ListBox.DoEnd();

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(g_Config.m_ClLanguagefile, g_Localization.Languages()[s_SelectedLanguage].m_FileName.c_str());
		GameClient()->OnLanguageChange();
	}

	return s_ListBox.WasItemActivated();
}

void CMenus::RenderSettings(CUIRect MainView)
{
	// render background
	CUIRect Button, TabBar, RestartBar;
	MainView.VSplitRight(120.0f, &MainView, &TabBar);
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(20.0f, &MainView);

	const bool NeedRestart = m_NeedRestartGraphics || m_NeedRestartSound || m_NeedRestartUpdate;
	if(NeedRestart)
	{
		MainView.HSplitBottom(20.0f, &MainView, &RestartBar);
		MainView.HSplitBottom(10.0f, &MainView, nullptr);
	}

	TabBar.HSplitTop(50.0f, &Button, &TabBar);
	Button.Draw(ms_ColorTabbarActive, IGraphics::CORNER_BR, 10.0f);

	const char *apTabs[SETTINGS_LENGTH] = {
		Localize("Language"),
		Localize("General"),
		Localize("Player"),
		Client()->IsSixup() ? "Tee 0.7" : Localize("Tee"),
		Localize("Appearance"),
		Localize("Controls"),
		Localize("Graphics"),
		Localize("Sound"),
		Localize("DDNet"),
		Localize("Assets")};
	static CButtonContainer s_aTabButtons[SETTINGS_LENGTH];

	for(int i = 0; i < SETTINGS_LENGTH; i++)
	{
		TabBar.HSplitTop(10.0f, nullptr, &TabBar);
		TabBar.HSplitTop(26.0f, &Button, &TabBar);
		if(DoButton_MenuTab(&s_aTabButtons[i], apTabs[i], g_Config.m_UiSettingsPage == i, &Button, IGraphics::CORNER_R, &m_aAnimatorsSettingsTab[i]))
			g_Config.m_UiSettingsPage = i;
	}

	if(g_Config.m_UiSettingsPage == SETTINGS_LANGUAGE)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_LANGUAGE);
		RenderLanguageSettings(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_GENERAL)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GENERAL);
		RenderSettingsGeneral(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_PLAYER)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_PLAYER);
		RenderSettingsPlayer(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_TEE)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_TEE);
		if(Client()->IsSixup())
			RenderSettingsTee7(MainView);
		else
			RenderSettingsTee(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_APPEARANCE)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_APPEARANCE);
		RenderSettingsAppearance(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_CONTROLS)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_CONTROLS);
		RenderSettingsControls(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_GRAPHICS)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_GRAPHICS);
		RenderSettingsGraphics(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_SOUND)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_SOUND);
		RenderSettingsSound(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_DDNET)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_DDNET);
		RenderSettingsDDNet(MainView);
	}
	else if(g_Config.m_UiSettingsPage == SETTINGS_ASSETS)
	{
		GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_SETTINGS_ASSETS);
		RenderSettingsCustom(MainView);
	}
	else
	{
		dbg_assert(false, "ui_settings_page invalid");
	}

	if(NeedRestart)
	{
		CUIRect RestartWarning, RestartButton;
		RestartBar.VSplitRight(125.0f, &RestartWarning, &RestartButton);
		RestartWarning.VSplitRight(10.0f, &RestartWarning, nullptr);
		if(m_NeedRestartUpdate)
		{
			TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
			Ui()->DoLabel(&RestartWarning, Localize("DDNet Client needs to be restarted to complete update!"), 14.0f, TEXTALIGN_ML);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			Ui()->DoLabel(&RestartWarning, Localize("You must restart the game for all settings to take effect."), 14.0f, TEXTALIGN_ML);
		}

		static CButtonContainer s_RestartButton;
		if(DoButton_Menu(&s_RestartButton, Localize("Restart"), 0, &RestartButton))
		{
			if(Client()->State() == IClient::STATE_ONLINE || m_pClient->Editor()->HasUnsavedData())
			{
				m_Popup = POPUP_RESTART;
			}
			else
			{
				Client()->Restart();
			}
		}
	}
}

bool CMenus::RenderHslaScrollbars(CUIRect *pRect, unsigned int *pColor, bool Alpha, float DarkestLight)
{
	const unsigned PrevPackedColor = *pColor;
	ColorHSLA Color(*pColor, Alpha);
	const ColorHSLA OriginalColor = Color;
	const char *apLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};
	const float SizePerEntry = 20.0f;
	const float MarginPerEntry = 5.0f;
	const float PreviewMargin = 2.5f;
	const float PreviewHeight = 40.0f + 2 * PreviewMargin;
	const float OffY = (SizePerEntry + MarginPerEntry) * (3 + (Alpha ? 1 : 0)) - PreviewHeight;

	CUIRect Preview;
	pRect->VSplitLeft(PreviewHeight, &Preview, pRect);
	Preview.HSplitTop(OffY / 2.0f, nullptr, &Preview);
	Preview.HSplitTop(PreviewHeight, &Preview, nullptr);

	Preview.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);
	Preview.Margin(PreviewMargin, &Preview);
	Preview.Draw(color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight)), IGraphics::CORNER_ALL, 4.0f + PreviewMargin);

	auto &&RenderHueRect = [&](CUIRect *pColorRect) {
		float CurXOff = pColorRect->x;
		const float SizeColor = pColorRect->w / 6;

		// red to yellow
		{
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 1, 0, 0, 1),
				IGraphics::CColorVertex(1, 1, 1, 0, 1),
				IGraphics::CColorVertex(2, 1, 0, 0, 1),
				IGraphics::CColorVertex(3, 1, 1, 0, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

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
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 1, 1, 0, 1),
				IGraphics::CColorVertex(1, 0, 1, 0, 1),
				IGraphics::CColorVertex(2, 1, 1, 0, 1),
				IGraphics::CColorVertex(3, 0, 1, 0, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

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
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 0, 1, 0, 1),
				IGraphics::CColorVertex(1, 0, 1, 1, 1),
				IGraphics::CColorVertex(2, 0, 1, 0, 1),
				IGraphics::CColorVertex(3, 0, 1, 1, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

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
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 0, 1, 1, 1),
				IGraphics::CColorVertex(1, 0, 0, 1, 1),
				IGraphics::CColorVertex(2, 0, 1, 1, 1),
				IGraphics::CColorVertex(3, 0, 0, 1, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

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
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 0, 0, 1, 1),
				IGraphics::CColorVertex(1, 1, 0, 1, 1),
				IGraphics::CColorVertex(2, 0, 0, 1, 1),
				IGraphics::CColorVertex(3, 1, 0, 1, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

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
			IGraphics::CColorVertex aColorVertices[] = {
				IGraphics::CColorVertex(0, 1, 0, 1, 1),
				IGraphics::CColorVertex(1, 1, 0, 0, 1),
				IGraphics::CColorVertex(2, 1, 0, 1, 1),
				IGraphics::CColorVertex(3, 1, 0, 0, 1)};
			Graphics()->SetColorVertex(aColorVertices, std::size(aColorVertices));

			IGraphics::CFreeformItem Freeform(
				CurXOff, pColorRect->y,
				CurXOff + SizeColor, pColorRect->y,
				CurXOff, pColorRect->y + pColorRect->h,
				CurXOff + SizeColor, pColorRect->y + pColorRect->h);
			Graphics()->QuadsDrawFreeform(&Freeform, 1);
		}
	};

	auto &&RenderSaturationRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.s = 0.0f;
		RightColor.s = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderLightingRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColor) {
		ColorHSLA LeftColor = color_cast<ColorHSLA>(CurColor);
		ColorHSLA RightColor = color_cast<ColorHSLA>(CurColor);

		LeftColor.l = DarkestLight;
		RightColor.l = 1.0f;

		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(LeftColor);
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(RightColor);

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	auto &&RenderAlphaRect = [&](CUIRect *pColorRect, const ColorRGBA &CurColorFull) {
		const ColorRGBA LeftColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(0.0f));
		const ColorRGBA RightColorRGBA = color_cast<ColorRGBA>(color_cast<ColorHSLA>(CurColorFull).WithAlpha(1.0f));

		Graphics()->SetColor4(LeftColorRGBA, RightColorRGBA, RightColorRGBA, LeftColorRGBA);

		IGraphics::CFreeformItem Freeform(
			pColorRect->x, pColorRect->y,
			pColorRect->x + pColorRect->w, pColorRect->y,
			pColorRect->x, pColorRect->y + pColorRect->h,
			pColorRect->x + pColorRect->w, pColorRect->y + pColorRect->h);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);
	};

	for(int i = 0; i < 3 + Alpha; i++)
	{
		CUIRect Button, Label;
		pRect->HSplitTop(SizePerEntry, &Button, pRect);
		pRect->HSplitTop(MarginPerEntry, nullptr, pRect);
		Button.VSplitLeft(10.0f, nullptr, &Button);
		Button.VSplitLeft(100.0f, &Label, &Button);

		Button.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

		CUIRect Rail;
		Button.Margin(2.0f, &Rail);

		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%s: %03d", apLabels[i], round_to_int(Color[i] * 255.0f));
		Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_ML);

		ColorRGBA HandleColor;
		Graphics()->TextureClear();
		Graphics()->TrianglesBegin();
		if(i == 0)
		{
			RenderHueRect(&Rail);
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f));
		}
		else if(i == 1)
		{
			RenderSaturationRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, 1.0f, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f));
		}
		else if(i == 2)
		{
			RenderLightingRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, 0.5f, 1.0f)));
			HandleColor = color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight));
		}
		else if(i == 3)
		{
			RenderAlphaRect(&Rail, color_cast<ColorRGBA>(ColorHSLA(Color.h, Color.s, Color.l, 1.0f).UnclampLighting(DarkestLight)));
			HandleColor = color_cast<ColorRGBA>(Color.UnclampLighting(DarkestLight));
		}
		Graphics()->TrianglesEnd();

		Color[i] = Ui()->DoScrollbarH(&((char *)pColor)[i], &Button, Color[i], &HandleColor);
	}

	if(OriginalColor != Color)
	{
		*pColor = Color.Pack(Alpha);
	}
	return PrevPackedColor != *pColor;
}

enum
{
	APPEARANCE_TAB_HUD = 0,
	APPEARANCE_TAB_CHAT = 1,
	APPEARANCE_TAB_NAME_PLATE = 2,
	APPEARANCE_TAB_HOOK_COLLISION = 3,
	APPEARANCE_TAB_INFO_MESSAGES = 4,
	APPEARANCE_TAB_LASER = 5,
	NUMBER_OF_APPEARANCE_TABS = 6,
};

void CMenus::RenderSettingsAppearance(CUIRect MainView)
{
	char aBuf[128];
	static int s_CurTab = 0;

	CUIRect TabBar, LeftView, RightView, Button, Label;

	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / NUMBER_OF_APPEARANCE_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_APPEARANCE_TABS] = {};
	const char *apTabNames[NUMBER_OF_APPEARANCE_TABS] = {
		Localize("HUD"),
		Localize("Chat"),
		Localize("Name Plate"),
		Localize("Hook Collisions"),
		Localize("Info Messages"),
		Localize("Laser")};

	for(int Tab = APPEARANCE_TAB_HUD; Tab < NUMBER_OF_APPEARANCE_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == APPEARANCE_TAB_HUD ? IGraphics::CORNER_L : Tab == NUMBER_OF_APPEARANCE_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
		{
			s_CurTab = Tab;
		}
	}

	MainView.HSplitTop(10.0f, nullptr, &MainView);

	const float LineSize = 20.0f;
	const float ColorPickerLineSize = 25.0f;
	const float HeadlineFontSize = 20.0f;
	const float HeadlineHeight = 30.0f;
	const float MarginSmall = 5.0f;
	const float MarginBetweenViews = 20.0f;

	const float ColorPickerLabelSize = 13.0f;
	const float ColorPickerLineSpacing = 5.0f;

	if(s_CurTab == APPEARANCE_TAB_HUD)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** HUD ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("HUD"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// Switch of the entire HUD
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhud, Localize("Show ingame HUD"), &g_Config.m_ClShowhud, &LeftView, LineSize);

		// Switches of the various normal HUD elements
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudHealthAmmo, Localize("Show health, shields and ammo"), &g_Config.m_ClShowhudHealthAmmo, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudScore, Localize("Show score"), &g_Config.m_ClShowhudScore, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowLocalTimeAlways, Localize("Show local time always"), &g_Config.m_ClShowLocalTimeAlways, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSpecCursor, Localize("Show spectator cursor"), &g_Config.m_ClSpecCursor, &LeftView, LineSize);

		// Settings of the HUD element for votes
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowVotesAfterVoting, Localize("Show votes window after voting"), &g_Config.m_ClShowVotesAfterVoting, &LeftView, LineSize);

		// ***** Scoreboard ***** //
		LeftView.HSplitTop(MarginBetweenViews, nullptr, &LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Scoreboard"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		ColorRGBA GreenDefault(0.78f, 1.0f, 0.8f, 1.0f);
		static CButtonContainer s_AuthedColor, s_SameClanColor;
		DoLine_ColorPicker(&s_AuthedColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Authed name color in scoreboard"), &g_Config.m_ClAuthedPlayerColor, GreenDefault, false);
		DoLine_ColorPicker(&s_SameClanColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Same clan color in scoreboard"), &g_Config.m_ClSameClanColor, GreenDefault, false);

		// ***** DDRace HUD ***** //
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, Localize("DDRace HUD"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		// Switches of various DDRace HUD elements
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowIds, Localize("Show client IDs (scoreboard, chat, spectator)"), &g_Config.m_ClShowIds, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudDDRace, Localize("Show DDRace HUD"), &g_Config.m_ClShowhudDDRace, &RightView, LineSize);
		if(g_Config.m_ClShowhudDDRace)
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudJumpsIndicator, Localize("Show jumps indicator"), &g_Config.m_ClShowhudJumpsIndicator, &RightView, LineSize);
		}
		else
		{
			RightView.HSplitTop(LineSize, nullptr, &RightView); // Create empty space for hidden option
		}

		// Switch for dummy actions display
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudDummyActions, Localize("Show dummy actions"), &g_Config.m_ClShowhudDummyActions, &RightView, LineSize);

		// Player movement information display settings
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerPosition, Localize("Show player position"), &g_Config.m_ClShowhudPlayerPosition, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerSpeed, Localize("Show player speed"), &g_Config.m_ClShowhudPlayerSpeed, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerAngle, Localize("Show player target angle"), &g_Config.m_ClShowhudPlayerAngle, &RightView, LineSize);

		// Freeze bar settings
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFreezeBars, Localize("Show freeze bars"), &g_Config.m_ClShowFreezeBars, &RightView, LineSize);
		RightView.HSplitTop(2 * LineSize, &Button, &RightView);
		if(g_Config.m_ClShowFreezeBars)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClFreezeBarsAlphaInsideFreeze, &g_Config.m_ClFreezeBarsAlphaInsideFreeze, &Button, Localize("Opacity of freeze bars inside freeze"), 0, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
		}
	}
	else if(s_CurTab == APPEARANCE_TAB_CHAT)
	{
		CChat &Chat = GameClient()->m_Chat;
		CUIRect TopView, PreviewView;
		MainView.HSplitBottom(220.0f, &TopView, &PreviewView);
		TopView.HSplitBottom(MarginBetweenViews, &TopView, nullptr);
		TopView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Chat ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Chat"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General chat settings
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowChat, Localize("Show chat"), g_Config.m_ClShowChat, &Button))
		{
			g_Config.m_ClShowChat = g_Config.m_ClShowChat ? 0 : 1;
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClShowChat)
		{
			static int s_ShowChat = 0;
			if(DoButton_CheckBox(&s_ShowChat, Localize("Always show chat"), g_Config.m_ClShowChat == 2, &Button))
				g_Config.m_ClShowChat = g_Config.m_ClShowChat != 2 ? 2 : 1;
		}

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatTeamColors, Localize("Show names in chat in team colors"), &g_Config.m_ClChatTeamColors, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowChatFriends, Localize("Show only chat messages from friends"), &g_Config.m_ClShowChatFriends, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowChatTeamMembersOnly, Localize("Show only chat messages from team members"), &g_Config.m_ClShowChatTeamMembersOnly, &LeftView, LineSize);

		if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatOld, Localize("Use old chat style"), &g_Config.m_ClChatOld, &LeftView, LineSize))
			GameClient()->m_Chat.RebuildChat();

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClChatFontSize, &g_Config.m_ClChatFontSize, &Button, Localize("Chat font size"), 10, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE))
		{
			Chat.EnsureCoherentWidth();
			Chat.RebuildChat();
		}

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClChatWidth, &g_Config.m_ClChatWidth, &Button, Localize("Chat width"), 120, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE))
		{
			Chat.EnsureCoherentFontSize();
			Chat.RebuildChat();
		}

		// ***** Messages ***** //
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, Localize("Messages"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		// Message Colors and extra settings
		static CButtonContainer s_SystemMessageColor;
		DoLine_ColorPicker(&s_SystemMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, Localize("System message"), &g_Config.m_ClMessageSystemColor, ColorRGBA(1.0f, 1.0f, 0.5f), true, &g_Config.m_ClShowChatSystem);
		static CButtonContainer s_HighlightedMessageColor;
		DoLine_ColorPicker(&s_HighlightedMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, Localize("Highlighted message"), &g_Config.m_ClMessageHighlightColor, ColorRGBA(1.0f, 0.5f, 0.5f));
		static CButtonContainer s_TeamMessageColor;
		DoLine_ColorPicker(&s_TeamMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, Localize("Team message"), &g_Config.m_ClMessageTeamColor, ColorRGBA(0.65f, 1.0f, 0.65f));
		static CButtonContainer s_FriendMessageColor;
		DoLine_ColorPicker(&s_FriendMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, Localize("Friend message"), &g_Config.m_ClMessageFriendColor, ColorRGBA(1.0f, 0.137f, 0.137f), true, &g_Config.m_ClMessageFriend);
		static CButtonContainer s_NormalMessageColor;
		DoLine_ColorPicker(&s_NormalMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, Localize("Normal message"), &g_Config.m_ClMessageColor, ColorRGBA(1.0f, 1.0f, 1.0f));

		str_format(aBuf, sizeof(aBuf), "%s (echo)", Localize("Client message"));
		static CButtonContainer s_ClientMessageColor;
		DoLine_ColorPicker(&s_ClientMessageColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, aBuf, &g_Config.m_ClMessageClientColor, ColorRGBA(0.5f, 0.78f, 1.0f));

		// ***** Chat Preview ***** //
		PreviewView.HSplitTop(HeadlineHeight, &Label, &PreviewView);
		Ui()->DoLabel(&Label, Localize("Preview"), HeadlineFontSize, TEXTALIGN_ML);
		PreviewView.HSplitTop(MarginSmall, nullptr, &PreviewView);

		// Use the rest of the view for preview
		PreviewView.Draw(ColorRGBA(1, 1, 1, 0.1f), IGraphics::CORNER_ALL, 5.0f);
		PreviewView.Margin(MarginSmall, &PreviewView);

		ColorRGBA SystemColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		ColorRGBA HighlightedColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		ColorRGBA FriendColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
		ColorRGBA NormalColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageColor));
		ColorRGBA ClientColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		ColorRGBA DefaultNameColor(0.8f, 0.8f, 0.8f, 1.0f);

		const float RealFontSize = Chat.FontSize() * 2;
		const float RealMsgPaddingX = (!g_Config.m_ClChatOld ? Chat.MessagePaddingX() : 0) * 2;
		const float RealMsgPaddingY = (!g_Config.m_ClChatOld ? Chat.MessagePaddingY() : 0) * 2;
		const float RealMsgPaddingTee = (!g_Config.m_ClChatOld ? Chat.MessageTeeSize() + CChat::MESSAGE_TEE_PADDING_RIGHT : 0) * 2;
		const float RealOffsetY = RealFontSize + RealMsgPaddingY;

		const float X = RealMsgPaddingX / 2.0f + PreviewView.x;
		float Y = PreviewView.y;
		float LineWidth = g_Config.m_ClChatWidth * 2 - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

		str_copy(aBuf, Client()->PlayerName());

		const CAnimState *pIdleState = CAnimState::GetIdle();
		const float RealTeeSize = Chat.MessageTeeSize() * 2;
		const float RealTeeSizeHalved = Chat.MessageTeeSize();
		constexpr float TWSkinUnreliableOffset = -0.25f;
		const float OffsetTeeY = RealTeeSizeHalved;
		const float FullHeightMinusTee = RealOffsetY - RealTeeSize;

		struct SPreviewLine
		{
			int m_ClientId;
			bool m_Team;
			char m_aName[64];
			char m_aText[256];
			bool m_Friend;
			bool m_Player;
			bool m_Client;
			bool m_Highlighted;
			int m_TimesRepeated;

			CTeeRenderInfo m_RenderInfo;
		};

		static std::vector<SPreviewLine> s_vLines;

		enum ELineFlag
		{
			FLAG_TEAM = 1 << 0,
			FLAG_FRIEND = 1 << 1,
			FLAG_HIGHLIGHT = 1 << 2,
			FLAG_CLIENT = 1 << 3
		};
		enum
		{
			PREVIEW_SYS,
			PREVIEW_HIGHLIGHT,
			PREVIEW_TEAM,
			PREVIEW_FRIEND,
			PREVIEW_SPAMMER,
			PREVIEW_CLIENT
		};
		auto &&SetPreviewLine = [](int Index, int ClientId, const char *pName, const char *pText, int Flag, int Repeats) {
			SPreviewLine *pLine;
			if((int)s_vLines.size() <= Index)
			{
				s_vLines.emplace_back();
				pLine = &s_vLines.back();
			}
			else
			{
				pLine = &s_vLines[Index];
			}
			pLine->m_ClientId = ClientId;
			pLine->m_Team = Flag & FLAG_TEAM;
			pLine->m_Friend = Flag & FLAG_FRIEND;
			pLine->m_Player = ClientId >= 0;
			pLine->m_Highlighted = Flag & FLAG_HIGHLIGHT;
			pLine->m_Client = Flag & FLAG_CLIENT;
			pLine->m_TimesRepeated = Repeats;
			str_copy(pLine->m_aName, pName);
			str_copy(pLine->m_aText, pText);
		};
		auto &&SetLineSkin = [RealTeeSize](int Index, const CSkin *pSkin) {
			if(Index >= (int)s_vLines.size())
				return;
			s_vLines[Index].m_RenderInfo.m_Size = RealTeeSize;
			s_vLines[Index].m_RenderInfo.Apply(pSkin);
		};

		auto &&RenderPreview = [&](int LineIndex, int x, int y, bool Render = true) {
			if(LineIndex >= (int)s_vLines.size())
				return vec2(0, 0);
			CTextCursor LocalCursor;
			TextRender()->SetCursor(&LocalCursor, x, y, RealFontSize, Render ? TEXTFLAG_RENDER : 0);
			LocalCursor.m_LineWidth = LineWidth;
			const auto &Line = s_vLines[LineIndex];

			char aClientId[16] = "";
			if(g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_FORCE);
			}

			char aCount[12];
			if(Line.m_ClientId < 0)
				str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
			else
				str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

			if(Line.m_Player)
			{
				LocalCursor.m_X += RealMsgPaddingTee;

				if(Line.m_Friend && g_Config.m_ClMessageFriend)
				{
					if(Render)
						TextRender()->TextColor(FriendColor);
					TextRender()->TextEx(&LocalCursor, "♥ ", -1);
				}
			}

			ColorRGBA NameColor;
			if(Line.m_Team)
				NameColor = CalculateNameColor(color_cast<ColorHSLA>(TeamColor));
			else if(Line.m_Player)
				NameColor = DefaultNameColor;
			else if(Line.m_Client)
				NameColor = ClientColor;
			else
				NameColor = SystemColor;

			if(Render)
				TextRender()->TextColor(NameColor);

			TextRender()->TextEx(&LocalCursor, aClientId);
			TextRender()->TextEx(&LocalCursor, Line.m_aName);

			if(Line.m_TimesRepeated > 0)
			{
				if(Render)
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
				TextRender()->TextEx(&LocalCursor, aCount, -1);
			}

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				if(Render)
					TextRender()->TextColor(NameColor);
				TextRender()->TextEx(&LocalCursor, ": ", -1);
			}

			CTextCursor AppendCursor = LocalCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = LocalCursor.m_X;
				AppendCursor.m_LineWidth -= LocalCursor.m_LongestLineWidth;
			}

			if(Render)
			{
				if(Line.m_Highlighted)
					TextRender()->TextColor(HighlightedColor);
				else if(Line.m_Team)
					TextRender()->TextColor(TeamColor);
				else if(Line.m_Player)
					TextRender()->TextColor(NormalColor);
			}

			TextRender()->TextEx(&AppendCursor, Line.m_aText, -1);
			if(Render)
				TextRender()->TextColor(TextRender()->DefaultTextColor());

			return vec2{LocalCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth, AppendCursor.Height() + RealMsgPaddingY};
		};

		// Set preview lines
		{
			char aLineBuilder[128];

			str_format(aLineBuilder, sizeof(aLineBuilder), "'%s' entered and joined the game", aBuf);
			SetPreviewLine(PREVIEW_SYS, -1, "*** ", aLineBuilder, 0, 0);

			str_format(aLineBuilder, sizeof(aLineBuilder), "Hey, how are you %s?", aBuf);
			SetPreviewLine(PREVIEW_HIGHLIGHT, 7, "Random Tee", aLineBuilder, FLAG_HIGHLIGHT, 0);

			SetPreviewLine(PREVIEW_TEAM, 11, "Your Teammate", "Let's speedrun this!", FLAG_TEAM, 0);
			SetPreviewLine(PREVIEW_FRIEND, 8, "Friend", "Hello there", FLAG_FRIEND, 0);
			SetPreviewLine(PREVIEW_SPAMMER, 9, "Spammer", "Hey fools, I'm spamming here!", 0, 5);
			SetPreviewLine(PREVIEW_CLIENT, -1, "— ", "Echo command executed", FLAG_CLIENT, 0);
		}

		SetLineSkin(1, GameClient()->m_Skins.Find("pinky"));
		SetLineSkin(2, GameClient()->m_Skins.Find("default"));
		SetLineSkin(3, GameClient()->m_Skins.Find("cammostripes"));
		SetLineSkin(4, GameClient()->m_Skins.Find("beast"));

		// Backgrounds first
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0, 0, 0, 0.12f);

			float TempY = Y;
			const float RealBackgroundRounding = Chat.MessageRounding() * 2.0f;

			auto &&RenderMessageBackground = [&](int LineIndex) {
				auto Size = RenderPreview(LineIndex, 0, 0, false);
				Graphics()->DrawRectExt(X - RealMsgPaddingX / 2.0f, TempY - RealMsgPaddingY / 2.0f, Size.x + RealMsgPaddingX * 1.5f, Size.y, RealBackgroundRounding, IGraphics::CORNER_ALL);
				return Size.y;
			};

			if(g_Config.m_ClShowChatSystem)
			{
				TempY += RenderMessageBackground(PREVIEW_SYS);
			}

			if(!g_Config.m_ClShowChatFriends)
			{
				if(!g_Config.m_ClShowChatTeamMembersOnly)
					TempY += RenderMessageBackground(PREVIEW_HIGHLIGHT);
				TempY += RenderMessageBackground(PREVIEW_TEAM);
			}

			if(!g_Config.m_ClShowChatTeamMembersOnly)
				TempY += RenderMessageBackground(PREVIEW_FRIEND);

			if(!g_Config.m_ClShowChatFriends && !g_Config.m_ClShowChatTeamMembersOnly)
			{
				TempY += RenderMessageBackground(PREVIEW_SPAMMER);
			}

			TempY += RenderMessageBackground(PREVIEW_CLIENT);

			Graphics()->QuadsEnd();
		}

		// System
		if(g_Config.m_ClShowChatSystem)
		{
			Y += RenderPreview(PREVIEW_SYS, X, Y).y;
		}

		if(!g_Config.m_ClShowChatFriends)
		{
			// Highlighted
			if(!g_Config.m_ClChatOld && !g_Config.m_ClShowChatTeamMembersOnly)
				RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_HIGHLIGHT].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
			if(!g_Config.m_ClShowChatTeamMembersOnly)
				Y += RenderPreview(PREVIEW_HIGHLIGHT, X, Y).y;

			// Team
			if(!g_Config.m_ClChatOld)
				RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_TEAM].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
			Y += RenderPreview(PREVIEW_TEAM, X, Y).y;
		}

		// Friend
		if(!g_Config.m_ClChatOld && !g_Config.m_ClShowChatTeamMembersOnly)
			RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_FRIEND].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
		if(!g_Config.m_ClShowChatTeamMembersOnly)
			Y += RenderPreview(PREVIEW_FRIEND, X, Y).y;

		// Normal
		if(!g_Config.m_ClShowChatFriends && !g_Config.m_ClShowChatTeamMembersOnly)
		{
			if(!g_Config.m_ClChatOld)
				RenderTools()->RenderTee(pIdleState, &s_vLines[PREVIEW_SPAMMER].m_RenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), vec2(X + RealTeeSizeHalved, Y + OffsetTeeY + FullHeightMinusTee / 2.0f + TWSkinUnreliableOffset));
			Y += RenderPreview(PREVIEW_SPAMMER, X, Y).y;
		}
		// Client
		RenderPreview(PREVIEW_CLIENT, X, Y);

		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else if(s_CurTab == APPEARANCE_TAB_NAME_PLATE)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Name Plate ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Name Plate"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General name plate settings
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlates, Localize("Show name plates"), &g_Config.m_ClNamePlates, &LeftView, LineSize);
		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesSize, &g_Config.m_ClNamePlatesSize, &Button, Localize("Name plates size"), -50, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesClan, Localize("Show clan above name plates"), &g_Config.m_ClNamePlatesClan, &LeftView, LineSize);
		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesClan)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesClanSize, &g_Config.m_ClNamePlatesClanSize, &Button, Localize("Clan plates size"), -50, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesTeamcolors, Localize("Use team colors for name plates"), &g_Config.m_ClNamePlatesTeamcolors, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesFriendMark, Localize("Show friend mark (♥) in name plates"), &g_Config.m_ClNamePlatesFriendMark, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesIds, Localize("Show client IDs in name plates"), &g_Config.m_ClNamePlatesIds, &LeftView, LineSize);

		// ***** Hook Strength ***** //
		LeftView.HSplitTop(MarginBetweenViews, nullptr, &LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Hook Strength"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClNamePlatesStrong, Localize("Show hook strength icon indicator"), g_Config.m_ClNamePlatesStrong, &Button))
		{
			g_Config.m_ClNamePlatesStrong = g_Config.m_ClNamePlatesStrong ? 0 : 1;
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesStrong)
		{
			static int s_NamePlatesStrong = 0;
			if(DoButton_CheckBox(&s_NamePlatesStrong, Localize("Show hook strength number indicator"), g_Config.m_ClNamePlatesStrong == 2, &Button))
				g_Config.m_ClNamePlatesStrong = g_Config.m_ClNamePlatesStrong != 2 ? 2 : 1;
		}

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesStrong)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesStrongSize, &g_Config.m_ClNamePlatesStrongSize, &Button, Localize("Size of hook strength icon and number indicator"), -50, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		// ***** Key Presses ***** //
		LeftView.HSplitTop(MarginBetweenViews, nullptr, &LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Key Presses"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowDirection, Localize("Show other players' key presses"), g_Config.m_ClShowDirection >= 1 && g_Config.m_ClShowDirection != 3, &Button))
		{
			g_Config.m_ClShowDirection = g_Config.m_ClShowDirection ^ 1;
		}

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		static int s_ShowLocalPlayer = 0;
		if(DoButton_CheckBox(&s_ShowLocalPlayer, Localize("Show local player's key presses"), g_Config.m_ClShowDirection >= 2, &Button))
		{
			g_Config.m_ClShowDirection = g_Config.m_ClShowDirection ^ 3;
		}

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		if(g_Config.m_ClShowDirection > 0)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClDirectionSize, &g_Config.m_ClDirectionSize, &Button, Localize("Size of key press icons"), -50, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		// ***** Name Plate Preview ***** //
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, Localize("Preview"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(2.0f * MarginSmall, nullptr, &RightView);

		CTeeRenderInfo TeeRenderInfo;
		TeeRenderInfo.Apply(m_pClient->m_Skins.Find(g_Config.m_ClPlayerSkin));
		TeeRenderInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		TeeRenderInfo.m_Size = 64.0f;

		const vec2 Position = RightView.Center();
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, 0, vec2(1.0f, 0.0f), Position);

		GameClient()->m_NamePlates.RenderNamePlatePreview(Position);
	}
	else if(s_CurTab == APPEARANCE_TAB_HOOK_COLLISION)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Hookline ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Hook collision line"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General hookline settings
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowHookCollOwn, Localize("Show own player's hook collision line"), g_Config.m_ClShowHookCollOwn, &Button))
		{
			g_Config.m_ClShowHookCollOwn = g_Config.m_ClShowHookCollOwn ? 0 : 1;
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClShowHookCollOwn)
		{
			static int s_ShowHookCollOwn = 0;
			if(DoButton_CheckBox(&s_ShowHookCollOwn, Localize("Always show own player's hook collision line"), g_Config.m_ClShowHookCollOwn == 2, &Button))
				g_Config.m_ClShowHookCollOwn = g_Config.m_ClShowHookCollOwn != 2 ? 2 : 1;
		}

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowHookCollOther, Localize("Show other players' hook collision lines"), g_Config.m_ClShowHookCollOther, &Button))
		{
			g_Config.m_ClShowHookCollOther = g_Config.m_ClShowHookCollOther >= 1 ? 0 : 1;
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClShowHookCollOther)
		{
			static int s_ShowHookCollOther = 0;
			if(DoButton_CheckBox(&s_ShowHookCollOther, Localize("Always show other players' hook collision lines"), g_Config.m_ClShowHookCollOther == 2, &Button))
				g_Config.m_ClShowHookCollOther = g_Config.m_ClShowHookCollOther != 2 ? 2 : 1;
		}

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollSize, &g_Config.m_ClHookCollSize, &Button, Localize("Width of your own hook collision line"), 0, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollSizeOther, &g_Config.m_ClHookCollSizeOther, &Button, Localize("Width of others' hook collision line"), 0, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

		LeftView.HSplitTop(2 * LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollAlpha, &g_Config.m_ClHookCollAlpha, &Button, Localize("Hook collision line opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

		static CButtonContainer s_HookCollNoCollResetId, s_HookCollHookableCollResetId, s_HookCollTeeCollResetId;
		static int s_HookCollToolTip;

		LeftView.HSplitTop(LineSize, &Label, &LeftView);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		Ui()->DoLabel(&Label, Localize("Colors of the hook collision line, in case of a possible collision with:"), 13.0f, TEXTALIGN_ML);
		Ui()->DoButtonLogic(&s_HookCollToolTip, 0, &Label); // Just for the tooltip, result ignored
		GameClient()->m_Tooltips.DoToolTip(&s_HookCollToolTip, &Label, Localize("Your movements are not taken into account when calculating the line colors"));
		DoLine_ColorPicker(&s_HookCollNoCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Nothing hookable"), &g_Config.m_ClHookCollColorNoColl, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), false);
		DoLine_ColorPicker(&s_HookCollHookableCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Something hookable"), &g_Config.m_ClHookCollColorHookableColl, ColorRGBA(130.0f / 255.0f, 232.0f / 255.0f, 160.0f / 255.0f, 1.0f), false);
		DoLine_ColorPicker(&s_HookCollTeeCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("A Tee"), &g_Config.m_ClHookCollColorTeeColl, ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f), false);

		// ***** Hook collisions preview ***** //
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, Localize("Preview"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);

		auto DoHookCollision = [this](const vec2 &Pos, const float &Length, const int &Size, const ColorRGBA &Color, const bool &Invert) {
			ColorRGBA ColorModified = Color;
			if(Invert)
				ColorModified = color_invert(ColorModified);
			ColorModified = ColorModified.WithAlpha((float)g_Config.m_ClHookCollAlpha / 100);
			Graphics()->TextureClear();
			if(Size > 0)
			{
				Graphics()->QuadsBegin();
				Graphics()->SetColor(ColorModified);
				float LineWidth = 0.5f + (float)(Size - 1) * 0.25f;
				IGraphics::CQuadItem QuadItem(Pos.x, Pos.y - LineWidth, Length, LineWidth * 2.f);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
			else
			{
				Graphics()->LinesBegin();
				Graphics()->SetColor(ColorModified);
				IGraphics::CLineItem LineItem(Pos.x, Pos.y, Pos.x + Length, Pos.y);
				Graphics()->LinesDraw(&LineItem, 1);
				Graphics()->LinesEnd();
			}
		};

		CTeeRenderInfo OwnSkinInfo;
		OwnSkinInfo.Apply(m_pClient->m_Skins.Find(g_Config.m_ClPlayerSkin));
		OwnSkinInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		OwnSkinInfo.m_Size = 50.0f;

		CTeeRenderInfo DummySkinInfo;
		DummySkinInfo.Apply(m_pClient->m_Skins.Find(g_Config.m_ClDummySkin));
		DummySkinInfo.ApplyColors(g_Config.m_ClDummyUseCustomColor, g_Config.m_ClDummyColorBody, g_Config.m_ClDummyColorFeet);
		DummySkinInfo.m_Size = 50.0f;

		vec2 TeeRenderPos, DummyRenderPos;

		const float LineLength = 150.f;
		const float LeftMargin = 30.f;

		const int TileScale = 32.0f;

		// Toggled via checkbox later, inverts some previews
		static bool s_HookCollPressed = false;

		CUIRect PreviewColl;

		// ***** Unhookable Tile Preview *****
		CUIRect PreviewNoColl;
		RightView.HSplitTop(50.0f, &PreviewNoColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewNoColl.x + LeftMargin, PreviewNoColl.y + PreviewNoColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewNoColl.w - LineLength, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl)), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		CUIRect NoHookTileRect;
		PreviewNoColl.VSplitRight(LineLength, &PreviewNoColl, &NoHookTileRect);
		NoHookTileRect.VSplitLeft(50.0f, &NoHookTileRect, nullptr);
		NoHookTileRect.Margin(10.0f, &NoHookTileRect);

		// Render unhookable tile
		Graphics()->TextureClear();
		Graphics()->TextureSet(m_pClient->m_MapImages.GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->BlendNormal();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		RenderTools()->RenderTile(NoHookTileRect.x, NoHookTileRect.y, TILE_NOHOOK, TileScale, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		// ***** Hookable Tile Preview *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl)), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		CUIRect HookTileRect;
		PreviewColl.VSplitRight(LineLength, &PreviewColl, &HookTileRect);
		HookTileRect.VSplitLeft(50.0f, &HookTileRect, nullptr);
		HookTileRect.Margin(10.0f, &HookTileRect);

		// Render hookable tile
		Graphics()->TextureClear();
		Graphics()->TextureSet(m_pClient->m_MapImages.GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->BlendNormal();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		RenderTools()->RenderTile(HookTileRect.x, HookTileRect.y, TILE_SOLID, TileScale, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		// ***** Hook Dummy Preivew *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DummyRenderPos = vec2(PreviewColl.x + PreviewColl.w - LineLength - 5.f + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength - 15.f, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &DummySkinInfo, 0, vec2(1.0f, 0.0f), DummyRenderPos);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		// ***** Hook Dummy Reverse Preivew *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DummyRenderPos = vec2(PreviewColl.x + PreviewColl.w - LineLength - 5.f + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength - 15.f, g_Config.m_ClHookCollSizeOther, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)), false);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), DummyRenderPos);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &DummySkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		// ***** Preview +hookcoll pressed toggle *****
		RightView.HSplitTop(LineSize, &Button, &RightView);
		if(DoButton_CheckBox(&s_HookCollPressed, Localize("Preview 'Hook collisions' being pressed"), s_HookCollPressed, &Button))
			s_HookCollPressed = !s_HookCollPressed;
	}
	else if(s_CurTab == APPEARANCE_TAB_INFO_MESSAGES)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Info Messages ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Info Messages"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General info messages settings
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowKillMessages, Localize("Show kill messages"), g_Config.m_ClShowKillMessages, &Button))
		{
			g_Config.m_ClShowKillMessages ^= 1;
		}

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_CheckBox(&g_Config.m_ClShowFinishMessages, Localize("Show finish messages"), g_Config.m_ClShowFinishMessages, &Button))
		{
			g_Config.m_ClShowFinishMessages ^= 1;
		}

		static CButtonContainer s_KillMessageNormalColorId, s_KillMessageHighlightColorId;
		DoLine_ColorPicker(&s_KillMessageNormalColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Normal Color"), &g_Config.m_ClKillMessageNormalColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		DoLine_ColorPicker(&s_KillMessageHighlightColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Highlight Color"), &g_Config.m_ClKillMessageHighlightColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
	}
	else if(s_CurTab == APPEARANCE_TAB_LASER)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Weapons ***** //
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Weapons"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General weapon laser settings
		static CButtonContainer s_LaserRifleOutResetId, s_LaserRifleInResetId, s_LaserShotgunOutResetId, s_LaserShotgunInResetId;

		ColorHSLA LaserRifleOutlineColor = DoLine_ColorPicker(&s_LaserRifleOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Rifle Laser Outline Color"), &g_Config.m_ClLaserRifleOutlineColor, ColorRGBA(0.074402f, 0.074402f, 0.247166f, 1.0f), false);
		ColorHSLA LaserRifleInnerColor = DoLine_ColorPicker(&s_LaserRifleInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Rifle Laser Inner Color"), &g_Config.m_ClLaserRifleInnerColor, ColorRGBA(0.498039f, 0.498039f, 1.0f, 1.0f), false);
		ColorHSLA LaserShotgunOutlineColor = DoLine_ColorPicker(&s_LaserShotgunOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Shotgun Laser Outline Color"), &g_Config.m_ClLaserShotgunOutlineColor, ColorRGBA(0.125490f, 0.098039f, 0.043137f, 1.0f), false);
		ColorHSLA LaserShotgunInnerColor = DoLine_ColorPicker(&s_LaserShotgunInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Shotgun Laser Inner Color"), &g_Config.m_ClLaserShotgunInnerColor, ColorRGBA(0.570588f, 0.417647f, 0.252941f, 1.0f), false);

		// ***** Entities ***** //
		LeftView.HSplitTop(10.0f, nullptr, &LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, Localize("Entities"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General entity laser settings
		static CButtonContainer s_LaserDoorOutResetId, s_LaserDoorInResetId, s_LaserFreezeOutResetId, s_LaserFreezeInResetId;

		ColorHSLA LaserDoorOutlineColor = DoLine_ColorPicker(&s_LaserDoorOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Door Laser Outline Color"), &g_Config.m_ClLaserDoorOutlineColor, ColorRGBA(0.0f, 0.131372f, 0.096078f, 1.0f), false);
		ColorHSLA LaserDoorInnerColor = DoLine_ColorPicker(&s_LaserDoorInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Door Laser Inner Color"), &g_Config.m_ClLaserDoorInnerColor, ColorRGBA(0.262745f, 0.760784f, 0.639215f, 1.0f), false);
		ColorHSLA LaserFreezeOutlineColor = DoLine_ColorPicker(&s_LaserFreezeOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Freeze Laser Outline Color"), &g_Config.m_ClLaserFreezeOutlineColor, ColorRGBA(0.131372f, 0.123529f, 0.182352f, 1.0f), false);
		ColorHSLA LaserFreezeInnerColor = DoLine_ColorPicker(&s_LaserFreezeInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Freeze Laser Inner Color"), &g_Config.m_ClLaserFreezeInnerColor, ColorRGBA(0.482352f, 0.443137f, 0.564705f, 1.0f), false);

		static CButtonContainer s_AllToRifleResetId, s_AllToDefaultResetId;

		LeftView.HSplitTop(4 * MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_Menu(&s_AllToRifleResetId, Localize("Set all to Rifle"), 0, &Button))
		{
			g_Config.m_ClLaserShotgunOutlineColor = g_Config.m_ClLaserRifleOutlineColor;
			g_Config.m_ClLaserShotgunInnerColor = g_Config.m_ClLaserRifleInnerColor;
			g_Config.m_ClLaserDoorOutlineColor = g_Config.m_ClLaserRifleOutlineColor;
			g_Config.m_ClLaserDoorInnerColor = g_Config.m_ClLaserRifleInnerColor;
			g_Config.m_ClLaserFreezeOutlineColor = g_Config.m_ClLaserRifleOutlineColor;
			g_Config.m_ClLaserFreezeInnerColor = g_Config.m_ClLaserRifleInnerColor;
		}

		// values taken from the CL commands
		LeftView.HSplitTop(2 * MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(DoButton_Menu(&s_AllToDefaultResetId, Localize("Reset to defaults"), 0, &Button))
		{
			g_Config.m_ClLaserRifleOutlineColor = 11176233;
			g_Config.m_ClLaserRifleInnerColor = 11206591;
			g_Config.m_ClLaserShotgunOutlineColor = 1866773;
			g_Config.m_ClLaserShotgunInnerColor = 1467241;
			g_Config.m_ClLaserDoorOutlineColor = 7667473;
			g_Config.m_ClLaserDoorInnerColor = 7701379;
			g_Config.m_ClLaserFreezeOutlineColor = 11613223;
			g_Config.m_ClLaserFreezeInnerColor = 12001153;
		}

		// ***** Laser Preview ***** //
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, Localize("Preview"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		const float LaserPreviewHeight = 50.0f;
		CUIRect LaserPreview;
		RightView.HSplitTop(LaserPreviewHeight, &LaserPreview, &RightView);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);
		DoLaserPreview(&LaserPreview, LaserRifleOutlineColor, LaserRifleInnerColor, LASERTYPE_RIFLE);

		RightView.HSplitTop(LaserPreviewHeight, &LaserPreview, &RightView);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);
		DoLaserPreview(&LaserPreview, LaserShotgunOutlineColor, LaserShotgunInnerColor, LASERTYPE_SHOTGUN);

		RightView.HSplitTop(LaserPreviewHeight, &LaserPreview, &RightView);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);
		DoLaserPreview(&LaserPreview, LaserDoorOutlineColor, LaserDoorInnerColor, LASERTYPE_DOOR);

		RightView.HSplitTop(LaserPreviewHeight, &LaserPreview, &RightView);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);
		DoLaserPreview(&LaserPreview, LaserFreezeOutlineColor, LaserFreezeInnerColor, LASERTYPE_DOOR);
	}
}

void CMenus::RenderSettingsDDNet(CUIRect MainView)
{
	CUIRect Button, Left, Right, LeftLeft, Label;

#if defined(CONF_AUTOUPDATE)
	CUIRect UpdaterRect;
	MainView.HSplitBottom(20.0f, &MainView, &UpdaterRect);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
#endif

	// demo
	CUIRect Demo;
	MainView.HSplitTop(110.0f, &Demo, &MainView);
	Demo.HSplitTop(30.0f, &Label, &Demo);
	Ui()->DoLabel(&Label, Localize("Demo"), 20.0f, TEXTALIGN_ML);
	Label.VSplitMid(nullptr, &Label, 20.0f);
	Ui()->DoLabel(&Label, Localize("Ghost"), 20.0f, TEXTALIGN_ML);

	Demo.HSplitTop(5.0f, nullptr, &Demo);
	Demo.VSplitMid(&Left, &Right, 20.0f);

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClAutoRaceRecord, Localize("Save the best demo of each race"), g_Config.m_ClAutoRaceRecord, &Button))
	{
		g_Config.m_ClAutoRaceRecord ^= 1;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClReplays, Localize("Enable replays"), g_Config.m_ClReplays, &Button))
	{
		g_Config.m_ClReplays ^= 1;
		Client()->DemoRecorder_UpdateReplayRecorder();
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(g_Config.m_ClReplays)
		Ui()->DoScrollbarOption(&g_Config.m_ClReplayLength, &g_Config.m_ClReplayLength, &Button, Localize("Default length"), 10, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClRaceGhost, Localize("Enable ghost"), g_Config.m_ClRaceGhost, &Button))
	{
		g_Config.m_ClRaceGhost ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClRaceGhost, &Button, Localize("When you cross the start line, show a ghost tee replicating the movements of your best time"));

	if(g_Config.m_ClRaceGhost)
	{
		Right.HSplitTop(20.0f, &Button, &Right);
		Button.VSplitMid(&LeftLeft, &Button);
		if(DoButton_CheckBox(&g_Config.m_ClRaceShowGhost, Localize("Show ghost"), g_Config.m_ClRaceShowGhost, &LeftLeft))
		{
			g_Config.m_ClRaceShowGhost ^= 1;
		}
		Ui()->DoScrollbarOption(&g_Config.m_ClRaceGhostAlpha, &g_Config.m_ClRaceGhostAlpha, &Button, Localize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

		Right.HSplitTop(20.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClRaceSaveGhost, Localize("Save ghost"), g_Config.m_ClRaceSaveGhost, &Button))
		{
			g_Config.m_ClRaceSaveGhost ^= 1;
		}

		if(g_Config.m_ClRaceSaveGhost)
		{
			Right.HSplitTop(20.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClRaceGhostSaveBest, Localize("Only save improvements"), g_Config.m_ClRaceGhostSaveBest, &Button))
			{
				g_Config.m_ClRaceGhostSaveBest ^= 1;
			}
		}
	}

	// gameplay
	CUIRect Gameplay;
	MainView.HSplitTop(150.0f, &Gameplay, &MainView);
	Gameplay.HSplitTop(30.0f, &Label, &Gameplay);
	Ui()->DoLabel(&Label, Localize("Gameplay"), 20.0f, TEXTALIGN_ML);
	Gameplay.HSplitTop(5.0f, nullptr, &Gameplay);
	Gameplay.VSplitMid(&Left, &Right, 20.0f);

	Left.HSplitTop(20.0f, &Button, &Left);
	Ui()->DoScrollbarOption(&g_Config.m_ClOverlayEntities, &g_Config.m_ClOverlayEntities, &Button, Localize("Overlay entities"), 0, 100);

	Left.HSplitTop(20.0f, &Button, &Left);
	Button.VSplitMid(&LeftLeft, &Button);

	if(DoButton_CheckBox(&g_Config.m_ClTextEntities, Localize("Show text entities"), g_Config.m_ClTextEntities, &LeftLeft))
		g_Config.m_ClTextEntities ^= 1;

	if(g_Config.m_ClTextEntities)
	{
		if(Ui()->DoScrollbarOption(&g_Config.m_ClTextEntitiesSize, &g_Config.m_ClTextEntitiesSize, &Button, Localize("Size"), 0, 100))
			m_pClient->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	Button.VSplitMid(&LeftLeft, &Button);

	if(DoButton_CheckBox(&g_Config.m_ClShowOthers, Localize("Show others"), g_Config.m_ClShowOthers == SHOW_OTHERS_ON, &LeftLeft))
		g_Config.m_ClShowOthers = g_Config.m_ClShowOthers != SHOW_OTHERS_ON ? SHOW_OTHERS_ON : SHOW_OTHERS_OFF;

	Ui()->DoScrollbarOption(&g_Config.m_ClShowOthersAlpha, &g_Config.m_ClShowOthersAlpha, &Button, Localize("Opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClShowOthersAlpha, &Button, Localize("Adjust the opacity of entities belonging to other teams, such as tees and name plates"));

	Left.HSplitTop(20.0f, &Button, &Left);
	static int s_ShowOwnTeamId = 0;
	if(DoButton_CheckBox(&s_ShowOwnTeamId, Localize("Show others (own team only)"), g_Config.m_ClShowOthers == SHOW_OTHERS_ONLY_TEAM, &Button))
	{
		g_Config.m_ClShowOthers = g_Config.m_ClShowOthers != SHOW_OTHERS_ONLY_TEAM ? SHOW_OTHERS_ONLY_TEAM : SHOW_OTHERS_OFF;
	}

	Left.HSplitTop(20.0f, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_ClShowQuads, Localize("Show quads"), g_Config.m_ClShowQuads, &Button))
	{
		g_Config.m_ClShowQuads ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClShowQuads, &Button, Localize("Quads are used for background decoration"));

	Right.HSplitTop(20.0f, &Button, &Right);
	if(Ui()->DoScrollbarOption(&g_Config.m_ClDefaultZoom, &g_Config.m_ClDefaultZoom, &Button, Localize("Default zoom"), 0, 20))
		m_pClient->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClAntiPing, Localize("AntiPing"), g_Config.m_ClAntiPing, &Button))
	{
		g_Config.m_ClAntiPing ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClAntiPing, &Button, Localize("Tries to predict other entities to give a feel of low latency"));

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

	CUIRect Background, Miscellaneous;
	MainView.VSplitMid(&Background, &Miscellaneous, 20.0f);

	// background
	Background.HSplitTop(30.0f, &Label, &Background);
	Background.HSplitTop(5.0f, nullptr, &Background);
	Ui()->DoLabel(&Label, Localize("Background"), 20.0f, TEXTALIGN_ML);

	ColorRGBA GreyDefault(0.5f, 0.5f, 0.5f, 1);

	static CButtonContainer s_ResetId1;
	DoLine_ColorPicker(&s_ResetId1, 25.0f, 13.0f, 5.0f, &Background, Localize("Regular background color"), &g_Config.m_ClBackgroundColor, GreyDefault, false);

	static CButtonContainer s_ResetId2;
	DoLine_ColorPicker(&s_ResetId2, 25.0f, 13.0f, 5.0f, &Background, Localize("Entities background color"), &g_Config.m_ClBackgroundEntitiesColor, GreyDefault, false);

	CUIRect EditBox, ReloadButton;
	Background.HSplitTop(20.0f, &Label, &Background);
	Background.HSplitTop(2.0f, nullptr, &Background);
	Label.VSplitLeft(100.0f, &Label, &EditBox);
	EditBox.VSplitRight(60.0f, &EditBox, &Button);
	Button.VSplitMid(&ReloadButton, &Button, 5.0f);
	EditBox.VSplitRight(5.0f, &EditBox, nullptr);

	Ui()->DoLabel(&Label, Localize("Map"), 14.0f, TEXTALIGN_ML);

	static CLineInput s_BackgroundEntitiesInput(g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities));
	Ui()->DoEditBox(&s_BackgroundEntitiesInput, &EditBox, 14.0f);

	static CButtonContainer s_BackgroundEntitiesMapPicker, s_BackgroundEntitiesReload;

	if(DoButton_FontIcon(&s_BackgroundEntitiesReload, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &ReloadButton))
	{
		m_pClient->m_Background.LoadBackground();
	}

	if(DoButton_FontIcon(&s_BackgroundEntitiesMapPicker, FONT_ICON_FOLDER, 0, &Button))
	{
		static SPopupMenuId s_PopupMapPickerId;
		static CPopupMapPickerContext s_PopupMapPickerContext;
		s_PopupMapPickerContext.m_pMenus = this;
		s_PopupMapPickerContext.MapListPopulate();
		Ui()->DoPopupMenu(&s_PopupMapPickerId, Ui()->MouseX(), Ui()->MouseY(), 300.0f, 250.0f, &s_PopupMapPickerContext, PopupMapPicker);
	}

	Background.HSplitTop(20.0f, &Button, &Background);
	const bool UseCurrentMap = str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0;
	static int s_UseCurrentMapId = 0;
	if(DoButton_CheckBox(&s_UseCurrentMapId, Localize("Use current map as background"), UseCurrentMap, &Button))
	{
		if(UseCurrentMap)
			g_Config.m_ClBackgroundEntities[0] = '\0';
		else
			str_copy(g_Config.m_ClBackgroundEntities, CURRENT_MAP);
		m_pClient->m_Background.LoadBackground();
	}

	Background.HSplitTop(20.0f, &Button, &Background);
	if(DoButton_CheckBox(&g_Config.m_ClBackgroundShowTilesLayers, Localize("Show tiles layers from BG map"), g_Config.m_ClBackgroundShowTilesLayers, &Button))
		g_Config.m_ClBackgroundShowTilesLayers ^= 1;

	// miscellaneous
	Miscellaneous.HSplitTop(30.0f, &Label, &Miscellaneous);
	Miscellaneous.HSplitTop(5.0f, nullptr, &Miscellaneous);

	Ui()->DoLabel(&Label, Localize("Miscellaneous"), 20.0f, TEXTALIGN_ML);

	static CButtonContainer s_ButtonTimeout;
	Miscellaneous.HSplitTop(20.0f, &Button, &Miscellaneous);
	if(DoButton_Menu(&s_ButtonTimeout, Localize("New random timeout code"), 0, &Button))
	{
		Client()->GenerateTimeoutSeed();
	}

	Miscellaneous.HSplitTop(5.0f, nullptr, &Miscellaneous);
	Miscellaneous.HSplitTop(20.0f, &Label, &Miscellaneous);
	Miscellaneous.HSplitTop(2.0f, nullptr, &Miscellaneous);
	Ui()->DoLabel(&Label, Localize("Run on join"), 14.0f, TEXTALIGN_ML);
	Miscellaneous.HSplitTop(20.0f, &Button, &Miscellaneous);
	static CLineInput s_RunOnJoinInput(g_Config.m_ClRunOnJoin, sizeof(g_Config.m_ClRunOnJoin));
	s_RunOnJoinInput.SetEmptyText(Localize("Chat command (e.g. showall 1)"));
	Ui()->DoEditBox(&s_RunOnJoinInput, &Button, 14.0f);

#if defined(CONF_FAMILY_WINDOWS)
	static CButtonContainer s_ButtonUnregisterShell;
	Miscellaneous.HSplitTop(10.0f, nullptr, &Miscellaneous);
	Miscellaneous.HSplitTop(20.0f, &Button, &Miscellaneous);
	if(DoButton_Menu(&s_ButtonUnregisterShell, Localize("Unregister protocol and file extensions"), 0, &Button))
	{
		Client()->ShellUnregister();
	}
#endif

	// Updater
#if defined(CONF_AUTOUPDATE)
	{
		bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");
		IUpdater::EUpdaterState State = Updater()->GetCurrentState();

		// Update Button
		char aBuf[256];
		if(NeedUpdate && State <= IUpdater::CLEAN)
		{
			str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is available:"), Client()->LatestVersion());
			UpdaterRect.VSplitLeft(TextRender()->TextWidth(14.0f, aBuf, -1, -1.0f) + 10.0f, &UpdaterRect, &Button);
			Button.VSplitLeft(100.0f, &Button, nullptr);
			static CButtonContainer s_ButtonUpdate;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Update now"), 0, &Button))
			{
				Updater()->InitiateUpdate();
			}
		}
		else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
			str_copy(aBuf, Localize("Updating…"));
		else if(State == IUpdater::NEED_RESTART)
		{
			str_copy(aBuf, Localize("DDNet Client updated!"));
			m_NeedRestartUpdate = true;
		}
		else
		{
			str_copy(aBuf, Localize("No updates available"));
			UpdaterRect.VSplitLeft(TextRender()->TextWidth(14.0f, aBuf, -1, -1.0f) + 10.0f, &UpdaterRect, &Button);
			Button.VSplitLeft(100.0f, &Button, nullptr);
			static CButtonContainer s_ButtonUpdate;
			if(DoButton_Menu(&s_ButtonUpdate, Localize("Check now"), 0, &Button))
			{
				Client()->RequestDDNetInfo();
			}
		}
		Ui()->DoLabel(&UpdaterRect, aBuf, 14.0f, TEXTALIGN_ML);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
#endif
}

CUi::EPopupMenuFunctionResult CMenus::PopupMapPicker(void *pContext, CUIRect View, bool Active)
{
	CPopupMapPickerContext *pPopupContext = static_cast<CPopupMapPickerContext *>(pContext);
	CMenus *pMenus = pPopupContext->m_pMenus;

	static CListBox s_ListBox;
	s_ListBox.SetActive(Active);
	s_ListBox.DoStart(20.0f, pPopupContext->m_vMaps.size(), 1, 3, -1, &View, false);

	int MapIndex = 0;
	for(auto &Map : pPopupContext->m_vMaps)
	{
		MapIndex++;
		const CListboxItem Item = s_ListBox.DoNextItem(&Map, MapIndex == pPopupContext->m_Selection);
		if(!Item.m_Visible)
			continue;

		CUIRect Label, Icon;
		Item.m_Rect.VSplitLeft(20.0f, &Icon, &Label);

		char aLabelText[IO_MAX_PATH_LENGTH];
		str_copy(aLabelText, Map.m_aFilename);
		if(Map.m_IsDirectory)
			str_append(aLabelText, "/", sizeof(aLabelText));

		const char *pIconType;
		if(!Map.m_IsDirectory)
		{
			pIconType = FONT_ICON_MAP;
		}
		else
		{
			if(!str_comp(Map.m_aFilename, ".."))
				pIconType = FONT_ICON_FOLDER_TREE;
			else
				pIconType = FONT_ICON_FOLDER;
		}

		pMenus->TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		pMenus->TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		pMenus->Ui()->DoLabel(&Icon, pIconType, 12.0f, TEXTALIGN_ML);
		pMenus->TextRender()->SetRenderFlags(0);
		pMenus->TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		pMenus->Ui()->DoLabel(&Label, aLabelText, 10.0f, TEXTALIGN_ML);
	}

	const int NewSelected = s_ListBox.DoEnd();
	pPopupContext->m_Selection = NewSelected >= 0 ? NewSelected : -1;
	if(s_ListBox.WasItemSelected() || s_ListBox.WasItemActivated())
	{
		const CMapListItem &SelectedItem = pPopupContext->m_vMaps[pPopupContext->m_Selection];

		if(SelectedItem.m_IsDirectory)
		{
			if(!str_comp(SelectedItem.m_aFilename, ".."))
			{
				fs_parent_dir(pPopupContext->m_aCurrentMapFolder);
			}
			else
			{
				str_append(pPopupContext->m_aCurrentMapFolder, "/", sizeof(pPopupContext->m_aCurrentMapFolder));
				str_append(pPopupContext->m_aCurrentMapFolder, SelectedItem.m_aFilename, sizeof(pPopupContext->m_aCurrentMapFolder));
			}
			pPopupContext->MapListPopulate();
		}
		else
		{
			str_format(g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities), "%s/%s", pPopupContext->m_aCurrentMapFolder, SelectedItem.m_aFilename);
			pMenus->m_pClient->m_Background.LoadBackground();
			return CUi::POPUP_CLOSE_CURRENT;
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

void CMenus::CPopupMapPickerContext::MapListPopulate()
{
	m_vMaps.clear();
	char aTemp[IO_MAX_PATH_LENGTH];
	str_format(aTemp, sizeof(aTemp), "maps/%s", m_aCurrentMapFolder);
	m_pMenus->Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, aTemp, MapListFetchCallback, this);
	std::stable_sort(m_vMaps.begin(), m_vMaps.end(), CompareFilenameAscending);
	m_Selection = -1;
}

int CMenus::CPopupMapPickerContext::MapListFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CPopupMapPickerContext *pRealUser = (CPopupMapPickerContext *)pUser;
	if((!IsDir && !str_endswith(pInfo->m_pName, ".map")) || !str_comp(pInfo->m_pName, ".") || (!str_comp(pInfo->m_pName, "..") && (!str_comp(pRealUser->m_aCurrentMapFolder, ""))))
		return 0;

	CMapListItem Item;
	str_copy(Item.m_aFilename, pInfo->m_pName);
	Item.m_IsDirectory = IsDir;

	pRealUser->m_vMaps.emplace_back(Item);

	return 0;
}
