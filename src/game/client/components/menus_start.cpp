/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>

#include <engine/client/updater.h>
#include <engine/shared/config.h>

#include <game/version.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include "menus.h"

void CMenus::RenderStartMenu(CUIRect MainView)
{
	// render logo
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BANNER].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
	IGraphics::CQuadItem QuadItem(MainView.w/2-170, 60, 360, 103);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	const float Rounding = 10.0f;
	const float VMargin = MainView.w/2-190.0f;

	CUIRect TopMenu, BottomMenu;
	MainView.VMargin(VMargin, &TopMenu);
	TopMenu.HSplitTop(410.0f, &TopMenu, &BottomMenu);

	TopMenu.HSplitTop(145.0f, 0, &TopMenu);

	CUIRect Button;
	int NewPage = -1;

	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_SettingsButton;
	if(DoButton_Menu(&s_SettingsButton, Localize("Settings"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "settings" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || CheckHotKey(KEY_S))
		NewPage = PAGE_SETTINGS;

	/*TopMenu.HSplitBottom(5.0f, &TopMenu, 0); // little space
	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_LocalServerButton = 0;
	if(DoButton_Menu(&s_LocalServerButton, Localize("Local server"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "local_server" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || CheckHotKey(KEY_L))
	{
	}*/

	TopMenu.HSplitBottom(5.0f, &TopMenu, 0); // little space
	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_DemoButton;
	if(DoButton_Menu(&s_DemoButton, Localize("Demos"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "demos" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || CheckHotKey(KEY_D))
	{
		NewPage = PAGE_DEMOS;
	}

	static bool EditorHotkeyWasPressed = true;
	static float EditorHotKeyChecktime = 0;
	TopMenu.HSplitBottom(5.0f, &TopMenu, 0); // little space
	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_MapEditorButton;
	if(DoButton_Menu(&s_MapEditorButton, Localize("Editor"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "editor" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || (!EditorHotkeyWasPressed && Client()->LocalTime() - EditorHotKeyChecktime < 0.1f && CheckHotKey(KEY_E)))
	{
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
		EditorHotkeyWasPressed = true;
	}
	if(!Input()->KeyIsPressed(KEY_E))
	{
		EditorHotkeyWasPressed = false;
		EditorHotKeyChecktime = Client()->LocalTime();
	}

	TopMenu.HSplitBottom(5.0f, &TopMenu, 0); // little space
	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_PlayButton;
	if(DoButton_Menu(&s_PlayButton, Localize("Play"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "play_game" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || m_EnterPressed || CheckHotKey(KEY_P))
		NewPage = g_Config.m_UiPage;

	TopMenu.HSplitBottom(5.0f, &TopMenu, 0); // little space
	TopMenu.HSplitBottom(40.0f, &TopMenu, &Button);
	static int s_NewsButton;
	if(DoButton_Menu(&s_NewsButton, Localize("News"), 0, &Button, g_Config.m_ClShowStartMenuImages ? "news" : 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || CheckHotKey(KEY_N))
		NewPage = PAGE_NEWS;

	BottomMenu.HSplitTop(90.0f, 0, &BottomMenu);

	BottomMenu.HSplitTop(40.0f, &Button, &TopMenu);
	static int s_QuitButton;
	if(DoButton_Menu(&s_QuitButton, Localize("Quit"), 0, &Button, 0, CUI::CORNER_ALL, Rounding, 0.5f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true) || m_EscapePressed || CheckHotKey(KEY_Q))
		m_Popup = POPUP_QUIT;

	// render version
	CUIRect Version;
	MainView.HSplitBottom(30.0f, 0, &Version);
	MainView.HSplitBottom(20.0f, 0, &Version);
	Version.VSplitLeft(VMargin, 0, &Version);
	Version.VSplitRight(50.0f, &Version, 0);
	char aBuf[64];

#if defined(CONF_AUTOUPDATE)
	CUIRect Part;
	int State = Updater()->GetCurrentState();
	bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");
	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::CLEAN)
	{
		aBuf[0] = '\0';
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		char aCurrentFile[64];
		Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
		str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
	}
	else if(State == IUpdater::FAIL)
	{
		str_format(aBuf, sizeof(aBuf), Localize("Update failed! Check log..."));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet Client updated!"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	UI()->DoLabel(&Version, aBuf, 14.0f, -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	Version.VSplitLeft(TextRender()->TextWidth(0, 14.0f, aBuf, -1, -1.0f) + 10.0f, 0, &Part);

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		CUIRect Update;
		Part.VSplitLeft(100.0f, &Update, NULL);

		static int s_VersionUpdate = 0;
		if(DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &Update, 0, CUI::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true))
		{
			Updater()->InitiateUpdate();
		}
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		CUIRect Restart;
		Part.VSplitLeft(50.0f, &Restart, &Part);

		static int s_VersionUpdate = 0;
		if(DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &Restart, 0, CUI::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f), true))
		{
			Client()->Restart();
		}
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		CUIRect ProgressBar, Percent;
		Part.VSplitLeft(100.0f, &ProgressBar, &Percent);
		ProgressBar.y += 2.0f;
		ProgressBar.HMargin(1.0f, &ProgressBar);
		RenderTools()->DrawUIRect(&ProgressBar, vec4(1.0f, 1.0f, 1.0f, 0.25f), CUI::CORNER_ALL, 5.0f);
		ProgressBar.w = clamp((float)Updater()->GetCurrentPercent(), 10.0f, 100.0f);
		RenderTools()->DrawUIRect(&ProgressBar, vec4(1.0f, 1.0f, 1.0f, 0.5f), CUI::CORNER_ALL, 5.0f);
	}
#else
	if(str_comp(Client()->LatestVersion(), "0") != 0)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		UI()->DoLabel(&Version, aBuf, 14.0f, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
#endif

	UI()->DoLabel(&Version, GAME_RELEASE_VERSION, 14.0f, 1);

	if(NewPage != -1)
	{
		m_MenuPage = NewPage;
		m_ShowStart = false;
	}
}
