/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/math.h>
#include <base/str.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/client/animstate.h>
#include <game/client/components/background.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/nameplates.h>
#include <game/client/components/skins.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <vector>

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
		Ui()->DoScrollbarOption(&g_Config.m_ClRefreshRate, &g_Config.m_ClRefreshRate, &Button, Localize("Update Rate"), 10, 1000, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_NOCLAMPVALUE | CUi::SCROLLBAR_OPTION_DELAYUPDATE, aBuf);
		Left.HSplitTop(5.0f, nullptr, &Left);
		Left.HSplitTop(20.0f, &Button, &Left);
		static int s_LowerRefreshRate;
		if(DoButton_CheckBox(&s_LowerRefreshRate, Localize("Save power by lowering update rate (higher input latency)"), g_Config.m_ClRefreshRate <= 480 && g_Config.m_ClRefreshRate != 0, &Button))
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

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	CUIRect Button;
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndEnable, Localize("Use sounds"), g_Config.m_SndEnable, &Button))
	{
		g_Config.m_SndEnable ^= 1;
		UpdateMusicState();
	}

	m_NeedRestartSound = g_Config.m_SndEnable && !Sound()->IsSoundEnabled();

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
		Ui()->DoScrollbarOption(&g_Config.m_SndGameVolume, &g_Config.m_SndGameVolume, &Button, Localize("Game sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider gui sounds
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndChatVolume, &g_Config.m_SndChatVolume, &Button, Localize("Chat sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
	}

	// volume slider map sounds
	{
		MainView.HSplitTop(5.0f, nullptr, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Ui()->DoScrollbarOption(&g_Config.m_SndMapVolume, &g_Config.m_SndMapVolume, &Button, Localize("Map sound volume"), 0, 100, &CUi::ms_LogarithmicScrollbarScale, 0u, "%");
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
	MainView.HSplitBottom(4.0f * CreditsFontSize + 2.0f * CreditsMargin, &List, &CreditsScroll);
	List.HSplitBottom(5.0f, &List, nullptr);

	RenderLanguageSelection(List);

	CreditsScroll.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);

	static CScrollRegion s_CreditsScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = CreditsFontSize;
	s_CreditsScrollRegion.Begin(&CreditsScroll, &ScrollParams);

	CTextCursor Cursor;
	Cursor.m_FontSize = CreditsFontSize;
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
			if(str_comp(g_Localization.Languages()[i].m_Filename.c_str(), g_Config.m_ClLanguagefile) == 0)
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
		GameClient()->m_CountryFlags.Render(Language.m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		Ui()->DoLabel(&Label, Language.m_Name.c_str(), 16.0f, TEXTALIGN_ML);
	}

	s_SelectedLanguage = s_ListBox.DoEnd();

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(g_Config.m_ClLanguagefile, g_Localization.Languages()[s_SelectedLanguage].m_Filename.c_str());
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
		m_MenusSettingsControls.Render(MainView);
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
		RenderSettingsAssets(MainView);
	}
	else
	{
		dbg_assert_failed("ui_settings_page invalid");
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
			if(Client()->State() == IClient::STATE_ONLINE || GameClient()->Editor()->HasUnsavedData())
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
		Button.VSplitLeft(140.0f, &Label, &Button);
		Label.VMargin(10.0f, &Label);

		Button.Draw(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f), IGraphics::CORNER_ALL, 1.0f);

		CUIRect Rail;
		Button.Margin(2.0f, &Rail);

		char aBuf[32];

		// Hue
		if(i == 0)
			str_format(aBuf, sizeof(aBuf), "%s: %.1f° (%03d)", apLabels[i], Color[i] * 360.0f, round_to_int(Color[i] * 255.0f));
		// Lht
		else if(i == 2)
		{
			// handle internal light clamping, see `UnclampLighting`
			float Lht = DarkestLight + Color[i] * (1.0f - DarkestLight);
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Lht * 100.0f, round_to_int(Color[i] * 255.0f));
		}
		// Sat and Alpha
		else
			str_format(aBuf, sizeof(aBuf), "%s: %.1f%% (%03d)", apLabels[i], Color[i] * 100.0f, round_to_int(Color[i] * 255.0f));
		Ui()->DoLabel(&Label, aBuf, 12.0f, TEXTALIGN_ML);

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

	CUIRect TabBar, LeftView, RightView, Button;

	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / (float)NUMBER_OF_APPEARANCE_TABS;
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
		const int Corners = Tab == APPEARANCE_TAB_HUD ? IGraphics::CORNER_L : (Tab == NUMBER_OF_APPEARANCE_TABS - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
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
		Ui()->DoLabel_AutoLineSize(Localize("HUD"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
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
		Ui()->DoLabel_AutoLineSize(Localize("Scoreboard"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		ColorRGBA GreenDefault(0.78f, 1.0f, 0.8f, 1.0f);
		static CButtonContainer s_AuthedColor, s_SameClanColor;
		DoLine_ColorPicker(&s_AuthedColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Authed name color in scoreboard"), &g_Config.m_ClAuthedPlayerColor, GreenDefault, false);
		DoLine_ColorPicker(&s_SameClanColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Same clan color in scoreboard"), &g_Config.m_ClSameClanColor, GreenDefault, false);

		// ***** DDRace HUD ***** //
		Ui()->DoLabel_AutoLineSize(Localize("DDRace HUD"), HeadlineFontSize,
			TEXTALIGN_ML, &RightView, HeadlineHeight);
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

		// Eye with a number of spectators
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudSpectatorCount, Localize("Show number of spectators"), &g_Config.m_ClShowhudSpectatorCount, &RightView, LineSize);

		// Switch for dummy actions display
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudDummyActions, Localize("Show dummy actions"), &g_Config.m_ClShowhudDummyActions, &RightView, LineSize);

		// Player movement information display settings
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerPosition, Localize("Show player position"), &g_Config.m_ClShowhudPlayerPosition, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerSpeed, Localize("Show player speed"), &g_Config.m_ClShowhudPlayerSpeed, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowhudPlayerAngle, Localize("Show player target angle"), &g_Config.m_ClShowhudPlayerAngle, &RightView, LineSize);

		// Freeze bar settings
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowFreezeBars, Localize("Show freeze bars"), &g_Config.m_ClShowFreezeBars, &RightView, LineSize);
		RightView.HSplitTop(LineSize * 2.0f, &Button, &RightView);
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
		Ui()->DoLabel_AutoLineSize(Localize("Chat"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
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

		// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClCensorChat, Localize("Censor profanity"), &g_Config.m_ClCensorChat, &LeftView, LineSize);

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClChatFontSize, &g_Config.m_ClChatFontSize, &Button, Localize("Chat font size"), 10, 100))
		{
			Chat.EnsureCoherentWidth();
			Chat.RebuildChat();
		}

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClChatWidth, &g_Config.m_ClChatWidth, &Button, Localize("Chat width"), 120, 400))
		{
			Chat.EnsureCoherentFontSize();
			Chat.RebuildChat();
		}

		static CButtonContainer s_BackgroundColor;
		DoLine_ColorPicker(&s_BackgroundColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Chat background color"), &g_Config.m_ClChatBackgroundColor, color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::ClChatBackgroundColor, true)), false, nullptr, true);

		// ***** Messages ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Messages"), HeadlineFontSize,
			TEXTALIGN_ML, &RightView, HeadlineHeight);
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
		Ui()->DoLabel_AutoLineSize(Localize("Preview"), HeadlineFontSize,
			TEXTALIGN_ML, &PreviewView, HeadlineHeight);
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
			LocalCursor.SetPosition(vec2(x, y));
			LocalCursor.m_FontSize = RealFontSize;
			LocalCursor.m_Flags = Render ? TEXTFLAG_RENDER : 0;
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
			Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true)));

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
		Ui()->DoLabel_AutoLineSize(Localize("Name Plate"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General name plate settings
		{
			int Pressed = (g_Config.m_ClNamePlates ? 2 : 0) + (g_Config.m_ClNamePlatesOwn ? 1 : 0);
			if(DoLine_RadioMenu(LeftView, Localize("Show name plates"),
				   m_vButtonContainersNamePlateShow,
				   {Localize("None", "Show name plates"), Localize("Own", "Show name plates"), Localize("Others", "Show name plates"), Localize("All", "Show name plates")},
				   {0, 1, 2, 3},
				   Pressed))
			{
				g_Config.m_ClNamePlates = Pressed & 2 ? 1 : 0;
				g_Config.m_ClNamePlatesOwn = Pressed & 1 ? 1 : 0;
			}
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesSize, &g_Config.m_ClNamePlatesSize, &Button, Localize("Name plates size"), -50, 100);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesOffset, &g_Config.m_ClNamePlatesOffset, &Button, Localize("Name plates offset"), 10, 50);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesClan, Localize("Show clan above name plates"), &g_Config.m_ClNamePlatesClan, &LeftView, LineSize);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesClan)
			Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesClanSize, &g_Config.m_ClNamePlatesClanSize, &Button, Localize("Clan plates size"), -50, 100);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesTeamcolors, Localize("Use team colors for name plates"), &g_Config.m_ClNamePlatesTeamcolors, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesFriendMark, Localize("Show friend icon in name plates"), &g_Config.m_ClNamePlatesFriendMark, &LeftView, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesIds, Localize("Show client IDs in name plates"), &g_Config.m_ClNamePlatesIds, &LeftView, LineSize);
		if(g_Config.m_ClNamePlatesIds > 0)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesIdsSeparateLine, Localize("Show client IDs on a separate line"), &g_Config.m_ClNamePlatesIdsSeparateLine, &LeftView, LineSize);
		else
			LeftView.HSplitTop(LineSize, nullptr, &LeftView);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesIds > 0 && g_Config.m_ClNamePlatesIdsSeparateLine > 0)
			Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesIdsSize, &g_Config.m_ClNamePlatesIdsSize, &Button, Localize("Client IDs size"), -50, 100);

		// ***** Hook Strength ***** //
		LeftView.HSplitTop(MarginBetweenViews, nullptr, &LeftView);
		Ui()->DoLabel_AutoLineSize(Localize("Hook Strength"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
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

		LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
		if(g_Config.m_ClNamePlatesStrong)
		{
			Ui()->DoScrollbarOption(&g_Config.m_ClNamePlatesStrongSize, &g_Config.m_ClNamePlatesStrongSize, &Button, Localize("Size of hook strength icon and number indicator"), -50, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);
		}

		// ***** Key Presses ***** //
		LeftView.HSplitTop(MarginBetweenViews, nullptr, &LeftView);
		Ui()->DoLabel_AutoLineSize(Localize("Key Presses"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		DoLine_RadioMenu(LeftView, Localize("Show players' key presses"),
			m_vButtonContainersNamePlateKeyPresses,
			{Localize("None", "Show players' key presses"), Localize("Own", "Show players' key presses"), Localize("Others", "Show players' key presses"), Localize("All", "Show players' key presses")},
			{0, 3, 1, 2},
			g_Config.m_ClShowDirection);

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		if(g_Config.m_ClShowDirection > 0)
			Ui()->DoScrollbarOption(&g_Config.m_ClDirectionSize, &g_Config.m_ClDirectionSize, &Button, Localize("Size of key press icons"), -50, 100);

		// ***** Name Plate Preview ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Preview"), HeadlineFontSize,
			TEXTALIGN_ML, &RightView, HeadlineHeight);
		RightView.HSplitTop(2.0f * MarginSmall, nullptr, &RightView);

		// ***** Name Plate Dummy Preview ***** //
		RightView.HSplitBottom(LineSize, &RightView, &Button);
		if(DoButton_CheckBox(&m_DummyNamePlatePreview, g_Config.m_ClDummy ? Localize("Preview player's name plate") : Localize("Preview dummy's name plate"), m_DummyNamePlatePreview, &Button))
			m_DummyNamePlatePreview = !m_DummyNamePlatePreview;

		int Dummy = g_Config.m_ClDummy != (m_DummyNamePlatePreview ? 1 : 0);

		const vec2 Position = RightView.Center();

		GameClient()->m_NamePlates.RenderNamePlatePreview(Position, Dummy);
	}
	else if(s_CurTab == APPEARANCE_TAB_HOOK_COLLISION)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Hookline ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Hook collision line"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
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

		LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollSize, &g_Config.m_ClHookCollSize, &Button, Localize("Width of your own hook collision line"), 0, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

		LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollSizeOther, &g_Config.m_ClHookCollSizeOther, &Button, Localize("Width of others' hook collision line"), 0, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

		LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_ClHookCollAlpha, &g_Config.m_ClHookCollAlpha, &Button, Localize("Hook collision line opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

		static CButtonContainer s_HookCollNoCollResetId, s_HookCollHookableCollResetId, s_HookCollTeeCollResetId, s_HookCollTipColorResetId;
		static int s_HookCollToolTip;

		Ui()->DoLabel_AutoLineSize(Localize("Colors of the hook collision line:"), 13.0f,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);

		Ui()->DoButtonLogic(&s_HookCollToolTip, 0, &LeftView, BUTTONFLAG_NONE); // Just for the tooltip, result ignored
		GameClient()->m_Tooltips.DoToolTip(&s_HookCollToolTip, &LeftView, Localize("Your movements are not taken into account when calculating the line colors"));
		DoLine_ColorPicker(&s_HookCollNoCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("When nothing is hookable", "Hook collision line color"), &g_Config.m_ClHookCollColorNoColl, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f), false);
		DoLine_ColorPicker(&s_HookCollHookableCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("When something is hookable", "Hook collision line color"), &g_Config.m_ClHookCollColorHookableColl, ColorRGBA(130.0f / 255.0f, 232.0f / 255.0f, 160.0f / 255.0f, 1.0f), false);
		DoLine_ColorPicker(&s_HookCollTeeCollResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("When a Tee is hookable", "Hook collision line color"), &g_Config.m_ClHookCollColorTeeColl, ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f), false);
		DoLine_ColorPicker(&s_HookCollTipColorResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Hook collision line tip", "Hook collision line color"), &g_Config.m_ClHookCollTipColor, ColorRGBA(1.0f, 1.0f, 0.0f, 0.5f), false, nullptr, true);

		// ***** Hook collisions preview ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Preview"), HeadlineFontSize,
			TEXTALIGN_ML, &RightView, HeadlineHeight);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);

		auto DoHookCollision = [this](const vec2 &Pos, const float &Length, const int &Size, const ColorRGBA &Color, const ColorRGBA &TipColor, const bool &Invert) {
			ColorRGBA ColorModified = Color;
			ColorRGBA TipColorModified = TipColor;
			if(Invert)
				ColorModified = color_invert(ColorModified);
			ColorModified = ColorModified.WithAlpha((float)g_Config.m_ClHookCollAlpha / 100);
			TipColorModified = TipColor.WithMultipliedAlpha((float)g_Config.m_ClHookCollAlpha / 100);
			Graphics()->TextureClear();
			if(Size > 0)
			{
				Graphics()->QuadsBegin();
				Graphics()->SetColor(ColorModified);
				float LineWidth = 0.5f + (float)(Size - 1) * 0.25f;
				IGraphics::CQuadItem QuadItem(Pos.x, Pos.y - LineWidth, Length, LineWidth * 2.f);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				if(TipColor.a > 0.0f)
				{
					Graphics()->SetColor(TipColorModified);
					IGraphics::CQuadItem TipQuadItem(Pos.x + Length, Pos.y - LineWidth, 15.f, LineWidth * 2.f);
					Graphics()->QuadsDrawTL(&TipQuadItem, 1);
				}
				Graphics()->QuadsEnd();
			}
			else
			{
				Graphics()->LinesBegin();
				Graphics()->SetColor(ColorModified);
				IGraphics::CLineItem LineItem(Pos.x, Pos.y, Pos.x + Length, Pos.y);
				Graphics()->LinesDraw(&LineItem, 1);
				if(TipColor.a > 0.0f)
				{
					Graphics()->SetColor(TipColorModified);
					IGraphics::CLineItem TipLineItem(Pos.x + Length, Pos.y, Pos.x + Length + 15.f, Pos.y);
					Graphics()->LinesDraw(&TipLineItem, 1);
				}
				Graphics()->LinesEnd();
			}
		};

		CTeeRenderInfo OwnSkinInfo;
		OwnSkinInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
		OwnSkinInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		OwnSkinInfo.m_Size = 50.0f;

		CTeeRenderInfo DummySkinInfo;
		DummySkinInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClDummySkin));
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
		DoHookCollision(TeeRenderPos, PreviewNoColl.w - LineLength, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl)), ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		CUIRect NoHookTileRect;
		PreviewNoColl.VSplitRight(LineLength, &PreviewNoColl, &NoHookTileRect);
		NoHookTileRect.VSplitLeft(50.0f, &NoHookTileRect, nullptr);
		NoHookTileRect.Margin(10.0f, &NoHookTileRect);

		// Render unhookable tile
		Graphics()->TextureClear();
		Graphics()->TextureSet(GameClient()->m_MapImages.GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		RenderMap()->RenderTile(NoHookTileRect.x, NoHookTileRect.y, TILE_NOHOOK, TileScale, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		// ***** Hookable Tile Preview *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl)), ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		CUIRect HookTileRect;
		PreviewColl.VSplitRight(LineLength, &PreviewColl, &HookTileRect);
		HookTileRect.VSplitLeft(50.0f, &HookTileRect, nullptr);
		HookTileRect.Margin(10.0f, &HookTileRect);

		// Render hookable tile
		Graphics()->TextureClear();
		Graphics()->TextureSet(GameClient()->m_MapImages.GetEntities(MAP_IMAGE_ENTITY_LAYER_TYPE_ALL_EXCEPT_SWITCH));
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		RenderMap()->RenderTile(HookTileRect.x, HookTileRect.y, TILE_SOLID, TileScale, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		// ***** Hook Dummy Preview *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DummyRenderPos = vec2(PreviewColl.x + PreviewColl.w - LineLength - 5.f + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength - 15.f, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)), ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &DummySkinInfo, 0, vec2(1.0f, 0.0f), DummyRenderPos);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		// ***** Hook Dummy Reverse Preview *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DummyRenderPos = vec2(PreviewColl.x + PreviewColl.w - LineLength - 5.f + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength - 15.f, g_Config.m_ClHookCollSizeOther, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)), ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), false);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), DummyRenderPos);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &DummySkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		// ***** Hook Line Tip Preview *****
		RightView.HSplitTop(50.0f, &PreviewColl, &RightView);
		RightView.HSplitTop(4 * MarginSmall, nullptr, &RightView);
		TeeRenderPos = vec2(PreviewColl.x + LeftMargin, PreviewColl.y + PreviewColl.h / 2.0f);
		DoHookCollision(TeeRenderPos, PreviewColl.w - LineLength - 15.f, g_Config.m_ClHookCollSize, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl)), color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollTipColor, true)), s_HookCollPressed);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &OwnSkinInfo, 0, vec2(1.0f, 0.0f), TeeRenderPos);

		// ***** Preview +hookcoll pressed toggle *****
		RightView.HSplitTop(LineSize, &Button, &RightView);
		if(DoButton_CheckBox(&s_HookCollPressed, Localize("Preview 'Hook collisions' being pressed"), s_HookCollPressed, &Button))
			s_HookCollPressed = !s_HookCollPressed;
	}
	else if(s_CurTab == APPEARANCE_TAB_INFO_MESSAGES)
	{
		MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);

		// ***** Info Messages ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Info Messages"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
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
		Ui()->DoLabel_AutoLineSize(Localize("Weapons"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General weapon laser settings
		static CButtonContainer s_LaserRifleOutResetId, s_LaserRifleInResetId, s_LaserShotgunOutResetId, s_LaserShotgunInResetId;

		ColorHSLA LaserRifleOutlineColor = DoLine_ColorPicker(&s_LaserRifleOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Rifle Laser Outline Color"), &g_Config.m_ClLaserRifleOutlineColor, ColorRGBA(0.074402f, 0.074402f, 0.247166f, 1.0f), false);
		ColorHSLA LaserRifleInnerColor = DoLine_ColorPicker(&s_LaserRifleInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Rifle Laser Inner Color"), &g_Config.m_ClLaserRifleInnerColor, ColorRGBA(0.498039f, 0.498039f, 1.0f, 1.0f), false);
		ColorHSLA LaserShotgunOutlineColor = DoLine_ColorPicker(&s_LaserShotgunOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Shotgun Laser Outline Color"), &g_Config.m_ClLaserShotgunOutlineColor, ColorRGBA(0.125490f, 0.098039f, 0.043137f, 1.0f), false);
		ColorHSLA LaserShotgunInnerColor = DoLine_ColorPicker(&s_LaserShotgunInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Shotgun Laser Inner Color"), &g_Config.m_ClLaserShotgunInnerColor, ColorRGBA(0.570588f, 0.417647f, 0.252941f, 1.0f), false);

		// ***** Entities ***** //
		LeftView.HSplitTop(10.0f, nullptr, &LeftView);
		Ui()->DoLabel_AutoLineSize(Localize("Entities"), HeadlineFontSize,
			TEXTALIGN_ML, &LeftView, HeadlineHeight);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		// General entity laser settings
		static CButtonContainer s_LaserDoorOutResetId, s_LaserDoorInResetId, s_LaserFreezeOutResetId, s_LaserFreezeInResetId, s_LaserDraggerOutResetId, s_LaserDraggerInResetId;

		ColorHSLA LaserDoorOutlineColor = DoLine_ColorPicker(&s_LaserDoorOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Door Laser Outline Color"), &g_Config.m_ClLaserDoorOutlineColor, ColorRGBA(0.0f, 0.131372f, 0.096078f, 1.0f), false);
		ColorHSLA LaserDoorInnerColor = DoLine_ColorPicker(&s_LaserDoorInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Door Laser Inner Color"), &g_Config.m_ClLaserDoorInnerColor, ColorRGBA(0.262745f, 0.760784f, 0.639215f, 1.0f), false);
		ColorHSLA LaserFreezeOutlineColor = DoLine_ColorPicker(&s_LaserFreezeOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Freeze Laser Outline Color"), &g_Config.m_ClLaserFreezeOutlineColor, ColorRGBA(0.131372f, 0.123529f, 0.182352f, 1.0f), false);
		ColorHSLA LaserFreezeInnerColor = DoLine_ColorPicker(&s_LaserFreezeInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Freeze Laser Inner Color"), &g_Config.m_ClLaserFreezeInnerColor, ColorRGBA(0.482352f, 0.443137f, 0.564705f, 1.0f), false);
		ColorHSLA LaserDraggerOutlineColor = DoLine_ColorPicker(&s_LaserDraggerOutResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Dragger Outline Color"), &g_Config.m_ClLaserDraggerOutlineColor, ColorRGBA(0.1640625f, 0.015625f, 0.015625f, 1.0f), false);
		ColorHSLA LaserDraggerInnerColor = DoLine_ColorPicker(&s_LaserDraggerInResetId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, Localize("Dragger Inner Color"), &g_Config.m_ClLaserDraggerInnerColor, ColorRGBA(.8666666f, .3725490f, .3725490f, 1.0f), false);

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
			g_Config.m_ClLaserDraggerOutlineColor = g_Config.m_ClLaserRifleOutlineColor;
			g_Config.m_ClLaserDraggerInnerColor = g_Config.m_ClLaserRifleInnerColor;
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
			g_Config.m_ClLaserDraggerOutlineColor = 57618;
			g_Config.m_ClLaserDraggerInnerColor = 42398;
		}

		// ***** Laser Preview ***** //
		Ui()->DoLabel_AutoLineSize(Localize("Preview"), HeadlineFontSize,
			TEXTALIGN_ML, &RightView, HeadlineHeight);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		const float LaserPreviewHeight = 60.0f;
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
		DoLaserPreview(&LaserPreview, LaserFreezeOutlineColor, LaserFreezeInnerColor, LASERTYPE_FREEZE);

		RightView.HSplitTop(LaserPreviewHeight, &LaserPreview, &RightView);
		RightView.HSplitTop(2 * MarginSmall, nullptr, &RightView);
		DoLaserPreview(&LaserPreview, LaserDraggerOutlineColor, LaserDraggerInnerColor, LASERTYPE_DRAGGER);
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
		if(Client()->State() == IClient::STATE_ONLINE)
		{
			Client()->DemoRecorder_UpdateReplayRecorder();
		}
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
	MainView.HSplitTop(170.0f, &Gameplay, &MainView);
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
		if(Ui()->DoScrollbarOption(&g_Config.m_ClTextEntitiesSize, &g_Config.m_ClTextEntitiesSize, &Button, Localize("Size"), 20, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE))
			GameClient()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
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
	if(DoButton_CheckBox(&g_Config.m_ClShowQuads, Localize("Show background quads"), g_Config.m_ClShowQuads, &Button))
	{
		g_Config.m_ClShowQuads ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_ClShowQuads, &Button, Localize("Quads are used for background decoration"));

	Left.HSplitTop(20.0f, &Button, &Left);
	if(Ui()->DoScrollbarOption(&g_Config.m_ClDefaultZoom, &g_Config.m_ClDefaultZoom, &Button, Localize("Default zoom"), 0, 20))
		GameClient()->m_Camera.SetZoom(CCamera::ZoomStepsToValue(g_Config.m_ClDefaultZoom - 10), g_Config.m_ClSmoothZoomTime, true);

	Right.HSplitTop(20.0f, &Button, &Right);
	Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, Localize("Prediction margin"), 1, 300);

	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClPredictEvents, Localize("Predict events (experimental)"), g_Config.m_ClPredictEvents, &Button))
	{
		g_Config.m_ClPredictEvents ^= 1;
	}

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

	if(Ui()->DoButton_FontIcon(&s_BackgroundEntitiesReload, FontIcon::ARROW_ROTATE_RIGHT, 0, &ReloadButton, BUTTONFLAG_LEFT))
	{
		GameClient()->m_Background.LoadBackground();
	}

	if(Ui()->DoButton_FontIcon(&s_BackgroundEntitiesMapPicker, FontIcon::FOLDER, 0, &Button, BUTTONFLAG_LEFT))
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
		GameClient()->m_Background.LoadBackground();
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
			str_append(aLabelText, "/");

		const char *pIconType;
		if(!Map.m_IsDirectory)
		{
			pIconType = FontIcon::MAP;
		}
		else
		{
			if(!str_comp(Map.m_aFilename, ".."))
				pIconType = FontIcon::FOLDER_TREE;
			else
				pIconType = FontIcon::FOLDER;
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
				dbg_assert(fs_parent_dir(pPopupContext->m_aCurrentMapFolder) == 0, "Parent folder item selected but there is no parent folder");
			}
			else
			{
				str_append(pPopupContext->m_aCurrentMapFolder, "/");
				str_append(pPopupContext->m_aCurrentMapFolder, SelectedItem.m_aFilename);
			}
			pPopupContext->MapListPopulate();
		}
		else
		{
			str_format(g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities), "%s/%s", pPopupContext->m_aCurrentMapFolder, SelectedItem.m_aFilename);
			pMenus->GameClient()->m_Background.LoadBackground();
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
