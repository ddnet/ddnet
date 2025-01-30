/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>
#include <base/system.h>

#include <engine/demo.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/touch_controls.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "menus.h"
#include "motd.h"
#include "voting.h"

#include "ghost.h"
#include <engine/keys.h>
#include <engine/storage.h>

#include <chrono>

using namespace FontIcons;
using namespace std::chrono_literals;

void CMenus::RenderGame(CUIRect MainView)
{
	CUIRect Button, ButtonBars, ButtonBar, ButtonBar2;
	bool ShowDDRaceButtons = MainView.w > 855.0f;
	MainView.HSplitTop(45.0f + (g_Config.m_ClTouchControls ? 35.0f : 0.0f), &ButtonBars, &MainView);
	ButtonBars.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	ButtonBars.Margin(10.0f, &ButtonBars);
	ButtonBars.HSplitTop(25.0f, &ButtonBar, &ButtonBars);
	if(g_Config.m_ClTouchControls)
	{
		ButtonBars.HSplitTop(10.0f, nullptr, &ButtonBars);
		ButtonBars.HSplitTop(25.0f, &ButtonBar2, &ButtonBars);
	}

	ButtonBar.VSplitRight(120.0f, &ButtonBar, &Button);
	static CButtonContainer s_DisconnectButton;
	if(DoButton_Menu(&s_DisconnectButton, Localize("Disconnect"), 0, &Button))
	{
		if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
		{
			PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnect);
		}
		else
		{
			Client()->Disconnect();
			RefreshBrowserTab(true);
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(170.0f, &ButtonBar, &Button);

	static CButtonContainer s_DummyButton;
	if(!Client()->DummyAllowed())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connect Dummy"), 1, &Button);
		GameClient()->m_Tooltips.DoToolTip(&s_DummyButton, &Button, Localize("Dummy is not allowed on this server"));
	}
	else if(Client()->DummyConnectingDelayed())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connect Dummy"), 1, &Button);
		GameClient()->m_Tooltips.DoToolTip(&s_DummyButton, &Button, Localize("Please wait…"));
	}
	else if(Client()->DummyConnecting())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connecting dummy"), 1, &Button);
	}
	else if(DoButton_Menu(&s_DummyButton, Client()->DummyConnected() ? Localize("Disconnect Dummy") : Localize("Connect Dummy"), 0, &Button))
	{
		if(!Client()->DummyConnected())
		{
			Client()->DummyConnect();
		}
		else
		{
			if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				PopupConfirm(Localize("Disconnect Dummy"), Localize("Are you sure that you want to disconnect your dummy?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnectDummy);
			}
			else
			{
				Client()->DummyDisconnect(nullptr);
				SetActive(false);
			}
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(140.0f, &ButtonBar, &Button);
	static CButtonContainer s_DemoButton;
	const bool Recording = DemoRecorder(RECORDER_MANUAL)->IsRecording();
	if(DoButton_Menu(&s_DemoButton, Recording ? Localize("Stop record") : Localize("Record demo"), 0, &Button))
	{
		if(!Recording)
			Client()->DemoRecorder_Start(Client()->GetCurrentMap(), true, RECORDER_MANUAL);
		else
			Client()->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
	}

	bool Paused = false;
	bool Spec = false;
	if(m_pClient->m_Snap.m_LocalClientId >= 0)
	{
		Paused = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_Paused;
		Spec = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_Spec;
	}

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pGameInfoObj && !Paused && !Spec)
	{
		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
			static CButtonContainer s_SpectateButton;
			if(!Client()->DummyConnecting() && DoButton_Menu(&s_SpectateButton, Localize("Spectate"), 0, &Button))
			{
				if(g_Config.m_ClDummy == 0 || Client()->DummyConnected())
				{
					m_pClient->SendSwitchTeam(TEAM_SPECTATORS);
					SetActive(false);
				}
			}
		}

		if(m_pClient->IsTeamPlay())
		{
			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_RED)
			{
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinRedButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinRedButton, Localize("Join red"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_RED);
					SetActive(false);
				}
			}

			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_BLUE)
			{
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinBlueButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinBlueButton, Localize("Join blue"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_BLUE);
					SetActive(false);
				}
			}
		}
		else
		{
			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_RED)
			{
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);
				static CButtonContainer s_JoinGameButton;
				if(!Client()->DummyConnecting() && DoButton_Menu(&s_JoinGameButton, Localize("Join game"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_RED);
					SetActive(false);
				}
			}
		}

		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && (ShowDDRaceButtons || !m_pClient->IsTeamPlay()))
		{
			ButtonBar.VSplitLeft(65.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

			static CButtonContainer s_KillButton;
			if(DoButton_Menu(&s_KillButton, Localize("Kill"), 0, &Button))
			{
				m_pClient->SendKill(-1);
				SetActive(false);
			}
		}
	}

	if(m_pClient->m_ReceivedDDNetPlayer && m_pClient->m_Snap.m_pLocalInfo && (ShowDDRaceButtons || !m_pClient->IsTeamPlay()))
	{
		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS || Paused || Spec)
		{
			ButtonBar.VSplitLeft((!Paused && !Spec) ? 65.0f : 120.0f, &Button, &ButtonBar);
			ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

			static CButtonContainer s_PauseButton;
			if(DoButton_Menu(&s_PauseButton, (!Paused && !Spec) ? Localize("Pause") : Localize("Join game"), 0, &Button))
			{
				Console()->ExecuteLine("say /pause");
				SetActive(false);
			}
		}
	}

	if(m_pClient->m_Snap.m_pLocalInfo && (m_pClient->m_Snap.m_pLocalInfo->m_Team == TEAM_SPECTATORS || Paused || Spec))
	{
		ButtonBar.VSplitLeft(32.0f, &Button, &ButtonBar);
		ButtonBar.VSplitLeft(5.0f, nullptr, &ButtonBar);

		static CButtonContainer s_AutoCameraButton;

		bool Active = m_pClient->m_Camera.m_AutoSpecCamera && m_pClient->m_Camera.SpectatingPlayer() && m_pClient->m_Camera.CanUseAutoSpecCamera();
		bool Enabled = g_Config.m_ClSpecAutoSync;
		if(DoButton_FontIcon(&s_AutoCameraButton, FONT_ICON_CAMERA, !Active, &Button, IGraphics::CORNER_ALL, Enabled))
		{
			m_pClient->m_Camera.ToggleAutoSpecCamera();
		}
		m_pClient->m_Camera.UpdateAutoSpecCameraTooltip();
		GameClient()->m_Tooltips.DoToolTip(&s_AutoCameraButton, &Button, m_pClient->m_Camera.AutoSpecCameraTooltip());
	}

	if(g_Config.m_ClTouchControls)
	{
		ButtonBar2.VSplitLeft(200.0f, &Button, &ButtonBar2);
		static char s_TouchControlsEditCheckbox;
		if(DoButton_CheckBox(&s_TouchControlsEditCheckbox, Localize("Edit touch controls"), GameClient()->m_TouchControls.IsEditingActive(), &Button))
		{
			GameClient()->m_TouchControls.SetEditingActive(!GameClient()->m_TouchControls.IsEditingActive());
		}

		ButtonBar2.VSplitRight(80.0f, &ButtonBar2, &Button);
		static CButtonContainer s_CloseButton;
		if(DoButton_Menu(&s_CloseButton, Localize("Close"), 0, &Button))
		{
			SetActive(false);
		}

		ButtonBar2.VSplitRight(5.0f, &ButtonBar2, nullptr);
		ButtonBar2.VSplitRight(160.0f, &ButtonBar2, &Button);
		static CButtonContainer s_RemoveConsoleButton;
		if(DoButton_Menu(&s_RemoveConsoleButton, Localize("Remote console"), 0, &Button))
		{
			Console()->ExecuteLine("toggle_remote_console");
		}

		ButtonBar2.VSplitRight(5.0f, &ButtonBar2, nullptr);
		ButtonBar2.VSplitRight(120.0f, &ButtonBar2, &Button);
		static CButtonContainer s_LocalConsoleButton;
		if(DoButton_Menu(&s_LocalConsoleButton, Localize("Console"), 0, &Button))
		{
			Console()->ExecuteLine("toggle_local_console");
		}

		if(GameClient()->m_TouchControls.IsEditingActive())
		{
			CUIRect TouchControlsEditor;
			MainView.VMargin((MainView.w - 505.0f) / 2.0f, &TouchControlsEditor);
			TouchControlsEditor.HMargin((TouchControlsEditor.h - 230.0f) / 2.0f, &TouchControlsEditor);
			RenderTouchControlsEditor(TouchControlsEditor);
		}
	}
}

void CMenus::RenderTouchControlsEditor(CUIRect MainView)
{
	CUIRect Label, Button, Row;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_ALL, 10.0f);
	MainView.Margin(10.0f, &MainView);

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);
	Row.VSplitLeft(Row.h, nullptr, &Row);
	Row.VSplitRight(Row.h, &Row, &Button);
	Row.VMargin(5.0f, &Label);
	Ui()->DoLabel(&Label, Localize("Edit touch controls"), 20.0f, TEXTALIGN_MC);

	static CButtonContainer s_OpenHelpButton;
	if(DoButton_FontIcon(&s_OpenHelpButton, FONT_ICON_QUESTION, 0, &Button))
	{
		Client()->ViewLink(Localize("https://wiki.ddnet.org/wiki/Touch_controls"));
	}

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);

	Row.VSplitLeft(240.0f, &Button, &Row);
	static CButtonContainer s_SaveConfigurationButton;
	if(DoButton_Menu(&s_SaveConfigurationButton, Localize("Save changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
	{
		if(GameClient()->m_TouchControls.SaveConfigurationToFile())
		{
			GameClient()->m_TouchControls.SetEditingChanges(false);
		}
		else
		{
			SWarning Warning(Localize("Error saving touch controls"), Localize("Could not save touch controls to file. See local console for details."));
			Warning.m_AutoHide = false;
			Client()->AddWarning(Warning);
		}
	}

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(240.0f, &Button, &Row);
	if(GameClient()->m_TouchControls.HasEditingChanges())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		Ui()->DoLabel(&Button, Localize("Unsaved changes"), 14.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);

	Row.VSplitLeft(240.0f, &Button, &Row);
	static CButtonContainer s_DiscardChangesButton;
	if(DoButton_Menu(&s_DiscardChangesButton, Localize("Discard changes"), GameClient()->m_TouchControls.HasEditingChanges() ? 0 : 1, &Button))
	{
		PopupConfirm(Localize("Discard changes"),
			Localize("Are you sure that you want to discard the current changes to the touch controls?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmDiscardTouchControlsChanges);
	}

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(240.0f, &Button, &Row);
	static CButtonContainer s_ResetButton;
	if(DoButton_Menu(&s_ResetButton, Localize("Reset to defaults"), 0, &Button))
	{
		PopupConfirm(Localize("Reset to defaults"),
			Localize("Are you sure that you want to reset the touch controls to default?"),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmResetTouchControls);
	}

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	Row.VSplitLeft(240.0f, &Button, &Row);
	static CButtonContainer s_ClipboardImportButton;
	if(DoButton_Menu(&s_ClipboardImportButton, Localize("Import from clipboard"), 0, &Button))
	{
		PopupConfirm(Localize("Import from clipboard"),
			Localize("Are you sure that you want to import the touch controls from the clipboard? This will overwrite your current touch controls."),
			Localize("Yes"), Localize("No"),
			&CMenus::PopupConfirmImportTouchControlsClipboard);
	}

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(240.0f, &Button, &Row);
	static CButtonContainer s_ClipboardExportButton;
	if(DoButton_Menu(&s_ClipboardExportButton, Localize("Export to clipboard"), 0, &Button))
	{
		GameClient()->m_TouchControls.SaveConfigurationToClipboard();
	}

	MainView.HSplitTop(25.0f, &Label, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);
	Ui()->DoLabel(&Label, Localize("Settings"), 20.0f, TEXTALIGN_MC);

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);

	Row.VSplitLeft(300.0f, &Label, &Row);
	Ui()->DoLabel(&Label, Localize("Direct touch input while ingame"), 16.0f, TEXTALIGN_ML);

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(180.0f, &Button, &Row);
	const char *apIngameTouchModes[(int)CTouchControls::EDirectTouchIngameMode::NUM_STATES] = {Localize("Disabled", "Direct touch input"), Localize("Active action", "Direct touch input"), Localize("Aim", "Direct touch input"), Localize("Fire", "Direct touch input"), Localize("Hook", "Direct touch input")};
	const CTouchControls::EDirectTouchIngameMode OldDirectTouchIngame = GameClient()->m_TouchControls.DirectTouchIngame();
	static CUi::SDropDownState s_DirectTouchIngameDropDownState;
	static CScrollRegion s_DirectTouchIngameDropDownScrollRegion;
	s_DirectTouchIngameDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DirectTouchIngameDropDownScrollRegion;
	const CTouchControls::EDirectTouchIngameMode NewDirectTouchIngame = (CTouchControls::EDirectTouchIngameMode)Ui()->DoDropDown(&Button, (int)OldDirectTouchIngame, apIngameTouchModes, std::size(apIngameTouchModes), s_DirectTouchIngameDropDownState);
	if(OldDirectTouchIngame != NewDirectTouchIngame)
	{
		GameClient()->m_TouchControls.SetDirectTouchIngame(NewDirectTouchIngame);
	}

	MainView.HSplitTop(25.0f, &Row, &MainView);
	MainView.HSplitTop(5.0f, nullptr, &MainView);

	Row.VSplitLeft(300.0f, &Label, &Row);
	Ui()->DoLabel(&Label, Localize("Direct touch input while spectating"), 16.0f, TEXTALIGN_ML);

	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(180.0f, &Button, &Row);
	const char *apSpectateTouchModes[(int)CTouchControls::EDirectTouchSpectateMode::NUM_STATES] = {Localize("Disabled", "Direct touch input"), Localize("Aim", "Direct touch input")};
	const CTouchControls::EDirectTouchSpectateMode OldDirectTouchSpectate = GameClient()->m_TouchControls.DirectTouchSpectate();
	static CUi::SDropDownState s_DirectTouchSpectateDropDownState;
	static CScrollRegion s_DirectTouchSpectateDropDownScrollRegion;
	s_DirectTouchSpectateDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DirectTouchSpectateDropDownScrollRegion;
	const CTouchControls::EDirectTouchSpectateMode NewDirectTouchSpectate = (CTouchControls::EDirectTouchSpectateMode)Ui()->DoDropDown(&Button, (int)OldDirectTouchSpectate, apSpectateTouchModes, std::size(apSpectateTouchModes), s_DirectTouchSpectateDropDownState);
	if(OldDirectTouchSpectate != NewDirectTouchSpectate)
	{
		GameClient()->m_TouchControls.SetDirectTouchSpectate(NewDirectTouchSpectate);
	}
}

void CMenus::PopupConfirmDisconnect()
{
	Client()->Disconnect();
}

void CMenus::PopupConfirmDisconnectDummy()
{
	Client()->DummyDisconnect(nullptr);
	SetActive(false);
}

void CMenus::PopupConfirmDiscardTouchControlsChanges()
{
	if(GameClient()->m_TouchControls.LoadConfigurationFromFile(IStorage::TYPE_ALL))
	{
		GameClient()->m_TouchControls.SetEditingChanges(false);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load touch controls from file. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::PopupConfirmResetTouchControls()
{
	bool Success = false;
	for(int StorageType = IStorage::TYPE_SAVE + 1; StorageType < Storage()->NumPaths(); ++StorageType)
	{
		if(GameClient()->m_TouchControls.LoadConfigurationFromFile(StorageType))
		{
			Success = true;
			break;
		}
	}
	if(Success)
	{
		GameClient()->m_TouchControls.SetEditingChanges(true);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load default touch controls from file. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::PopupConfirmImportTouchControlsClipboard()
{
	if(GameClient()->m_TouchControls.LoadConfigurationFromClipboard())
	{
		GameClient()->m_TouchControls.SetEditingChanges(true);
	}
	else
	{
		SWarning Warning(Localize("Error loading touch controls"), Localize("Could not load touch controls from clipboard. See local console for details."));
		Warning.m_AutoHide = false;
		Client()->AddWarning(Warning);
	}
}

void CMenus::RenderPlayers(CUIRect MainView)
{
	CUIRect Button, Button2, ButtonBar, PlayerList, Player;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	// list background color
	MainView.Margin(10.0f, &PlayerList);
	PlayerList.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	PlayerList.Margin(10.0f, &PlayerList);

	// headline
	PlayerList.HSplitTop(34.0f, &ButtonBar, &PlayerList);
	ButtonBar.VSplitRight(231.0f, &Player, &ButtonBar);
	Ui()->DoLabel(&Player, Localize("Player"), 24.0f, TEXTALIGN_ML);

	ButtonBar.HMargin(1.0f, &ButtonBar);
	float Width = ButtonBar.h * 2.0f;
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_EMOTICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_FRIEND, &Button);

	int TotalPlayers = 0;
	for(const auto &pInfoByName : m_pClient->m_Snap.m_apInfoByName)
	{
		if(!pInfoByName)
			continue;

		int Index = pInfoByName->m_ClientId;

		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		TotalPlayers++;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(24.0f, TotalPlayers, 1, 3, -1, &PlayerList);

	// options
	static char s_aPlayerIds[MAX_CLIENTS][4] = {{0}};

	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId)
			continue;

		CGameClient::CClientData &CurrentClient = m_pClient->m_aClients[Index];
		const CListboxItem Item = s_ListBox.DoNextItem(&CurrentClient);

		Count++;

		if(!Item.m_Visible)
			continue;

		CUIRect Row = Item.m_Rect;
		if(Count % 2 == 1)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
		Row.VSplitRight(s_ListBox.ScrollbarWidthMax() - s_ListBox.ScrollbarWidth(), &Row, nullptr);
		Row.VSplitRight(300.0f, &Player, &Row);

		// player info
		Player.VSplitLeft(28.0f, &Button, &Player);

		CTeeRenderInfo TeeInfo = CurrentClient.m_RenderInfo;
		TeeInfo.m_Size = Button.h;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos(Button.x + Button.h / 2, Button.y + Button.h / 2 + OffsetToMid.y);
		RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);
		Ui()->DoButtonLogic(&s_aPlayerIds[Index][3], 0, &Button);
		GameClient()->m_Tooltips.DoToolTip(&s_aPlayerIds[Index][3], &Button, CurrentClient.m_aSkinName);

		Player.HSplitTop(1.5f, nullptr, &Player);
		Player.VSplitMid(&Player, &Button);
		Row.VSplitRight(210.0f, &Button2, &Row);

		Ui()->DoLabel(&Player, CurrentClient.m_aName, 14.0f, TEXTALIGN_ML);
		Ui()->DoLabel(&Button, CurrentClient.m_aClan, 14.0f, TEXTALIGN_ML);

		m_pClient->m_CountryFlags.Render(CurrentClient.m_Country, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f),
			Button2.x, Button2.y + Button2.h / 2.0f - 0.75f * Button2.h / 2.0f, 1.5f * Button2.h, 0.75f * Button2.h);

		// ignore chat button
		Row.HMargin(2.0f, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(g_Config.m_ClShowChatFriends && !CurrentClient.m_Friend)
			DoButton_Toggle(&s_aPlayerIds[Index][0], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIds[Index][0], CurrentClient.m_ChatIgnore, &Button, true))
			CurrentClient.m_ChatIgnore ^= 1;

		// ignore emoticon button
		Row.VSplitLeft(30.0f, nullptr, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(g_Config.m_ClShowChatFriends && !CurrentClient.m_Friend)
			DoButton_Toggle(&s_aPlayerIds[Index][1], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIds[Index][1], CurrentClient.m_EmoticonIgnore, &Button, true))
			CurrentClient.m_EmoticonIgnore ^= 1;

		// friend button
		Row.VSplitLeft(10.0f, nullptr, &Row);
		Row.VSplitLeft(Width, &Button, &Row);
		Button.VSplitLeft((Width - Button.h) / 4.0f, nullptr, &Button);
		Button.VSplitLeft(Button.h, &Button, nullptr);
		if(DoButton_Toggle(&s_aPlayerIds[Index][2], CurrentClient.m_Friend, &Button, true))
		{
			if(CurrentClient.m_Friend)
				m_pClient->Friends()->RemoveFriend(CurrentClient.m_aName, CurrentClient.m_aClan);
			else
				m_pClient->Friends()->AddFriend(CurrentClient.m_aName, CurrentClient.m_aClan);

			m_pClient->Client()->ServerBrowserUpdate();
		}
	}

	s_ListBox.DoEnd();
}

void CMenus::RenderServerInfo(CUIRect MainView)
{
	const float FontSizeTitle = 32.0f;
	const float FontSizeBody = 20.0f;

	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);

	CUIRect ServerInfo, GameInfo, Motd;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitMid(&ServerInfo, &Motd, 10.0f);
	ServerInfo.VSplitMid(&ServerInfo, &GameInfo, 10.0f);

	ServerInfo.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	ServerInfo.Margin(10.0f, &ServerInfo);

	CUIRect Label;
	ServerInfo.HSplitTop(FontSizeTitle, &Label, &ServerInfo);
	ServerInfo.HSplitTop(5.0f, nullptr, &ServerInfo);
	Ui()->DoLabel(&Label, Localize("Server info"), FontSizeTitle, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	ServerInfo.HSplitTop(FontSizeBody, nullptr, &ServerInfo);
	Ui()->DoLabel(&Label, CurrentServerInfo.m_aName, FontSizeBody, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Address"), CurrentServerInfo.m_aAddress);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	if(m_pClient->m_Snap.m_pLocalInfo)
	{
		ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Ping"), m_pClient->m_Snap.m_pLocalInfo->m_Latency);
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
	}

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Version"), CurrentServerInfo.m_aVersion);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Password"), CurrentServerInfo.m_Flags & SERVER_FLAG_PASSWORD ? Localize("Yes") : Localize("No"));
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	const CCommunity *pCommunity = ServerBrowser()->Community(CurrentServerInfo.m_aCommunityId);
	if(pCommunity != nullptr)
	{
		ServerInfo.HSplitTop(FontSizeBody, &Label, &ServerInfo);
		str_format(aBuf, sizeof(aBuf), "%s:", Localize("Community"));
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

		const SCommunityIcon *pIcon = FindCommunityIcon(pCommunity->Id());
		if(pIcon != nullptr)
		{
			Label.VSplitLeft(TextRender()->TextWidth(FontSizeBody, aBuf) + 8.0f, nullptr, &Label);
			Label.VSplitLeft(2.0f * Label.h, &Label, nullptr);
			RenderCommunityIcon(pIcon, Label, true);
			static char s_CommunityTooltipButtonId;
			Ui()->DoButtonLogic(&s_CommunityTooltipButtonId, 0, &Label);
			GameClient()->m_Tooltips.DoToolTip(&s_CommunityTooltipButtonId, &Label, pCommunity->Name());
		}
	}

	// copy info button
	{
		CUIRect Button;
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		Button.VSplitRight(200.0f, &ServerInfo, &Button);
		static CButtonContainer s_CopyButton;
		if(DoButton_Menu(&s_CopyButton, Localize("Copy info"), 0, &Button))
		{
			char aInfo[256];
			str_format(
				aInfo,
				sizeof(aInfo),
				"%s\n"
				"Address: ddnet://%s\n"
				"My IGN: %s\n",
				CurrentServerInfo.m_aName,
				CurrentServerInfo.m_aAddress,
				Client()->PlayerName());
			Input()->SetClipboardText(aInfo);
		}
	}

	// favorite checkbox
	{
		CUIRect Button;
		TRISTATE IsFavorite = Favorites()->IsFavorite(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		static int s_AddFavButton = 0;
		if(DoButton_CheckBox(&s_AddFavButton, Localize("Favorite"), IsFavorite != TRISTATE::NONE, &Button))
		{
			if(IsFavorite != TRISTATE::NONE)
				Favorites()->Remove(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
			else
				Favorites()->Add(CurrentServerInfo.m_aAddresses, CurrentServerInfo.m_NumAddresses);
		}
	}

	GameInfo.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	GameInfo.Margin(10.0f, &GameInfo);

	GameInfo.HSplitTop(FontSizeTitle, &Label, &GameInfo);
	GameInfo.HSplitTop(5.0f, nullptr, &GameInfo);
	Ui()->DoLabel(&Label, Localize("Game info"), FontSizeTitle, TEXTALIGN_ML);

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Game type"), CurrentServerInfo.m_aGameType);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Map"), CurrentServerInfo.m_aMap);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	const auto *pGameInfoObj = m_pClient->m_Snap.m_pGameInfoObj;
	if(pGameInfoObj)
	{
		if(pGameInfoObj->m_ScoreLimit)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Score limit"), pGameInfoObj->m_ScoreLimit);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}

		if(pGameInfoObj->m_TimeLimit)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), Localize("Time limit: %d min"), pGameInfoObj->m_TimeLimit);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}

		if(pGameInfoObj->m_RoundCurrent && pGameInfoObj->m_RoundNum)
		{
			GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
			str_format(aBuf, sizeof(aBuf), Localize("Round %d/%d"), pGameInfoObj->m_RoundCurrent, pGameInfoObj->m_RoundNum);
			Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
		}
	}

	if(m_pClient->m_GameInfo.m_DDRaceTeam)
	{
		const char *pTeamMode = nullptr;
		switch(Config()->m_SvTeam)
		{
		case SV_TEAM_FORBIDDEN:
			pTeamMode = Localize("forbidden", "Teaming status");
			break;
		case SV_TEAM_ALLOWED:
			if(g_Config.m_SvSoloServer)
				pTeamMode = Localize("solo", "Teaming status");
			else
				pTeamMode = Localize("allowed", "Teaming status");
			break;
		case SV_TEAM_MANDATORY:
			pTeamMode = Localize("required", "Teaming status");
			break;
		case SV_TEAM_FORCED_SOLO:
			pTeamMode = Localize("solo", "Teaming status");
			break;
		default:
			dbg_assert(false, "unknown team mode");
		}
		if((Config()->m_SvTeam == SV_TEAM_ALLOWED || Config()->m_SvTeam == SV_TEAM_MANDATORY) && (Config()->m_SvMinTeamSize != CConfig::ms_SvMinTeamSize || Config()->m_SvMaxTeamSize != CConfig::ms_SvMaxTeamSize))
		{
			if(Config()->m_SvMinTeamSize != CConfig::ms_SvMinTeamSize && Config()->m_SvMaxTeamSize != CConfig::ms_SvMaxTeamSize)
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d, %s %d)", Localize("Teams"), pTeamMode, Localize("minimum", "Team size"), Config()->m_SvMinTeamSize, Localize("maximum", "Team size"), Config()->m_SvMaxTeamSize);
			else if(Config()->m_SvMinTeamSize != CConfig::ms_SvMinTeamSize)
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d)", Localize("Teams"), pTeamMode, Localize("minimum", "Team size"), Config()->m_SvMinTeamSize);
			else
				str_format(aBuf, sizeof(aBuf), "%s: %s (%s %d)", Localize("Teams"), pTeamMode, Localize("maximum", "Team size"), Config()->m_SvMaxTeamSize);
		}
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Teams"), pTeamMode);
		GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
		Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);
	}

	GameInfo.HSplitTop(FontSizeBody, &Label, &GameInfo);
	str_format(aBuf, sizeof(aBuf), "%s: %d/%d", Localize("Players"), m_pClient->m_Snap.m_NumPlayers, CurrentServerInfo.m_MaxClients);
	Ui()->DoLabel(&Label, aBuf, FontSizeBody, TEXTALIGN_ML);

	RenderServerInfoMotd(Motd);
}

void CMenus::RenderServerInfoMotd(CUIRect Motd)
{
	const float MotdFontSize = 16.0f;
	Motd.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Motd.Margin(10.0f, &Motd);

	CUIRect MotdHeader;
	Motd.HSplitTop(2.0f * MotdFontSize, &MotdHeader, &Motd);
	Motd.HSplitTop(5.0f, nullptr, &Motd);
	Ui()->DoLabel(&MotdHeader, Localize("MOTD"), 2.0f * MotdFontSize, TEXTALIGN_ML);

	if(!m_pClient->m_Motd.ServerMotd()[0])
		return;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 5 * MotdFontSize;
	s_ScrollRegion.Begin(&Motd, &ScrollOffset, &ScrollParams);
	Motd.y += ScrollOffset.y;

	static float s_MotdHeight = 0.0f;
	static int64_t s_MotdLastUpdateTime = -1;
	if(!m_MotdTextContainerIndex.Valid() || s_MotdLastUpdateTime == -1 || s_MotdLastUpdateTime != m_pClient->m_Motd.ServerMotdUpdateTime())
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0.0f, 0.0f, MotdFontSize, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = Motd.w;
		TextRender()->RecreateTextContainer(m_MotdTextContainerIndex, &Cursor, m_pClient->m_Motd.ServerMotd());
		s_MotdHeight = Cursor.Height();
		s_MotdLastUpdateTime = m_pClient->m_Motd.ServerMotdUpdateTime();
	}

	CUIRect MotdTextArea;
	Motd.HSplitTop(s_MotdHeight, &MotdTextArea, &Motd);
	s_ScrollRegion.AddRect(MotdTextArea);

	if(m_MotdTextContainerIndex.Valid())
		TextRender()->RenderTextContainer(m_MotdTextContainerIndex, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), MotdTextArea.x, MotdTextArea.y);

	s_ScrollRegion.End();
}

bool CMenus::RenderServerControlServer(CUIRect MainView, bool UpdateScroll)
{
	CUIRect List = MainView;
	int NumVoteOptions = 0;
	int aIndices[MAX_VOTE_OPTIONS];
	int Selected = -1;
	int TotalShown = 0;

	int i = 0;
	for(CVoteOptionClient *pOption = m_pClient->m_Voting.m_pFirst; pOption; pOption = pOption->m_pNext, i++)
	{
		if(!m_FilterInput.IsEmpty() && !str_utf8_find_nocase(pOption->m_aDescription, m_FilterInput.GetString()))
			continue;
		if(i == m_CallvoteSelectedOption)
			Selected = TotalShown;
		TotalShown++;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(19.0f, TotalShown, 1, 3, Selected, &List);

	i = 0;
	for(CVoteOptionClient *pOption = m_pClient->m_Voting.m_pFirst; pOption; pOption = pOption->m_pNext, i++)
	{
		if(!m_FilterInput.IsEmpty() && !str_utf8_find_nocase(pOption->m_aDescription, m_FilterInput.GetString()))
			continue;
		aIndices[NumVoteOptions] = i;
		NumVoteOptions++;

		const CListboxItem Item = s_ListBox.DoNextItem(pOption);
		if(!Item.m_Visible)
			continue;

		CUIRect Label;
		Item.m_Rect.VMargin(2.0f, &Label);
		Ui()->DoLabel(&Label, pOption->m_aDescription, 13.0f, TEXTALIGN_ML);
	}

	Selected = s_ListBox.DoEnd();
	if(UpdateScroll)
		s_ListBox.ScrollToSelected();
	m_CallvoteSelectedOption = Selected != -1 ? aIndices[Selected] : -1;
	return s_ListBox.WasItemActivated();
}

bool CMenus::RenderServerControlKick(CUIRect MainView, bool FilterSpectators, bool UpdateScroll)
{
	int NumOptions = 0;
	int Selected = -1;
	int aPlayerIds[MAX_CLIENTS];
	for(const auto &pInfoByName : m_pClient->m_Snap.m_apInfoByName)
	{
		if(!pInfoByName)
			continue;

		int Index = pInfoByName->m_ClientId;
		if(Index == m_pClient->m_Snap.m_LocalClientId || (FilterSpectators && pInfoByName->m_Team == TEAM_SPECTATORS))
			continue;

		if(!str_utf8_find_nocase(m_pClient->m_aClients[Index].m_aName, m_FilterInput.GetString()))
			continue;

		if(m_CallvoteSelectedPlayer == Index)
			Selected = NumOptions;
		aPlayerIds[NumOptions] = Index;
		NumOptions++;
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(24.0f, NumOptions, 1, 3, Selected, &MainView);

	for(int i = 0; i < NumOptions; i++)
	{
		const CListboxItem Item = s_ListBox.DoNextItem(&aPlayerIds[i]);
		if(!Item.m_Visible)
			continue;

		CUIRect TeeRect, Label;
		Item.m_Rect.VSplitLeft(Item.m_Rect.h, &TeeRect, &Label);

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[aPlayerIds[i]].m_RenderInfo;
		TeeInfo.m_Size = TeeRect.h;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos(TeeRect.x + TeeInfo.m_Size / 2, TeeRect.y + TeeInfo.m_Size / 2 + OffsetToMid.y);

		RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);

		Ui()->DoLabel(&Label, m_pClient->m_aClients[aPlayerIds[i]].m_aName, 16.0f, TEXTALIGN_ML);
	}

	Selected = s_ListBox.DoEnd();
	if(UpdateScroll)
		s_ListBox.ScrollToSelected();
	m_CallvoteSelectedPlayer = Selected != -1 ? aPlayerIds[Selected] : -1;
	return s_ListBox.WasItemActivated();
}

void CMenus::RenderServerControl(CUIRect MainView)
{
	enum class EServerControlTab
	{
		SETTINGS,
		KICKVOTE,
		SPECVOTE,
	};
	static EServerControlTab s_ControlPage = EServerControlTab::SETTINGS;

	// render background
	CUIRect Bottom, RconExtension, TabBar, Button;
	MainView.HSplitTop(20.0f, &Bottom, &MainView);
	Bottom.Draw(ms_ColorTabbarActive, IGraphics::CORNER_NONE, 0.0f);
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

	if(Client()->RconAuthed())
		MainView.HSplitBottom(90.0f, &MainView, &RconExtension);

	// tab bar
	TabBar.VSplitLeft(TabBar.w / 3, &Button, &TabBar);
	static CButtonContainer s_Button0;
	if(DoButton_MenuTab(&s_Button0, Localize("Change settings"), s_ControlPage == EServerControlTab::SETTINGS, &Button, IGraphics::CORNER_NONE))
		s_ControlPage = EServerControlTab::SETTINGS;

	TabBar.VSplitMid(&Button, &TabBar);
	static CButtonContainer s_Button1;
	if(DoButton_MenuTab(&s_Button1, Localize("Kick player"), s_ControlPage == EServerControlTab::KICKVOTE, &Button, IGraphics::CORNER_NONE))
		s_ControlPage = EServerControlTab::KICKVOTE;

	static CButtonContainer s_Button2;
	if(DoButton_MenuTab(&s_Button2, Localize("Move player to spectators"), s_ControlPage == EServerControlTab::SPECVOTE, &TabBar, IGraphics::CORNER_NONE))
		s_ControlPage = EServerControlTab::SPECVOTE;

	// render page
	MainView.HSplitBottom(ms_ButtonHeight + 5 * 2, &MainView, &Bottom);
	Bottom.HMargin(5.0f, &Bottom);
	Bottom.HSplitTop(5.0f, nullptr, &Bottom);

	// render quick search
	CUIRect QuickSearch;
	Bottom.VSplitLeft(5.0f, nullptr, &Bottom);
	Bottom.VSplitLeft(250.0f, &QuickSearch, &Bottom);
	if(m_ControlPageOpening)
	{
		m_ControlPageOpening = false;
		Ui()->SetActiveItem(&m_FilterInput);
		m_FilterInput.SelectAll();
	}
	bool Searching = Ui()->DoEditBox_Search(&m_FilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !m_pClient->m_GameConsole.IsActive());

	// vote menu
	bool Call = false;
	if(s_ControlPage == EServerControlTab::SETTINGS)
		Call = RenderServerControlServer(MainView, Searching);
	else if(s_ControlPage == EServerControlTab::KICKVOTE)
		Call = RenderServerControlKick(MainView, false, Searching);
	else if(s_ControlPage == EServerControlTab::SPECVOTE)
		Call = RenderServerControlKick(MainView, true, Searching);

	// call vote
	Bottom.VSplitRight(10.0f, &Bottom, nullptr);
	Bottom.VSplitRight(120.0f, &Bottom, &Button);

	static CButtonContainer s_CallVoteButton;
	if(DoButton_Menu(&s_CallVoteButton, Localize("Call vote"), 0, &Button) || Call)
	{
		if(s_ControlPage == EServerControlTab::SETTINGS)
		{
			if(0 <= m_CallvoteSelectedOption && m_CallvoteSelectedOption < m_pClient->m_Voting.m_NumVoteOptions)
			{
				m_pClient->m_Voting.CallvoteOption(m_CallvoteSelectedOption, m_CallvoteReasonInput.GetString());
				if(g_Config.m_UiCloseWindowAfterChangingSetting)
					SetActive(false);
			}
		}
		else if(s_ControlPage == EServerControlTab::KICKVOTE)
		{
			if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
				m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
			{
				m_pClient->m_Voting.CallvoteKick(m_CallvoteSelectedPlayer, m_CallvoteReasonInput.GetString());
				SetActive(false);
			}
		}
		else if(s_ControlPage == EServerControlTab::SPECVOTE)
		{
			if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
				m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
			{
				m_pClient->m_Voting.CallvoteSpectate(m_CallvoteSelectedPlayer, m_CallvoteReasonInput.GetString());
				SetActive(false);
			}
		}
		m_CallvoteReasonInput.Clear();
	}

	// render kick reason
	CUIRect Reason;
	Bottom.VSplitRight(20.0f, &Bottom, nullptr);
	Bottom.VSplitRight(200.0f, &Bottom, &Reason);
	const char *pLabel = Localize("Reason:");
	Ui()->DoLabel(&Reason, pLabel, 14.0f, TEXTALIGN_ML);
	float w = TextRender()->TextWidth(14.0f, pLabel, -1, -1.0f);
	Reason.VSplitLeft(w + 10.0f, nullptr, &Reason);
	if(Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed())
	{
		Ui()->SetActiveItem(&m_CallvoteReasonInput);
		m_CallvoteReasonInput.SelectAll();
	}
	Ui()->DoEditBox(&m_CallvoteReasonInput, &Reason, 14.0f);

	// vote option loading indicator
	if(s_ControlPage == EServerControlTab::SETTINGS && m_pClient->m_Voting.IsReceivingOptions())
	{
		CUIRect Spinner, LoadingLabel;
		Bottom.VSplitLeft(20.0f, nullptr, &Bottom);
		Bottom.VSplitLeft(16.0f, &Spinner, &Bottom);
		Bottom.VSplitLeft(5.0f, nullptr, &Bottom);
		Bottom.VSplitRight(10.0f, &LoadingLabel, nullptr);
		Ui()->RenderProgressSpinner(Spinner.Center(), 8.0f);
		Ui()->DoLabel(&LoadingLabel, Localize("Loading…"), 14.0f, TEXTALIGN_ML);
	}

	// extended features (only available when authed in rcon)
	if(Client()->RconAuthed())
	{
		// background
		RconExtension.HSplitTop(10.0f, nullptr, &RconExtension);
		RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
		RconExtension.HSplitTop(5.0f, nullptr, &RconExtension);

		// force vote
		Bottom.VSplitLeft(5.0f, nullptr, &Bottom);
		Bottom.VSplitLeft(120.0f, &Button, &Bottom);

		static CButtonContainer s_ForceVoteButton;
		if(DoButton_Menu(&s_ForceVoteButton, Localize("Force vote"), 0, &Button))
		{
			if(s_ControlPage == EServerControlTab::SETTINGS)
			{
				m_pClient->m_Voting.CallvoteOption(m_CallvoteSelectedOption, m_CallvoteReasonInput.GetString(), true);
			}
			else if(s_ControlPage == EServerControlTab::KICKVOTE)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_Voting.CallvoteKick(m_CallvoteSelectedPlayer, m_CallvoteReasonInput.GetString(), true);
					SetActive(false);
				}
			}
			else if(s_ControlPage == EServerControlTab::SPECVOTE)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_Voting.CallvoteSpectate(m_CallvoteSelectedPlayer, m_CallvoteReasonInput.GetString(), true);
					SetActive(false);
				}
			}
			m_CallvoteReasonInput.Clear();
		}

		if(s_ControlPage == EServerControlTab::SETTINGS)
		{
			// remove vote
			Bottom.VSplitRight(10.0f, &Bottom, nullptr);
			Bottom.VSplitRight(120.0f, nullptr, &Button);
			static CButtonContainer s_RemoveVoteButton;
			if(DoButton_Menu(&s_RemoveVoteButton, Localize("Remove"), 0, &Button))
				m_pClient->m_Voting.RemovevoteOption(m_CallvoteSelectedOption);

			// add vote
			RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
			Bottom.VSplitLeft(5.0f, nullptr, &Bottom);
			Bottom.VSplitLeft(250.0f, &Button, &Bottom);
			Ui()->DoLabel(&Button, Localize("Vote description:"), 14.0f, TEXTALIGN_ML);

			Bottom.VSplitLeft(20.0f, nullptr, &Button);
			Ui()->DoLabel(&Button, Localize("Vote command:"), 14.0f, TEXTALIGN_ML);

			static CLineInputBuffered<VOTE_DESC_LENGTH> s_VoteDescriptionInput;
			static CLineInputBuffered<VOTE_CMD_LENGTH> s_VoteCommandInput;
			RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
			Bottom.VSplitRight(10.0f, &Bottom, nullptr);
			Bottom.VSplitRight(120.0f, &Bottom, &Button);
			static CButtonContainer s_AddVoteButton;
			if(DoButton_Menu(&s_AddVoteButton, Localize("Add"), 0, &Button))
				if(!s_VoteDescriptionInput.IsEmpty() && !s_VoteCommandInput.IsEmpty())
					m_pClient->m_Voting.AddvoteOption(s_VoteDescriptionInput.GetString(), s_VoteCommandInput.GetString());

			Bottom.VSplitLeft(5.0f, nullptr, &Bottom);
			Bottom.VSplitLeft(250.0f, &Button, &Bottom);
			Ui()->DoEditBox(&s_VoteDescriptionInput, &Button, 14.0f);

			Bottom.VMargin(20.0f, &Button);
			Ui()->DoEditBox(&s_VoteCommandInput, &Button, 14.0f);
		}
	}
}

void CMenus::RenderInGameNetwork(CUIRect MainView)
{
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	CUIRect TabBar, Button;
	MainView.HSplitTop(24.0f, &TabBar, &MainView);

	int NewPage = g_Config.m_UiPage;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_InternetButton;
	if(DoButton_MenuTab(&s_InternetButton, FONT_ICON_EARTH_AMERICAS, g_Config.m_UiPage == PAGE_INTERNET, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_INTERNET;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_InternetButton, &Button, Localize("Internet"));

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_LanButton;
	if(DoButton_MenuTab(&s_LanButton, FONT_ICON_NETWORK_WIRED, g_Config.m_UiPage == PAGE_LAN, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_LAN;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_LanButton, &Button, Localize("LAN"));

	TabBar.VSplitLeft(75.0f, &Button, &TabBar);
	static CButtonContainer s_FavoritesButton;
	if(DoButton_MenuTab(&s_FavoritesButton, FONT_ICON_STAR, g_Config.m_UiPage == PAGE_FAVORITES, &Button, IGraphics::CORNER_NONE))
	{
		NewPage = PAGE_FAVORITES;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_FavoritesButton, &Button, Localize("Favorites"));

	size_t FavoriteCommunityIndex = 0;
	static CButtonContainer s_aFavoriteCommunityButtons[5];
	static_assert(std::size(s_aFavoriteCommunityButtons) == (size_t)PAGE_FAVORITE_COMMUNITY_5 - PAGE_FAVORITE_COMMUNITY_1 + 1);
	for(const CCommunity *pCommunity : ServerBrowser()->FavoriteCommunities())
	{
		TabBar.VSplitLeft(75.0f, &Button, &TabBar);
		const int Page = PAGE_FAVORITE_COMMUNITY_1 + FavoriteCommunityIndex;
		if(DoButton_MenuTab(&s_aFavoriteCommunityButtons[FavoriteCommunityIndex], FONT_ICON_ELLIPSIS, g_Config.m_UiPage == Page, &Button, IGraphics::CORNER_NONE, nullptr, nullptr, nullptr, nullptr, 10.0f, FindCommunityIcon(pCommunity->Id())))
		{
			NewPage = Page;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_aFavoriteCommunityButtons[FavoriteCommunityIndex], &Button, pCommunity->Name());

		++FavoriteCommunityIndex;
		if(FavoriteCommunityIndex >= std::size(s_aFavoriteCommunityButtons))
			break;
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	if(NewPage != g_Config.m_UiPage)
	{
		SetMenuPage(NewPage);
	}

	RenderServerbrowser(MainView);
}

// ghost stuff
int CMenus::GhostlistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	const char *pMap = pSelf->Client()->GetCurrentMap();
	if(IsDir || !str_endswith(pInfo->m_pName, ".gho") || !str_startswith(pInfo->m_pName, pMap))
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/%s", pSelf->m_pClient->m_Ghost.GetGhostDir(), pInfo->m_pName);

	CGhostInfo Info;
	if(!pSelf->m_pClient->m_Ghost.GhostLoader()->GetGhostInfo(aFilename, &Info, pMap, pSelf->Client()->GetCurrentMapSha256(), pSelf->Client()->GetCurrentMapCrc()))
		return 0;

	CGhostItem Item;
	str_copy(Item.m_aFilename, aFilename);
	str_copy(Item.m_aPlayer, Info.m_aOwner);
	Item.m_Date = pInfo->m_TimeModified;
	Item.m_Time = Info.m_Time;
	if(Item.m_Time > 0)
		pSelf->m_vGhosts.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_GhostPopulateStartTime > 500ms)
	{
		pSelf->RenderLoading(Localize("Loading ghost files"), "", 0);
	}

	return 0;
}

void CMenus::GhostlistPopulate()
{
	m_vGhosts.clear();
	m_GhostPopulateStartTime = time_get_nanoseconds();
	Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, m_pClient->m_Ghost.GetGhostDir(), GhostlistFetchCallback, this);
	SortGhostlist();

	CGhostItem *pOwnGhost = nullptr;
	for(auto &Ghost : m_vGhosts)
	{
		Ghost.m_Failed = false;
		if(str_comp(Ghost.m_aPlayer, Client()->PlayerName()) == 0 && (!pOwnGhost || Ghost < *pOwnGhost))
			pOwnGhost = &Ghost;
	}

	if(pOwnGhost)
	{
		pOwnGhost->m_Own = true;
		pOwnGhost->m_Slot = m_pClient->m_Ghost.Load(pOwnGhost->m_aFilename);
	}
}

CMenus::CGhostItem *CMenus::GetOwnGhost()
{
	for(auto &Ghost : m_vGhosts)
		if(Ghost.m_Own)
			return &Ghost;
	return nullptr;
}

void CMenus::UpdateOwnGhost(CGhostItem Item)
{
	int Own = -1;
	for(size_t i = 0; i < m_vGhosts.size(); i++)
		if(m_vGhosts[i].m_Own)
			Own = i;

	if(Own == -1)
	{
		Item.m_Own = true;
	}
	else if(g_Config.m_ClRaceGhostSaveBest && (Item.HasFile() || !m_vGhosts[Own].HasFile()))
	{
		Item.m_Own = true;
		DeleteGhostItem(Own);
	}
	else if(m_vGhosts[Own].m_Time > Item.m_Time)
	{
		Item.m_Own = true;
		m_vGhosts[Own].m_Own = false;
		m_vGhosts[Own].m_Slot = -1;
	}
	else
	{
		Item.m_Own = false;
		Item.m_Slot = -1;
	}

	Item.m_Date = std::time(nullptr);
	Item.m_Failed = false;
	m_vGhosts.insert(std::lower_bound(m_vGhosts.begin(), m_vGhosts.end(), Item), Item);
	SortGhostlist();
}

void CMenus::DeleteGhostItem(int Index)
{
	if(m_vGhosts[Index].HasFile())
		Storage()->RemoveFile(m_vGhosts[Index].m_aFilename, IStorage::TYPE_SAVE);
	m_vGhosts.erase(m_vGhosts.begin() + Index);
}

void CMenus::SortGhostlist()
{
	if(g_Config.m_GhSort == GHOST_SORT_NAME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (str_comp(Left.m_aPlayer, Right.m_aPlayer) > 0) : (str_comp(Left.m_aPlayer, Right.m_aPlayer) < 0);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_TIME)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Time > Right.m_Time) : (Left.m_Time < Right.m_Time);
		});
	else if(g_Config.m_GhSort == GHOST_SORT_DATE)
		std::stable_sort(m_vGhosts.begin(), m_vGhosts.end(), [](const CGhostItem &Left, const CGhostItem &Right) {
			return g_Config.m_GhSortOrder ? (Left.m_Date > Right.m_Date) : (Left.m_Date < Right.m_Date);
		});
}

void CMenus::RenderGhost(CUIRect MainView)
{
	// render background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitRight(5.0f, &MainView, nullptr);

	CUIRect Headers, Status;
	CUIRect View = MainView;

	View.HSplitTop(17.0f, &Headers, &View);
	View.HSplitBottom(28.0f, &View, &Status);

	// split of the scrollbar
	Headers.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(20.0f, &Headers, nullptr);

	class CColumn
	{
	public:
		const char *m_pCaption;
		int m_Id;
		int m_Sort;
		float m_Width;
		CUIRect m_Rect;
	};

	enum
	{
		COL_ACTIVE = 0,
		COL_NAME,
		COL_TIME,
		COL_DATE,
	};

	static CColumn s_aCols[] = {
		{"", -1, GHOST_SORT_NONE, 2.0f, {0}},
		{"", COL_ACTIVE, GHOST_SORT_NONE, 30.0f, {0}},
		{Localizable("Name"), COL_NAME, GHOST_SORT_NAME, 200.0f, {0}},
		{Localizable("Time"), COL_TIME, GHOST_SORT_TIME, 90.0f, {0}},
		{Localizable("Date"), COL_DATE, GHOST_SORT_DATE, 150.0f, {0}},
	};

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

		if(i + 1 < NumCols)
			Headers.VSplitLeft(2, nullptr, &Headers);
	}

	// do headers
	for(const auto &Col : s_aCols)
	{
		if(DoButton_GridHeader(&Col.m_Id, Localize(Col.m_pCaption), g_Config.m_GhSort == Col.m_Sort, &Col.m_Rect))
		{
			if(Col.m_Sort != GHOST_SORT_NONE)
			{
				if(g_Config.m_GhSort == Col.m_Sort)
					g_Config.m_GhSortOrder ^= 1;
				else
					g_Config.m_GhSortOrder = 0;
				g_Config.m_GhSort = Col.m_Sort;

				SortGhostlist();
			}
		}
	}

	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 0);

	const int NumGhosts = m_vGhosts.size();
	int NumFailed = 0;
	int NumActivated = 0;
	static int s_SelectedIndex = 0;
	static CListBox s_ListBox;
	s_ListBox.DoStart(17.0f, NumGhosts, 1, 3, s_SelectedIndex, &View, false);

	for(int i = 0; i < NumGhosts; i++)
	{
		const CGhostItem *pGhost = &m_vGhosts[i];
		const CListboxItem Item = s_ListBox.DoNextItem(pGhost);

		if(pGhost->m_Failed)
			NumFailed++;
		if(pGhost->Active())
			NumActivated++;

		if(!Item.m_Visible)
			continue;

		ColorRGBA rgb = ColorRGBA(1.0f, 1.0f, 1.0f);
		if(pGhost->m_Own)
			rgb = color_cast<ColorRGBA>(ColorHSLA(0.33f, 1.0f, 0.75f));

		if(pGhost->m_Failed)
			rgb = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f);

		TextRender()->TextColor(rgb.WithAlpha(pGhost->HasFile() ? 1.0f : 0.5f));

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Item.m_Rect.y;
			Button.h = Item.m_Rect.h;
			Button.w = s_aCols[c].m_Rect.w;

			int Id = s_aCols[c].m_Id;

			if(Id == COL_ACTIVE)
			{
				if(pGhost->Active())
				{
					Graphics()->WrapClamp();
					Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[(SPRITE_OOP + 7) - SPRITE_OOP]);
					Graphics()->QuadsBegin();
					IGraphics::CQuadItem QuadItem(Button.x + Button.w / 2, Button.y + Button.h / 2, 20.0f, 20.0f);
					Graphics()->QuadsDraw(&QuadItem, 1);

					Graphics()->QuadsEnd();
					Graphics()->WrapNormal();
				}
			}
			else if(Id == COL_NAME)
			{
				Ui()->DoLabel(&Button, pGhost->m_aPlayer, 12.0f, TEXTALIGN_ML);
			}
			else if(Id == COL_TIME)
			{
				char aBuf[64];
				str_time(pGhost->m_Time / 10, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_ML);
			}
			else if(Id == COL_DATE)
			{
				char aBuf[64];
				str_timestamp_ex(pGhost->m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_ML);
			}
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	s_SelectedIndex = s_ListBox.DoEnd();

	Status.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_B, 5.0f);
	Status.Margin(5.0f, &Status);

	CUIRect Button;
	Status.VSplitLeft(25.0f, &Button, &Status);

	static CButtonContainer s_ReloadButton;
	static CButtonContainer s_DirectoryButton;
	static CButtonContainer s_ActivateAll;

	if(DoButton_FontIcon(&s_ReloadButton, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &Button) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		m_pClient->m_Ghost.UnloadAll();
		GhostlistPopulate();
	}

	Status.VSplitLeft(5.0f, &Button, &Status);
	Status.VSplitLeft(175.0f, &Button, &Status);
	if(DoButton_Menu(&s_DirectoryButton, Localize("Ghosts directory"), 0, &Button))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ghosts", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("ghosts", IStorage::TYPE_SAVE);
		Client()->ViewFile(aBuf);
	}

	Status.VSplitLeft(5.0f, &Button, &Status);
	if(NumGhosts - NumFailed > 0)
	{
		Status.VSplitLeft(175.0f, &Button, &Status);
		bool ActivateAll = ((NumGhosts - NumFailed) != NumActivated) && m_pClient->m_Ghost.FreeSlots();

		const char *pActionText = ActivateAll ? Localize("Activate all") : Localize("Deactivate all");
		if(DoButton_Menu(&s_ActivateAll, pActionText, 0, &Button))
		{
			for(int i = 0; i < NumGhosts; i++)
			{
				CGhostItem *pGhost = &m_vGhosts[i];
				if(pGhost->m_Failed || (ActivateAll && pGhost->m_Slot != -1))
					continue;

				if(ActivateAll)
				{
					if(!m_pClient->m_Ghost.FreeSlots())
						break;

					pGhost->m_Slot = m_pClient->m_Ghost.Load(pGhost->m_aFilename);
					if(pGhost->m_Slot == -1)
						pGhost->m_Failed = true;
				}
				else
				{
					m_pClient->m_Ghost.UnloadAll();
					pGhost->m_Slot = -1;
				}
			}
		}
	}

	if(s_SelectedIndex == -1 || s_SelectedIndex >= (int)m_vGhosts.size())
		return;

	CGhostItem *pGhost = &m_vGhosts[s_SelectedIndex];

	CGhostItem *pOwnGhost = GetOwnGhost();
	int ReservedSlots = !pGhost->m_Own && !(pOwnGhost && pOwnGhost->Active());
	if(!pGhost->m_Failed && pGhost->HasFile() && (pGhost->Active() || m_pClient->m_Ghost.FreeSlots() > ReservedSlots))
	{
		Status.VSplitRight(120.0f, &Status, &Button);

		static CButtonContainer s_GhostButton;
		const char *pText = pGhost->Active() ? Localize("Deactivate") : Localize("Activate");
		if(DoButton_Menu(&s_GhostButton, pText, 0, &Button) || s_ListBox.WasItemActivated())
		{
			if(pGhost->Active())
			{
				m_pClient->m_Ghost.Unload(pGhost->m_Slot);
				pGhost->m_Slot = -1;
			}
			else
			{
				pGhost->m_Slot = m_pClient->m_Ghost.Load(pGhost->m_aFilename);
				if(pGhost->m_Slot == -1)
					pGhost->m_Failed = true;
			}
		}
		Status.VSplitRight(5.0f, &Status, nullptr);
	}

	Status.VSplitRight(120.0f, &Status, &Button);

	static CButtonContainer s_DeleteButton;
	if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
	{
		if(pGhost->Active())
			m_pClient->m_Ghost.Unload(pGhost->m_Slot);
		DeleteGhostItem(s_SelectedIndex);
	}

	Status.VSplitRight(5.0f, &Status, nullptr);

	bool Recording = m_pClient->m_Ghost.GhostRecorder()->IsRecording();
	if(!pGhost->HasFile() && !Recording && pGhost->Active())
	{
		static CButtonContainer s_SaveButton;
		Status.VSplitRight(120.0f, &Status, &Button);
		if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button))
			m_pClient->m_Ghost.SaveGhost(pGhost);
	}
}

void CMenus::RenderIngameHint()
{
	// With touch controls enabled there is a Close button in the menu and usually no Escape key available.
	if(g_Config.m_ClTouchControls)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->Text(5, 280, 5, Localize("Menu opened. Press Esc key again to close menu."), -1.0f);
	Ui()->MapScreen();
}
