/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <engine/shared/config.h>
#include <engine/sound.h>

#include <game/localization.h>

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
