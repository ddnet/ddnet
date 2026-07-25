/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/components/effects.h>
#include <game/client/components/items.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/nameplates.h>
#include <game/client/components/skins.h>
#include <game/client/components/tooltips.h>
#include <game/client/gameclient.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

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

void CMenus::DoLaserPreview(const CUIRect *pRect, const ColorHSLA LaserOutlineColor, const ColorHSLA LaserInnerColor, const int LaserType)
{
	CUIRect Section = *pRect;
	vec2 From = vec2(Section.x + 30.0f, Section.y + Section.h / 2.0f);
	vec2 Pos = vec2(Section.x + Section.w - 20.0f, Section.y + Section.h / 2.0f);

	const ColorRGBA OuterColor = color_cast<ColorRGBA>(ColorHSLA(LaserOutlineColor));
	const ColorRGBA InnerColor = color_cast<ColorRGBA>(ColorHSLA(LaserInnerColor));
	const float TicksHead = Client()->GlobalTime() * Client()->GameTickSpeed();

	// TicksBody = 4.0 for less laser width for weapon alignment
	GameClient()->m_Items.RenderLaser(From, Pos, OuterColor, InnerColor, 4.0f, TicksHead, LaserType);

	switch(LaserType)
	{
	case LASERTYPE_RIFLE:
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponLaser);
		Graphics()->SelectSprite(SPRITE_WEAPON_LASER_BODY);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);
		Graphics()->QuadsEnd();
		break;
	case LASERTYPE_SHOTGUN:
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponShotgun);
		Graphics()->SelectSprite(SPRITE_WEAPON_SHOTGUN_BODY);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);
		Graphics()->QuadsEnd();
		break;
	case LASERTYPE_DRAGGER:
	{
		CTeeRenderInfo TeeRenderInfo;
		TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
		TeeRenderInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		TeeRenderInfo.m_Size = 64.0f;
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL, vec2(-1, 0), Pos);
		break;
	}
	case LASERTYPE_FREEZE:
	{
		CTeeRenderInfo TeeRenderInfo;
		if(g_Config.m_ClShowNinja)
			TeeRenderInfo.Apply(GameClient()->m_Skins.Find("x_ninja"));
		else
			TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
		TeeRenderInfo.m_TeeRenderFlags = TEE_EFFECT_FROZEN;
		TeeRenderInfo.m_Size = 64.0f;
		TeeRenderInfo.m_ColorBody = ColorRGBA(1, 1, 1);
		TeeRenderInfo.m_ColorFeet = ColorRGBA(1, 1, 1);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_PAIN, vec2(1, 0), From);
		GameClient()->m_Effects.FreezingFlakes(From, vec2(32, 32), 1.0f);
		break;
	}
	default:
		GameClient()->m_Items.RenderLaser(From, From, OuterColor, InnerColor, 4.0f, TicksHead, LaserType);
	}
}
