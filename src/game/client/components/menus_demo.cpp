/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/hash.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/console.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/generated/client_data.h>
#include <game/localization.h>

#include "maplayers.h"
#include "menus.h"

#include <chrono>

using namespace std::chrono_literals;

int CMenus::DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	pRect->Draw(ColorRGBA(1, 1, 1, (Checked ? 0.10f : 0.5f) * UI()->ButtonColorMul(pID)), IGraphics::CORNER_ALL, 5.0f);
	UI()->DoLabel(pRect, pText, 14.0f, TEXTALIGN_CENTER);
	return UI()->DoButtonLogic(pID, Checked, pRect);
}

int CMenus::DoButton_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, bool Enabled)
{
	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, (Checked ? 0.10f : 0.5f) * UI()->ButtonColorMul(pButtonContainer)), Corners, 5.0f);

	TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	CUIRect Rect = *pRect;
	CUIRect Temp;
	Rect.HMargin(2.0f, &Temp);
	SLabelProperties Props;
	UI()->DoLabel(&Temp, pText, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);

	if(!Enabled)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
		UI()->DoLabel(&Temp, "\xEF\x9C\x95", Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_CENTER, Props);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	TextRender()->SetCurFont(nullptr);

	return UI()->DoButtonLogic(pButtonContainer, Checked, pRect);
}

bool CMenus::DemoFilterChat(const void *pData, int Size, void *pUser)
{
	bool DoFilterChat = *(bool *)pUser;
	if(!DoFilterChat)
	{
		return false;
	}

	CUnpacker Unpacker;
	Unpacker.Reset(pData, Size);

	int Msg = Unpacker.GetInt();
	int Sys = Msg & 1;
	Msg >>= 1;

	return !Unpacker.Error() && !Sys && Msg == NETMSGTYPE_SV_CHAT;
}

void CMenus::HandleDemoSeeking(float PositionToSeek, float TimeToSeek)
{
	if((PositionToSeek >= 0.0f && PositionToSeek <= 1.0f) || TimeToSeek != 0.0f)
	{
		m_pClient->m_Chat.Reset();
		m_pClient->m_KillMessages.OnReset();
		m_pClient->m_Particles.OnReset();
		m_pClient->m_Sounds.OnReset();
		m_pClient->m_Scoreboard.OnReset();
		m_pClient->m_Statboard.OnReset();
		m_pClient->m_SuppressEvents = true;
		if(TimeToSeek != 0.0f)
			DemoPlayer()->SeekTime(TimeToSeek);
		else
			DemoPlayer()->SeekPercent(PositionToSeek);
		m_pClient->m_SuppressEvents = false;
		m_pClient->m_MapLayersBackGround.EnvelopeUpdate();
		m_pClient->m_MapLayersForeGround.EnvelopeUpdate();
	}
}

void CMenus::DemoSeekTick(IDemoPlayer::ETickOffset TickOffset)
{
	m_pClient->m_SuppressEvents = true;
	DemoPlayer()->SeekTick(TickOffset);
	m_pClient->m_SuppressEvents = false;
	DemoPlayer()->Pause();
	m_pClient->m_MapLayersBackGround.EnvelopeUpdate();
	m_pClient->m_MapLayersForeGround.EnvelopeUpdate();
}

void CMenus::RenderDemoPlayer(CUIRect MainView)
{
	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();

	const float SeekBarHeight = 15.0f;
	const float ButtonbarHeight = 20.0f;
	const float NameBarHeight = 20.0f;
	const float Margins = 5.0f;
	static int64_t s_LastSpeedChange = 0;

	// render popups
	if(m_DemoPlayerState == DEMOPLAYER_SLICE_SAVE)
	{
		CUIRect Screen = *UI()->Screen();
		CUIRect Box, Part, Part2;
		Box = Screen;
		Box.Margin(150.0f, &Box);

		// render the box
		Box.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f, 0, &Box);
		Box.HSplitTop(24.f, &Part, &Box);
		UI()->DoLabel(&Part, Localize("Select a name"), 24.f, TEXTALIGN_CENTER);
		Box.HSplitTop(20.f, 0, &Box);
		Box.HSplitTop(20.f, &Part, &Box);
		Part.VMargin(20.f, &Part);
		UI()->DoLabel(&Part, m_aDemoPlayerPopupHint, 20.f, TEXTALIGN_CENTER);
		Box.HSplitTop(20.f, 0, &Box);

		CUIRect Label, TextBox, Ok, Abort;

		Box.HSplitBottom(20.f, &Box, 0);
		Box.HSplitBottom(24.f, &Box, &Part);
		Part.VMargin(80.0f, &Part);

		Part.VSplitMid(&Abort, &Ok);

		Ok.VMargin(20.0f, &Ok);
		Abort.VMargin(20.0f, &Abort);

		static int s_RemoveChat = 0;

		static CButtonContainer s_ButtonAbort;
		if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE))
			m_DemoPlayerState = DEMOPLAYER_NONE;

		static CButtonContainer s_ButtonOk;
		if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER))
		{
			char aDemoName[IO_MAX_PATH_LENGTH];
			DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
			str_append(aDemoName, ".demo", sizeof(aDemoName));

			if(!str_endswith(m_aCurrentDemoFile, ".demo"))
				str_append(m_aCurrentDemoFile, ".demo", sizeof(m_aCurrentDemoFile));

			if(str_comp(aDemoName, m_aCurrentDemoFile) == 0)
				str_copy(m_aDemoPlayerPopupHint, Localize("Please use a different name"));
			else
			{
				char aPath[IO_MAX_PATH_LENGTH];
				str_format(aPath, sizeof(aPath), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);

				IOHANDLE DemoFile = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
				const char *pStr = Localize("File already exists, do you want to overwrite it?");
				if(DemoFile && str_comp(m_aDemoPlayerPopupHint, pStr) != 0)
				{
					io_close(DemoFile);
					str_copy(m_aDemoPlayerPopupHint, pStr);
				}
				else
				{
					if(DemoFile)
						io_close(DemoFile);
					m_DemoPlayerState = DEMOPLAYER_NONE;
					Client()->DemoSlice(aPath, CMenus::DemoFilterChat, &s_RemoveChat);
					DemolistPopulate();
					DemolistOnUpdate(false);
				}
			}
		}

		Box.HSplitTop(24.f, &Part, &Box);
		Box.HSplitTop(10.f, 0, &Box);
		Box.HSplitTop(24.f, &Part2, &Box);

		Part2.VSplitLeft(60.0f, 0, &Label);
		if(DoButton_CheckBox(&s_RemoveChat, Localize("Remove chat"), s_RemoveChat, &Label))
		{
			s_RemoveChat ^= 1;
		}

		Part.VSplitLeft(60.0f, 0, &Label);
		Label.VSplitLeft(120.0f, 0, &TextBox);
		TextBox.VSplitLeft(20.0f, 0, &TextBox);
		TextBox.VSplitRight(60.0f, &TextBox, 0);
		UI()->DoLabel(&Label, Localize("New name:"), 18.0f, TEXTALIGN_LEFT);
		static float s_Offset = 0.0f;
		if(UI()->DoEditBox(&s_Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &s_Offset))
		{
			m_aDemoPlayerPopupHint[0] = '\0';
		}
	}

	// handle keyboard shortcuts independent of active menu
	float PositionToSeek = -1.0f;
	float TimeToSeek = 0.0f;
	if(m_pClient->m_GameConsole.IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE && g_Config.m_ClDemoKeyboardShortcuts)
	{
		// increase/decrease speed
		if(!Input()->ShiftIsPressed())
		{
			if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP) || Input()->KeyPress(KEY_UP))
			{
				DemoPlayer()->SetSpeedIndex(+1);
				s_LastSpeedChange = time_get();
			}
			else if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) || Input()->KeyPress(KEY_DOWN))
			{
				DemoPlayer()->SetSpeedIndex(-1);
				s_LastSpeedChange = time_get();
			}
		}

		// pause/unpause
		if(Input()->KeyPress(KEY_SPACE) || Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_K))
		{
			if(pInfo->m_Paused)
			{
				DemoPlayer()->Unpause();
			}
			else
			{
				DemoPlayer()->Pause();
			}
		}

		// seek backward/forward 10/5 seconds
		if(Input()->KeyPress(KEY_J))
		{
			TimeToSeek = -10.0f;
		}
		else if(Input()->KeyPress(KEY_L))
		{
			TimeToSeek = 10.0f;
		}
		else if(Input()->KeyPress(KEY_LEFT))
		{
			TimeToSeek = -5.0f;
		}
		else if(Input()->KeyPress(KEY_RIGHT))
		{
			TimeToSeek = 5.0f;
		}

		// seek to 0-90%
		const int aSeekPercentKeys[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
		for(unsigned i = 0; i < std::size(aSeekPercentKeys); i++)
		{
			if(Input()->KeyPress(aSeekPercentKeys[i]))
			{
				PositionToSeek = i * 0.1f;
				break;
			}
		}

		// seek to the beginning/end
		if(Input()->KeyPress(KEY_HOME))
		{
			PositionToSeek = 0.0f;
		}
		else if(Input()->KeyPress(KEY_END))
		{
			PositionToSeek = 1.0f;
		}

		// Advance single frame forward/backward with period/comma key
		const bool TickForwards = Input()->KeyPress(KEY_PERIOD);
		const bool TickBackwards = Input()->KeyPress(KEY_COMMA);
		if(TickForwards || TickBackwards)
		{
			DemoSeekTick(TickForwards ? IDemoPlayer::TICK_NEXT : IDemoPlayer::TICK_PREVIOUS);
		}
	}

	float TotalHeight = SeekBarHeight + ButtonbarHeight + NameBarHeight + Margins * 3;

	// render speed info
	if(g_Config.m_ClDemoShowSpeed && time_get() - s_LastSpeedChange < time_freq() * 1)
	{
		CUIRect Screen = *UI()->Screen();

		char aSpeedBuf[256];
		str_format(aSpeedBuf, sizeof(aSpeedBuf), "×%.2f", pInfo->m_Speed);
		TextRender()->Text(0, 120.0f, Screen.y + Screen.h - 120.0f - TotalHeight, 60.0f, aSpeedBuf, -1.0f);
	}

	const int CurrentTick = pInfo->m_CurrentTick - pInfo->m_FirstTick;
	const int TotalTicks = pInfo->m_LastTick - pInfo->m_FirstTick;

	if(CurrentTick == TotalTicks)
	{
		DemoPlayer()->Pause();
		PositionToSeek = 0.0f;
	}

	if(!m_MenuActive)
	{
		HandleDemoSeeking(PositionToSeek, TimeToSeek);
		return;
	}

	MainView.HSplitBottom(TotalHeight, 0, &MainView);
	MainView.VSplitLeft(50.0f, 0, &MainView);
	MainView.VSplitLeft(600.0f, &MainView, 0);

	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_T, 10.0f);

	MainView.Margin(5.0f, &MainView);

	CUIRect SeekBar, ButtonBar, NameBar, SpeedBar;

	MainView.HSplitTop(SeekBarHeight, &SeekBar, &ButtonBar);
	ButtonBar.HSplitTop(Margins, 0, &ButtonBar);
	ButtonBar.HSplitBottom(NameBarHeight, &ButtonBar, &NameBar);
	NameBar.HSplitTop(4.0f, 0, &NameBar);

	// do seekbar
	{
		static int s_SeekBarID = 0;
		void *pId = &s_SeekBarID;
		char aBuffer[128];

		// draw seek bar
		SeekBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 5.0f);

		// draw filled bar
		float Amount = CurrentTick / (float)TotalTicks;
		CUIRect FilledBar = SeekBar;
		FilledBar.w = 10.0f + (FilledBar.w - 10.0f) * Amount;
		FilledBar.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_ALL, 5.0f);

		// draw highlighting
		if(g_Config.m_ClDemoSliceBegin != -1 && g_Config.m_ClDemoSliceEnd != -1)
		{
			float RatioBegin = (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick) / (float)TotalTicks;
			float RatioEnd = (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick) / (float)TotalTicks;
			float Span = ((SeekBar.w - 10.0f) * RatioEnd) - ((SeekBar.w - 10.0f) * RatioBegin);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.25f);
			IGraphics::CQuadItem QuadItem(10.0f + SeekBar.x + (SeekBar.w - 10.0f) * RatioBegin, SeekBar.y, Span, SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw markers
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			float Ratio = (pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(8.0f + SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw slice markers
		// begin
		if(g_Config.m_ClDemoSliceBegin != -1)
		{
			float Ratio = (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(10.0f + SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// end
		if(g_Config.m_ClDemoSliceEnd != -1)
		{
			float Ratio = (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick) / (float)TotalTicks;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(10.0f + SeekBar.x + (SeekBar.w - 10.0f) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw time
		char aCurrentTime[32];
		str_time((int64_t)CurrentTick / SERVER_TICK_SPEED * 100, TIME_HOURS, aCurrentTime, sizeof(aCurrentTime));
		char aTotalTime[32];
		str_time((int64_t)TotalTicks / SERVER_TICK_SPEED * 100, TIME_HOURS, aTotalTime, sizeof(aTotalTime));
		str_format(aBuffer, sizeof(aBuffer), "%s / %s", aCurrentTime, aTotalTime);
		UI()->DoLabel(&SeekBar, aBuffer, SeekBar.h * 0.70f, TEXTALIGN_CENTER);

		// do the logic
		const bool Inside = UI()->MouseInside(&SeekBar);

		if(UI()->CheckActiveItem(pId))
		{
			if(!UI()->MouseButton(0))
				UI()->SetActiveItem(nullptr);
			else
			{
				static float s_PrevAmount = 0.0f;
				float AmountSeek = (UI()->MouseX() - SeekBar.x) / SeekBar.w;

				if(Input()->ShiftIsPressed())
				{
					AmountSeek = s_PrevAmount + (AmountSeek - s_PrevAmount) * 0.05f;
					if(AmountSeek > 0.0f && AmountSeek < 1.0f && absolute(s_PrevAmount - AmountSeek) >= 0.0001f)
					{
						PositionToSeek = AmountSeek;
					}
				}
				else
				{
					if(AmountSeek > 0.0f && AmountSeek < 1.0f && absolute(s_PrevAmount - AmountSeek) >= 0.001f)
					{
						s_PrevAmount = AmountSeek;
						PositionToSeek = AmountSeek;
					}
				}
			}
		}
		else if(UI()->HotItem() == pId)
		{
			if(UI()->MouseButton(0))
				UI()->SetActiveItem(pId);
		}

		if(Inside)
			UI()->SetHotItem(pId);
	}

	bool IncreaseDemoSpeed = false, DecreaseDemoSpeed = false;

	// do buttons
	CUIRect Button;

	// combined play and pause button
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_PlayPauseButton;
	if(DoButton_FontIcon(&s_PlayPauseButton, pInfo->m_Paused ? "\xEF\x81\x8B" : "\xEF\x81\x8C", false, &Button, IGraphics::CORNER_ALL))
	{
		if(pInfo->m_Paused)
		{
			DemoPlayer()->Unpause();
		}
		else
		{
			DemoPlayer()->Pause();
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_PlayPauseButton, &Button, pInfo->m_Paused ? Localize("Play the current demo") : Localize("Pause the current demo"));

	// stop button
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_ResetButton;
	if(DoButton_FontIcon(&s_ResetButton, "\xEF\x81\x8D", false, &Button, IGraphics::CORNER_ALL))
	{
		DemoPlayer()->Pause();
		PositionToSeek = 0.0f;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_ResetButton, &Button, Localize("Stop the current demo"));

	// one tick back
	ButtonBar.VSplitLeft(Margins + 10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickBackButton;
	if(DoButton_FontIcon(&s_OneTickBackButton, "\xEF\x81\x93", 0, &Button, IGraphics::CORNER_ALL))
		DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickBackButton, &Button, Localize("Go back one tick"));

	// one tick forward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickForwardButton;
	if(DoButton_FontIcon(&s_OneTickForwardButton, "\xEF\x81\x94", 0, &Button, IGraphics::CORNER_ALL))
		DemoSeekTick(IDemoPlayer::TICK_NEXT);
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickForwardButton, &Button, Localize("Go forward one tick"));

	// slowdown
	ButtonBar.VSplitLeft(Margins + 10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SlowDownButton;
	if(DoButton_FontIcon(&s_SlowDownButton, "\xEF\x81\x8A", 0, &Button, IGraphics::CORNER_ALL))
		DecreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_SlowDownButton, &Button, Localize("Slow down the demo"));

	// fastforward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_FastForwardButton;
	if(DoButton_FontIcon(&s_FastForwardButton, "\xEF\x81\x8E", 0, &Button, IGraphics::CORNER_ALL))
		IncreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_FastForwardButton, &Button, Localize("Speed up the demo"));

	// speed meter
	ButtonBar.VSplitLeft(Margins * 12, &SpeedBar, &ButtonBar);
	char aBuffer[64];
	str_format(aBuffer, sizeof(aBuffer), "×%g", pInfo->m_Speed);
	UI()->DoLabel(&SpeedBar, aBuffer, Button.h * 0.7f, TEXTALIGN_CENTER);

	// slice begin button
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceBeginButton;
	if(DoButton_FontIcon(&s_SliceBeginButton, "\xEF\x8B\xB5", 0, &Button, IGraphics::CORNER_ALL))
	{
		Client()->DemoSliceBegin();
		if(CurrentTick > (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick))
			g_Config.m_ClDemoSliceEnd = -1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceBeginButton, &Button, Localize("Mark the beginning of a cut"));

	// slice end button
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceEndButton;
	if(DoButton_FontIcon(&s_SliceEndButton, "\xEF\x8B\xB6", 0, &Button, IGraphics::CORNER_ALL))
	{
		Client()->DemoSliceEnd();
		if(CurrentTick < (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick))
			g_Config.m_ClDemoSliceBegin = -1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceEndButton, &Button, Localize("Mark the end of a cut"));

	// slice save button
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceSaveButton;
	if(DoButton_FontIcon(&s_SliceSaveButton, "\xEF\x82\x8E", 0, &Button, IGraphics::CORNER_ALL))
	{
		DemoPlayer()->GetDemoName(m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile));
		str_append(m_aCurrentDemoFile, ".demo", sizeof(m_aCurrentDemoFile));
		m_aDemoPlayerPopupHint[0] = '\0';
		m_DemoPlayerState = DEMOPLAYER_SLICE_SAVE;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceSaveButton, &Button, Localize("Export cut as a separate demo"));

	// threshold value, accounts for slight inaccuracy when setting demo position
	const int Threshold = 10;

	// one marker back
	ButtonBar.VSplitLeft(Margins + 20.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerBackButton;
	if(DoButton_FontIcon(&s_OneMarkerBackButton, "\xEF\x81\x88", 0, &Button, IGraphics::CORNER_ALL))
		for(int i = pInfo->m_NumTimelineMarkers - 1; i >= 0; i--)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) < CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				PositionToSeek = (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
				break;
			}
			PositionToSeek = 0.0f;
		}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerBackButton, &Button, Localize("Go back one marker"));

	// one marker forward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerForwardButton;
	if(DoButton_FontIcon(&s_OneMarkerForwardButton, "\xEF\x81\x91", 0, &Button, IGraphics::CORNER_ALL))
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) > CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				PositionToSeek = (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
				break;
			}
			PositionToSeek = 1.0f;
		}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerForwardButton, &Button, Localize("Go forward one marker"));

	// close button
	ButtonBar.VSplitRight(ButtonbarHeight * 3, &ButtonBar, &Button);
	static int s_ExitButton = 0;
	if(DoButton_DemoPlayer(&s_ExitButton, Localize("Close"), 0, &Button) || (Input()->KeyPress(KEY_C) && m_pClient->m_GameConsole.IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE))
	{
		Client()->Disconnect();
		DemolistOnUpdate(false);
	}

	// toggle keyboard shortcuts button
	ButtonBar.VSplitRight(Margins, &ButtonBar, 0);
	ButtonBar.VSplitRight(ButtonbarHeight, &ButtonBar, &Button);
	static CButtonContainer s_KeyboardShortcutsButton;
	if(DoButton_FontIcon(&s_KeyboardShortcutsButton, "\xE2\x8C\xA8", 0, &Button, IGraphics::CORNER_ALL, g_Config.m_ClDemoKeyboardShortcuts != 0))
	{
		g_Config.m_ClDemoKeyboardShortcuts ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_KeyboardShortcutsButton, &Button, Localize("Toggle keyboard shortcuts"));

	// demo name
	char aDemoName[IO_MAX_PATH_LENGTH];
	DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
	char aBuf[IO_MAX_PATH_LENGTH + 128];
	str_format(aBuf, sizeof(aBuf), Localize("Demofile: %s"), aDemoName);
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, NameBar.x, NameBar.y + (NameBar.h - (Button.h * 0.5f)) / 2.f, Button.h * 0.5f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = MainView.w;
	TextRender()->TextEx(&Cursor, aBuf, -1);

	if(IncreaseDemoSpeed)
	{
		DemoPlayer()->SetSpeedIndex(+1);
		s_LastSpeedChange = time_get();
	}
	else if(DecreaseDemoSpeed)
	{
		DemoPlayer()->SetSpeedIndex(-1);
		s_LastSpeedChange = time_get();
	}

	HandleDemoSeeking(PositionToSeek, TimeToSeek);
}

int CMenus::DemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	if(str_comp(pInfo->m_pName, ".") == 0 || (str_comp(pInfo->m_pName, "..") == 0 && str_comp(pSelf->m_aCurrentDemoFolder, "demos") == 0) || (!IsDir && !str_endswith(pInfo->m_pName, ".demo")))
	{
		return 0;
	}

	CDemoItem Item;
	str_copy(Item.m_aFilename, pInfo->m_pName);
	if(IsDir)
	{
		str_format(Item.m_aName, sizeof(Item.m_aName), "%s/", pInfo->m_pName);
		Item.m_InfosLoaded = false;
		Item.m_Valid = false;
		Item.m_Date = 0;
	}
	else
	{
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pInfo->m_pName, str_length(pInfo->m_pName) - str_length(".demo"));
		Item.m_InfosLoaded = false;
		Item.m_Date = pInfo->m_TimeModified;
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_StorageType = StorageType;
	pSelf->m_vDemos.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_DemoPopulateStartTime > 500ms)
	{
		pSelf->GameClient()->m_Menus.RenderLoading(Localize("Loading demo files"), "", 0, false);
	}

	return 0;
}

void CMenus::DemolistPopulate()
{
	m_vDemos.clear();
	if(!str_comp(m_aCurrentDemoFolder, "demos"))
		m_DemolistStorageType = IStorage::TYPE_ALL;
	m_DemoPopulateStartTime = time_get_nanoseconds();
	Storage()->ListDirectoryInfo(m_DemolistStorageType, m_aCurrentDemoFolder, DemolistFetchCallback, this);

	if(g_Config.m_BrDemoFetchInfo)
		FetchAllHeaders();

	std::stable_sort(m_vDemos.begin(), m_vDemos.end());
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	if(Reset)
		g_Config.m_UiDemoSelected[0] = '\0';
	else
	{
		bool Found = false;
		int SelectedIndex = -1;
		// search for selected index
		for(auto &Item : m_vDemos)
		{
			SelectedIndex++;

			if(str_comp(g_Config.m_UiDemoSelected, Item.m_aName) == 0)
			{
				Found = true;
				break;
			}
		}

		if(Found)
			m_DemolistSelectedIndex = SelectedIndex;
	}

	m_DemolistSelectedIndex = Reset ? !m_vDemos.empty() ? 0 : -1 :
					  m_DemolistSelectedIndex >= (int)m_vDemos.size() ? m_vDemos.size() - 1 : m_DemolistSelectedIndex;
	m_DemolistSelectedIsDir = m_DemolistSelectedIndex < 0 ? false : m_vDemos[m_DemolistSelectedIndex].m_IsDir;
}

bool CMenus::FetchHeader(CDemoItem &Item)
{
	if(!Item.m_InfosLoaded)
	{
		char aBuffer[IO_MAX_PATH_LENGTH];
		str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_aCurrentDemoFolder, Item.m_aFilename);
		Item.m_Valid = DemoPlayer()->GetDemoInfo(Storage(), aBuffer, Item.m_StorageType, &Item.m_Info, &Item.m_TimelineMarkers, &Item.m_MapInfo);
		Item.m_InfosLoaded = true;
	}
	return Item.m_Valid;
}

void CMenus::FetchAllHeaders()
{
	for(auto &Item : m_vDemos)
	{
		FetchHeader(Item);
	}
	std::stable_sort(m_vDemos.begin(), m_vDemos.end());
}

void CMenus::RenderDemoList(CUIRect MainView)
{
	static int s_Inited = 0;
	if(!s_Inited)
	{
		DemolistPopulate();
		DemolistOnUpdate(true);
		s_Inited = 1;
	}

	char aFooterLabel[128] = {0};
	if(m_DemolistSelectedIndex >= 0)
	{
		CDemoItem &Item = m_vDemos[m_DemolistSelectedIndex];
		if(str_comp(Item.m_aFilename, "..") == 0)
			str_copy(aFooterLabel, Localize("Parent Folder"));
		else if(m_DemolistSelectedIsDir)
			str_copy(aFooterLabel, Localize("Folder"));
		else if(!FetchHeader(Item))
			str_copy(aFooterLabel, Localize("Invalid Demo"));
		else
			str_copy(aFooterLabel, Localize("Demo details"));
	}

	// render background
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

#if defined(CONF_VIDEORECORDER)
	CUIRect RenderRect;
#endif
	CUIRect ButtonBar, RefreshRect, FetchRect, PlayRect, DeleteRect, RenameRect, LabelRect, ListBox;
	CUIRect ButtonBar2, DirectoryButton;

	MainView.HSplitBottom((ms_ButtonHeight + 5.0f) * 2.0f, &MainView, &ButtonBar2);
	ButtonBar2.HSplitTop(5.0f, 0, &ButtonBar2);
	ButtonBar2.HSplitTop(ms_ButtonHeight, &ButtonBar2, &ButtonBar);
	ButtonBar.HSplitTop(5.0f, 0, &ButtonBar);
	ButtonBar2.VSplitLeft(110.0f, &FetchRect, &ButtonBar2);
	ButtonBar2.VSplitLeft(10.0f, 0, &ButtonBar2);
	ButtonBar2.VSplitLeft(230.0f, &DirectoryButton, &ButtonBar2);
	ButtonBar2.VSplitLeft(10.0f, 0, &ButtonBar2);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &PlayRect);
	ButtonBar.VSplitLeft(110.0f, &RefreshRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &DeleteRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &RenameRect, &ButtonBar);
	ButtonBar.VSplitLeft(10.0f, 0, &ButtonBar);
#if defined(CONF_VIDEORECORDER)
	ButtonBar2.VSplitRight(110.0f, &ButtonBar2, &RenderRect);
	ButtonBar2.VSplitRight(10.0f, &ButtonBar2, 0);
#endif
	ButtonBar.VSplitLeft(110.0f, &LabelRect, &ButtonBar);
	MainView.HSplitBottom(140.0f, &ListBox, &MainView);

	// render demo info
	MainView.VMargin(5.0f, &MainView);
	MainView.HSplitBottom(5.0f, &MainView, 0);
	MainView.Draw(ColorRGBA(0, 0, 0, 0.15f), IGraphics::CORNER_B, 4.0f);
	if(!m_DemolistSelectedIsDir && m_DemolistSelectedIndex >= 0 && m_vDemos[m_DemolistSelectedIndex].m_Valid)
	{
		CUIRect Left, Right, Labels;
		MainView.VMargin(20.0f, &MainView);
		MainView.HMargin(10.0f, &MainView);
		MainView.VSplitMid(&Labels, &MainView);

		// left side
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Created:"), 14.0f, TEXTALIGN_LEFT);

		char aTimestamp[256];
		str_timestamp_ex(m_vDemos[m_DemolistSelectedIndex].m_Date, aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

		UI()->DoLabel(&Right, aTimestamp, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Type:"), 14.0f, TEXTALIGN_LEFT);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aType, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Length:"), 14.0f, TEXTALIGN_LEFT);
		int Length = m_vDemos[m_DemolistSelectedIndex].Length();
		char aBuf[64];
		str_time((int64_t)Length * 100, TIME_HOURS, aBuf, sizeof(aBuf));
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Version:"), 14.0f, TEXTALIGN_LEFT);
		str_format(aBuf, sizeof(aBuf), "%d", m_vDemos[m_DemolistSelectedIndex].m_Info.m_Version);
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Markers:"), 14.0f, TEXTALIGN_LEFT);
		str_format(aBuf, sizeof(aBuf), "%d", m_vDemos[m_DemolistSelectedIndex].NumMarkers());
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_LEFT);

		// right side
		Labels = MainView;
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Map:"), 14.0f, TEXTALIGN_LEFT);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aMapName, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Size:"), 14.0f, TEXTALIGN_LEFT);
		const float Size = m_vDemos[m_DemolistSelectedIndex].Size() / 1024.0f;
		if(Size > 1024)
			str_format(aBuf, sizeof(aBuf), Localize("%.2f MiB"), Size / 1024.0f);
		else
			str_format(aBuf, sizeof(aBuf), Localize("%.2f KiB"), Size);
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_LEFT);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		if(m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256 != SHA256_ZEROED)
		{
			UI()->DoLabel(&Left, "SHA256:", 14.0f, TEXTALIGN_LEFT);
			char aSha[SHA256_MAXSTRSIZE];
			sha256_str(m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256, aSha, sizeof(aSha) / 2);
			UI()->DoLabel(&Right, aSha, Right.w > 235 ? 14.0f : 11.0f, TEXTALIGN_LEFT);
		}
		else
		{
			UI()->DoLabel(&Left, Localize("Crc:"), 14.0f, TEXTALIGN_LEFT);
			str_format(aBuf, sizeof(aBuf), "%08x", m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Crc);
			UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_LEFT);
		}
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);

		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Netversion:"), 14.0f, TEXTALIGN_LEFT);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aNetversion, 14.0f, TEXTALIGN_LEFT);
	}

	// demo list

	CUIRect Headers;

	ListBox.HSplitTop(ms_ListheaderHeight, &Headers, &ListBox);

	struct CColumn
	{
		int m_ID;
		int m_Sort;
		CLocConstString m_Caption;
		int m_Direction;
		float m_Width;
		CUIRect m_Rect;
		CUIRect m_Spacer;
	};

	enum
	{
		COL_DEMONAME = 0,
		COL_MARKERS,
		COL_LENGTH,
		COL_DATE,
	};

	static CColumn s_aCols[] = {
		{COL_DEMONAME, SORT_DEMONAME, "Demo", 0, 0.0f, {0}, {0}},
		{COL_MARKERS, SORT_MARKERS, "Markers", 1, 75.0f, {0}, {0}},
		{COL_LENGTH, SORT_LENGTH, "Length", 1, 75.0f, {0}, {0}},
		{COL_DATE, SORT_DATE, "Date", 1, 160.0f, {0}, {0}},
	};
	/* This is just for scripts/update_localization.py to work correctly. Don't remove!
		Localize("Demo");Localize("Markers");Localize("Length");Localize("Date");
	*/

	Headers.Draw(ColorRGBA(0.0f, 0, 0, 0.15f), 0, 0);

	int NumCols = std::size(s_aCols);

	// do layout
	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == -1)
		{
			Headers.VSplitLeft(s_aCols[i].m_Width, &s_aCols[i].m_Rect, &Headers);

			if(i + 1 < NumCols)
			{
				//Cols[i].flags |= SPACER;
				Headers.VSplitLeft(2, &s_aCols[i].m_Spacer, &Headers);
			}
		}
	}

	for(int i = NumCols - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
			Headers.VSplitRight(2, &Headers, &s_aCols[i].m_Spacer);
		}
	}

	for(int i = 0; i < NumCols; i++)
	{
		if(s_aCols[i].m_Direction == 0)
			s_aCols[i].m_Rect = Headers;
	}

	// do headers
	for(int i = 0; i < NumCols; i++)
	{
		if(DoButton_GridHeader(s_aCols[i].m_Caption, Localize(s_aCols[i].m_Caption), g_Config.m_BrDemoSort == s_aCols[i].m_Sort, &s_aCols[i].m_Rect))
		{
			if(s_aCols[i].m_Sort != -1)
			{
				if(g_Config.m_BrDemoSort == s_aCols[i].m_Sort)
					g_Config.m_BrDemoSortOrder ^= 1;
				else
					g_Config.m_BrDemoSortOrder = 0;
				g_Config.m_BrDemoSort = s_aCols[i].m_Sort;
			}

			// Don't rescan in order to keep fetched headers, just resort
			std::stable_sort(m_vDemos.begin(), m_vDemos.end());
			DemolistOnUpdate(false);
		}
	}

	static CListBox s_ListBox;
	s_ListBox.DoStart(ms_ListheaderHeight, m_vDemos.size(), 1, 3, m_DemolistSelectedIndex, &ListBox, false);

	int ItemIndex = -1;
	for(auto &Item : m_vDemos)
	{
		ItemIndex++;

		const CListboxItem ListItem = s_ListBox.DoNextItem(&Item, ItemIndex == m_DemolistSelectedIndex);
		if(!ListItem.m_Visible)
			continue;

		CUIRect Row = ListItem.m_Rect;
		CUIRect FileIcon;
		Row.VSplitLeft(Row.h, &FileIcon, &Row);
		Row.VSplitLeft(5.0f, 0, &Row);
		FileIcon.Margin(1.0f, &FileIcon);
		FileIcon.x += 2.0f;

		const char *pIconType;
		if(str_comp(Item.m_aFilename, "..") == 0)
			pIconType = "\xEF\xA0\x82";
		else if(Item.m_IsDir)
			pIconType = "\xEF\x81\xBB";
		else
			pIconType = "\xEF\x80\x88";

		ColorRGBA IconColor(1.0f, 1.0f, 1.0f, 1.0f);
		if(!Item.m_IsDir && (!Item.m_InfosLoaded || !Item.m_Valid))
			IconColor = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f); // not loaded

		TextRender()->SetCurFont(TextRender()->GetFont(TEXT_FONT_ICON_FONT));
		TextRender()->TextColor(IconColor);
		UI()->DoLabel(&FileIcon, pIconType, 12.0f, TEXTALIGN_LEFT);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetCurFont(nullptr);

		for(int c = 0; c < NumCols; c++)
		{
			CUIRect Button;
			Button.x = s_aCols[c].m_Rect.x;
			Button.y = Row.y;
			Button.h = Row.h;
			Button.w = s_aCols[c].m_Rect.w;

			int ID = s_aCols[c].m_ID;

			if(ID == COL_DEMONAME)
			{
				Button.x += FileIcon.w + 6.0f;
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, Button.x, Button.y + (Button.h - 12.0f) / 2.f, 12.0f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = Button.w;
				TextRender()->TextEx(&Cursor, Item.m_aName, -1);
			}
			else if(ID == COL_MARKERS && !Item.m_IsDir && Item.m_InfosLoaded)
			{
				char aBuf[3];
				str_format(aBuf, sizeof(aBuf), "%d", Item.NumMarkers());
				Button.VMargin(4.0f, &Button);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_RIGHT);
			}
			else if(ID == COL_LENGTH && !Item.m_IsDir && Item.m_InfosLoaded)
			{
				int Length = Item.Length();
				char aBuf[32];
				str_time((int64_t)Length * 100, TIME_HOURS, aBuf, sizeof(aBuf));
				Button.VMargin(4.0f, &Button);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_RIGHT);
			}
			else if(ID == COL_DATE && !Item.m_IsDir)
			{
				char aBuf[64];
				str_timestamp_ex(Item.m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
				Button.VSplitRight(24.0f, &Button, 0);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_RIGHT);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != m_DemolistSelectedIndex)
	{
		m_DemolistSelectedIndex = NewSelected;
		if(m_DemolistSelectedIndex >= 0)
			str_copy(g_Config.m_UiDemoSelected, m_vDemos[m_DemolistSelectedIndex].m_aName);
		DemolistOnUpdate(false);
	}

	static CButtonContainer s_RefreshButton;
	if(DoButton_Menu(&s_RefreshButton, Localize("Refresh"), 0, &RefreshRect) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}

	if(DoButton_CheckBox(&g_Config.m_BrDemoFetchInfo, Localize("Fetch Info"), g_Config.m_BrDemoFetchInfo, &FetchRect))
	{
		g_Config.m_BrDemoFetchInfo ^= 1;
		if(g_Config.m_BrDemoFetchInfo)
			FetchAllHeaders();
	}

	static CButtonContainer s_PlayButton;
	if(DoButton_Menu(&s_PlayButton, m_DemolistSelectedIsDir ? Localize("Open") : Localize("Play", "Demo browser"), 0, &PlayRect) || s_ListBox.WasItemActivated() || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || (Input()->KeyPress(KEY_P) && m_pClient->m_GameConsole.IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE))
	{
		if(m_DemolistSelectedIndex >= 0)
		{
			if(m_DemolistSelectedIsDir) // folder
			{
				if(str_comp(m_vDemos[m_DemolistSelectedIndex].m_aFilename, "..") == 0) // parent folder
					fs_parent_dir(m_aCurrentDemoFolder);
				else // sub folder
				{
					str_append(m_aCurrentDemoFolder, "/", sizeof(m_aCurrentDemoFolder));
					str_append(m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename, sizeof(m_aCurrentDemoFolder));
					m_DemolistStorageType = m_vDemos[m_DemolistSelectedIndex].m_StorageType;
				}
				DemolistPopulate();
				DemolistOnUpdate(true);
			}
			else // file
			{
				char aBuf[IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				const char *pError = Client()->DemoPlayer_Play(aBuf, m_vDemos[m_DemolistSelectedIndex].m_StorageType);
				if(pError)
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
				else
				{
					UI()->SetActiveItem(nullptr);
					return;
				}
			}
		}
	}

	static CButtonContainer s_DirectoryButtonID;
	if(DoButton_Menu(&s_DirectoryButtonID, Localize("Demos directory"), 0, &DirectoryButton))
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "demos", aBuf, sizeof(aBuf));
		Storage()->CreateFolder("demos", IStorage::TYPE_SAVE);
		if(!open_file(aBuf))
		{
			dbg_msg("menus", "couldn't open file '%s'", aBuf);
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_DirectoryButtonID, &DirectoryButton, Localize("Open the directory that contains the demo files"));

	if(!m_DemolistSelectedIsDir)
	{
		static CButtonContainer s_DeleteButton;
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &DeleteRect) || UI()->ConsumeHotkey(CUI::HOTKEY_DELETE) || (Input()->KeyPress(KEY_D) && m_pClient->m_GameConsole.IsClosed()))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				char aBuf[128 + IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), Localize("Are you sure that you want to delete the demo '%s'?"), m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				PopupConfirm(Localize("Delete demo"), aBuf, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDeleteDemo);
				return;
			}
		}

		static CButtonContainer s_RenameButton;
		if(DoButton_Menu(&s_RenameButton, Localize("Rename"), 0, &RenameRect))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(nullptr);
				m_Popup = POPUP_RENAME_DEMO;
				str_copy(m_aCurrentDemoFile, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				return;
			}
		}

#if defined(CONF_VIDEORECORDER)
		static CButtonContainer s_RenderButton;
		if(DoButton_Menu(&s_RenderButton, Localize("Render"), 0, &RenderRect) || (Input()->KeyPress(KEY_R) && m_pClient->m_GameConsole.IsClosed()))
		{
			if(m_DemolistSelectedIndex >= 0)
			{
				UI()->SetActiveItem(nullptr);
				m_Popup = POPUP_RENDER_DEMO;
				str_copy(m_aCurrentDemoFile, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				return;
			}
		}
#endif
	}

	UI()->DoLabel(&LabelRect, aFooterLabel, 14.0f, TEXTALIGN_LEFT);
}

void CMenus::PopupConfirmDeleteDemo()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
	if(Storage()->RemoveFile(aBuf, m_vDemos[m_DemolistSelectedIndex].m_StorageType))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the demo '%s'"), m_vDemos[m_DemolistSelectedIndex].m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}
