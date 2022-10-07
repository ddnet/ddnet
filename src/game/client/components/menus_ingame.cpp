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
#include <engine/textrender.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include "menus.h"
#include "motd.h"
#include "voting.h"

#include "ghost.h"
#include <engine/keys.h>
#include <engine/storage.h>

#include <chrono>

using namespace std::chrono_literals;

void CMenus::RenderGame(CUIRect MainView)
{
	CUIRect Button, ButtonBar, ButtonBar2;
	MainView.HSplitTop(45.0f, &ButtonBar, &MainView);
	ButtonBar.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	// button bar
	ButtonBar.HSplitTop(10.0f, 0, &ButtonBar);
	ButtonBar.HSplitTop(25.0f, &ButtonBar, 0);
	ButtonBar.VMargin(10.0f, &ButtonBar);

	ButtonBar.HSplitTop(30.0f, 0, &ButtonBar2);
	ButtonBar2.HSplitTop(25.0f, &ButtonBar2, 0);

	ButtonBar.VSplitRight(120.0f, &ButtonBar, &Button);

	static CButtonContainer s_DisconnectButton;
	if(DoButton_Menu(&s_DisconnectButton, Localize("Disconnect"), 0, &Button))
	{
		if(Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
		{
			m_Popup = POPUP_DISCONNECT;
		}
		else
		{
			Client()->Disconnect();
			RefreshBrowserTab(g_Config.m_UiPage);
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, 0);
	ButtonBar.VSplitRight(170.0f, &ButtonBar, &Button);

	bool DummyConnecting = Client()->DummyConnecting();
	static CButtonContainer s_DummyButton;
	if(!Client()->DummyAllowed())
	{
		DoButton_Menu(&s_DummyButton, Localize("Connect Dummy"), 1, &Button, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(1.0f, 0.5f, 0.5f, 0.75f), vec4(1, 0.5f, 0.5f, 0.5f));
	}
	else if(DummyConnecting)
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
			if(Client()->GetCurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				m_Popup = POPUP_DISCONNECT_DUMMY;
			}
			else
			{
				Client()->DummyDisconnect(0);
				SetActive(false);
			}
		}
	}

	ButtonBar.VSplitRight(5.0f, &ButtonBar, 0);
	ButtonBar.VSplitRight(140.0f, &ButtonBar, &Button);

	static CButtonContainer s_DemoButton;
	bool Recording = DemoRecorder(RECORDER_MANUAL)->IsRecording();
	if(DoButton_Menu(&s_DemoButton, Recording ? Localize("Stop record") : Localize("Record demo"), 0, &Button))
	{
		if(!Recording)
			Client()->DemoRecorder_Start(Client()->GetCurrentMap(), true, RECORDER_MANUAL);
		else
			Client()->DemoRecorder_Stop(RECORDER_MANUAL);
	}

	static CButtonContainer s_SpectateButton;
	static CButtonContainer s_JoinRedButton;
	static CButtonContainer s_JoinBlueButton;

	bool Paused = false;
	bool Spec = false;
	if(m_pClient->m_Snap.m_LocalClientID >= 0)
	{
		Paused = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Paused;
		Spec = m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_Spec;
	}

	if(m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pGameInfoObj && !Paused && !Spec)
	{
		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
			ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
			if(!DummyConnecting && DoButton_Menu(&s_SpectateButton, Localize("Spectate"), 0, &Button))
			{
				if(g_Config.m_ClDummy == 0 || Client()->DummyConnected())
				{
					m_pClient->SendSwitchTeam(TEAM_SPECTATORS);
					SetActive(false);
				}
			}
		}

		if(m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS)
		{
			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_RED)
			{
				ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				if(!DummyConnecting && DoButton_Menu(&s_JoinRedButton, Localize("Join red"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_RED);
					SetActive(false);
				}
			}

			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_BLUE)
			{
				ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				if(!DummyConnecting && DoButton_Menu(&s_JoinBlueButton, Localize("Join blue"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(TEAM_BLUE);
					SetActive(false);
				}
			}
		}
		else
		{
			if(m_pClient->m_Snap.m_pLocalInfo->m_Team != 0)
			{
				ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
				ButtonBar.VSplitLeft(120.0f, &Button, &ButtonBar);
				if(!DummyConnecting && DoButton_Menu(&s_SpectateButton, Localize("Join game"), 0, &Button))
				{
					m_pClient->SendSwitchTeam(0);
					SetActive(false);
				}
			}
		}

		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS)
		{
			ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
			ButtonBar.VSplitLeft(65.0f, &Button, &ButtonBar);

			static CButtonContainer s_KillButton;
			if(DoButton_Menu(&s_KillButton, Localize("Kill"), 0, &Button))
			{
				m_pClient->SendKill(-1);
				SetActive(false);
			}
		}
	}

	if(m_pClient->m_ReceivedDDNetPlayer && m_pClient->m_Snap.m_pLocalInfo && m_pClient->m_Snap.m_pGameInfoObj)
	{
		if(m_pClient->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS || Paused || Spec)
		{
			ButtonBar.VSplitLeft(5.0f, 0, &ButtonBar);
			ButtonBar.VSplitLeft((!Paused && !Spec) ? 65.0f : 120.0f, &Button, &ButtonBar);

			static CButtonContainer s_PauseButton;
			if(DoButton_Menu(&s_PauseButton, (!Paused && !Spec) ? Localize("Pause") : Localize("Join game"), 0, &Button))
			{
				m_pClient->Console()->ExecuteLine("say /pause");
				SetActive(false);
			}
		}
	}
}

void CMenus::RenderPlayers(CUIRect MainView)
{
	CUIRect Button, Button2, ButtonBar, Options, Player;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	// player options
	MainView.Margin(10.0f, &Options);
	Options.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Options.Margin(10.0f, &Options);
	Options.HSplitTop(50.0f, &Button, &Options);
	UI()->DoLabel(&Button, Localize("Player options"), 34.0f, TEXTALIGN_LEFT);

	// headline
	Options.HSplitTop(34.0f, &ButtonBar, &Options);
	ButtonBar.VSplitRight(231.0f, &Player, &ButtonBar);
	UI()->DoLabel(&Player, Localize("Player"), 24.0f, TEXTALIGN_LEFT);

	ButtonBar.HMargin(1.0f, &ButtonBar);
	float Width = ButtonBar.h * 2.0f;
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_EMOTICON_MUTE, &Button);

	ButtonBar.VSplitLeft(20.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(Width, &Button, &ButtonBar);
	RenderTools()->RenderIcon(IMAGE_GUIICONS, SPRITE_GUIICON_FRIEND, &Button);

	int TotalPlayers = 0;

	for(auto &pInfoByName : m_pClient->m_Snap.m_apInfoByName)
	{
		if(!pInfoByName)
			continue;

		int Index = pInfoByName->m_ClientID;

		if(Index == m_pClient->m_Snap.m_LocalClientID)
			continue;

		TotalPlayers++;
	}

	static int s_VoteList = 0;
	static float s_ScrollValue = 0;
	CUIRect List = Options;
	// List.HSplitTop(28.0f, 0, &List);
	UiDoListboxStart(&s_VoteList, &List, 24.0f, "", "", TotalPlayers, 1, -1, s_ScrollValue);

	// options
	static int s_aPlayerIDs[MAX_CLIENTS][3] = {{0}};

	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!m_pClient->m_Snap.m_apInfoByName[i])
			continue;

		int Index = m_pClient->m_Snap.m_apInfoByName[i]->m_ClientID;

		if(Index == m_pClient->m_Snap.m_LocalClientID)
			continue;

		CListboxItem Item = UiDoListboxNextItem(&m_pClient->m_aClients[Index]);

		Count++;

		if(!Item.m_Visible)
			continue;

		if(Count % 2 == 1)
			Item.m_Rect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
		Item.m_Rect.VSplitRight(300.0f, &Player, &Item.m_Rect);

		// player info
		Player.VSplitLeft(28.0f, &Button, &Player);

		CTeeRenderInfo TeeInfo = m_pClient->m_aClients[Index].m_RenderInfo;
		TeeInfo.m_Size = Button.h;

		CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos(Button.x + Button.h / 2, Button.y + Button.h / 2 + OffsetToMid.y);

		RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);

		Player.HSplitTop(1.5f, 0, &Player);
		Player.VSplitMid(&Player, &Button);
		Item.m_Rect.VSplitRight(200.0f, &Button2, &Item.m_Rect);
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, Player.x, Player.y + (Player.h - 14.f) / 2.f, 14.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Player.w;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[Index].m_aName, -1);

		TextRender()->SetCursor(&Cursor, Button.x, Button.y + (Button.h - 14.f) / 2.f, 14.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = Button.w;
		TextRender()->TextEx(&Cursor, m_pClient->m_aClients[Index].m_aClan, -1);

		// TextRender()->SetCursor(&Cursor, Button2.x,Button2.y, 14.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		// Cursor.m_LineWidth = Button.w;
		ColorRGBA Color(1.0f, 1.0f, 1.0f, 0.5f);
		m_pClient->m_CountryFlags.Render(m_pClient->m_aClients[Index].m_Country, &Color,
			Button2.x, Button2.y + Button2.h / 2.0f - 0.75f * Button2.h / 2.0f, 1.5f * Button2.h, 0.75f * Button2.h);

		// ignore chat button
		Item.m_Rect.HMargin(2.0f, &Item.m_Rect);
		Item.m_Rect.VSplitLeft(Width, &Button, &Item.m_Rect);
		Button.VSplitLeft((Width - Button.h) / 4.0f, 0, &Button);
		Button.VSplitLeft(Button.h, &Button, 0);
		if(g_Config.m_ClShowChatFriends && !m_pClient->m_aClients[Index].m_Friend)
			DoButton_Toggle(&s_aPlayerIDs[Index][0], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIDs[Index][0], m_pClient->m_aClients[Index].m_ChatIgnore, &Button, true))
			m_pClient->m_aClients[Index].m_ChatIgnore ^= 1;

		// ignore emoticon button
		Item.m_Rect.VSplitLeft(20.0f, &Button, &Item.m_Rect);
		Item.m_Rect.VSplitLeft(Width, &Button, &Item.m_Rect);
		Button.VSplitLeft((Width - Button.h) / 4.0f, 0, &Button);
		Button.VSplitLeft(Button.h, &Button, 0);
		if(g_Config.m_ClShowChatFriends && !m_pClient->m_aClients[Index].m_Friend)
			DoButton_Toggle(&s_aPlayerIDs[Index][1], 1, &Button, false);
		else if(DoButton_Toggle(&s_aPlayerIDs[Index][1], m_pClient->m_aClients[Index].m_EmoticonIgnore, &Button, true))
			m_pClient->m_aClients[Index].m_EmoticonIgnore ^= 1;

		// friend button
		Item.m_Rect.VSplitLeft(20.0f, &Button, &Item.m_Rect);
		Item.m_Rect.VSplitLeft(Width, &Button, &Item.m_Rect);
		Button.VSplitLeft((Width - Button.h) / 4.0f, 0, &Button);
		Button.VSplitLeft(Button.h, &Button, 0);
		if(DoButton_Toggle(&s_aPlayerIDs[Index][2], m_pClient->m_aClients[Index].m_Friend, &Button, true))
		{
			if(m_pClient->m_aClients[Index].m_Friend)
				m_pClient->Friends()->RemoveFriend(m_pClient->m_aClients[Index].m_aName, m_pClient->m_aClients[Index].m_aClan);
			else
				m_pClient->Friends()->AddFriend(m_pClient->m_aClients[Index].m_aName, m_pClient->m_aClients[Index].m_aClan);
		}
	}

	UiDoListboxEnd(&s_ScrollValue, 0);
}

void CMenus::RenderServerInfo(CUIRect MainView)
{
	if(!m_pClient->m_Snap.m_pLocalInfo)
		return;

	// fetch server info
	CServerInfo CurrentServerInfo;
	Client()->GetServerInfo(&CurrentServerInfo);

	// render background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	CUIRect View, ServerInfo, GameInfo, Motd;

	float x = 0.0f;
	float y = 0.0f;

	char aBuf[1024];

	// set view to use for all sub-modules
	MainView.Margin(10.0f, &View);

	// serverinfo
	View.HSplitTop(View.h / 2 - 5.0f, &ServerInfo, &Motd);
	ServerInfo.VSplitLeft(View.w / 2 - 5.0f, &ServerInfo, &GameInfo);
	ServerInfo.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);

	ServerInfo.Margin(5.0f, &ServerInfo);

	x = 5.0f;
	y = 0.0f;

	TextRender()->Text(0, ServerInfo.x + x, ServerInfo.y + y, 32, Localize("Server info"), 250.0f);
	y += 32.0f + 5.0f;

	mem_zero(aBuf, sizeof(aBuf));
	str_format(
		aBuf,
		sizeof(aBuf),
		"%s\n\n"
		"%s: %s\n"
		"%s: %d\n"
		"%s: %s\n"
		"%s: %s\n",
		CurrentServerInfo.m_aName,
		Localize("Address"), CurrentServerInfo.m_aAddress,
		Localize("Ping"), m_pClient->m_Snap.m_pLocalInfo->m_Latency,
		Localize("Version"), CurrentServerInfo.m_aVersion,
		Localize("Password"), CurrentServerInfo.m_Flags & 1 ? Localize("Yes") : Localize("No"));

	TextRender()->Text(0, ServerInfo.x + x, ServerInfo.y + y, 20, aBuf, 250.0f);

	{
		CUIRect Button;
		NETADDR ServerAddr = Client()->ServerAddress();
		TRISTATE IsFavorite = Favorites()->IsFavorite(&ServerAddr, 1);
		ServerInfo.HSplitBottom(20.0f, &ServerInfo, &Button);
		static int s_AddFavButton = 0;
		if(DoButton_CheckBox(&s_AddFavButton, Localize("Favorite"), IsFavorite != TRISTATE::NONE, &Button))
		{
			if(IsFavorite != TRISTATE::NONE)
				Favorites()->Remove(&ServerAddr, 1);
			else
				Favorites()->Add(&ServerAddr, 1);
		}
	}

	// gameinfo
	GameInfo.VSplitLeft(10.0f, 0x0, &GameInfo);
	GameInfo.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);

	GameInfo.Margin(5.0f, &GameInfo);

	x = 5.0f;
	y = 0.0f;

	TextRender()->Text(0, GameInfo.x + x, GameInfo.y + y, 32, Localize("Game info"), 250.0f);
	y += 32.0f + 5.0f;

	if(m_pClient->m_Snap.m_pGameInfoObj)
	{
		mem_zero(aBuf, sizeof(aBuf));
		str_format(
			aBuf,
			sizeof(aBuf),
			"\n\n"
			"%s: %s\n"
			"%s: %s\n"
			"%s: %d\n"
			"%s: %d\n"
			"\n"
			"%s: %d/%d\n",
			Localize("Game type"), CurrentServerInfo.m_aGameType,
			Localize("Map"), CurrentServerInfo.m_aMap,
			Localize("Score limit"), m_pClient->m_Snap.m_pGameInfoObj->m_ScoreLimit,
			Localize("Time limit"), m_pClient->m_Snap.m_pGameInfoObj->m_TimeLimit,
			Localize("Players"), m_pClient->m_Snap.m_NumPlayers, CurrentServerInfo.m_MaxClients);
		TextRender()->Text(0, GameInfo.x + x, GameInfo.y + y, 20, aBuf, 250.0f);
	}

	// motd
	const float MotdFontSize = 16.0f;
	Motd.HSplitTop(10.0f, nullptr, &Motd);
	Motd.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	Motd.HMargin(5.0f, &Motd);
	Motd.VMargin(10.0f, &Motd);

	CUIRect MotdHeader;
	Motd.HSplitTop(2.0f * MotdFontSize, &MotdHeader, &Motd);
	Motd.HSplitTop(5.0f, nullptr, &Motd);
	TextRender()->Text(nullptr, MotdHeader.x, MotdHeader.y, 2.0f * MotdFontSize, Localize("MOTD"), -1.0f);

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 5 * MotdFontSize;
	s_ScrollRegion.Begin(&Motd, &ScrollOffset, &ScrollParams);
	Motd.y += ScrollOffset.y;

	CUIRect MotdTextArea;
	Motd.HSplitTop((str_countchr(m_pClient->m_Motd.m_aServerMotd, '\n') + 1) * MotdFontSize, &MotdTextArea, &Motd);
	s_ScrollRegion.AddRect(MotdTextArea);
	TextRender()->Text(nullptr, MotdTextArea.x, MotdTextArea.y, MotdFontSize, m_pClient->m_Motd.m_aServerMotd, MotdTextArea.w);

	s_ScrollRegion.End();
}

bool CMenus::RenderServerControlServer(CUIRect MainView)
{
	static int s_VoteList = 0;
	static float s_ScrollValue = 0;
	CUIRect List = MainView;
	int Total = m_pClient->m_Voting.m_NumVoteOptions;
	int NumVoteOptions = 0;
	int aIndices[MAX_VOTE_OPTIONS];
	static int s_CurVoteOption = 0;
	int TotalShown = 0;

	for(CVoteOptionClient *pOption = m_pClient->m_Voting.m_pFirst; pOption; pOption = pOption->m_pNext)
	{
		if(m_aFilterString[0] != '\0' && !str_utf8_find_nocase(pOption->m_aDescription, m_aFilterString))
			continue;
		TotalShown++;
	}

	UiDoListboxStart(&s_VoteList, &List, 19.0f, "", "", TotalShown, 1, s_CurVoteOption, s_ScrollValue);

	int i = -1;
	for(CVoteOptionClient *pOption = m_pClient->m_Voting.m_pFirst; pOption; pOption = pOption->m_pNext)
	{
		i++;
		if(m_aFilterString[0] != '\0' && !str_utf8_find_nocase(pOption->m_aDescription, m_aFilterString))
			continue;

		CListboxItem Item = UiDoListboxNextItem(pOption);

		if(Item.m_Visible)
			UI()->DoLabel(&Item.m_Rect, pOption->m_aDescription, 13.0f, TEXTALIGN_LEFT);

		if(NumVoteOptions < Total)
			aIndices[NumVoteOptions] = i;
		NumVoteOptions++;
	}

	bool Call;
	s_CurVoteOption = UiDoListboxEnd(&s_ScrollValue, &Call);
	if(s_CurVoteOption < Total)
		m_CallvoteSelectedOption = aIndices[s_CurVoteOption];
	return Call;
}

bool CMenus::RenderServerControlKick(CUIRect MainView, bool FilterSpectators)
{
	int NumOptions = 0;
	int Selected = 0;
	static int aPlayerIDs[MAX_CLIENTS];
	for(auto &pInfoByName : m_pClient->m_Snap.m_apInfoByName)
	{
		if(!pInfoByName)
			continue;

		int Index = pInfoByName->m_ClientID;
		if(Index == m_pClient->m_Snap.m_LocalClientID || (FilterSpectators && pInfoByName->m_Team == TEAM_SPECTATORS))
			continue;

		if(!str_utf8_find_nocase(m_pClient->m_aClients[Index].m_aName, m_aFilterString))
			continue;

		if(m_CallvoteSelectedPlayer == Index)
			Selected = NumOptions;
		aPlayerIDs[NumOptions++] = Index;
	}

	static int s_VoteList = 0;
	static float s_ScrollValue = 0;
	CUIRect List = MainView;
	UiDoListboxStart(&s_VoteList, &List, 24.0f, "", "", NumOptions, 1, Selected, s_ScrollValue);

	for(int i = 0; i < NumOptions; i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&aPlayerIDs[i]);

		if(Item.m_Visible)
		{
			CTeeRenderInfo TeeInfo = m_pClient->m_aClients[aPlayerIDs[i]].m_RenderInfo;
			TeeInfo.m_Size = Item.m_Rect.h;

			CAnimState *pIdleState = CAnimState::GetIdle();
			vec2 OffsetToMid;
			RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
			vec2 TeeRenderPos(Item.m_Rect.x + Item.m_Rect.h / 2, Item.m_Rect.y + Item.m_Rect.h / 2 + OffsetToMid.y);

			RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos);

			Item.m_Rect.x += TeeInfo.m_Size;
			UI()->DoLabel(&Item.m_Rect, m_pClient->m_aClients[aPlayerIDs[i]].m_aName, 16.0f, TEXTALIGN_LEFT);
		}
	}

	bool Call;
	Selected = UiDoListboxEnd(&s_ScrollValue, &Call);
	m_CallvoteSelectedPlayer = Selected != -1 ? aPlayerIDs[Selected] : -1;
	return Call;
}

void CMenus::RenderServerControl(CUIRect MainView)
{
	static int s_ControlPage = 0;

	// render background
	CUIRect Bottom, RconExtension, TabBar, Button;
	MainView.HSplitTop(20.0f, &Bottom, &MainView);
	Bottom.Draw(ms_ColorTabbarActive, 0, 10.0f);
	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

	if(Client()->RconAuthed())
		MainView.HSplitBottom(90.0f, &MainView, &RconExtension);

	// tab bar
	{
		TabBar.VSplitLeft(TabBar.w / 3, &Button, &TabBar);
		static CButtonContainer s_Button0;
		if(DoButton_MenuTab(&s_Button0, Localize("Change settings"), s_ControlPage == 0, &Button, 0))
			s_ControlPage = 0;

		TabBar.VSplitMid(&Button, &TabBar);
		static CButtonContainer s_Button1;
		if(DoButton_MenuTab(&s_Button1, Localize("Kick player"), s_ControlPage == 1, &Button, 0))
			s_ControlPage = 1;

		static CButtonContainer s_Button2;
		if(DoButton_MenuTab(&s_Button2, Localize("Move player to spectators"), s_ControlPage == 2, &TabBar, 0))
			s_ControlPage = 2;
	}

	// render page
	MainView.HSplitBottom(ms_ButtonHeight + 5 * 2, &MainView, &Bottom);
	Bottom.HMargin(5.0f, &Bottom);

	bool Call = false;
	if(s_ControlPage == 0)
		Call = RenderServerControlServer(MainView);
	else if(s_ControlPage == 1)
		Call = RenderServerControlKick(MainView, false);
	else if(s_ControlPage == 2)
		Call = RenderServerControlKick(MainView, true);

	// vote menu
	{
		CUIRect QuickSearch;

		// render quick search
		{
			Bottom.VSplitLeft(240.0f, &QuickSearch, &Bottom);
			QuickSearch.HSplitTop(5.0f, 0, &QuickSearch);
			const char *pSearchLabel = "\xEF\x80\x82";
			TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
			TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);

			SLabelProperties Props;
			Props.m_AlignVertically = 0;
			UI()->DoLabel(&QuickSearch, pSearchLabel, 14.0f, TEXTALIGN_LEFT, Props);
			float wSearch = TextRender()->TextWidth(0, 14.0f, pSearchLabel, -1, -1.0f);
			TextRender()->SetRenderFlags(0);
			TextRender()->SetCurFont(NULL);
			QuickSearch.VSplitLeft(wSearch, 0, &QuickSearch);
			QuickSearch.VSplitLeft(5.0f, 0, &QuickSearch);
			static int s_ClearButton = 0;
			static float s_Offset = 0.0f;

			SUIExEditBoxProperties EditProps;
			if(m_ControlPageOpening || (Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed()))
			{
				UI()->SetActiveItem(&m_aFilterString);
				m_ControlPageOpening = false;
				EditProps.m_SelectText = true;
			}
			EditProps.m_pEmptyText = Localize("Search");
			UI()->DoClearableEditBox(&m_aFilterString, &s_ClearButton, &QuickSearch, m_aFilterString, sizeof(m_aFilterString), 14.0f, &s_Offset, false, IGraphics::CORNER_ALL, EditProps);
		}

		Bottom.VSplitRight(120.0f, &Bottom, &Button);

		static CButtonContainer s_CallVoteButton;
		if(DoButton_Menu(&s_CallVoteButton, Localize("Call vote"), 0, &Button) || Call)
		{
			if(s_ControlPage == 0)
			{
				m_pClient->m_Voting.CallvoteOption(m_CallvoteSelectedOption, m_aCallvoteReason);
				if(g_Config.m_UiCloseWindowAfterChangingSetting)
					SetActive(false);
			}
			else if(s_ControlPage == 1)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_Voting.CallvoteKick(m_CallvoteSelectedPlayer, m_aCallvoteReason);
					SetActive(false);
				}
			}
			else if(s_ControlPage == 2)
			{
				if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
					m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
				{
					m_pClient->m_Voting.CallvoteSpectate(m_CallvoteSelectedPlayer, m_aCallvoteReason);
					SetActive(false);
				}
			}
			m_aCallvoteReason[0] = 0;
		}

		// render kick reason
		CUIRect Reason;
		Bottom.VSplitRight(40.0f, &Bottom, 0);
		Bottom.VSplitRight(160.0f, &Bottom, &Reason);
		Reason.HSplitTop(5.0f, 0, &Reason);
		const char *pLabel = Localize("Reason:");
		UI()->DoLabel(&Reason, pLabel, 14.0f, TEXTALIGN_LEFT);
		float w = TextRender()->TextWidth(0, 14.0f, pLabel, -1, -1.0f);
		Reason.VSplitLeft(w + 10.0f, 0, &Reason);
		static float s_Offset = 0.0f;
		if(Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed())
			UI()->SetActiveItem(&m_aCallvoteReason);
		UI()->DoEditBox(&m_aCallvoteReason, &Reason, m_aCallvoteReason, sizeof(m_aCallvoteReason), 14.0f, &s_Offset, false, IGraphics::CORNER_ALL);

		// extended features (only available when authed in rcon)
		if(Client()->RconAuthed())
		{
			// background
			RconExtension.Margin(10.0f, &RconExtension);
			RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
			RconExtension.HSplitTop(5.0f, 0, &RconExtension);

			// force vote
			Bottom.VSplitLeft(5.0f, 0, &Bottom);
			Bottom.VSplitLeft(120.0f, &Button, &Bottom);

			static CButtonContainer s_ForceVoteButton;
			if(DoButton_Menu(&s_ForceVoteButton, Localize("Force vote"), 0, &Button))
			{
				if(s_ControlPage == 0)
					m_pClient->m_Voting.CallvoteOption(m_CallvoteSelectedOption, m_aCallvoteReason, true);
				else if(s_ControlPage == 1)
				{
					if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
						m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
					{
						m_pClient->m_Voting.CallvoteKick(m_CallvoteSelectedPlayer, m_aCallvoteReason, true);
						SetActive(false);
					}
				}
				else if(s_ControlPage == 2)
				{
					if(m_CallvoteSelectedPlayer >= 0 && m_CallvoteSelectedPlayer < MAX_CLIENTS &&
						m_pClient->m_Snap.m_apPlayerInfos[m_CallvoteSelectedPlayer])
					{
						m_pClient->m_Voting.CallvoteSpectate(m_CallvoteSelectedPlayer, m_aCallvoteReason, true);
						SetActive(false);
					}
				}
				m_aCallvoteReason[0] = 0;
			}

			if(s_ControlPage == 0)
			{
				// remove vote
				Bottom.VSplitRight(10.0f, &Bottom, 0);
				Bottom.VSplitRight(120.0f, 0, &Button);
				static CButtonContainer s_RemoveVoteButton;
				if(DoButton_Menu(&s_RemoveVoteButton, Localize("Remove"), 0, &Button))
					m_pClient->m_Voting.RemovevoteOption(m_CallvoteSelectedOption);

				// add vote
				RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
				Bottom.VSplitLeft(5.0f, 0, &Bottom);
				Bottom.VSplitLeft(250.0f, &Button, &Bottom);
				UI()->DoLabel(&Button, Localize("Vote description:"), 14.0f, TEXTALIGN_LEFT);

				Bottom.VSplitLeft(20.0f, 0, &Button);
				UI()->DoLabel(&Button, Localize("Vote command:"), 14.0f, TEXTALIGN_LEFT);

				static char s_aVoteDescription[64] = {0};
				static char s_aVoteCommand[512] = {0};
				RconExtension.HSplitTop(20.0f, &Bottom, &RconExtension);
				Bottom.VSplitRight(10.0f, &Bottom, 0);
				Bottom.VSplitRight(120.0f, &Bottom, &Button);
				static CButtonContainer s_AddVoteButton;
				if(DoButton_Menu(&s_AddVoteButton, Localize("Add"), 0, &Button))
					if(s_aVoteDescription[0] != 0 && s_aVoteCommand[0] != 0)
						m_pClient->m_Voting.AddvoteOption(s_aVoteDescription, s_aVoteCommand);

				Bottom.VSplitLeft(5.0f, 0, &Bottom);
				Bottom.VSplitLeft(250.0f, &Button, &Bottom);
				static float s_OffsetDesc = 0.0f;
				UI()->DoEditBox(&s_aVoteDescription, &Button, s_aVoteDescription, sizeof(s_aVoteDescription), 14.0f, &s_OffsetDesc, false, IGraphics::CORNER_ALL);

				Bottom.VMargin(20.0f, &Button);
				static float s_OffsetCmd = 0.0f;
				UI()->DoEditBox(&s_aVoteCommand, &Button, s_aVoteCommand, sizeof(s_aVoteCommand), 14.0f, &s_OffsetCmd, false, IGraphics::CORNER_ALL);
			}
		}
	}
}

void CMenus::RenderInGameNetwork(CUIRect MainView)
{
	CUIRect Box = MainView;
	CUIRect Button;

	int Page = g_Config.m_UiPage;
	int NewPage = -1;

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	Box.HSplitTop(5.0f, &MainView, &MainView);
	Box.HSplitTop(24.0f, &Box, &MainView);

	Box.VSplitLeft(100.0f, &Button, &Box);
	static CButtonContainer s_InternetButton;
	if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), Page == PAGE_INTERNET, &Button, 0))
	{
		if(Page != PAGE_INTERNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		NewPage = PAGE_INTERNET;
	}

	Box.VSplitLeft(80.0f, &Button, &Box);
	static CButtonContainer s_LanButton;
	if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), Page == PAGE_LAN, &Button, 0))
	{
		if(Page != PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		NewPage = PAGE_LAN;
	}

	Box.VSplitLeft(110.0f, &Button, &Box);
	static CButtonContainer s_FavoritesButton;
	if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), Page == PAGE_FAVORITES, &Button, 0))
	{
		if(Page != PAGE_FAVORITES)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
		NewPage = PAGE_FAVORITES;
	}

	Box.VSplitLeft(110.0f, &Button, &Box);
	static CButtonContainer s_DDNetButton;
	if(DoButton_MenuTab(&s_DDNetButton, "DDNet", Page == PAGE_DDNET, &Button, 0) || Page < PAGE_INTERNET || Page > PAGE_KOG)
	{
		if(Page != PAGE_DDNET)
		{
			Client()->RequestDDNetInfo();
			ServerBrowser()->Refresh(IServerBrowser::TYPE_DDNET);
		}
		NewPage = PAGE_DDNET;
	}

	Box.VSplitLeft(110.0f, &Button, &Box);
	static CButtonContainer s_KoGButton;
	if(DoButton_MenuTab(&s_KoGButton, "KoG", Page == PAGE_KOG, &Button, IGraphics::CORNER_BR))
	{
		if(Page != PAGE_KOG)
		{
			Client()->RequestDDNetInfo();
			ServerBrowser()->Refresh(IServerBrowser::TYPE_KOG);
		}
		NewPage = PAGE_KOG;
	}

	if(NewPage != -1)
	{
		if(Client()->State() != IClient::STATE_OFFLINE)
			SetMenuPage(NewPage);
	}

	RenderServerbrowser(MainView);
}

// ghost stuff
int CMenus::GhostlistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	const char *pMap = pSelf->Client()->GetCurrentMap();
	if(IsDir || !str_endswith(pName, ".gho") || !str_startswith(pName, pMap))
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/%s", pSelf->m_pClient->m_Ghost.GetGhostDir(), pName);

	CGhostInfo Info;
	if(!pSelf->m_pClient->m_Ghost.GhostLoader()->GetGhostInfo(aFilename, &Info, pMap, pSelf->Client()->GetCurrentMapSha256(), pSelf->Client()->GetCurrentMapCrc()))
		return 0;

	CGhostItem Item;
	str_copy(Item.m_aFilename, aFilename);
	str_copy(Item.m_aPlayer, Info.m_aOwner);
	Item.m_Time = Info.m_Time;
	if(Item.m_Time > 0)
		pSelf->m_vGhosts.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_GhostPopulateStartTime > 500ms)
	{
		pSelf->GameClient()->m_Menus.RenderLoading(Localize("Loading ghost files"), "", 0, false);
	}

	return 0;
}

void CMenus::GhostlistPopulate()
{
	m_vGhosts.clear();
	m_GhostPopulateStartTime = time_get_nanoseconds();
	Storage()->ListDirectory(IStorage::TYPE_ALL, m_pClient->m_Ghost.GetGhostDir(), GhostlistFetchCallback, this);
	std::sort(m_vGhosts.begin(), m_vGhosts.end());

	CGhostItem *pOwnGhost = 0;
	for(auto &Ghost : m_vGhosts)
		if(str_comp(Ghost.m_aPlayer, Client()->PlayerName()) == 0 && (!pOwnGhost || Ghost < *pOwnGhost))
			pOwnGhost = &Ghost;

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

	if(Own != -1)
	{
		m_vGhosts[Own].m_Slot = -1;
		m_vGhosts[Own].m_Own = false;
		if(Item.HasFile() || !m_vGhosts[Own].HasFile())
			DeleteGhostItem(Own);
	}

	Item.m_Own = true;
	m_vGhosts.insert(std::lower_bound(m_vGhosts.begin(), m_vGhosts.end(), Item), Item);
}

void CMenus::DeleteGhostItem(int Index)
{
	if(m_vGhosts[Index].HasFile())
		Storage()->RemoveFile(m_vGhosts[Index].m_aFilename, IStorage::TYPE_SAVE);
	m_vGhosts.erase(m_vGhosts.begin() + Index);
}

void CMenus::RenderGhost(CUIRect MainView)
{
	// render background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);

	MainView.HSplitTop(10.0f, 0, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, 0);
	MainView.VSplitLeft(5.0f, 0, &MainView);
	MainView.VSplitRight(5.0f, &MainView, 0);

	CUIRect Headers, Status;
	CUIRect View = MainView;

	View.HSplitTop(17.0f, &Headers, &View);
	View.HSplitBottom(28.0f, &View, &Status);

	// split of the scrollbar
	Headers.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitRight(20.0f, &Headers, 0);

	struct CColumn
	{
		CLocConstString m_Caption;
		int m_Id;
		float m_Width;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		COL_ACTIVE = 0,
		COL_NAME,
		COL_TIME,
	};

	static CColumn s_aCols[] = {
		{" ", -1, 2.0f, {0}, {0}},
		{" ", COL_ACTIVE, 30.0f, {0}, {0}},
		{"Name", COL_NAME, 300.0f, {0}, {0}}, // Localize("Name")
		{"Time", COL_TIME, 200.0f, {0}, {0}}, // Localize("Time")
	};

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

		if(i + 1 < NumCols)
			Headers.VSplitLeft(2, &s_aCols[i].m_Spacer, &Headers);
	}

	// do headers
	for(int i = 0; i < NumCols; i++)
		DoButton_GridHeader(s_aCols[i].m_Caption, Localize(s_aCols[i].m_Caption), 0, &s_aCols[i].m_Rect);

	View.Draw(ColorRGBA(0, 0, 0, 0.15f), 0, 0);

	CUIRect Scroll;
	View.VSplitRight(20.0f, &View, &Scroll);

	static float s_ScrollValue = 0;
	s_ScrollValue = UI()->DoScrollbarV(&s_ScrollValue, &Scroll, s_ScrollValue);

	int NumGhosts = m_vGhosts.size();
	static int s_SelectedIndex = 0;
	HandleListInputs(View, s_ScrollValue, 1.0f, nullptr, s_aCols[0].m_Rect.h, s_SelectedIndex, NumGhosts);

	// set clipping
	UI()->ClipEnable(&View);

	CUIRect OriginalView = View;
	int Num = (int)(View.h / s_aCols[0].m_Rect.h) + 1;
	int ScrollNum = maximum(NumGhosts - Num + 1, 0);
	View.y -= s_ScrollValue * ScrollNum * s_aCols[0].m_Rect.h;

	int NewSelected = -1;
	bool DoubleClicked = false;

	for(int i = 0; i < NumGhosts; i++)
	{
		const CGhostItem *pItem = &m_vGhosts[i];
		CUIRect Row;
		View.HSplitTop(17.0f, &Row, &View);

		// make sure that only those in view can be selected
		if(Row.y + Row.h > OriginalView.y && Row.y < OriginalView.y + OriginalView.h)
		{
			if(i == s_SelectedIndex)
			{
				CUIRect r = Row;
				r.Margin(1.5f, &r);
				r.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_ALL, 4.0f);
			}

			if(UI()->DoButtonLogic(pItem, 0, &Row))
			{
				NewSelected = i;
				DoubleClicked |= NewSelected == m_DoubleClickIndex;
				m_DoubleClickIndex = NewSelected;
			}
		}

		ColorRGBA rgb = ColorRGBA(1.0f, 1.0f, 1.0f);
		if(pItem->m_Own)
			rgb = color_cast<ColorRGBA>(ColorHSLA(0.33f, 1.0f, 0.75f));

		TextRender()->TextColor(rgb.WithAlpha(pItem->HasFile() ? 1.0f : 0.5f));

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Row.y;
			Button.h = Row.h;
			Button.w = s_aCols[c].m_Rect.w;

			int Id = s_aCols[c].m_Id;

			if(Id == COL_ACTIVE)
			{
				if(pItem->Active())
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
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, Button.x, Button.y + (Button.h - 12.0f) / 2.f, 12.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;

				TextRender()->TextEx(&Cursor, pItem->m_aPlayer, -1);
			}
			else if(Id == COL_TIME)
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, Button.x, Button.y + (Button.h - 12.0f) / 2.f, 12.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;

				char aBuf[64];
				str_time(pItem->m_Time / 10, TIME_HOURS_CENTISECS, aBuf, sizeof(aBuf));
				TextRender()->TextEx(&Cursor, aBuf, -1);
			}
		}

		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	UI()->ClipDisable();

	if(NewSelected != -1)
		s_SelectedIndex = NewSelected;

	Status.Draw(ColorRGBA(1, 1, 1, 0.25f), IGraphics::CORNER_B, 5.0f);
	Status.Margin(5.0f, &Status);

	CUIRect Button;
	Status.VSplitLeft(120.0f, &Button, &Status);

	static CButtonContainer s_ReloadButton;
	if(DoButton_Menu(&s_ReloadButton, Localize("Reload"), 0, &Button) || Input()->KeyPress(KEY_F5))
	{
		m_pClient->m_Ghost.UnloadAll();
		GhostlistPopulate();
	}

	if(s_SelectedIndex == -1 || s_SelectedIndex >= (int)m_vGhosts.size())
		return;

	CGhostItem *pGhost = &m_vGhosts[s_SelectedIndex];

	CGhostItem *pOwnGhost = GetOwnGhost();
	int ReservedSlots = !pGhost->m_Own && !(pOwnGhost && pOwnGhost->Active());
	if(pGhost->HasFile() && (pGhost->Active() || m_pClient->m_Ghost.FreeSlots() > ReservedSlots))
	{
		Status.VSplitRight(120.0f, &Status, &Button);

		static CButtonContainer s_GhostButton;
		const char *pText = pGhost->Active() ? Localize("Deactivate") : Localize("Activate");
		if(DoButton_Menu(&s_GhostButton, pText, 0, &Button) || (DoubleClicked && Input()->MouseDoubleClick()))
		{
			if(pGhost->Active())
			{
				m_pClient->m_Ghost.Unload(pGhost->m_Slot);
				pGhost->m_Slot = -1;
			}
			else
				pGhost->m_Slot = m_pClient->m_Ghost.Load(pGhost->m_aFilename);
		}

		Status.VSplitRight(5.0f, &Status, 0);
	}

	Status.VSplitRight(120.0f, &Status, &Button);

	static CButtonContainer s_DeleteButton;
	if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
	{
		if(pGhost->Active())
			m_pClient->m_Ghost.Unload(pGhost->m_Slot);
		DeleteGhostItem(s_SelectedIndex);
	}

	Status.VSplitRight(5.0f, &Status, 0);

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
	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->Text(0x0, 5, 280, 5, Localize("Menu opened. Press Esc key again to close menu."), -1.0f);
	UI()->MapScreen();
}
