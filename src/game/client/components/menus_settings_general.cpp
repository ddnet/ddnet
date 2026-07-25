/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menu_background.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

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

void CMenus::RenderThemeSelection(CUIRect MainView)
{
	const std::vector<CTheme> &vThemes = GameClient()->m_MenuBackground.GetThemes();

	int SelectedTheme = -1;
	for(int i = 0; i < (int)vThemes.size(); i++)
	{
		if(str_comp(vThemes[i].m_Name.c_str(), g_Config.m_ClMenuMap) == 0)
		{
			SelectedTheme = i;
			break;
		}
	}
	const int OldSelected = SelectedTheme;

	static CListBox s_ListBox;
	s_ListBox.DoHeader(&MainView, Localize("Theme"), 20.0f);
	s_ListBox.DoStart(20.0f, vThemes.size(), 1, 3, SelectedTheme);

	for(int i = 0; i < (int)vThemes.size(); i++)
	{
		const CTheme &Theme = vThemes[i];
		const CListboxItem Item = s_ListBox.DoNextItem(&Theme.m_Name, i == SelectedTheme);

		if(!Item.m_Visible)
			continue;

		CUIRect Icon, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Icon, &Label);

		// draw icon if it exists
		if(Theme.m_IconTexture.IsValid())
		{
			Icon.VMargin(6.0f, &Icon);
			Icon.HMargin(3.0f, &Icon);
			Graphics()->TextureSet(Theme.m_IconTexture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(Icon.x, Icon.y, Icon.w, Icon.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		char aName[128];
		if(Theme.m_Name.empty())
			str_copy(aName, "(none)");
		else if(str_comp(Theme.m_Name.c_str(), "auto") == 0)
			str_copy(aName, "(seasons)");
		else if(str_comp(Theme.m_Name.c_str(), "rand") == 0)
			str_copy(aName, "(random)");
		else if(Theme.m_HasDay && Theme.m_HasNight)
			str_copy(aName, Theme.m_Name.c_str());
		else if(Theme.m_HasDay && !Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (day)", Theme.m_Name.c_str());
		else if(!Theme.m_HasDay && Theme.m_HasNight)
			str_format(aName, sizeof(aName), "%s (night)", Theme.m_Name.c_str());
		else // generic
			str_copy(aName, Theme.m_Name.c_str());

		Ui()->DoLabel(&Label, aName, 16.0f * CUi::ms_FontmodHeight, TEXTALIGN_ML);
	}

	SelectedTheme = s_ListBox.DoEnd();

	if(OldSelected != SelectedTheme)
	{
		const CTheme &Theme = vThemes[SelectedTheme];
		str_copy(g_Config.m_ClMenuMap, Theme.m_Name.c_str());
		GameClient()->m_MenuBackground.LoadMenuBackground(Theme.m_HasDay, Theme.m_HasNight);
	}
}
