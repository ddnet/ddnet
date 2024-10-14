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



enum {
		AIODOB_TAB_PAGE1 = 0,
		AIODOB_TAB_BINDWHEEL = 1,
		AIODOB_TAB_PAGE3 = 2,
		AIODOB_TAB_PAGE4 = 3,
		NUMBER_OF_AIODOB_TABS = 4,

	};

void CMenus::RenderSettingsAiodob(CUIRect MainView)
{
	char aBuf[128];
	static int s_CurTab = 0;

	CUIRect TabBar, Button, Label;

	MainView.HSplitTop(20.0f, &TabBar, &MainView);
	const float TabWidth = TabBar.w / NUMBER_OF_AIODOB_TABS;
	static CButtonContainer s_aPageTabs[NUMBER_OF_AIODOB_TABS] = {};
	const char *apTabNames[NUMBER_OF_AIODOB_TABS] = {
		Localize("Usefull Settings"),
		Localize("BindWheel"),
		Localize("Useless Stuff"),
		Localize("Mostly Useless Stuff")};

	for(int Tab = AIODOB_TAB_PAGE1; Tab < NUMBER_OF_AIODOB_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == AIODOB_TAB_PAGE1 ? IGraphics::CORNER_L : Tab == NUMBER_OF_AIODOB_TABS - 1 ? IGraphics::CORNER_R :
														       IGraphics::CORNER_NONE;
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
	const float MarginSmall = 6.0f;
	const float MarginBetweenViews = 20.0f;

	const float ColorPickerLabelSize = 13.0f;
	const float ColorPickerLineSpacing = 5.0f;

	

	// Client Page 1

	

	if(s_CurTab == AIODOB_TAB_PAGE1)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		const float FontSize = 14.0f;
		const float Margin = 10.0f;
		const float HeaderHeight = FontSize + 5.0f + Margin;

		// left side in settings menu

		CUIRect ChillerBotSettings, MiscSettings, FriendSettings, PlayerIndicatorSettings, UiSettings, WarSettings, ChatScoNameSettings;
		MainView.VSplitMid(&ChillerBotSettings, &UiSettings);

		{
			ChillerBotSettings.VMargin(5.0f, &ChillerBotSettings);
			ChillerBotSettings.HSplitTop(100.0f, &ChillerBotSettings, &WarSettings);
			if(s_ScrollRegion.AddRect(ChillerBotSettings))
			{
				ChillerBotSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ChillerBotSettings.VMargin(Margin, &ChillerBotSettings);

				ChillerBotSettings.HSplitTop(HeaderHeight, &Button, &ChillerBotSettings);
				Ui()->DoLabel(&Button, Localize("Chillerbot Features"), FontSize, TEXTALIGN_MC);
				{
					
					ChillerBotSettings.HSplitTop(20.0f, &Button, &MainView);
					
					Button.VSplitLeft(0.0f, &Button, &ChillerBotSettings);
					Button.VSplitLeft(100.0f, &Label, &Button);
					Button.VSplitLeft(230.0f, &Button, 0);
				
					static CLineInput s_ReplyMsg;
					s_ReplyMsg.SetBuffer(g_Config.m_ClAutoReplyMsg, sizeof(g_Config.m_ClAutoReplyMsg));
					s_ReplyMsg.SetEmptyText("Use '%n' to ping back");
					
					if(DoButton_CheckBox(&g_Config.m_ClTabbedOutMsg, "Auto reply", g_Config.m_ClTabbedOutMsg, &ChillerBotSettings))
					{
						g_Config.m_ClTabbedOutMsg ^= 1;
					}
					Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
				}
				ChillerBotSettings.HSplitTop(21.0f, &Button, &ChillerBotSettings);
				{
					ChillerBotSettings.HSplitTop(19.9f, &Button, &MainView);
				
					
					Button.VSplitLeft(0.0f, &Button, &ChillerBotSettings);
					Button.VSplitLeft(140.0f, &Label, &Button);
					Button.VSplitLeft(190.0f, &Button, 0);

					static CLineInput s_ReplyMsg;
					s_ReplyMsg.SetBuffer(g_Config.m_ClFinishName, sizeof(g_Config.m_ClFinishName));
					s_ReplyMsg.SetEmptyText("qxdFox");

					if(DoButton_CheckBox(&g_Config.m_ClFinishRename, "Rename on Finish", g_Config.m_ClFinishRename, &ChillerBotSettings))
					{
						g_Config.m_ClFinishRename ^= 1;
					}
					Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
				}
				ChillerBotSettings.HSplitTop(20.0f, &Button, &ChillerBotSettings);


				/* Disabled due to breaking randomly and I cant be bothered to fix it sorry :p
				{ 
					ChillerBotSettings.HSplitTop(19.9f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &ChillerBotSettings);
					Button.VSplitLeft(140.0f, &Label, &Button);
					Button.VSplitLeft(190.0f, &Button, 0);

		

					if(DoButton_CheckBox(&g_Config.m_ClSkinStealer, "Skin Stealer", g_Config.m_ClSkinStealer, &ChillerBotSettings))
					{
						str_format(aBuf, sizeof(aBuf), "cb_skin_stealer %d", !g_Config.m_ClSkinStealer);
						Console()->ExecuteLine(aBuf);
					}
			
				}*/
				
				if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChangeTileNotification, ("Notify When Player is Being Moved"), &g_Config.m_ClChangeTileNotification, &ChillerBotSettings, LineSize));

			

			}
		}
		
		
		
			{
			WarSettings.HSplitTop(Margin, nullptr, &WarSettings);
				WarSettings.HSplitTop(115.0f, &WarSettings, &PlayerIndicatorSettings);
			if(s_ScrollRegion.AddRect(WarSettings))
			{
				WarSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				WarSettings.VMargin(Margin, &WarSettings);

				WarSettings.HSplitTop(HeaderHeight, &Button, &WarSettings);
				Ui()->DoLabel(&Button, Localize("Team/War Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWarList, Localize(" Do Team/War Name Colors?"), &g_Config.m_ClWarList, &WarSettings, LineSize);

				static CButtonContainer s_TeamColor;
				DoLine_ColorPicker(&s_TeamColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &WarSettings, Localize("Nameplate Color of Teammates"), &g_Config.m_ClTeamColor, ColorRGBA(0.3f, 1.0f, 0.3f), true);

				static CButtonContainer s_WarColor;
				DoLine_ColorPicker(&s_WarColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &WarSettings, Localize("Nameplate Color of Enemies"), &g_Config.m_ClWarColor, ColorRGBA(1.0f, 0.3f, 0.3f), true);

				ColorRGBA TeamColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClTeamColor));
				ColorRGBA WarColor = color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(g_Config.m_ClWarColor));
			}
		}
	

		{
			PlayerIndicatorSettings.HSplitTop(Margin, nullptr, &PlayerIndicatorSettings);
			if(g_Config.m_ClIndicatorVariableDistance)
			{
				PlayerIndicatorSettings.HSplitTop(350.0f, &PlayerIndicatorSettings, 0);
			}
			else
			{
				PlayerIndicatorSettings.HSplitTop(300.0f, &PlayerIndicatorSettings, 0);
			}

			if(s_ScrollRegion.AddRect(PlayerIndicatorSettings))
			{
				PlayerIndicatorSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				PlayerIndicatorSettings.VMargin(Margin, &PlayerIndicatorSettings);

				PlayerIndicatorSettings.HSplitTop(HeaderHeight, &Button, &PlayerIndicatorSettings);
				Ui()->DoLabel(&Button, Localize("Player Indicator"), FontSize, TEXTALIGN_MC);

								
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicator, ("Show any enabled Indicators"), &g_Config.m_ClPlayerIndicator, &PlayerIndicatorSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPlayerIndicatorFreeze, ("Show only freeze Players"), &g_Config.m_ClPlayerIndicatorFreeze, &PlayerIndicatorSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTeamOnly, ("Only show after joining a team"), &g_Config.m_ClIndicatorTeamOnly, &PlayerIndicatorSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorTees, ("Render tiny tees instead of circles"), &g_Config.m_ClIndicatorTees, &PlayerIndicatorSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClIndicatorVariableDistance, ("Change indicator offset based on distance to other tees"), &g_Config.m_ClIndicatorVariableDistance, &PlayerIndicatorSettings, LineSize);
				
				

				static CButtonContainer IndicatorAliveColorID, IndicatorDeadColorID, IndicatorSavedColorID;
				DoLine_ColorPicker(&IndicatorAliveColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, ("Indicator alive color"), &g_Config.m_ClIndicatorAlive, ColorRGBA(0.6f, 1.0f, 0.6f, 1.0f), true);
				
				DoLine_ColorPicker(&IndicatorDeadColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, ("Indicator dead color"), &g_Config.m_ClIndicatorFreeze, ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f), true);
				
				DoLine_ColorPicker(&IndicatorSavedColorID, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &PlayerIndicatorSettings, ("Indicator save color"), &g_Config.m_ClIndicatorSaved, ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f), true);
				
				{
					CUIRect Button, Label;
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					PlayerIndicatorSettings.HSplitTop(20.0f, &Button, &PlayerIndicatorSettings);
					Button.VSplitLeft(150.0f, &Label, &Button);
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %i ", "Indicator size", g_Config.m_ClIndicatorRadius);
					Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
					g_Config.m_ClIndicatorRadius = (int)(Ui()->DoScrollbarH(&g_Config.m_ClIndicatorRadius, &Button, (g_Config.m_ClIndicatorRadius - 1) / 15.0f) * 15.0f) + 1;
				}
				{
					CUIRect Button, Label;
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					PlayerIndicatorSettings.HSplitTop(20.0f, &Button, &PlayerIndicatorSettings);
					Button.VSplitLeft(150.0f, &Label, &Button);
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %i ", "Indicator opacity", g_Config.m_ClIndicatorOpacity);
					Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
					g_Config.m_ClIndicatorOpacity = (int)(Ui()->DoScrollbarH(&g_Config.m_ClIndicatorOpacity, &Button, (g_Config.m_ClIndicatorOpacity) / 100.0f) * 100.0f);
				}
				{
					CUIRect Button, Label;
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					PlayerIndicatorSettings.HSplitTop(20.0f, &Button, &PlayerIndicatorSettings);
					Button.VSplitLeft(150.0f, &Label, &Button);
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %i ", "Indicator offset", g_Config.m_ClIndicatorOffset);
					if(g_Config.m_ClIndicatorVariableDistance)
						str_format(aBuf, sizeof(aBuf), "%s: %i ", "Min offset", g_Config.m_ClIndicatorOffset);
					Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
					g_Config.m_ClIndicatorOffset = (int)(Ui()->DoScrollbarH(&g_Config.m_ClIndicatorOffset, &Button, (g_Config.m_ClIndicatorOffset - 16) / 184.0f) * 184.0f) + 16;
				}
				if(g_Config.m_ClIndicatorVariableDistance)
				{
					CUIRect Button, Label;
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					PlayerIndicatorSettings.HSplitTop(20.0f, &Button, &PlayerIndicatorSettings);
					Button.VSplitLeft(150.0f, &Label, &Button);
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %i ", "Max offset", g_Config.m_ClIndicatorOffsetMax);
					Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
					g_Config.m_ClIndicatorOffsetMax = (int)(Ui()->DoScrollbarH(&g_Config.m_ClIndicatorOffsetMax, &Button, (g_Config.m_ClIndicatorOffsetMax - 16) / 184.0f) * 184.0f) + 16;
				}
				if(g_Config.m_ClIndicatorVariableDistance)
				{
					CUIRect Button, Label;
					PlayerIndicatorSettings.HSplitTop(5.0f, &Button, &PlayerIndicatorSettings);
					PlayerIndicatorSettings.HSplitTop(20.0f, &Button, &PlayerIndicatorSettings);
					Button.VSplitLeft(150.0f, &Label, &Button);
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "%s: %i ", "Max distance", g_Config.m_ClIndicatorMaxDistance);
					Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);
					int NewValue = (g_Config.m_ClIndicatorMaxDistance) / 50.0f;
					NewValue = (int)(Ui()->DoScrollbarH(&g_Config.m_ClIndicatorMaxDistance, &Button, (NewValue - 10) / 130.0f) * 130.0f) + 10;
					g_Config.m_ClIndicatorMaxDistance = NewValue * 50;
				}
			
			}
		}


		// right side

			// UI/Hud Settings

		{
			UiSettings.VMargin(5.0f, &UiSettings);
			if(g_Config.m_ClFpsSpoofer)
			{
				UiSettings.HSplitTop(160.0f, &UiSettings, &MiscSettings);
			}
			else
			{
				UiSettings.HSplitTop(125.0f, &UiSettings, &MiscSettings);
			}
			if(s_ScrollRegion.AddRect(UiSettings))
			{
				UiSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				UiSettings.VMargin(10.0f, &UiSettings);

				UiSettings.HSplitTop(HeaderHeight, &Button, &UiSettings);
				Ui()->DoLabel(&Button, Localize("Ui/Hud Settings"), FontSize, TEXTALIGN_MC);

				UiSettings.HSplitTop(2 * LineSize, &Button, &UiSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClCornerRoundness, &g_Config.m_ClCornerRoundness, &Button, Localize("How round Corners should be"), 0, 150, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				static CButtonContainer s_UiColor;
				//DoLine_ColorPicker(&s_UiColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &FriendSettings, Localize("Foe (war?) Name Color (old)"), &g_Config.m_AiodobColor, ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f), true);

				DoLine_ColorPicker(&s_UiColor, 25.0f, 13.0f, 2.0f, &UiSettings, Localize("Background Color"), &g_Config.m_AiodobColor, color_cast<ColorRGBA>(ColorHSLA(463470847, true)), false, nullptr, true);

	


				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFpsSpoofer, Localize("Fps Spoofer"), &g_Config.m_ClFpsSpoofer, &UiSettings, LineSize);
				if(g_Config.m_ClFpsSpoofer)
				{
					UiSettings.HSplitTop(2 * LineSize, &Button, &UiSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClFpsSpooferMargin, &g_Config.m_ClFpsSpooferMargin, &Button, Localize("Fps Spoofer Margin"), 1, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
				}
			}
		}

		{
			MiscSettings.HSplitTop(Margin, nullptr, &MiscSettings);
			if(g_Config.m_ClRenderCursorSpec)
			{
				if(g_Config.m_ClDoCursorSpecOpacity)
				{
					MiscSettings.HSplitTop(200.0f, &MiscSettings, &FriendSettings);
				}
				else
				{
					MiscSettings.HSplitTop(160.0f, &MiscSettings, &FriendSettings);
				}
			}
			else
			{
				MiscSettings.HSplitTop(140.0f, &MiscSettings, &FriendSettings);
			}

			if(s_ScrollRegion.AddRect(MiscSettings))
			{
				MiscSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				MiscSettings.VMargin(Margin, &MiscSettings);

				MiscSettings.HSplitTop(HeaderHeight, &Button, &MiscSettings);
				Ui()->DoLabel(&Button, Localize("Miscellaneous"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoVerify, ("Auto Verify"), &g_Config.m_ClAutoVerify, &MiscSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatBubble, ("Show Chat Bubble"), &g_Config.m_ClChatBubble, &MiscSettings, LineSize);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClPingNameCircle, ("Show Ping Circles Next To Names"), &g_Config.m_ClPingNameCircle, &MiscSettings, LineSize);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClRenderCursorSpec, ("Show Cursor While Spectating"), &g_Config.m_ClRenderCursorSpec, &MiscSettings, LineSize);
				if(g_Config.m_ClRenderCursorSpec)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoCursorSpecOpacity, ("Change Cursor Opacity while Spectating?"), &g_Config.m_ClDoCursorSpecOpacity, &MiscSettings, LineSize);
					if(g_Config.m_ClDoCursorSpecOpacity)
					{
						MiscSettings.HSplitTop(2 * LineSize, &Button, &MiscSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClRenderCursorSpecOpacity, &g_Config.m_ClRenderCursorSpecOpacity, &Button, Localize("Cursor Opacity While Spectating"), 1, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
					}
				}
			
				{
					MiscSettings.HSplitTop(20.0f, &Button, &MainView);

					Button.VSplitLeft(0.0f, &Button, &MiscSettings);
					Button.VSplitLeft(158.0f, &Label, &Button);
					Button.VSplitLeft(248.0f, &Button, 0);

					static CLineInput s_ReplyMsg;
					s_ReplyMsg.SetBuffer(g_Config.m_ClRunOnJoinMsg, sizeof(g_Config.m_ClRunOnJoinMsg));
					s_ReplyMsg.SetEmptyText("Any Console Command");

					if(DoButton_CheckBox(&g_Config.m_ClRunOnJoinConsole, "Run on Join Console", g_Config.m_ClRunOnJoinConsole, &MiscSettings))
					{
						g_Config.m_ClRunOnJoinConsole ^= 1;
					}
					Ui()->DoEditBox(&s_ReplyMsg, &Button, 14.0f);
				}
			
			
			}
		}
		{
			FriendSettings.HSplitTop(Margin, nullptr, &FriendSettings);
			FriendSettings.HSplitTop(135.0f, &FriendSettings, &ChatScoNameSettings);
			if(s_ScrollRegion.AddRect(FriendSettings))
			{
				FriendSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				FriendSettings.VMargin(Margin, &FriendSettings);

				FriendSettings.HSplitTop(HeaderHeight, &Button, &FriendSettings);
				Ui()->DoLabel(&Button, Localize("Friend Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoFriendColorInchat, Localize("Friend Name Color in Chat"), &g_Config.m_ClDoFriendColorInchat, &FriendSettings, LineSize + 1.45);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClDoFriendColorScoreboard, Localize("Friend Name Color in Scoreboard"), &g_Config.m_ClDoFriendColorScoreboard, &FriendSettings, LineSize + 1.45);
			
				static CButtonContainer s_FriendColor;
				FriendSettings.VMargin(-2, &FriendSettings);
				DoLine_ColorPicker(&s_FriendColor, ColorPickerLineSize + 0.25f, ColorPickerLabelSize + 0.25f, ColorPickerLineSpacing, &FriendSettings, Localize("Friend Name Color (Setting Can be Off)"), &g_Config.m_ClFriendColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(14745554)), true, &g_Config.m_ClDoFriendNameColor);
				static CButtonContainer s_FriendAfkColor;
				DoLine_ColorPicker(&s_FriendAfkColor, ColorPickerLineSize + 0.25f, ColorPickerLabelSize + 0.25f, ColorPickerLineSpacing, &FriendSettings, Localize("Afk Friend Color in Scoreboard"), &g_Config.m_ClFriendAfkColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(14694222)), true, &g_Config.m_ClDoFriendAfkColor);

			}
		}
		{
			ChatScoNameSettings.HSplitTop(Margin, nullptr, &ChatScoNameSettings);
			ChatScoNameSettings.HSplitTop(160.0f, &ChatScoNameSettings, 0);
			if(s_ScrollRegion.AddRect(ChatScoNameSettings))
			{
				ChatScoNameSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ChatScoNameSettings.VMargin(Margin, &ChatScoNameSettings);

				ChatScoNameSettings.HSplitTop(HeaderHeight, &Button, &ChatScoNameSettings);
				Ui()->DoLabel(&Button, Localize("Chat/Scoreboard/Nameplate/Menu"), FontSize, TEXTALIGN_MC);


				static CButtonContainer s_AfkColor;
				DoLine_ColorPicker(&s_AfkColor, ColorPickerLineSize + 0.3f, ColorPickerLabelSize + 0.3f, ColorPickerLineSpacing -3.0f, &ChatScoNameSettings, Localize("Afk Name Color in Scoreboard"), &g_Config.m_ClAfkColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(10951270)), true, &g_Config.m_ClAfkNameColor);
				
				ChatScoNameSettings.VMargin(2, &ChatScoNameSettings);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClChatSpecMark, Localize("Say (s) Nexto Spectating Tees in Chat"), &g_Config.m_ClChatSpecMark, &ChatScoNameSettings, LineSize + 1.45);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClScoreboardSpecPlayer, Localize("Make Tees Sit in Scoreboard When Spectating"), &g_Config.m_ClScoreboardSpecPlayer, &ChatScoNameSettings, LineSize + 1.45);


				static CButtonContainer s_SpecColor;
				ChatScoNameSettings.VMargin(-2, &ChatScoNameSettings);
				DoLine_ColorPicker(&s_SpecColor, ColorPickerLineSize + 0.3f, ColorPickerLabelSize + 0.3f, ColorPickerLineSpacing, &ChatScoNameSettings, Localize("Say (s) Nexto Spectators in Scoreboard"), &g_Config.m_ClSpecColor, color_cast<ColorRGBA, ColorHSLA>(ColorHSLA(8936607)), true, &g_Config.m_ClScoreboardSpecMark);
				
				ChatScoNameSettings.VMargin(2, &ChatScoNameSettings);
				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowOthersInMenu, Localize("Show Zzz Emote When Tee's in a Menu"), &g_Config.m_ClShowOthersInMenu, &ChatScoNameSettings, LineSize + 1.25);
		
			}
		

		
		}
		s_ScrollRegion.End();
	}
	const float LineMargin = 20.0f;
	CUIRect Column, Section;

		if(s_CurTab == AIODOB_TAB_BINDWHEEL)
	{
		CUIRect Screen = *Ui()->Screen();
		MainView.VSplitLeft(MainView.w * 0.5, &MainView, &Column);
		CUIRect LeftColumn = MainView;
		MainView.HSplitTop(30.0f, &Section, &MainView);

		CUIRect buttons[NUM_BINDWHEEL];
		char pD[NUM_BINDWHEEL][MAX_BINDWHEEL_DESC];
		char pC[NUM_BINDWHEEL][MAX_BINDWHEEL_CMD];
		CUIRect Label;

		// Draw Circle
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(0, 0, 0, 0.3f);
		Graphics()->DrawCircle(Screen.w / 2 - 55.0f, Screen.h / 2, 190.0f, 64);
		Graphics()->QuadsEnd();

		Graphics()->WrapClamp();
		for(int i = 0; i < NUM_BINDWHEEL; i++)
		{
			float Angle = 2 * pi * i / NUM_BINDWHEEL;
			float margin = 120.0f;

			if(Angle > pi)
			{
				Angle -= 2 * pi;
			}

			int orgAngle = 2 * pi * i / NUM_BINDWHEEL;
			if(((orgAngle >= 0 && orgAngle < 2)) || ((orgAngle >= 4 && orgAngle < 6)))
			{
				margin = 170.0f;
			}

			float Size = 12.0f;

			float NudgeX = margin * cosf(Angle);
			float NudgeY = 150.0f * sinf(Angle);

			char aBuf[MAX_BINDWHEEL_DESC];
			str_format(aBuf, sizeof(aBuf), "%s", GameClient()->m_Bindwheel.m_BindWheelList[i].description);
			TextRender()->Text(Screen.w / 2 - 100.0f + NudgeX, Screen.h / 2 + NudgeY, Size, aBuf, -1.0f);
		}
		Graphics()->WrapNormal();

		static CLineInput s_BindWheelDesc[NUM_BINDWHEEL];
		static CLineInput s_BindWheelCmd[NUM_BINDWHEEL];

		for(int i = 0; i < NUM_BINDWHEEL; i++)
		{
			if(i == NUM_BINDWHEEL / 2)
			{
				MainView = Column;
				MainView.VSplitRight(500, 0, &MainView);

				MainView.HSplitTop(30.0f, &Section, &MainView);
				MainView.VSplitLeft(MainView.w * 0.5, 0, &MainView);
			}

			str_format(pD[i], sizeof(pD[i]), "%s", GameClient()->m_Bindwheel.m_BindWheelList[i].description);

			str_format(pC[i], sizeof(pC[i]), "%s", GameClient()->m_Bindwheel.m_BindWheelList[i].command);

			// Description
			MainView.HSplitTop(15.0f, 0, &MainView);
			MainView.HSplitTop(20.0f, &buttons[i], &MainView);
			buttons[i].VSplitLeft(80.0f, &Label, &buttons[i]);
			buttons[i].VSplitLeft(150.0f, &buttons[i], 0);
			char aBuf[MAX_BINDWHEEL_CMD];
			str_format(aBuf, sizeof(aBuf), "%s %d:", Localize("Description"), i + 1);
			Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);

			s_BindWheelDesc[i].SetBuffer(GameClient()->m_Bindwheel.m_BindWheelList[i].description, sizeof(GameClient()->m_Bindwheel.m_BindWheelList[i].description));
			s_BindWheelDesc[i].SetEmptyText(Localize("Description"));

			Ui()->DoEditBox(&s_BindWheelDesc[i], &buttons[i], 14.0f);

			// Command
			MainView.HSplitTop(5.0f, 0, &MainView);
			MainView.HSplitTop(20.0f, &buttons[i], &MainView);
			buttons[i].VSplitLeft(80.0f, &Label, &buttons[i]);
			buttons[i].VSplitLeft(150.0f, &buttons[i], 0);
			str_format(aBuf, sizeof(aBuf), "%s %d:", Localize("Command"), i + 1);
			Ui()->DoLabel(&Label, aBuf, 14.0f, TEXTALIGN_LEFT);

			s_BindWheelCmd[i].SetBuffer(GameClient()->m_Bindwheel.m_BindWheelList[i].command, sizeof(GameClient()->m_Bindwheel.m_BindWheelList[i].command));
			s_BindWheelCmd[i].SetEmptyText(Localize("Command"));

			Ui()->DoEditBox(&s_BindWheelCmd[i], &buttons[i], 14.0f);
		}

		// Do Settings Key

		CKeyInfo Key = CKeyInfo{"Bind Wheel Key", "+bindwheel", 0, 0};
		for(int Mod = 0; Mod < CBinds::MODIFIER_COMBINATION_COUNT; Mod++)
		{
			for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
			{
				const char *pBind = m_pClient->m_Binds.Get(KeyId, Mod);
				if(!pBind[0])
					continue;

				if(str_comp(pBind, Key.m_pCommand) == 0)
				{
					Key.m_KeyId = KeyId;
					Key.m_ModifierCombination = Mod;
					break;
				}
			}
		}

		CUIRect Button, KeyLabel;
		LeftColumn.HSplitBottom(20.0f, &LeftColumn, 0);
		LeftColumn.HSplitBottom(20.0f, &LeftColumn, &Button);
		Button.VSplitLeft(120.0f, &KeyLabel, &Button);
		Button.VSplitLeft(100, &Button, 0);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s: \n\n\n(credits: Tater)", Localize((const char *)Key.m_pName));
		// Credits to Tater ^^^^^^^^^^^^

		Ui()->DoLabel(&KeyLabel, aBuf, 13.0f, TEXTALIGN_LEFT);
		int OldId = Key.m_KeyId, OldModifierCombination = Key.m_ModifierCombination, NewModifierCombination;
		int NewId = DoKeyReader((void *)&Key.m_pName, &Button, OldId, OldModifierCombination, &NewModifierCombination);
		if(NewId != OldId || NewModifierCombination != OldModifierCombination)
		{
			if(OldId != 0 || NewId == 0)
				m_pClient->m_Binds.Bind(OldId, "", false, OldModifierCombination);
			if(NewId != 0)
				m_pClient->m_Binds.Bind(NewId, Key.m_pCommand, false, NewModifierCombination);
		}
		LeftColumn.HSplitBottom(10.0f, &LeftColumn, 0);
		LeftColumn.HSplitBottom(LineMargin, &LeftColumn, &Button);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClResetBindWheelMouse, ("Reset position of mouse when opening bindwheel"), &g_Config.m_ClResetBindWheelMouse, &Button, LineMargin);
	}

		// Tater Client Bind wheel
	if(s_CurTab == AIODOB_TAB_PAGE3)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		const float FontSize = 14.0f;
		const float Margin = 10.0f;
		const float HeaderHeight = FontSize + 5.0f + Margin;

		// left side in settings menu

		CUIRect PlayerSettings, HookSettings, WeaponSettings, HammerSettings, ResetButton, GunSettings;
		MainView.VSplitMid(&WeaponSettings, &ResetButton);

	
		// Weapon Settings

		{
			WeaponSettings.VMargin(5.0f, &WeaponSettings);
			if(g_Config.m_ClShowWeapons == 1)
			{
				WeaponSettings.HSplitTop(60.0f, &WeaponSettings, &HammerSettings);
			}
			else
			{
				if(g_Config.m_ClWeaponRot == 0)
				{
					WeaponSettings.HSplitTop(195.0f, &WeaponSettings, &HammerSettings);
				}
				else
				{
					WeaponSettings.HSplitTop(165.0f, &WeaponSettings, &HammerSettings);
				}
			}

			if(s_ScrollRegion.AddRect(WeaponSettings))
			{
				WeaponSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				WeaponSettings.VMargin(Margin, &WeaponSettings);

				WeaponSettings.HSplitTop(HeaderHeight, &Button, &WeaponSettings);
				Ui()->DoLabel(&Button, Localize("All Weapons Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClShowWeapons, Localize("Don't Show Weapons"), &g_Config.m_ClShowWeapons, &WeaponSettings, LineSize);
				if(g_Config.m_ClShowWeapons == 0)
				{
					DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClWeaponRot, Localize("Turn off the rotating thing idfk the word :p"), &g_Config.m_ClWeaponRot, &WeaponSettings, LineSize);

					WeaponSettings.HSplitTop(2 * LineSize, &Button, &WeaponSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClWeaponSize, &g_Config.m_ClWeaponSize, &Button, Localize("Weapon Size"), -2, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					WeaponSettings.HSplitTop(2 * LineSize, &Button, &WeaponSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClWeaponRotSize2, &g_Config.m_ClWeaponRotSize2, &Button, Localize("Pointing Left?"), -1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					if(g_Config.m_ClWeaponRot == 0)
					{
						WeaponSettings.HSplitTop(2 * LineSize, &Button, &WeaponSettings);
						Ui()->DoScrollbarOption(&g_Config.m_ClWeaponRotSize3, &g_Config.m_ClWeaponRotSize3, &Button, Localize("Pointing Right?"), -1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
					}
				}
			}
		}

		// Hammer Settings

		{
			HammerSettings.HSplitTop(Margin, nullptr, &HammerSettings);
			HammerSettings.HSplitTop(115.0f, &HammerSettings, &GunSettings);
			if(s_ScrollRegion.AddRect(HammerSettings))
			{
				if(g_Config.m_ClShowWeapons == 0)
				{
					HammerSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
					HammerSettings.VMargin(Margin, &HammerSettings);

					HammerSettings.HSplitTop(HeaderHeight, &Button, &HammerSettings);
					Ui()->DoLabel(&Button, Localize("Hammer Settings"), FontSize, TEXTALIGN_MC);

					HammerSettings.HSplitTop(2 * LineSize, &Button, &HammerSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClHammerDir, &g_Config.m_ClHammerDir, &Button, Localize("Hammer Direction"), -50, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					HammerSettings.HSplitTop(2 * LineSize, &Button, &HammerSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClHammerRot, &g_Config.m_ClHammerRot, &Button, Localize("Breaks da Hammer"), -50, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
				}
			}
		}

		// Gun Settings

		{
			GunSettings.HSplitTop(Margin, nullptr, &GunSettings);
			GunSettings.HSplitTop(275.0f, &GunSettings, 0);
			if(s_ScrollRegion.AddRect(GunSettings))
			{
				if(g_Config.m_ClShowWeapons == 0)
				{
					GunSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
					GunSettings.VMargin(Margin, &GunSettings);

					GunSettings.HSplitTop(HeaderHeight, &Button, &GunSettings);
					Ui()->DoLabel(&Button, Localize("Gun Settings"), FontSize, TEXTALIGN_MC);

					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClGunPos, &g_Config.m_ClGunPos, &Button, Localize("Gun y position"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClGunPosSitting, &g_Config.m_ClGunPosSitting, &Button, Localize("Gun y position while sitting"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClGunReload, &g_Config.m_ClGunReload, &Button, Localize("How long the reload animation is"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClGunRecoil, &g_Config.m_ClGunRecoil, &Button, Localize("How far the recoil goes"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
				
					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClBulletSize, &g_Config.m_ClBulletSize, &Button, Localize("Bullet Effect Size"), -8, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

					GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
					Ui()->DoScrollbarOption(&g_Config.m_ClBulletLifetime, &g_Config.m_ClBulletLifetime, &Button, Localize("Bullet Effect Lifetime"), -1, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
				
				
				
				
				}
			}
		}

		// right side in settings menu

		// defaults
		{
			ResetButton.VMargin(5.0f, &ResetButton);
			ResetButton.HSplitTop(40.0f, &ResetButton, &PlayerSettings);
			if(s_ScrollRegion.AddRect(ResetButton))
			{
				ResetButton.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 7.5f));
				ResetButton.Margin(10.0f, &ResetButton);
				static CButtonContainer s_DefaultButton;
				if(DoButton_Menu(&s_DefaultButton, Localize("Reset to dumbos to defaults"), 0, &ResetButton))
				{
					PopupConfirm(Localize("Reset Dumbos"), Localize("Are you sure that you want to reset the Dumbo things to Default?"),
						Localize("Reset"), Localize("Cancel"), &CMenus::ResetSettingsCustomization);
				}
			}
		}

		// Player Settings

		{
			PlayerSettings.HSplitTop(Margin, nullptr, &PlayerSettings);
			PlayerSettings.HSplitTop(235.0f, &PlayerSettings, &HookSettings);
			if(s_ScrollRegion.AddRect(PlayerSettings))
			{
				PlayerSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				PlayerSettings.VMargin(10.0f, &PlayerSettings);

				PlayerSettings.HSplitTop(HeaderHeight, &Button, &PlayerSettings);
				Ui()->DoLabel(&Button, Localize("Player Settings"), FontSize, TEXTALIGN_MC);

				PlayerSettings.HSplitTop(2 * LineSize, &Button, &PlayerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClTeeSize, &g_Config.m_ClTeeSize, &Button, Localize("Tee Size"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				PlayerSettings.HSplitTop(2 * LineSize, &Button, &PlayerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClTeeWalkRuntime, &g_Config.m_ClTeeWalkRuntime, &Button, Localize("Tee Walk/Run -Time?"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				PlayerSettings.HSplitTop(2 * LineSize, &Button, &PlayerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClTeeFeetInAir, &g_Config.m_ClTeeFeetInAir, &Button, Localize("In Air Feet"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				PlayerSettings.HSplitTop(2 * LineSize, &Button, &PlayerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClTeeSitting, &g_Config.m_ClTeeSitting, &Button, Localize("Tee While Sitting"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				PlayerSettings.HSplitTop(2 * LineSize, &Button, &PlayerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClTeeFeetWalking, &g_Config.m_ClTeeFeetWalking, &Button, Localize("Tee While Walking"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
			}
		}

		// Hook Settings

		{
			HookSettings.HSplitTop(Margin, nullptr, &HookSettings);
			HookSettings.HSplitTop(115.0f, &HookSettings, 0);
			if(s_ScrollRegion.AddRect(HookSettings))
			{
				HookSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				;
				HookSettings.VMargin(Margin, &HookSettings);

				HookSettings.HSplitTop(HeaderHeight, &Button, &HookSettings);
				Ui()->DoLabel(&Button, Localize("Hook Settings"), FontSize, TEXTALIGN_MC);

				HookSettings.HSplitTop(2 * LineSize, &Button, &HookSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHookSizeX, &g_Config.m_ClHookSizeX, &Button, Localize("Hook Size X"), -2, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				HookSettings.HSplitTop(2 * LineSize, &Button, &HookSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHookSizeY, &g_Config.m_ClHookSizeY, &Button, Localize("Hook Size Y"), -2, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
			}
		}

		s_ScrollRegion.End();
	}


// Page 4

	if(s_CurTab == AIODOB_TAB_PAGE4)
	{
		static CScrollRegion s_ScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = 120.0f;
		s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
		MainView.y += ScrollOffset.y;

		const float FontSize = 14.0f;
		const float Margin = 10.0f;
		const float HeaderHeight = FontSize + 5.0f + Margin;

		// left side in settings menu

		CUIRect ReloadSettings, DoubleJumpSettings, SnofflakeSettings, SpriteReset, FreezeBarSettings, HookSettings, HammerSettings, GunSettings, GrenadeSettings, LaserSettings, ShotgunSettings, NinjaSettings;
		MainView.VSplitMid(&ReloadSettings, &SpriteReset);

		// Reload Settings

		{
			ReloadSettings.VMargin(5.0f, &ReloadSettings);
			ReloadSettings.HSplitTop(90.0f, &ReloadSettings, &DoubleJumpSettings);
			if(s_ScrollRegion.AddRect(ReloadSettings))
			{
				ReloadSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ReloadSettings.VMargin(Margin, &ReloadSettings);

				ReloadSettings.HSplitTop(HeaderHeight, &Button, &ReloadSettings);
				Ui()->DoLabel(&Button, Localize("Reload Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClAutoReloadSprite, Localize("Auto Reload Sprites (might crash on older machines)"), &g_Config.m_ClAutoReloadSprite, &ReloadSettings, LineSize);
				ReloadSettings.Margin(10.0f, &ReloadSettings);
				static CButtonContainer s_ReloadButtom;
				if(DoButton_Menu(&s_ReloadButtom, Localize("Reload Sprites"), 0, &ReloadSettings) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
				{
					GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
				}
			}
		}

		{
			DoubleJumpSettings.HSplitTop(Margin, nullptr, &DoubleJumpSettings);
			DoubleJumpSettings.HSplitTop(315.0f, &DoubleJumpSettings, &SnofflakeSettings);
			if(s_ScrollRegion.AddRect(DoubleJumpSettings))
			{
				DoubleJumpSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				DoubleJumpSettings.VMargin(10.0f, &DoubleJumpSettings);

				DoubleJumpSettings.HSplitTop(HeaderHeight, &Button, &DoubleJumpSettings);
				Ui()->DoLabel(&Button, Localize("Double Jump Settings"), FontSize, TEXTALIGN_MC);

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjSprite, &g_Config.m_ClDjSprite, &Button, Localize("What sprite DJ uses"), -8, 19, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjPosY, &g_Config.m_ClDjPosY, &Button, Localize("Dj Y spawn area"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjPosX, &g_Config.m_ClDjPosX, &Button, Localize("Dj X spawn area"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjGravity, &g_Config.m_ClDjGravity, &Button, Localize("Dj Gravity"), -1500, 1500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjLifespan, &g_Config.m_ClDjLifespan, &Button, Localize("Dj Sprite Lifetime"), 0, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjSize, &g_Config.m_ClDjSize, &Button, Localize("Dj Sprite Size"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				DoubleJumpSettings.HSplitTop(2 * LineSize, &Button, &DoubleJumpSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClDjRotSpeed, &g_Config.m_ClDjRotSpeed, &Button, Localize("Dj Sprite Rotation Speed"), -100, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
			}
		}

		// Snofflake Settings

		{
			SnofflakeSettings.HSplitTop(Margin, nullptr, &SnofflakeSettings);
			SnofflakeSettings.HSplitTop(235.0f, &SnofflakeSettings, &HookSettings);
			if(s_ScrollRegion.AddRect(SnofflakeSettings))
			{
				SnofflakeSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				SnofflakeSettings.VMargin(10.0f, &SnofflakeSettings);

				SnofflakeSettings.HSplitTop(HeaderHeight, &Button, &SnofflakeSettings);
				Ui()->DoLabel(&Button, Localize("Snow Flake Settings"), FontSize, TEXTALIGN_MC);

				DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSnowflakeCollision, Localize("Snowflake Collision with Tiles"), &g_Config.m_ClSnowflakeCollision, &SnofflakeSettings, LineSize);

				SnofflakeSettings.HSplitTop(2 * LineSize, &Button, &SnofflakeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClSnowflakeLifeSpan, &g_Config.m_ClSnowflakeLifeSpan, &Button, Localize("Snowflake Lifetime"), -2, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				SnofflakeSettings.HSplitTop(2 * LineSize, &Button, &SnofflakeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClSnowflakeGravity, &g_Config.m_ClSnowflakeGravity, &Button, Localize("Snowflake Gravity"), -750, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				// not well working code
				// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSnowflakeCollision, Localize("Random Snowflake Size"), &g_Config.m_ClSnowflakeCollision, &SnofflakeSettings, LineSize);
				// SnofflakeSettings.HSplitTop(2 * LineSize, &Button, &SnofflakeSettings);
				// Ui()->DoScrollbarOption(&g_Config.m_ClSnowflakeSize, &g_Config.m_ClSnowflakeSize, &Button, Localize("Lifetime of Snowflake"), -2, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				SnofflakeSettings.HSplitTop(2 * LineSize, &Button, &SnofflakeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClSnowflakeY, &g_Config.m_ClSnowflakeY, &Button, Localize("Snowflake Y spawn area"), 0, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				SnofflakeSettings.HSplitTop(2 * LineSize, &Button, &SnofflakeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClSnowflakeX, &g_Config.m_ClSnowflakeX, &Button, Localize("Snowflake X spawn area"), 0, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
			}
		}

		{
			HookSettings.HSplitTop(Margin, nullptr, &HookSettings);
			HookSettings.HSplitTop(115.0f, &HookSettings, &NinjaSettings);
			if(s_ScrollRegion.AddRect(HookSettings))
			{
				HookSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				HookSettings.VMargin(Margin, &HookSettings);

				HookSettings.HSplitTop(HeaderHeight, &Button, &HookSettings);
				Ui()->DoLabel(&Button, Localize("Hook Settings"), FontSize, TEXTALIGN_MC);

				HookSettings.HSplitTop(2 * LineSize, &Button, &HookSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHookHeadSprite, &g_Config.m_ClHookHeadSprite, &Button, Localize("What Sprite The Hook Head Uses"), -51, 87, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
				if(g_Config.m_ClHookHeadSprite == -3)
				{
					g_Config.m_ClHookHeadSprite = -4;
				}
				if(g_Config.m_ClHookHeadSprite == -7)
				{
					g_Config.m_ClHookHeadSprite = -8;
				}

				HookSettings.HSplitTop(2 * LineSize, &Button, &HookSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHookChainSprite, &g_Config.m_ClHookChainSprite, &Button, Localize("What Sprite The Hook Chain Uses"), -50, 88, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
				if(g_Config.m_ClHookChainSprite == -5)
				{
					g_Config.m_ClHookChainSprite = -6;
				}
				if(g_Config.m_ClHookChainSprite == -4)
				{
					g_Config.m_ClHookChainSprite = -6;
				}
				if(g_Config.m_ClHookChainSprite == -7)
				{
					g_Config.m_ClHookChainSprite = -8;
				}
			}
		}

		{
			NinjaSettings.HSplitTop(Margin, nullptr, &NinjaSettings);
			NinjaSettings.HSplitTop(115.0f, &NinjaSettings, 0);
			if(s_ScrollRegion.AddRect(NinjaSettings))
			{
				NinjaSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				NinjaSettings.VMargin(Margin, &NinjaSettings);

				NinjaSettings.HSplitTop(HeaderHeight, &Button, &NinjaSettings);
				Ui()->DoLabel(&Button, Localize("Ninja Settings"), FontSize, TEXTALIGN_MC);

				NinjaSettings.HSplitTop(2 * LineSize, &Button, &NinjaSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClNinjaSprite, &g_Config.m_ClNinjaSprite, &Button, Localize("What Sprite Ninja uses"), -44, 94, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				NinjaSettings.HSplitTop(2 * LineSize, &Button, &NinjaSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClNinjaCursorSprite, &g_Config.m_ClNinjaCursorSprite, &Button, Localize("What Sprite The Ninja Cursor uses"), -45, 93, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}
		// right side in settings menu

		// defaults
		{
			SpriteReset.VMargin(5.0f, &SpriteReset);
			SpriteReset.HSplitTop(40.0f, &SpriteReset, &FreezeBarSettings);
			if(s_ScrollRegion.AddRect(SpriteReset))
			{
				SpriteReset.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 7.5f));
				SpriteReset.Margin(10.0f, &SpriteReset);
				static CButtonContainer s_DefaultButton;
				if(DoButton_Menu(&s_DefaultButton, Localize("Reset to Sprite Settings to defaults"), 0, &SpriteReset))
				{
					PopupConfirm(Localize("Reset Sprite Settings?"), Localize("Are you sure that you want to reset the Sprite Settings to Default?"),
						Localize("Reset"), Localize("Cancel"), &CMenus::ResetSettingsSprites);
				}
			}
		}

		{
			FreezeBarSettings.HSplitTop(Margin, nullptr, &FreezeBarSettings);
			FreezeBarSettings.HSplitTop(195.0f, &FreezeBarSettings, &HammerSettings);
			if(s_ScrollRegion.AddRect(FreezeBarSettings))
			{
				FreezeBarSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				FreezeBarSettings.VMargin(Margin, &FreezeBarSettings);

				FreezeBarSettings.HSplitTop(HeaderHeight, &Button, &FreezeBarSettings);
				Ui()->DoLabel(&Button, Localize("Freeze Bar"), FontSize, TEXTALIGN_MC);

				FreezeBarSettings.HSplitTop(2 * LineSize, &Button, &FreezeBarSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClFreezeBarWidth, &g_Config.m_ClFreezeBarWidth, &Button, Localize("Freeze Bar Width"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				FreezeBarSettings.HSplitTop(2 * LineSize, &Button, &FreezeBarSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClFreezeBarHeight, &g_Config.m_ClFreezeBarHeight, &Button, Localize("Freeze Bar Height"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				FreezeBarSettings.HSplitTop(2 * LineSize, &Button, &FreezeBarSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClFreezeBarX, &g_Config.m_ClFreezeBarX, &Button, Localize("Freeze Bar X Position"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");

				FreezeBarSettings.HSplitTop(2 * LineSize, &Button, &FreezeBarSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClFreezeBarY, &g_Config.m_ClFreezeBarY, &Button, Localize("Freeze Bar Y Position"), -500, 500, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "%");
			}
		}
		{
			HammerSettings.HSplitTop(Margin, nullptr, &HammerSettings);
			HammerSettings.HSplitTop(115.0f, &HammerSettings, &GunSettings);
			if(s_ScrollRegion.AddRect(HammerSettings))
			{
				HammerSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				HammerSettings.VMargin(Margin, &HammerSettings);

				HammerSettings.HSplitTop(HeaderHeight, &Button, &HammerSettings);
				Ui()->DoLabel(&Button, Localize("Hammer Settings"), FontSize, TEXTALIGN_MC);

				HammerSettings.HSplitTop(2 * LineSize, &Button, &HammerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHammerSprite, &g_Config.m_ClHammerSprite, &Button, Localize("What Sprite The Hammer uses"), -41, 97, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				HammerSettings.HSplitTop(2 * LineSize, &Button, &HammerSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClHammerCursorSprite, &g_Config.m_ClHammerCursorSprite, &Button, Localize("What Sprite The Hammer Cursor uses"), -41, 97, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}
		{
			GunSettings.HSplitTop(Margin, nullptr, &GunSettings);
			GunSettings.HSplitTop(155.0f, &GunSettings, &GrenadeSettings);
			if(s_ScrollRegion.AddRect(GunSettings))
			{
				GunSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				GunSettings.VMargin(Margin, &GunSettings);

				GunSettings.HSplitTop(HeaderHeight, &Button, &GunSettings);
				Ui()->DoLabel(&Button, Localize("Gun Settings"), FontSize, TEXTALIGN_MC);

				GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClGunSprite, &g_Config.m_ClGunSprite, &Button, Localize("What Sprite The Gun uses"), -26, 112, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClGunCursorSprite, &g_Config.m_ClGunCursorSprite, &Button, Localize("What Sprite The Gun Cursor uses"), -26, 111, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				GunSettings.HSplitTop(2 * LineSize, &Button, &GunSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClSpriteGunFire, &g_Config.m_ClSpriteGunFire, &Button, Localize("What Sprite The Gun fire is"), -29, 106, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}

		{
			GrenadeSettings.HSplitTop(Margin, nullptr, &GrenadeSettings);
			GrenadeSettings.HSplitTop(115.0f, &GrenadeSettings, &LaserSettings);
			if(s_ScrollRegion.AddRect(GrenadeSettings))
			{
				GrenadeSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				GrenadeSettings.VMargin(Margin, &GrenadeSettings);

				GrenadeSettings.HSplitTop(HeaderHeight, &Button, &GrenadeSettings);
				Ui()->DoLabel(&Button, Localize("Grenade Settings"), FontSize, TEXTALIGN_MC);

				GrenadeSettings.HSplitTop(2 * LineSize, &Button, &GrenadeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClGrenadeSprite, &g_Config.m_ClGrenadeSprite, &Button, Localize("What Sprite The Grenade uses"), -38, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				GrenadeSettings.HSplitTop(2 * LineSize, &Button, &GrenadeSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClGrenadeCursorSprite, &g_Config.m_ClGrenadeCursorSprite, &Button, Localize("What Sprite The Grenade Cursor uses"), -39, 99, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}

		{
			LaserSettings.HSplitTop(Margin, nullptr, &LaserSettings);
			LaserSettings.HSplitTop(115.0f, &LaserSettings, &ShotgunSettings);
			if(s_ScrollRegion.AddRect(LaserSettings))
			{
				LaserSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				LaserSettings.VMargin(Margin, &LaserSettings);

				LaserSettings.HSplitTop(HeaderHeight, &Button, &LaserSettings);
				Ui()->DoLabel(&Button, Localize("Laser Settings"), FontSize, TEXTALIGN_MC);

				LaserSettings.HSplitTop(2 * LineSize, &Button, &LaserSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClLaserSprite, &g_Config.m_ClLaserSprite, &Button, Localize("What Sprite The Laser uses"), -47, 91, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				LaserSettings.HSplitTop(2 * LineSize, &Button, &LaserSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClLaserCursorSprite, &g_Config.m_ClLaserCursorSprite, &Button, Localize("What Sprite The Laser Cursor uses"), -48, 90, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}

		{
			ShotgunSettings.HSplitTop(Margin, nullptr, &ShotgunSettings);
			ShotgunSettings.HSplitTop(115.0f, &ShotgunSettings, 0);
			if(s_ScrollRegion.AddRect(ShotgunSettings))
			{
				ShotgunSettings.Draw(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_AiodobColor, true)), IGraphics::CORNER_ALL, (g_Config.m_ClCornerRoundness / 5.0f));
				ShotgunSettings.VMargin(Margin, &ShotgunSettings);

				ShotgunSettings.HSplitTop(HeaderHeight, &Button, &ShotgunSettings);
				Ui()->DoLabel(&Button, Localize("Shotgun Settings"), FontSize, TEXTALIGN_MC);

				ShotgunSettings.HSplitTop(2 * LineSize, &Button, &ShotgunSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClShotgunSprite, &g_Config.m_ClShotgunSprite, &Button, Localize("What Sprite The Shotgun uses"), -32, 106, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");

				ShotgunSettings.HSplitTop(2 * LineSize, &Button, &ShotgunSettings);
				Ui()->DoScrollbarOption(&g_Config.m_ClShotgunCursorSprite, &g_Config.m_ClShotgunCursorSprite, &Button, Localize("What Sprite The Shotgun Cursor uses"), -33, 105, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, "");
			}
		}
		if(g_Config.m_ClAutoReloadSprite == 1)
		{
			GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
		}
		s_ScrollRegion.End();
	}

}

void CMenus::ResetSettingsSprites()
{
	g_Config.m_ClSnowflakeGravity = 0;
	g_Config.m_ClSnowflakeLifeSpan = 0;
	g_Config.m_ClSnowflakeCollision = 0;
	g_Config.m_ClSnowflakeX = 0;
	g_Config.m_ClSnowflakeY = 0;

	g_Config.m_ClDjSize = 0;
	g_Config.m_ClDjGravity = 0;
	g_Config.m_ClDjLifespan = 0;
	g_Config.m_ClDjRotSpeed = 0;
	g_Config.m_ClDjSprite = 0;
	g_Config.m_ClDjPosX = 0;
	g_Config.m_ClDjPosY = 0;

	g_Config.m_ClFreezeBarWidth = 0;
	g_Config.m_ClFreezeBarHeight = 0;
	g_Config.m_ClFreezeBarX = 0;
	g_Config.m_ClFreezeBarY = 0;

	g_Config.m_ClGunSprite = 0;
	g_Config.m_ClSpriteGunFire = 0;
	g_Config.m_ClNoSpriteGunFire = 0;

	g_Config.m_ClHammerSprite = 0;
	g_Config.m_ClShotgunSprite = 0;
	g_Config.m_ClLaserSprite = 0;
	g_Config.m_ClGrenadeSprite = 0;
	g_Config.m_ClNinjaSprite = 0;

	g_Config.m_ClGunCursorSprite = 0;
	g_Config.m_ClHammerCursorSprite = 0;
	g_Config.m_ClShotgunCursorSprite = 0;
	g_Config.m_ClLaserCursorSprite = 0;
	g_Config.m_ClGrenadeCursorSprite = 0;
	g_Config.m_ClNinjaCursorSprite = 0;

	g_Config.m_ClHookChainSprite = 0;
	g_Config.m_ClHookHeadSprite = 0;

	GameClient()->LoadGameSkin(g_Config.m_ClAssetGame);
}

void CMenus::ResetSettingsCustomization()
{
	g_Config.m_ClTeeSize = 0;
	g_Config.m_ClTeeWalkRuntime = 0;
	g_Config.m_ClTeeFeetInAir = 0;
	g_Config.m_ClTeeSitting = 0;
	g_Config.m_ClTeeFeetWalking = 0;

	g_Config.m_ClHookSizeX = 0;
	g_Config.m_ClHookSizeY = 0;

	g_Config.m_ClHammerDir = 0;
	g_Config.m_ClHammerRot = 0;

	g_Config.m_ClWeaponSize = 0;
	g_Config.m_ClWeaponRot = 0;
	g_Config.m_ClWeaponRotSize2 = 0;
	g_Config.m_ClWeaponRotSize3 = 0;

	g_Config.m_ClGunPosSitting = 0;
	g_Config.m_ClGunPos = 0;
	g_Config.m_ClGunReload = 0;
	g_Config.m_ClGunRecoil = 0;
	g_Config.m_ClBulletSize = 0;
	g_Config.m_ClBulletLifetime = 0;


	g_Config.m_ClFreezeBarWidth = 0;
	g_Config.m_ClFreezeBarHeight = 0;
	g_Config.m_ClFreezeBarX = 0;
	g_Config.m_ClFreezeBarY = 0;
}



void CMenus::RenderSettingsProfiles(CUIRect MainView)
{
	CUIRect Label, LabelMid, Section, LabelRight;
	static int SelectedProfile = -1;

	const float LineMargin = 22.0f;
	char *pSkinName = g_Config.m_ClPlayerSkin;
	int *pUseCustomColor = &g_Config.m_ClPlayerUseCustomColor;
	unsigned *pColorBody = &g_Config.m_ClPlayerColorBody;
	unsigned *pColorFeet = &g_Config.m_ClPlayerColorFeet;
	int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;

	if(m_Dummy)
	{
		pSkinName = g_Config.m_ClDummySkin;
		pUseCustomColor = &g_Config.m_ClDummyUseCustomColor;
		pColorBody = &g_Config.m_ClDummyColorBody;
		pColorFeet = &g_Config.m_ClDummyColorFeet;
	}

	// skin info
	CTeeRenderInfo OwnSkinInfo;
	const CSkin *pSkin = m_pClient->m_Skins.Find(pSkinName);
	OwnSkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	OwnSkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	OwnSkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	OwnSkinInfo.m_CustomColoredSkin = *pUseCustomColor;
	if(*pUseCustomColor)
	{
		OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(*pColorBody).UnclampLighting(ColorHSLA::DARKEST_LGT));
		OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(*pColorFeet).UnclampLighting(ColorHSLA::DARKEST_LGT));
	}
	else
	{
		OwnSkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		OwnSkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	OwnSkinInfo.m_Size = 50.0f;

	//======YOUR PROFILE======
	MainView.HSplitTop(10.0f, &Label, &MainView);
	char aTempBuf[256];
	str_format(aTempBuf, sizeof(aTempBuf), "%s:", Localize("Your profile"));
	Ui()->DoLabel(&Label, aTempBuf, 14.0f, TEXTALIGN_LEFT);

	MainView.HSplitTop(50.0f, &Label, &MainView);
	Label.VSplitLeft(250.0f, &Label, &LabelMid);
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
	vec2 TeeRenderPos(Label.x + 20.0f, Label.y + Label.h / 2.0f + OffsetToMid.y);
	int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, Emote, vec2(1, 0), TeeRenderPos);

	char aName[64];
	char aClan[64];
	str_format(aName, sizeof(aName), ("%s"), m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
	str_format(aClan, sizeof(aClan), ("%s"), m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);

	CUIRect FlagRect;
	Label.VSplitLeft(90.0, &FlagRect, &Label);
	Label.HMargin(-5.0f, &Label);
	Label.HSplitTop(25.0f, &Section, &Label);

	str_format(aTempBuf, sizeof(aTempBuf), ("Name: %s"), aName);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	Label.HSplitTop(20.0f, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), ("Clan: %s"), aClan);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	Label.HSplitTop(20.0f, &Section, &Label);
	str_format(aTempBuf, sizeof(aTempBuf), ("Skin: %s"), pSkinName);
	Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

	FlagRect.VSplitRight(50, 0, &FlagRect);
	FlagRect.HSplitBottom(25, 0, &FlagRect);
	FlagRect.y -= 10.0f;
	ColorRGBA Color(1.0f, 1.0f, 1.0f, 1.0f);
	m_pClient->m_CountryFlags.Render(m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

	bool doSkin = g_Config.m_ClApplyProfileSkin;
	bool doColors = g_Config.m_ClApplyProfileColors;
	bool doEmote = g_Config.m_ClApplyProfileEmote;
	bool doName = g_Config.m_ClApplyProfileName;
	bool doClan = g_Config.m_ClApplyProfileClan;
	bool doFlag = g_Config.m_ClApplyProfileFlag;

	//======AFTER LOAD======
	if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
	{
		CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile];
		MainView.HSplitTop(20.0f, 0, &MainView);
		MainView.HSplitTop(10.0f, &Label, &MainView);
		str_format(aTempBuf, sizeof(aTempBuf), "%s:", ("After Load"));
		Ui()->DoLabel(&Label, aTempBuf, 14.0f, TEXTALIGN_LEFT);

		MainView.HSplitTop(50.0f, &Label, &MainView);
		Label.VSplitLeft(250.0f, &Label, 0);

		if(doSkin && strlen(LoadProfile.SkinName) != 0)
		{
			const CSkin *pLoadSkin = m_pClient->m_Skins.Find(LoadProfile.SkinName);
			OwnSkinInfo.m_OriginalRenderSkin = pLoadSkin->m_OriginalSkin;
			OwnSkinInfo.m_ColorableRenderSkin = pLoadSkin->m_ColorableSkin;
			OwnSkinInfo.m_SkinMetrics = pLoadSkin->m_Metrics;
		}
		if(*pUseCustomColor && doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
		{
			OwnSkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			OwnSkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(LoadProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		}

		RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &OwnSkinInfo, OffsetToMid);
		TeeRenderPos = vec2(Label.x + 20.0f, Label.y + Label.h / 2.0f + OffsetToMid.y);
		int LoadEmote = Emote;
		if(doEmote && LoadProfile.Emote != -1)
			LoadEmote = LoadProfile.Emote;
		RenderTools()->RenderTee(pIdleState, &OwnSkinInfo, LoadEmote, vec2(1, 0), TeeRenderPos);

		if(doName && strlen(LoadProfile.Name) != 0)
			str_format(aName, sizeof(aName), ("%s"), LoadProfile.Name);
		if(doClan && strlen(LoadProfile.Clan) != 0)
			str_format(aClan, sizeof(aClan), ("%s"), LoadProfile.Clan);

		Label.VSplitLeft(90.0, &FlagRect, &Label);
		Label.HMargin(-5.0f, &Label);
		Label.HSplitTop(25.0f, &Section, &Label);

		str_format(aTempBuf, sizeof(aTempBuf), ("Name: %s"), aName);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		Label.HSplitTop(20.0f, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), ("Clan: %s"), aClan);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		Label.HSplitTop(20.0f, &Section, &Label);
		str_format(aTempBuf, sizeof(aTempBuf), ("Skin: %s"), (doSkin && strlen(LoadProfile.SkinName) != 0) ? LoadProfile.SkinName : pSkinName);
		Ui()->DoLabel(&Section, aTempBuf, 15.0f, TEXTALIGN_LEFT);

		FlagRect.VSplitRight(50, 0, &FlagRect);
		FlagRect.HSplitBottom(25, 0, &FlagRect);
		FlagRect.y -= 10.0f;
		int RenderFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
		if(doFlag && LoadProfile.CountryFlag != -2)
			RenderFlag = LoadProfile.CountryFlag;
		m_pClient->m_CountryFlags.Render(RenderFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		str_format(aName, sizeof(aName), ("%s"), m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName);
		str_format(aClan, sizeof(aClan), ("%s"), m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan);
	}
	else
	{
		MainView.HSplitTop(80.0f, 0, &MainView);
	}

	//===BUTTONS AND CHECK BOX===
	CUIRect DummyCheck, CustomCheck;
	MainView.HSplitTop(30, &DummyCheck, 0);
	DummyCheck.HSplitTop(13, 0, &DummyCheck);

	DummyCheck.VSplitLeft(100, &DummyCheck, &CustomCheck);
	CustomCheck.VSplitLeft(150, &CustomCheck, 0);

	DoButton_CheckBoxAutoVMarginAndSet(&m_Dummy, Localize("Dummy"), (int *)&m_Dummy, &DummyCheck, LineMargin);

	static int s_CustomColorID = 0;
	CustomCheck.HSplitTop(LineMargin, &CustomCheck, 0);

	if(DoButton_CheckBox(&s_CustomColorID, Localize("Custom colors"), *pUseCustomColor, &CustomCheck))
	{
		*pUseCustomColor = *pUseCustomColor ? 0 : 1;
		SetNeedSendInfo();
	}

	LabelMid.VSplitLeft(20.0f, 0, &LabelMid);
	LabelMid.VSplitLeft(160.0f, &LabelMid, &LabelRight);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileSkin, ("Save/Load Skin"), &g_Config.m_ClApplyProfileSkin, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileColors, ("Save/Load Colors"), &g_Config.m_ClApplyProfileColors, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileEmote, ("Save/Load Emote"), &g_Config.m_ClApplyProfileEmote, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileName, ("Save/Load Name"), &g_Config.m_ClApplyProfileName, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileClan, ("Save/Load Clan"), &g_Config.m_ClApplyProfileClan, &LabelMid, LineMargin);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClApplyProfileFlag, ("Save/Load Flag"), &g_Config.m_ClApplyProfileFlag, &LabelMid, LineMargin);

	CUIRect Button;
	LabelRight.VSplitLeft(150.0f, &LabelRight, 0);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_LoadButton;

	if(DoButton_Menu(&s_LoadButton, Localize("Load"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
	{
		if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
		{
			CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile];
			if(!m_Dummy)
			{
				if(doSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClPlayerSkin, LoadProfile.SkinName, sizeof(g_Config.m_ClPlayerSkin));
				if(doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClPlayerColorBody = LoadProfile.BodyColor;
					g_Config.m_ClPlayerColorFeet = LoadProfile.FeetColor;
				}
				if(doEmote && LoadProfile.Emote != -1)
					g_Config.m_ClPlayerDefaultEyes = LoadProfile.Emote;
				if(doName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_PlayerName, LoadProfile.Name, sizeof(g_Config.m_PlayerName));
				if(doClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_PlayerClan, LoadProfile.Clan, sizeof(g_Config.m_PlayerClan));
				if(doFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_PlayerCountry = LoadProfile.CountryFlag;
			}
			else
			{
				if(doSkin && strlen(LoadProfile.SkinName) != 0)
					str_copy(g_Config.m_ClDummySkin, LoadProfile.SkinName, sizeof(g_Config.m_ClDummySkin));
				if(doColors && LoadProfile.BodyColor != -1 && LoadProfile.FeetColor != -1)
				{
					g_Config.m_ClDummyColorBody = LoadProfile.BodyColor;
					g_Config.m_ClDummyColorFeet = LoadProfile.FeetColor;
				}
				if(doEmote && LoadProfile.Emote != -1)
					g_Config.m_ClDummyDefaultEyes = LoadProfile.Emote;
				if(doName && strlen(LoadProfile.Name) != 0)
					str_copy(g_Config.m_ClDummyName, LoadProfile.Name, sizeof(g_Config.m_ClDummyName));
				if(doClan && strlen(LoadProfile.Clan) != 0)
					str_copy(g_Config.m_ClDummyClan, LoadProfile.Clan, sizeof(g_Config.m_ClDummyClan));
				if(doFlag && LoadProfile.CountryFlag != -2)
					g_Config.m_ClDummyCountry = LoadProfile.CountryFlag;
			}
		}
		SetNeedSendInfo();
	}
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	LabelRight.HSplitTop(30.0f, &Button, &LabelRight);
	static CButtonContainer s_SaveButton;
	if(DoButton_Menu(&s_SaveButton, Localize("Save"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
	{
		GameClient()->m_SkinProfiles.AddProfile(
			doColors ? *pColorBody : -1,
			doColors ? *pColorFeet : -1,
			doFlag ? CurrentFlag : -2,
			doEmote ? Emote : -1,
			doSkin ? pSkinName : "",
			doName ? aName : "",
			doClan ? aClan : "");
		GameClient()->m_SkinProfiles.SaveProfiles();
	}
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	static int s_AllowDelete;
	DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, ("Enable Deleting"), &s_AllowDelete, &LabelRight, LineMargin);
	LabelRight.HSplitTop(5.0f, 0, &LabelRight);

	if(s_AllowDelete)
	{
		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
		{
			if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + SelectedProfile);
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
		LabelRight.HSplitTop(5.0f, 0, &LabelRight);

		LabelRight.HSplitTop(28.0f, &Button, &LabelRight);
		static CButtonContainer s_OverrideButton;
		if(DoButton_Menu(&s_OverrideButton, Localize("Override"), 0, &Button, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, vec4(0.0f, 0.0f, 0.0f, 0.5f)))
		{
			if(SelectedProfile != -1 && SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				GameClient()->m_SkinProfiles.m_Profiles[SelectedProfile] = CProfile(
					doColors ? *pColorBody : -1,
					doColors ? *pColorFeet : -1,
					doFlag ? CurrentFlag : -2,
					doEmote ? Emote : -1,
					doSkin ? pSkinName : "",
					doName ? aName : "",
					doClan ? aClan : "");
				GameClient()->m_SkinProfiles.SaveProfiles();
			}
		}
	}

	//---RENDER THE SELECTOR---
	CUIRect SelectorRect;
	MainView.HSplitTop(50, 0, &SelectorRect);
	SelectorRect.HSplitBottom(15.0, &SelectorRect, 0);
	std::vector<CProfile> *pProfileList = &GameClient()->m_SkinProfiles.m_Profiles;

	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, pProfileList->size(), 4, 3, SelectedProfile, &SelectorRect, true);

	static bool s_Indexs[1024];

	for(size_t i = 0; i < pProfileList->size(); ++i)
	{
		CProfile CurrentProfile = GameClient()->m_SkinProfiles.m_Profiles[i];

		char RenderSkin[24];
		if(strlen(CurrentProfile.SkinName) == 0)
			str_copy(RenderSkin, pSkinName, sizeof(RenderSkin));
		else
			str_copy(RenderSkin, CurrentProfile.SkinName, sizeof(RenderSkin));

		const CSkin *pSkinToBeDraw = m_pClient->m_Skins.Find(RenderSkin);

		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexs[i], SelectedProfile >= 0 && (size_t)SelectedProfile == i);

		if(!Item.m_Visible)
			continue;

		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			Info.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
			Info.m_CustomColoredSkin = 1;
			Info.m_OriginalRenderSkin = pSkinToBeDraw->m_OriginalSkin;
			Info.m_ColorableRenderSkin = pSkinToBeDraw->m_ColorableSkin;
			Info.m_SkinMetrics = pSkinToBeDraw->m_Metrics;
			Info.m_Size = 50.0f;
			if(CurrentProfile.BodyColor == -1 && CurrentProfile.FeetColor == -1)
			{
				Info.m_CustomColoredSkin = m_Dummy ? g_Config.m_ClDummyUseCustomColor : g_Config.m_ClPlayerUseCustomColor;
				Info.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
				Info.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
			}

			RenderTools()->GetRenderTeeOffsetToRenderedTee(pIdleState, &Info, OffsetToMid);

			int RenderEmote = CurrentProfile.Emote == -1 ? Emote : CurrentProfile.Emote;
			TeeRenderPos = vec2(Item.m_Rect.x + 30, Item.m_Rect.y + Item.m_Rect.h / 2 + OffsetToMid.y);

			Item.m_Rect.VSplitLeft(60.0f, 0, &Item.m_Rect);
			CUIRect PlayerRect, ClanRect, FeetColorSquare, BodyColorSquare;

			Item.m_Rect.VSplitLeft(60.0f, 0, &BodyColorSquare); // Delete this maybe

			Item.m_Rect.VSplitRight(60.0, &BodyColorSquare, &FlagRect);
			BodyColorSquare.x -= 11.0;
			BodyColorSquare.VSplitLeft(10, &BodyColorSquare, 0);
			BodyColorSquare.HSplitMid(&BodyColorSquare, &FeetColorSquare);
			BodyColorSquare.HSplitMid(0, &BodyColorSquare);
			FeetColorSquare.HSplitMid(&FeetColorSquare, 0);
			FlagRect.HSplitBottom(10.0, &FlagRect, 0);
			FlagRect.HSplitTop(10.0, 0, &FlagRect);

			Item.m_Rect.HSplitMid(&PlayerRect, &ClanRect);

			SLabelProperties Props;
			Props.m_MaxWidth = Item.m_Rect.w;
			if(CurrentProfile.CountryFlag != -2)
				m_pClient->m_CountryFlags.Render(CurrentProfile.CountryFlag, Color, FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

			if(CurrentProfile.BodyColor != -1 && CurrentProfile.FeetColor != -1)
			{
				ColorRGBA BodyColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
				ColorRGBA FeetColor = color_cast<ColorRGBA>(ColorHSLA(CurrentProfile.FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));

				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BodyColor.r, BodyColor.g, BodyColor.b, 1.0f);
				IGraphics::CQuadItem Quads[2];
				Quads[0] = IGraphics::CQuadItem(BodyColorSquare.x, BodyColorSquare.y, BodyColorSquare.w, BodyColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[0], 1);
				Graphics()->SetColor(FeetColor.r, FeetColor.g, FeetColor.b, 1.0f);
				Quads[1] = IGraphics::CQuadItem(FeetColorSquare.x, FeetColorSquare.y, FeetColorSquare.w, FeetColorSquare.h);
				Graphics()->QuadsDrawTL(&Quads[1], 1);
				Graphics()->QuadsEnd();
			}
			RenderTools()->RenderTee(pIdleState, &Info, RenderEmote, vec2(1.0f, 0.0f), TeeRenderPos);

			if(strlen(CurrentProfile.Name) == 0 && strlen(CurrentProfile.Clan) == 0)
			{
				PlayerRect = Item.m_Rect;
				Ui()->DoLabel(&PlayerRect, CurrentProfile.SkinName, 12.0f, TEXTALIGN_LEFT, Props);
			}
			else
			{
				Ui()->DoLabel(&PlayerRect, CurrentProfile.Name, 12.0f, TEXTALIGN_LEFT, Props);
				Item.m_Rect.HSplitTop(20.0f, 0, &Item.m_Rect);
				Props.m_MaxWidth = Item.m_Rect.w;
				Ui()->DoLabel(&ClanRect, CurrentProfile.Clan, 12.0f, TEXTALIGN_LEFT, Props);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(SelectedProfile != NewSelected)
	{
		SelectedProfile = NewSelected;
	}
	static CButtonContainer s_ProfilesFile;
	CUIRect FileButton;
	MainView.HSplitBottom(25.0, 0, &FileButton);
	FileButton.y += 15.0;
	FileButton.VSplitLeft(130.0, &FileButton, 0);
	if(DoButton_Menu(&s_ProfilesFile, Localize("Profiles file"), 0, &FileButton))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, PROFILES_FILE, aTempBuf, sizeof(aTempBuf));
		if(!open_file(aTempBuf))
		{
			dbg_msg("menus", "couldn't open file");
		}
	}
}