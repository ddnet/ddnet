/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/hash.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/localization.h>
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

using namespace FontIcons;
using namespace std::chrono_literals;

int CMenus::DoButton_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners, bool Enabled)
{
	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, (Checked ? 0.10f : 0.5f) * UI()->ButtonColorMul(pButtonContainer)), Corners, 5.0f);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	UI()->DoLabel(&Temp, pText, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);

	if(!Enabled)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
		UI()->DoLabel(&Temp, FONT_ICON_SLASH, Temp.h * CUI::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

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
		if(!DemoPlayer()->BaseInfo()->m_Paused && PositionToSeek == 1.0f)
			DemoPlayer()->Pause();
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
	static int64_t s_LastSpeedChange = 0;

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
				DemoPlayer()->AdjustSpeedIndex(+1);
				s_LastSpeedChange = time_get();
			}
			else if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) || Input()->KeyPress(KEY_DOWN))
			{
				DemoPlayer()->AdjustSpeedIndex(-1);
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

	const float SeekBarHeight = 15.0f;
	const float ButtonbarHeight = 20.0f;
	const float NameBarHeight = 20.0f;
	const float Margins = 5.0f;
	const float TotalHeight = SeekBarHeight + ButtonbarHeight + NameBarHeight + Margins * 3;

	// render speed info
	if(g_Config.m_ClDemoShowSpeed && time_get() - s_LastSpeedChange < time_freq() * 1)
	{
		CUIRect Screen = *UI()->Screen();

		char aSpeedBuf[256];
		str_format(aSpeedBuf, sizeof(aSpeedBuf), "×%.2f", pInfo->m_Speed);
		TextRender()->Text(120.0f, Screen.y + Screen.h - 120.0f - TotalHeight, 60.0f, aSpeedBuf, -1.0f);
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

	CUIRect DemoControls;
	MainView.HSplitBottom(TotalHeight, nullptr, &DemoControls);
	DemoControls.VSplitLeft(50.0f, nullptr, &DemoControls);
	DemoControls.VSplitLeft(600.0f, &DemoControls, nullptr);
	const CUIRect DemoControlsOriginal = DemoControls;
	DemoControls.x += m_DemoControlsPositionOffset.x;
	DemoControls.y += m_DemoControlsPositionOffset.y;
	int Corners = IGraphics::CORNER_NONE;
	if(DemoControls.x > 0.0f && DemoControls.y > 0.0f)
		Corners |= IGraphics::CORNER_TL;
	if(DemoControls.x < MainView.w - DemoControls.w && DemoControls.y > 0.0f)
		Corners |= IGraphics::CORNER_TR;
	if(DemoControls.x > 0.0f && DemoControls.y < MainView.h - DemoControls.h)
		Corners |= IGraphics::CORNER_BL;
	if(DemoControls.x < MainView.w - DemoControls.w && DemoControls.y < MainView.h - DemoControls.h)
		Corners |= IGraphics::CORNER_BR;
	DemoControls.Draw(ms_ColorTabbarActive, Corners, 10.0f);
	const CUIRect DemoControlsDragRect = DemoControls;

	CUIRect SeekBar, ButtonBar, NameBar, SpeedBar;
	DemoControls.Margin(5.0f, &DemoControls);
	DemoControls.HSplitTop(SeekBarHeight, &SeekBar, &ButtonBar);
	ButtonBar.HSplitTop(Margins, nullptr, &ButtonBar);
	ButtonBar.HSplitBottom(NameBarHeight, &ButtonBar, &NameBar);
	NameBar.HSplitTop(4.0f, nullptr, &NameBar);

	// handle draggable demo controls
	{
		enum EDragOperation
		{
			OP_NONE,
			OP_DRAGGING,
			OP_CLICKED
		};
		static EDragOperation s_Operation = OP_NONE;
		static vec2 s_InitialMouse = vec2(0.0f, 0.0f);

		bool Clicked;
		bool Abrupted;
		if(int Result = UI()->DoDraggableButtonLogic(&s_Operation, 8, &DemoControlsDragRect, &Clicked, &Abrupted))
		{
			if(s_Operation == OP_NONE && Result == 1)
			{
				s_InitialMouse = UI()->MousePos();
				s_Operation = OP_CLICKED;
			}

			if(Clicked || Abrupted)
				s_Operation = OP_NONE;

			if(s_Operation == OP_CLICKED && length(UI()->MousePos() - s_InitialMouse) > 5.0f)
			{
				s_Operation = OP_DRAGGING;
				s_InitialMouse -= m_DemoControlsPositionOffset;
			}

			if(s_Operation == OP_DRAGGING)
			{
				m_DemoControlsPositionOffset = UI()->MousePos() - s_InitialMouse;
				m_DemoControlsPositionOffset.x = clamp(m_DemoControlsPositionOffset.x, -DemoControlsOriginal.x, MainView.w - DemoControlsDragRect.w - DemoControlsOriginal.x);
				m_DemoControlsPositionOffset.y = clamp(m_DemoControlsPositionOffset.y, -DemoControlsOriginal.y, MainView.h - DemoControlsDragRect.h - DemoControlsOriginal.y);
			}
		}
	}

	// do seekbar
	{
		const float Rounding = 5.0f;

		static int s_SeekBarID = 0;
		void *pId = &s_SeekBarID;
		char aBuffer[128];

		// draw seek bar
		SeekBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, Rounding);

		// draw filled bar
		float Amount = CurrentTick / (float)TotalTicks;
		CUIRect FilledBar = SeekBar;
		FilledBar.w = 2 * Rounding + (FilledBar.w - 2 * Rounding) * Amount;
		FilledBar.Draw(ColorRGBA(1, 1, 1, 0.5f), IGraphics::CORNER_ALL, Rounding);

		// draw highlighting
		if(g_Config.m_ClDemoSliceBegin != -1 && g_Config.m_ClDemoSliceEnd != -1)
		{
			float RatioBegin = (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick) / (float)TotalTicks;
			float RatioEnd = (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick) / (float)TotalTicks;
			float Span = ((SeekBar.w - 2 * Rounding) * RatioEnd) - ((SeekBar.w - 2 * Rounding) * RatioBegin);
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.0f, 0.0f, 0.25f);
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * RatioBegin, SeekBar.y, Span, SeekBar.h);
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, UI()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw time
		char aCurrentTime[32];
		str_time((int64_t)CurrentTick / SERVER_TICK_SPEED * 100, TIME_HOURS, aCurrentTime, sizeof(aCurrentTime));
		char aTotalTime[32];
		str_time((int64_t)TotalTicks / SERVER_TICK_SPEED * 100, TIME_HOURS, aTotalTime, sizeof(aTotalTime));
		str_format(aBuffer, sizeof(aBuffer), "%s / %s", aCurrentTime, aTotalTime);
		UI()->DoLabel(&SeekBar, aBuffer, SeekBar.h * 0.70f, TEXTALIGN_MC);

		// do the logic
		const bool Inside = UI()->MouseInside(&SeekBar);

		if(UI()->CheckActiveItem(pId))
		{
			if(!UI()->MouseButton(0))
				UI()->SetActiveItem(nullptr);
			else
			{
				static float s_PrevAmount = 0.0f;
				float AmountSeek = clamp((UI()->MouseX() - SeekBar.x - Rounding) / (float)(SeekBar.w - 2 * Rounding), 0.0f, 1.0f);

				if(Input()->ShiftIsPressed())
				{
					AmountSeek = s_PrevAmount + (AmountSeek - s_PrevAmount) * 0.05f;
					if(AmountSeek >= 0.0f && AmountSeek <= 1.0f && absolute(s_PrevAmount - AmountSeek) >= 0.0001f)
					{
						PositionToSeek = AmountSeek;
					}
				}
				else
				{
					if(AmountSeek >= 0.0f && AmountSeek <= 1.0f && absolute(s_PrevAmount - AmountSeek) >= 0.001f)
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
			{
				UI()->SetActiveItem(pId);
			}
			else
			{
				const int HoveredTick = (int)(clamp((UI()->MouseX() - SeekBar.x - Rounding) / (float)(SeekBar.w - 2 * Rounding), 0.0f, 1.0f) * TotalTicks);
				static char s_aHoveredTime[32];
				str_time((int64_t)HoveredTick / SERVER_TICK_SPEED * 100, TIME_HOURS, s_aHoveredTime, sizeof(s_aHoveredTime));
				GameClient()->m_Tooltips.DoToolTip(pId, &SeekBar, s_aHoveredTime);
			}
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
	if(DoButton_FontIcon(&s_PlayPauseButton, pInfo->m_Paused ? FONT_ICON_PLAY : FONT_ICON_PAUSE, false, &Button, IGraphics::CORNER_ALL))
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
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_ResetButton;
	if(DoButton_FontIcon(&s_ResetButton, FONT_ICON_STOP, false, &Button, IGraphics::CORNER_ALL))
	{
		DemoPlayer()->Pause();
		PositionToSeek = 0.0f;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_ResetButton, &Button, Localize("Stop the current demo"));

	// one tick back
	ButtonBar.VSplitLeft(Margins + 10.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickBackButton;
	if(DoButton_FontIcon(&s_OneTickBackButton, FONT_ICON_CHEVRON_LEFT, 0, &Button, IGraphics::CORNER_ALL))
		DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickBackButton, &Button, Localize("Go back one tick"));

	// one tick forward
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickForwardButton;
	if(DoButton_FontIcon(&s_OneTickForwardButton, FONT_ICON_CHEVRON_RIGHT, 0, &Button, IGraphics::CORNER_ALL))
		DemoSeekTick(IDemoPlayer::TICK_NEXT);
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickForwardButton, &Button, Localize("Go forward one tick"));

	// slowdown
	ButtonBar.VSplitLeft(Margins + 10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SlowDownButton;
	if(DoButton_FontIcon(&s_SlowDownButton, FONT_ICON_BACKWARD, 0, &Button, IGraphics::CORNER_ALL))
		DecreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_SlowDownButton, &Button, Localize("Slow down the demo"));

	// fastforward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_FastForwardButton;
	if(DoButton_FontIcon(&s_FastForwardButton, FONT_ICON_FORWARD, 0, &Button, IGraphics::CORNER_ALL))
		IncreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_FastForwardButton, &Button, Localize("Speed up the demo"));

	// speed meter
	ButtonBar.VSplitLeft(Margins * 12, &SpeedBar, &ButtonBar);
	char aBuffer[64];
	str_format(aBuffer, sizeof(aBuffer), "×%g", pInfo->m_Speed);
	UI()->DoLabel(&SpeedBar, aBuffer, Button.h * 0.7f, TEXTALIGN_MC);

	// slice begin button
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceBeginButton;
	const int SliceBeginButtonResult = DoButton_FontIcon(&s_SliceBeginButton, FONT_ICON_RIGHT_FROM_BRACKET, 0, &Button, IGraphics::CORNER_ALL);
	if(SliceBeginButtonResult == 1)
	{
		Client()->DemoSliceBegin();
		if(CurrentTick > (g_Config.m_ClDemoSliceEnd - pInfo->m_FirstTick))
			g_Config.m_ClDemoSliceEnd = -1;
	}
	else if(SliceBeginButtonResult == 2)
	{
		g_Config.m_ClDemoSliceBegin = -1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceBeginButton, &Button, Localize("Mark the beginning of a cut (right click to reset)"));

	// slice end button
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceEndButton;
	const int SliceEndButtonResult = DoButton_FontIcon(&s_SliceEndButton, FONT_ICON_RIGHT_TO_BRACKET, 0, &Button, IGraphics::CORNER_ALL);
	if(SliceEndButtonResult == 1)
	{
		Client()->DemoSliceEnd();
		if(CurrentTick < (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick))
			g_Config.m_ClDemoSliceBegin = -1;
	}
	else if(SliceEndButtonResult == 2)
	{
		g_Config.m_ClDemoSliceEnd = -1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceEndButton, &Button, Localize("Mark the end of a cut (right click to reset)"));

	// slice save button
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceSaveButton;
	if(DoButton_FontIcon(&s_SliceSaveButton, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
	{
		char aDemoName[IO_MAX_PATH_LENGTH];
		DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
		m_DemoSliceInput.Set(aDemoName);
		m_DemoSliceInput.Append(".demo");
		UI()->SetActiveItem(&m_DemoSliceInput);
		m_DemoPlayerState = DEMOPLAYER_SLICE_SAVE;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceSaveButton, &Button, Localize("Export cut as a separate demo"));

	// threshold value, accounts for slight inaccuracy when setting demo position
	const int Threshold = 10;

	// one marker back
	ButtonBar.VSplitLeft(Margins + 20.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerBackButton;
	if(DoButton_FontIcon(&s_OneMarkerBackButton, FONT_ICON_BACKWARD_STEP, 0, &Button, IGraphics::CORNER_ALL))
	{
		PositionToSeek = 0.0f;
		for(int i = pInfo->m_NumTimelineMarkers - 1; i >= 0; i--)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) < CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				PositionToSeek = (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
				break;
			}
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerBackButton, &Button, Localize("Go back one marker"));

	// one marker forward
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerForwardButton;
	if(DoButton_FontIcon(&s_OneMarkerForwardButton, FONT_ICON_FORWARD_STEP, 0, &Button, IGraphics::CORNER_ALL))
	{
		PositionToSeek = 1.0f;
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) > CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				PositionToSeek = (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
				break;
			}
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerForwardButton, &Button, Localize("Go forward one marker"));

	// close button
	ButtonBar.VSplitRight(ButtonbarHeight, &ButtonBar, &Button);
	static CButtonContainer s_ExitButton;
	if(DoButton_FontIcon(&s_ExitButton, FONT_ICON_XMARK, 0, &Button) || (Input()->KeyPress(KEY_C) && m_pClient->m_GameConsole.IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE))
	{
		Client()->Disconnect();
		DemolistOnUpdate(false);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_ExitButton, &Button, Localize("Close the demo player"));

	// toggle keyboard shortcuts button
	ButtonBar.VSplitRight(Margins, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(ButtonbarHeight, &ButtonBar, &Button);
	static CButtonContainer s_KeyboardShortcutsButton;
	if(DoButton_FontIcon(&s_KeyboardShortcutsButton, FONT_ICON_KEYBOARD, 0, &Button, IGraphics::CORNER_ALL, g_Config.m_ClDemoKeyboardShortcuts != 0))
	{
		g_Config.m_ClDemoKeyboardShortcuts ^= 1;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_KeyboardShortcutsButton, &Button, Localize("Toggle keyboard shortcuts"));

	// demo name
	char aDemoName[IO_MAX_PATH_LENGTH];
	DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
	char aBuf[IO_MAX_PATH_LENGTH + 128];
	str_format(aBuf, sizeof(aBuf), Localize("Demofile: %s"), aDemoName);
	SLabelProperties Props;
	Props.m_MaxWidth = NameBar.w;
	Props.m_EllipsisAtEnd = true;
	Props.m_EnableWidthCheck = false;
	UI()->DoLabel(&NameBar, aBuf, Button.h * 0.5f, TEXTALIGN_ML, Props);

	if(IncreaseDemoSpeed)
	{
		DemoPlayer()->AdjustSpeedIndex(+1);
		s_LastSpeedChange = time_get();
	}
	else if(DecreaseDemoSpeed)
	{
		DemoPlayer()->AdjustSpeedIndex(-1);
		s_LastSpeedChange = time_get();
	}

	HandleDemoSeeking(PositionToSeek, TimeToSeek);

	// render popups
	if(m_DemoPlayerState != DEMOPLAYER_NONE)
	{
		// prevent element under the active popup from being activated
		UI()->SetHotItem(nullptr);
	}
	if(m_DemoPlayerState == DEMOPLAYER_SLICE_SAVE)
	{
		RenderDemoPlayerSliceSavePopup(MainView);
	}

	UI()->RenderPopupMenus();
}

void CMenus::RenderDemoPlayerSliceSavePopup(CUIRect MainView)
{
	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();

	CUIRect Box;
	MainView.Margin(150.0f, &Box);

	// background
	Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 15.0f);
	Box.Margin(24.0f, &Box);

	// title
	CUIRect Title;
	Box.HSplitTop(24.0f, &Title, &Box);
	Box.HSplitTop(20.0f, nullptr, &Box);
	UI()->DoLabel(&Title, Localize("Export demo cut"), 24.0f, TEXTALIGN_MC);

	// slice times
	CUIRect SliceTimesBar, SliceInterval, SliceLength;
	Box.HSplitTop(24.0f, &SliceTimesBar, &Box);
	SliceTimesBar.VSplitMid(&SliceInterval, &SliceLength, 40.0f);
	Box.HSplitTop(20.0f, nullptr, &Box);
	const int64_t RealSliceBegin = g_Config.m_ClDemoSliceBegin == -1 ? 0 : (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick);
	const int64_t RealSliceEnd = (g_Config.m_ClDemoSliceEnd == -1 ? pInfo->m_LastTick : g_Config.m_ClDemoSliceEnd) - pInfo->m_FirstTick;
	char aSliceBegin[32];
	str_time(RealSliceBegin / SERVER_TICK_SPEED * 100, TIME_HOURS, aSliceBegin, sizeof(aSliceBegin));
	char aSliceEnd[32];
	str_time(RealSliceEnd / SERVER_TICK_SPEED * 100, TIME_HOURS, aSliceEnd, sizeof(aSliceEnd));
	char aSliceLength[32];
	str_time((RealSliceEnd - RealSliceBegin) / SERVER_TICK_SPEED * 100, TIME_HOURS, aSliceLength, sizeof(aSliceLength));
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s – %s", Localize("Cut interval"), aSliceBegin, aSliceEnd);
	UI()->DoLabel(&SliceInterval, aBuf, 18.0f, TEXTALIGN_ML);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Cut length"), aSliceLength);
	UI()->DoLabel(&SliceLength, aBuf, 18.0f, TEXTALIGN_ML);

	// file name
	CUIRect NameLabel, NameBox;
	Box.HSplitTop(24.0f, &NameLabel, &Box);
	Box.HSplitTop(20.0f, nullptr, &Box);
	NameLabel.VSplitLeft(150.0f, &NameLabel, &NameBox);
	NameBox.VSplitLeft(20.0f, nullptr, &NameBox);
	UI()->DoLabel(&NameLabel, Localize("New name:"), 18.0f, TEXTALIGN_ML);
	UI()->DoEditBox(&m_DemoSliceInput, &NameBox, 12.0f);

	// remove chat checkbox
	static int s_RemoveChat = 0;
	CUIRect RemoveChatCheckBox;
	Box.HSplitTop(24.0f, &RemoveChatCheckBox, &Box);
	Box.HSplitTop(20.0f, nullptr, &Box);
	if(DoButton_CheckBox(&s_RemoveChat, Localize("Remove chat"), s_RemoveChat, &RemoveChatCheckBox))
	{
		s_RemoveChat ^= 1;
	}

	// buttons
	CUIRect ButtonBar, AbortButton, OkButton;
	Box.HSplitBottom(24.0f, &Box, &ButtonBar);
	ButtonBar.VSplitMid(&AbortButton, &OkButton, 40.0f);

	static CButtonContainer s_ButtonAbort;
	if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &AbortButton) || (!UI()->IsPopupOpen() && UI()->ConsumeHotkey(CUI::HOTKEY_ESCAPE)))
		m_DemoPlayerState = DEMOPLAYER_NONE;

	static CUI::SConfirmPopupContext s_ConfirmPopupContext;
	static CButtonContainer s_ButtonOk;
	if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &OkButton) || (!UI()->IsPopupOpen() && UI()->ConsumeHotkey(CUI::HOTKEY_ENTER)))
	{
		char aDemoName[IO_MAX_PATH_LENGTH];
		DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
		str_append(aDemoName, ".demo");

		if(!str_endswith(m_DemoSliceInput.GetString(), ".demo"))
			m_DemoSliceInput.Append(".demo");

		if(str_comp(aDemoName, m_DemoSliceInput.GetString()) == 0)
		{
			static CUI::SMessagePopupContext s_MessagePopupContext;
			s_MessagePopupContext.ErrorColor();
			str_copy(s_MessagePopupContext.m_aMessage, Localize("Please use a different name"));
			UI()->ShowPopupMessage(UI()->MouseX(), OkButton.y + OkButton.h + 5.0f, &s_MessagePopupContext);
		}
		else
		{
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "%s/%s", m_aCurrentDemoFolder, m_DemoSliceInput.GetString());
			if(Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
			{
				s_ConfirmPopupContext.Reset();
				s_ConfirmPopupContext.YesNoButtons();
				str_copy(s_ConfirmPopupContext.m_aMessage, Localize("File already exists, do you want to overwrite it?"));
				UI()->ShowPopupConfirm(UI()->MouseX(), OkButton.y + OkButton.h + 5.0f, &s_ConfirmPopupContext);
			}
			else
				s_ConfirmPopupContext.m_Result = CUI::SConfirmPopupContext::CONFIRMED;
		}
	}

	if(s_ConfirmPopupContext.m_Result == CUI::SConfirmPopupContext::CONFIRMED)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s", m_aCurrentDemoFolder, m_DemoSliceInput.GetString());
		str_copy(m_aCurrentDemoSelectionName, m_DemoSliceInput.GetString());
		if(str_endswith(m_aCurrentDemoSelectionName, ".demo"))
			m_aCurrentDemoSelectionName[str_length(m_aCurrentDemoSelectionName) - str_length(".demo")] = '\0';
		m_DemoPlayerState = DEMOPLAYER_NONE;
		Client()->DemoSlice(aPath, CMenus::DemoFilterChat, &s_RemoveChat);
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	if(s_ConfirmPopupContext.m_Result != CUI::SConfirmPopupContext::UNSET)
	{
		s_ConfirmPopupContext.Reset();
	}
}

int CMenus::DemolistFetchCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CMenus *pSelf = (CMenus *)pUser;
	if(str_comp(pInfo->m_pName, ".") == 0 ||
		(str_comp(pInfo->m_pName, "..") == 0 && (pSelf->m_aCurrentDemoFolder[0] == '\0' || (!pSelf->m_DemolistMultipleStorages && str_comp(pSelf->m_aCurrentDemoFolder, "demos") == 0))) ||
		(!IsDir && !str_endswith(pInfo->m_pName, ".demo")))
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
	Item.m_IsLink = false;
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

	int NumStoragesWithDemos = 0;
	for(int StorageType = IStorage::TYPE_SAVE; StorageType < Storage()->NumPaths(); ++StorageType)
	{
		if(Storage()->FolderExists("demos", StorageType))
		{
			NumStoragesWithDemos++;
		}
	}
	m_DemolistMultipleStorages = NumStoragesWithDemos > 1;

	if(m_aCurrentDemoFolder[0] == '\0')
	{
		{
			CDemoItem Item;
			str_copy(Item.m_aFilename, "demos");
			str_copy(Item.m_aName, Localize("All combined"));
			Item.m_InfosLoaded = false;
			Item.m_Valid = false;
			Item.m_Date = 0;
			Item.m_IsDir = true;
			Item.m_IsLink = true;
			Item.m_StorageType = IStorage::TYPE_ALL;
			m_vDemos.push_back(Item);
		}

		for(int StorageType = IStorage::TYPE_SAVE; StorageType < Storage()->NumPaths(); ++StorageType)
		{
			if(Storage()->FolderExists("demos", StorageType))
			{
				CDemoItem Item;
				str_copy(Item.m_aFilename, "demos");
				Storage()->GetCompletePath(StorageType, "demos", Item.m_aName, sizeof(Item.m_aName));
				str_append(Item.m_aName, "/", sizeof(Item.m_aName));
				Item.m_InfosLoaded = false;
				Item.m_Valid = false;
				Item.m_Date = 0;
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				m_vDemos.push_back(Item);
			}
		}
	}
	else
	{
		m_DemoPopulateStartTime = time_get_nanoseconds();
		Storage()->ListDirectoryInfo(m_DemolistStorageType, m_aCurrentDemoFolder, DemolistFetchCallback, this);

		if(g_Config.m_BrDemoFetchInfo)
			FetchAllHeaders();

		std::stable_sort(m_vDemos.begin(), m_vDemos.end());
	}
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	if(Reset)
		m_aCurrentDemoSelectionName[0] = '\0';
	else
	{
		bool Found = false;
		int SelectedIndex = -1;
		// search for selected index
		for(auto &Item : m_vDemos)
		{
			SelectedIndex++;

			if(str_comp(m_aCurrentDemoSelectionName, Item.m_aName) == 0)
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
	m_DemolistSelectedReveal = true;
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
		else if(m_vDemos[m_DemolistSelectedIndex].m_IsLink)
			str_copy(aFooterLabel, Localize("Folder Link"));
		else if(m_vDemos[m_DemolistSelectedIndex].m_IsDir)
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
	if(m_DemolistSelectedIndex >= 0 && !m_vDemos[m_DemolistSelectedIndex].m_IsDir && m_vDemos[m_DemolistSelectedIndex].m_Valid)
	{
		CUIRect Left, Right, Labels;
		MainView.VMargin(20.0f, &MainView);
		MainView.HMargin(10.0f, &MainView);
		MainView.VSplitMid(&Labels, &MainView);

		// left side
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Created:"), 14.0f, TEXTALIGN_ML);

		char aTimestamp[256];
		str_timestamp_ex(m_vDemos[m_DemolistSelectedIndex].m_Date, aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

		UI()->DoLabel(&Right, aTimestamp, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Type:"), 14.0f, TEXTALIGN_ML);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aType, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Length:"), 14.0f, TEXTALIGN_ML);
		int Length = m_vDemos[m_DemolistSelectedIndex].Length();
		char aBuf[64];
		str_time((int64_t)Length * 100, TIME_HOURS, aBuf, sizeof(aBuf));
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Version:"), 14.0f, TEXTALIGN_ML);
		str_format(aBuf, sizeof(aBuf), "%d", m_vDemos[m_DemolistSelectedIndex].m_Info.m_Version);
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Markers:"), 14.0f, TEXTALIGN_ML);
		str_format(aBuf, sizeof(aBuf), "%d", m_vDemos[m_DemolistSelectedIndex].NumMarkers());
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_ML);

		// right side
		Labels = MainView;
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Map:"), 14.0f, TEXTALIGN_ML);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aMapName, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Size:"), 14.0f, TEXTALIGN_ML);
		const float Size = m_vDemos[m_DemolistSelectedIndex].Size() / 1024.0f;
		if(Size > 1024)
			str_format(aBuf, sizeof(aBuf), Localize("%.2f MiB"), Size / 1024.0f);
		else
			str_format(aBuf, sizeof(aBuf), Localize("%.2f KiB"), Size);
		UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_ML);
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);
		Left.VSplitLeft(150.0f, &Left, &Right);
		if(m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256 != SHA256_ZEROED)
		{
			UI()->DoLabel(&Left, "SHA256:", 14.0f, TEXTALIGN_ML);
			char aSha[SHA256_MAXSTRSIZE];
			sha256_str(m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Sha256, aSha, sizeof(aSha) / 2);
			UI()->DoLabel(&Right, aSha, Right.w > 235 ? 14.0f : 11.0f, TEXTALIGN_ML);
		}
		else
		{
			UI()->DoLabel(&Left, Localize("Crc:"), 14.0f, TEXTALIGN_ML);
			str_format(aBuf, sizeof(aBuf), "%08x", m_vDemos[m_DemolistSelectedIndex].m_MapInfo.m_Crc);
			UI()->DoLabel(&Right, aBuf, 14.0f, TEXTALIGN_ML);
		}
		Labels.HSplitTop(5.0f, 0, &Labels);
		Labels.HSplitTop(20.0f, &Left, &Labels);

		Left.VSplitLeft(150.0f, &Left, &Right);
		UI()->DoLabel(&Left, Localize("Netversion:"), 14.0f, TEXTALIGN_ML);
		UI()->DoLabel(&Right, m_vDemos[m_DemolistSelectedIndex].m_Info.m_aNetversion, 14.0f, TEXTALIGN_ML);
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
	};

	enum
	{
		COL_ICON = 0,
		COL_DEMONAME,
		COL_MARKERS,
		COL_LENGTH,
		COL_DATE,
	};

	static CListBox s_ListBox;
	static CColumn s_aCols[] = {
		{-1, -1, "", -1, 2.0f, {0}},
		{COL_ICON, -1, "", -1, ms_ListheaderHeight, {0}},
		{-1, -1, "", -1, 2.0f, {0}},
		{COL_DEMONAME, SORT_DEMONAME, Localizable("Demo"), 0, 0.0f, {0}},
		{-1, -1, "", 1, 2.0f, {0}},
		{COL_MARKERS, SORT_MARKERS, Localizable("Markers"), 1, 75.0f, {0}},
		{-1, -1, "", 1, 2.0f, {0}},
		{COL_LENGTH, SORT_LENGTH, Localizable("Length"), 1, 75.0f, {0}},
		{-1, -1, "", 1, 2.0f, {0}},
		{COL_DATE, SORT_DATE, Localizable("Date"), 1, 160.0f, {0}},
		{-1, -1, "", 1, s_ListBox.ScrollbarWidthMax(), {0}},
	};

	Headers.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_NONE, 0.0f);

	// do layout
	for(auto &Col : s_aCols)
	{
		if(Col.m_Direction == -1)
		{
			Headers.VSplitLeft(Col.m_Width, &Col.m_Rect, &Headers);
		}
	}

	for(int i = std::size(s_aCols) - 1; i >= 0; i--)
	{
		if(s_aCols[i].m_Direction == 1)
		{
			Headers.VSplitRight(s_aCols[i].m_Width, &Headers, &s_aCols[i].m_Rect);
		}
	}

	for(auto &Col : s_aCols)
	{
		if(Col.m_Direction == 0)
			Col.m_Rect = Headers;
	}

	// do headers
	for(auto &Col : s_aCols)
	{
		if(DoButton_GridHeader(&Col.m_ID, Col.m_Caption, g_Config.m_BrDemoSort == Col.m_Sort, &Col.m_Rect))
		{
			if(Col.m_Sort != -1)
			{
				if(g_Config.m_BrDemoSort == Col.m_Sort)
					g_Config.m_BrDemoSortOrder ^= 1;
				else
					g_Config.m_BrDemoSortOrder = 0;
				g_Config.m_BrDemoSort = Col.m_Sort;
			}

			// Don't rescan in order to keep fetched headers, just resort
			std::stable_sort(m_vDemos.begin(), m_vDemos.end());
			DemolistOnUpdate(false);
		}
	}

	if(m_DemolistSelectedReveal)
	{
		s_ListBox.ScrollToSelected();
		m_DemolistSelectedReveal = false;
	}
	s_ListBox.DoStart(ms_ListheaderHeight, m_vDemos.size(), 1, 3, m_DemolistSelectedIndex, &ListBox, false, IGraphics::CORNER_ALL, true);

	int ItemIndex = -1;
	for(auto &Item : m_vDemos)
	{
		ItemIndex++;

		const CListboxItem ListItem = s_ListBox.DoNextItem(&Item, ItemIndex == m_DemolistSelectedIndex);
		if(!ListItem.m_Visible)
			continue;

		for(const auto &Col : s_aCols)
		{
			CUIRect Button;
			Button.x = Col.m_Rect.x;
			Button.y = ListItem.m_Rect.y;
			Button.h = ListItem.m_Rect.h;
			Button.w = Col.m_Rect.w;

			int ID = Col.m_ID;

			if(ID == COL_ICON)
			{
				Button.Margin(1.0f, &Button);

				const char *pIconType;
				if(Item.m_IsLink || str_comp(Item.m_aFilename, "..") == 0)
					pIconType = FONT_ICON_FOLDER_TREE;
				else if(Item.m_IsDir)
					pIconType = FONT_ICON_FOLDER;
				else
					pIconType = FONT_ICON_FILM;

				ColorRGBA IconColor;
				if(!Item.m_IsDir && (!Item.m_InfosLoaded || !Item.m_Valid))
					IconColor = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f); // not loaded
				else
					IconColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->TextColor(IconColor);
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
				UI()->DoLabel(&Button, pIconType, 12.0f, TEXTALIGN_ML);
				TextRender()->SetRenderFlags(0);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			}
			else if(ID == COL_DEMONAME)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_EllipsisAtEnd = true;
				Props.m_EnableWidthCheck = false;
				UI()->DoLabel(&Button, Item.m_aName, 12.0f, TEXTALIGN_ML, Props);
			}
			else if(ID == COL_MARKERS && !Item.m_IsDir && Item.m_InfosLoaded && Item.m_Valid)
			{
				char aBuf[3];
				str_format(aBuf, sizeof(aBuf), "%d", Item.NumMarkers());
				Button.VMargin(4.0f, &Button);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_MR);
			}
			else if(ID == COL_LENGTH && !Item.m_IsDir && Item.m_InfosLoaded && Item.m_Valid)
			{
				char aBuf[32];
				str_time((int64_t)Item.Length() * 100, TIME_HOURS, aBuf, sizeof(aBuf));
				Button.VMargin(4.0f, &Button);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_MR);
			}
			else if(ID == COL_DATE && !Item.m_IsDir)
			{
				char aBuf[64];
				str_timestamp_ex(Item.m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
				Button.VMargin(4.0f, &Button);
				UI()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_MR);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != m_DemolistSelectedIndex)
	{
		m_DemolistSelectedIndex = NewSelected;
		if(m_DemolistSelectedIndex >= 0)
			str_copy(m_aCurrentDemoSelectionName, m_vDemos[m_DemolistSelectedIndex].m_aName);
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
	if(DoButton_Menu(&s_PlayButton, (m_DemolistSelectedIndex >= 0 && m_vDemos[m_DemolistSelectedIndex].m_IsDir) ? Localize("Open") : Localize("Play", "Demo browser"), 0, &PlayRect) || s_ListBox.WasItemActivated() || UI()->ConsumeHotkey(CUI::HOTKEY_ENTER) || (Input()->KeyPress(KEY_P) && m_pClient->m_GameConsole.IsClosed()))
	{
		if(m_DemolistSelectedIndex >= 0)
		{
			if(m_vDemos[m_DemolistSelectedIndex].m_IsDir) // folder
			{
				const bool ParentFolder = str_comp(m_vDemos[m_DemolistSelectedIndex].m_aFilename, "..") == 0;
				if(ParentFolder) // parent folder
				{
					str_copy(m_aCurrentDemoSelectionName, fs_filename(m_aCurrentDemoFolder));
					str_append(m_aCurrentDemoSelectionName, "/");
					if(fs_parent_dir(m_aCurrentDemoFolder))
					{
						m_aCurrentDemoFolder[0] = '\0';
						if(m_DemolistStorageType == IStorage::TYPE_ALL)
						{
							m_aCurrentDemoSelectionName[0] = '\0'; // will select first list item
						}
						else
						{
							Storage()->GetCompletePath(m_DemolistStorageType, "demos", m_aCurrentDemoSelectionName, sizeof(m_aCurrentDemoSelectionName));
							str_append(m_aCurrentDemoSelectionName, "/");
						}
					}
				}
				else // sub folder
				{
					if(m_aCurrentDemoFolder[0] != '\0')
						str_append(m_aCurrentDemoFolder, "/");
					else
						m_DemolistStorageType = m_vDemos[m_DemolistSelectedIndex].m_StorageType;
					str_append(m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				}
				DemolistPopulate();
				DemolistOnUpdate(!ParentFolder);
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
		Storage()->GetCompletePath(m_DemolistSelectedIndex >= 0 ? m_vDemos[m_DemolistSelectedIndex].m_StorageType : IStorage::TYPE_SAVE, m_aCurrentDemoFolder[0] == '\0' ? "demos" : m_aCurrentDemoFolder, aBuf, sizeof(aBuf));
		if(!open_file(aBuf))
		{
			dbg_msg("menus", "couldn't open file '%s'", aBuf);
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_DirectoryButtonID, &DirectoryButton, Localize("Open the directory that contains the demo files"));

	if(m_DemolistSelectedIndex >= 0 && m_aCurrentDemoFolder[0] != '\0')
	{
		if(str_comp(m_vDemos[m_DemolistSelectedIndex].m_aFilename, "..") != 0 && m_vDemos[m_DemolistSelectedIndex].m_StorageType == IStorage::TYPE_SAVE)
		{
			static CButtonContainer s_DeleteButton;
			if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &DeleteRect) || UI()->ConsumeHotkey(CUI::HOTKEY_DELETE) || (Input()->KeyPress(KEY_D) && m_pClient->m_GameConsole.IsClosed()))
			{
				char aBuf[128 + IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), m_vDemos[m_DemolistSelectedIndex].m_IsDir ? Localize("Are you sure that you want to delete the folder '%s'?") : Localize("Are you sure that you want to delete the demo '%s'?"), m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				PopupConfirm(m_vDemos[m_DemolistSelectedIndex].m_IsDir ? Localize("Delete folder") : Localize("Delete demo"), aBuf, Localize("Yes"), Localize("No"), m_vDemos[m_DemolistSelectedIndex].m_IsDir ? &CMenus::PopupConfirmDeleteFolder : &CMenus::PopupConfirmDeleteDemo);
				return;
			}

			static CButtonContainer s_RenameButton;
			if(DoButton_Menu(&s_RenameButton, Localize("Rename"), 0, &RenameRect))
			{
				m_Popup = POPUP_RENAME_DEMO;
				if(m_vDemos[m_DemolistSelectedIndex].m_IsDir)
				{
					m_DemoRenameInput.Set(m_vDemos[m_DemolistSelectedIndex].m_aFilename);
				}
				else
				{
					char aNameWithoutExt[IO_MAX_PATH_LENGTH];
					fs_split_file_extension(m_vDemos[m_DemolistSelectedIndex].m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
					m_DemoRenameInput.Set(aNameWithoutExt);
				}
				UI()->SetActiveItem(&m_DemoRenameInput);
				return;
			}
		}

#if defined(CONF_VIDEORECORDER)
		if(!m_vDemos[m_DemolistSelectedIndex].m_IsDir)
		{
			static CButtonContainer s_RenderButton;
			if(DoButton_Menu(&s_RenderButton, Localize("Render"), 0, &RenderRect) || (Input()->KeyPress(KEY_R) && m_pClient->m_GameConsole.IsClosed()))
			{
				m_Popup = POPUP_RENDER_DEMO;
				m_StartPaused = false;
				char aNameWithoutExt[IO_MAX_PATH_LENGTH];
				fs_split_file_extension(m_vDemos[m_DemolistSelectedIndex].m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
				m_DemoRenderInput.Set(aNameWithoutExt);
				UI()->SetActiveItem(&m_DemoRenderInput);
				return;
			}
		}
#endif
	}

	UI()->DoLabel(&LabelRect, aFooterLabel, 14.0f, TEXTALIGN_ML);
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

void CMenus::PopupConfirmDeleteFolder()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vDemos[m_DemolistSelectedIndex].m_aFilename);
	if(Storage()->RemoveFolder(aBuf, m_vDemos[m_DemolistSelectedIndex].m_StorageType))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the folder '%s'. Make sure it's empty first."), m_vDemos[m_DemolistSelectedIndex].m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}
