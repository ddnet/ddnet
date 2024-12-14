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
	pRect->Draw(ColorRGBA(1.0f, 1.0f, 1.0f, (Checked ? 0.10f : 0.5f) * Ui()->ButtonColorMul(pButtonContainer)), Corners, 5.0f);

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	Ui()->DoLabel(&Temp, pText, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	if(!Enabled)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f));
		TextRender()->TextOutlineColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f));
		Ui()->DoLabel(&Temp, FONT_ICON_SLASH, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
		TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect);
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
		m_pClient->m_DamageInd.OnReset();
		m_pClient->m_InfoMessages.OnReset();
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
}

void CMenus::RenderDemoPlayer(CUIRect MainView)
{
	const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
	const int CurrentTick = pInfo->m_CurrentTick - pInfo->m_FirstTick;
	const int TotalTicks = pInfo->m_LastTick - pInfo->m_FirstTick;

	// When rendering a demo and starting paused, render the pause indicator permanently.
#if defined(CONF_VIDEORECORDER)
	const bool VideoRendering = IVideo::Current() != nullptr;
	bool InitialVideoPause = VideoRendering && m_LastPauseChange < 0.0f && pInfo->m_Paused;
#else
	const bool VideoRendering = false;
	bool InitialVideoPause = false;
#endif

	const auto &&UpdateLastPauseChange = [&]() {
		// Immediately hide the pause indicator when unpausing the initial pause when rendering a demo.
		m_LastPauseChange = InitialVideoPause ? 0.0f : Client()->GlobalTime();
		InitialVideoPause = false;
	};
	const auto &&UpdateLastSpeedChange = [&]() {
		m_LastSpeedChange = Client()->GlobalTime();
	};

	// threshold value, accounts for slight inaccuracy when setting demo position
	constexpr int Threshold = 10;
	const auto &&FindPreviousMarkerPosition = [&]() {
		for(int i = pInfo->m_NumTimelineMarkers - 1; i >= 0; i--)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) < CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				return (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
			}
		}
		return 0.0f;
	};
	const auto &&FindNextMarkerPosition = [&]() {
		for(int i = 0; i < pInfo->m_NumTimelineMarkers; i++)
		{
			if((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) > CurrentTick && absolute(((pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) - CurrentTick)) > Threshold)
			{
				return (float)(pInfo->m_aTimelineMarkers[i] - pInfo->m_FirstTick) / TotalTicks;
			}
		}
		return 1.0f;
	};

	static int s_SkipDurationIndex = 1;
	static const int s_aSkipDurationsSeconds[] = {1, 5, 10, 30, 60, 5 * 60, 10 * 60};
	const int DemoLengthSeconds = TotalTicks / Client()->GameTickSpeed();
	int NumDurationLabels = 0;
	for(size_t i = 0; i < std::size(s_aSkipDurationsSeconds); ++i)
	{
		if(s_aSkipDurationsSeconds[i] >= DemoLengthSeconds)
			break;
		NumDurationLabels = i + 1;
	}
	if(NumDurationLabels > 0 && s_SkipDurationIndex >= NumDurationLabels)
		s_SkipDurationIndex = maximum(0, NumDurationLabels - 1);

	// handle keyboard shortcuts independent of active menu
	float PositionToSeek = -1.0f;
	float TimeToSeek = 0.0f;
	if(m_pClient->m_GameConsole.IsClosed() && m_DemoPlayerState == DEMOPLAYER_NONE && g_Config.m_ClDemoKeyboardShortcuts && !Ui()->IsPopupOpen())
	{
		// increase/decrease speed
		if(!Input()->ModifierIsPressed() && !Input()->ShiftIsPressed() && !Input()->AltIsPressed())
		{
			if(Input()->KeyPress(KEY_UP) || (m_MenuActive && Input()->KeyPress(KEY_MOUSE_WHEEL_UP)))
			{
				DemoPlayer()->AdjustSpeedIndex(+1);
				UpdateLastSpeedChange();
			}
			else if(Input()->KeyPress(KEY_DOWN) || (m_MenuActive && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN)))
			{
				DemoPlayer()->AdjustSpeedIndex(-1);
				UpdateLastSpeedChange();
			}
		}

		// pause/unpause
		if(Input()->KeyPress(KEY_SPACE) || Input()->KeyPress(KEY_RETURN) || Input()->KeyPress(KEY_KP_ENTER) || Input()->KeyPress(KEY_K))
		{
			if(pInfo->m_Paused)
			{
				DemoPlayer()->Unpause();
			}
			else
			{
				DemoPlayer()->Pause();
			}
			UpdateLastPauseChange();
		}

		// seek backward/forward configured time
		if(Input()->KeyPress(KEY_LEFT) || Input()->KeyPress(KEY_J))
		{
			if(Input()->ModifierIsPressed())
				PositionToSeek = FindPreviousMarkerPosition();
			else if(Input()->ShiftIsPressed())
				s_SkipDurationIndex = maximum(s_SkipDurationIndex - 1, 0);
			else
				TimeToSeek = -s_aSkipDurationsSeconds[s_SkipDurationIndex];
		}
		else if(Input()->KeyPress(KEY_RIGHT) || Input()->KeyPress(KEY_L))
		{
			if(Input()->ModifierIsPressed())
				PositionToSeek = FindNextMarkerPosition();
			else if(Input()->ShiftIsPressed())
				s_SkipDurationIndex = minimum(s_SkipDurationIndex + 1, NumDurationLabels - 1);
			else
				TimeToSeek = s_aSkipDurationsSeconds[s_SkipDurationIndex];
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
		if(Input()->KeyPress(KEY_PERIOD))
		{
			DemoSeekTick(IDemoPlayer::TICK_NEXT);
		}
		else if(Input()->KeyPress(KEY_COMMA))
		{
			DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
		}
	}

	const float SeekBarHeight = 15.0f;
	const float ButtonbarHeight = 20.0f;
	const float NameBarHeight = 20.0f;
	const float Margins = 5.0f;
	const float TotalHeight = SeekBarHeight + ButtonbarHeight + NameBarHeight + Margins * 3;

	if(!m_MenuActive)
	{
		// Render pause indicator
		if(g_Config.m_ClDemoShowPause && (InitialVideoPause || (!VideoRendering && Client()->GlobalTime() - m_LastPauseChange < 0.5f)))
		{
			const float Time = InitialVideoPause ? 0.5f : ((Client()->GlobalTime() - m_LastPauseChange) / 0.5f);
			const float Alpha = (Time < 0.5f ? Time : (1.0f - Time)) * 2.0f;
			if(Alpha > 0.0f)
			{
				TextRender()->TextColor(TextRender()->DefaultTextColor().WithMultipliedAlpha(Alpha));
				TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Alpha));
				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
				Ui()->DoLabel(Ui()->Screen(), pInfo->m_Paused ? FONT_ICON_PAUSE : FONT_ICON_PLAY, 36.0f + Time * 12.0f, TEXTALIGN_MC);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
				TextRender()->SetRenderFlags(0);
			}
		}

		// Render speed info
		if(g_Config.m_ClDemoShowSpeed && Client()->GlobalTime() - m_LastSpeedChange < 1.0f)
		{
			CUIRect Screen = *Ui()->Screen();

			char aSpeedBuf[16];
			str_format(aSpeedBuf, sizeof(aSpeedBuf), "×%.2f", pInfo->m_Speed);
			TextRender()->Text(120.0f, Screen.y + Screen.h - 120.0f - TotalHeight, 60.0f, aSpeedBuf, -1.0f);
		}
	}
	else
	{
		if(m_LastPauseChange > 0.0f)
			m_LastPauseChange = 0.0f;
		if(m_LastSpeedChange > 0.0f)
			m_LastSpeedChange = 0.0f;
	}

	if(CurrentTick == TotalTicks)
	{
		DemoPlayer()->Pause();
		PositionToSeek = 0.0f;
		UpdateLastPauseChange();
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
		if(int Result = Ui()->DoDraggableButtonLogic(&s_Operation, 8, &DemoControlsDragRect, &Clicked, &Abrupted))
		{
			if(s_Operation == OP_NONE && Result == 1)
			{
				s_InitialMouse = Ui()->MousePos();
				s_Operation = OP_CLICKED;
			}

			if(Clicked || Abrupted)
				s_Operation = OP_NONE;

			if(s_Operation == OP_CLICKED && length(Ui()->MousePos() - s_InitialMouse) > 5.0f)
			{
				s_Operation = OP_DRAGGING;
				s_InitialMouse -= m_DemoControlsPositionOffset;
			}

			if(s_Operation == OP_DRAGGING)
			{
				m_DemoControlsPositionOffset = Ui()->MousePos() - s_InitialMouse;
				m_DemoControlsPositionOffset.x = clamp(m_DemoControlsPositionOffset.x, -DemoControlsOriginal.x, MainView.w - DemoControlsDragRect.w - DemoControlsOriginal.x);
				m_DemoControlsPositionOffset.y = clamp(m_DemoControlsPositionOffset.y, -DemoControlsOriginal.y, MainView.h - DemoControlsDragRect.h - DemoControlsOriginal.y);
			}
		}
	}

	// do seekbar
	{
		const float Rounding = 5.0f;

		static int s_SeekBarId = 0;
		void *pId = &s_SeekBarId;
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, Ui()->PixelSize(), SeekBar.h);
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, Ui()->PixelSize(), SeekBar.h);
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
			IGraphics::CQuadItem QuadItem(2 * Rounding + SeekBar.x + (SeekBar.w - 2 * Rounding) * Ratio, SeekBar.y, Ui()->PixelSize(), SeekBar.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		// draw time
		char aCurrentTime[32];
		str_time((int64_t)CurrentTick / Client()->GameTickSpeed() * 100, TIME_HOURS, aCurrentTime, sizeof(aCurrentTime));
		char aTotalTime[32];
		str_time((int64_t)TotalTicks / Client()->GameTickSpeed() * 100, TIME_HOURS, aTotalTime, sizeof(aTotalTime));
		str_format(aBuffer, sizeof(aBuffer), "%s / %s", aCurrentTime, aTotalTime);
		Ui()->DoLabel(&SeekBar, aBuffer, SeekBar.h * 0.70f, TEXTALIGN_MC);

		// do the logic
		const bool Inside = Ui()->MouseInside(&SeekBar);

		if(Ui()->CheckActiveItem(pId))
		{
			if(!Ui()->MouseButton(0))
				Ui()->SetActiveItem(nullptr);
			else
			{
				static float s_PrevAmount = 0.0f;
				float AmountSeek = clamp((Ui()->MouseX() - SeekBar.x - Rounding) / (SeekBar.w - 2 * Rounding), 0.0f, 1.0f);

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
		else if(Ui()->HotItem() == pId)
		{
			if(Ui()->MouseButton(0))
			{
				Ui()->SetActiveItem(pId);
			}
		}

		if(Inside && !Ui()->MouseButton(0))
			Ui()->SetHotItem(pId);

		if(Ui()->HotItem() == pId)
		{
			const int HoveredTick = (int)(clamp((Ui()->MouseX() - SeekBar.x - Rounding) / (SeekBar.w - 2 * Rounding), 0.0f, 1.0f) * TotalTicks);
			static char s_aHoveredTime[32];
			str_time((int64_t)HoveredTick / Client()->GameTickSpeed() * 100, TIME_HOURS, s_aHoveredTime, sizeof(s_aHoveredTime));
			GameClient()->m_Tooltips.DoToolTip(pId, &SeekBar, s_aHoveredTime);
		}
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
		UpdateLastPauseChange();
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

	// skip time back
	ButtonBar.VSplitLeft(Margins + 10.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_TimeBackButton;
	if(DoButton_FontIcon(&s_TimeBackButton, FONT_ICON_BACKWARD, 0, &Button, IGraphics::CORNER_ALL))
	{
		TimeToSeek = -s_aSkipDurationsSeconds[s_SkipDurationIndex];
	}
	GameClient()->m_Tooltips.DoToolTip(&s_TimeBackButton, &Button, Localize("Go back the specified duration"));

	// skip time dropdown
	if(NumDurationLabels >= 2)
	{
		ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
		ButtonBar.VSplitLeft(4 * ButtonbarHeight, &Button, &ButtonBar);

		static std::vector<std::string> s_vDurationNames;
		static std::vector<const char *> s_vpDurationNames;
		s_vDurationNames.resize(NumDurationLabels);
		s_vpDurationNames.resize(NumDurationLabels);

		for(int i = 0; i < NumDurationLabels; ++i)
		{
			char aBuf[256];
			if(s_aSkipDurationsSeconds[i] >= 60)
				str_format(aBuf, sizeof(aBuf), Localize("%d min.", "Demo player duration"), s_aSkipDurationsSeconds[i] / 60);
			else
				str_format(aBuf, sizeof(aBuf), Localize("%d sec.", "Demo player duration"), s_aSkipDurationsSeconds[i]);
			s_vDurationNames[i] = aBuf;
			s_vpDurationNames[i] = s_vDurationNames[i].c_str();
		}

		static CUi::SDropDownState s_SkipDurationDropDownState;
		static CScrollRegion s_SkipDurationDropDownScrollRegion;
		s_SkipDurationDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_SkipDurationDropDownScrollRegion;
		s_SkipDurationIndex = Ui()->DoDropDown(&Button, s_SkipDurationIndex, s_vpDurationNames.data(), NumDurationLabels, s_SkipDurationDropDownState);
		GameClient()->m_Tooltips.DoToolTip(&s_SkipDurationDropDownState.m_ButtonContainer, &Button, Localize("Change the skip duration"));
	}

	// skip time forward
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_TimeForwardButton;
	if(DoButton_FontIcon(&s_TimeForwardButton, FONT_ICON_FORWARD, 0, &Button, IGraphics::CORNER_ALL))
	{
		TimeToSeek = s_aSkipDurationsSeconds[s_SkipDurationIndex];
	}
	GameClient()->m_Tooltips.DoToolTip(&s_TimeForwardButton, &Button, Localize("Go forward the specified duration"));

	// one tick back
	ButtonBar.VSplitLeft(Margins + 10.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickBackButton;
	if(DoButton_FontIcon(&s_OneTickBackButton, FONT_ICON_BACKWARD_STEP, 0, &Button, IGraphics::CORNER_ALL))
	{
		DemoSeekTick(IDemoPlayer::TICK_PREVIOUS);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickBackButton, &Button, Localize("Go back one tick"));

	// one tick forward
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneTickForwardButton;
	if(DoButton_FontIcon(&s_OneTickForwardButton, FONT_ICON_FORWARD_STEP, 0, &Button, IGraphics::CORNER_ALL))
	{
		DemoSeekTick(IDemoPlayer::TICK_NEXT);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneTickForwardButton, &Button, Localize("Go forward one tick"));

	// one marker back
	ButtonBar.VSplitLeft(Margins + 10.0f, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerBackButton;
	if(DoButton_FontIcon(&s_OneMarkerBackButton, FONT_ICON_BACKWARD_FAST, 0, &Button, IGraphics::CORNER_ALL))
	{
		PositionToSeek = FindPreviousMarkerPosition();
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerBackButton, &Button, Localize("Go back one marker"));

	// one marker forward
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_OneMarkerForwardButton;
	if(DoButton_FontIcon(&s_OneMarkerForwardButton, FONT_ICON_FORWARD_FAST, 0, &Button, IGraphics::CORNER_ALL))
	{
		PositionToSeek = FindNextMarkerPosition();
	}
	GameClient()->m_Tooltips.DoToolTip(&s_OneMarkerForwardButton, &Button, Localize("Go forward one marker"));

	// slowdown
	ButtonBar.VSplitLeft(Margins + 10.0f, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SlowDownButton;
	if(DoButton_FontIcon(&s_SlowDownButton, FONT_ICON_CHEVRON_DOWN, 0, &Button, IGraphics::CORNER_ALL))
		DecreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_SlowDownButton, &Button, Localize("Slow down the demo"));

	// fastforward
	ButtonBar.VSplitLeft(Margins, 0, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SpeedUpButton;
	if(DoButton_FontIcon(&s_SpeedUpButton, FONT_ICON_CHEVRON_UP, 0, &Button, IGraphics::CORNER_ALL))
		IncreaseDemoSpeed = true;
	GameClient()->m_Tooltips.DoToolTip(&s_SpeedUpButton, &Button, Localize("Speed up the demo"));

	// speed meter
	ButtonBar.VSplitLeft(Margins * 12, &SpeedBar, &ButtonBar);
	char aBuffer[64];
	str_format(aBuffer, sizeof(aBuffer), "×%g", pInfo->m_Speed);
	Ui()->DoLabel(&SpeedBar, aBuffer, Button.h * 0.7f, TEXTALIGN_MC);

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
#if defined(CONF_VIDEORECORDER)
	const bool SliceEnabled = IVideo::Current() == nullptr;
#else
	const bool SliceEnabled = true;
#endif
	ButtonBar.VSplitLeft(Margins, nullptr, &ButtonBar);
	ButtonBar.VSplitLeft(ButtonbarHeight, &Button, &ButtonBar);
	static CButtonContainer s_SliceSaveButton;
	if(DoButton_FontIcon(&s_SliceSaveButton, FONT_ICON_ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL, SliceEnabled) && SliceEnabled)
	{
		char aDemoName[IO_MAX_PATH_LENGTH];
		DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
		m_DemoSliceInput.Set(aDemoName);
		Ui()->SetActiveItem(&m_DemoSliceInput);
		m_DemoPlayerState = DEMOPLAYER_SLICE_SAVE;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SliceSaveButton, &Button, Localize("Export cut as a separate demo"));

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
	Ui()->DoLabel(&NameBar, aBuf, Button.h * 0.5f, TEXTALIGN_ML, Props);

	if(IncreaseDemoSpeed)
	{
		DemoPlayer()->AdjustSpeedIndex(+1);
		UpdateLastSpeedChange();
	}
	else if(DecreaseDemoSpeed)
	{
		DemoPlayer()->AdjustSpeedIndex(-1);
		UpdateLastSpeedChange();
	}

	HandleDemoSeeking(PositionToSeek, TimeToSeek);

	// render popups
	if(m_DemoPlayerState != DEMOPLAYER_NONE)
	{
		// prevent element under the active popup from being activated
		Ui()->SetHotItem(nullptr);
	}
	if(m_DemoPlayerState == DEMOPLAYER_SLICE_SAVE)
	{
		RenderDemoPlayerSliceSavePopup(MainView);
	}
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
	Ui()->DoLabel(&Title, Localize("Export demo cut"), 24.0f, TEXTALIGN_MC);

	// slice times
	CUIRect SliceTimesBar, SliceInterval, SliceLength;
	Box.HSplitTop(24.0f, &SliceTimesBar, &Box);
	SliceTimesBar.VSplitMid(&SliceInterval, &SliceLength, 40.0f);
	Box.HSplitTop(20.0f, nullptr, &Box);
	const int64_t RealSliceBegin = g_Config.m_ClDemoSliceBegin == -1 ? 0 : (g_Config.m_ClDemoSliceBegin - pInfo->m_FirstTick);
	const int64_t RealSliceEnd = (g_Config.m_ClDemoSliceEnd == -1 ? pInfo->m_LastTick : g_Config.m_ClDemoSliceEnd) - pInfo->m_FirstTick;
	char aSliceBegin[32];
	str_time(RealSliceBegin / Client()->GameTickSpeed() * 100, TIME_HOURS, aSliceBegin, sizeof(aSliceBegin));
	char aSliceEnd[32];
	str_time(RealSliceEnd / Client()->GameTickSpeed() * 100, TIME_HOURS, aSliceEnd, sizeof(aSliceEnd));
	char aSliceLength[32];
	str_time((RealSliceEnd - RealSliceBegin) / Client()->GameTickSpeed() * 100, TIME_HOURS, aSliceLength, sizeof(aSliceLength));
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %s – %s", Localize("Cut interval"), aSliceBegin, aSliceEnd);
	Ui()->DoLabel(&SliceInterval, aBuf, 18.0f, TEXTALIGN_ML);
	str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Cut length"), aSliceLength);
	Ui()->DoLabel(&SliceLength, aBuf, 18.0f, TEXTALIGN_ML);

	// file name
	CUIRect NameLabel, NameBox;
	Box.HSplitTop(24.0f, &NameLabel, &Box);
	Box.HSplitTop(20.0f, nullptr, &Box);
	NameLabel.VSplitLeft(150.0f, &NameLabel, &NameBox);
	NameBox.VSplitLeft(20.0f, nullptr, &NameBox);
	Ui()->DoLabel(&NameLabel, Localize("New name:"), 18.0f, TEXTALIGN_ML);
	Ui()->DoEditBox(&m_DemoSliceInput, &NameBox, 12.0f);

	// remove chat checkbox
	static int s_RemoveChat = 0;

	CUIRect CheckBoxBar, RemoveChatCheckBox, RenderCutCheckBox;
	Box.HSplitTop(24.0f, &CheckBoxBar, &Box);
	Box.HSplitTop(20.0f, nullptr, &Box);
	CheckBoxBar.VSplitMid(&RemoveChatCheckBox, &RenderCutCheckBox, 40.0f);
	if(DoButton_CheckBox(&s_RemoveChat, Localize("Remove chat"), s_RemoveChat, &RemoveChatCheckBox))
	{
		s_RemoveChat ^= 1;
	}
#if defined(CONF_VIDEORECORDER)
	static int s_RenderCut = 0;
	if(DoButton_CheckBox(&s_RenderCut, Localize("Render cut to video"), s_RenderCut, &RenderCutCheckBox))
	{
		s_RenderCut ^= 1;
	}
#endif

	// buttons
	CUIRect ButtonBar, AbortButton, OkButton;
	Box.HSplitBottom(24.0f, &Box, &ButtonBar);
	ButtonBar.VSplitMid(&AbortButton, &OkButton, 40.0f);

	static CButtonContainer s_ButtonAbort;
	if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &AbortButton) || (!Ui()->IsPopupOpen() && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
		m_DemoPlayerState = DEMOPLAYER_NONE;

	static CUi::SConfirmPopupContext s_ConfirmPopupContext;
	static CButtonContainer s_ButtonOk;
	if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &OkButton) || (!Ui()->IsPopupOpen() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(str_endswith(m_DemoSliceInput.GetString(), ".demo"))
		{
			char aNameWithoutExt[IO_MAX_PATH_LENGTH];
			fs_split_file_extension(m_DemoSliceInput.GetString(), aNameWithoutExt, sizeof(aNameWithoutExt));
			m_DemoSliceInput.Set(aNameWithoutExt);
		}

		char aDemoName[IO_MAX_PATH_LENGTH];
		DemoPlayer()->GetDemoName(aDemoName, sizeof(aDemoName));
		if(str_comp(aDemoName, m_DemoSliceInput.GetString()) == 0)
		{
			static CUi::SMessagePopupContext s_MessagePopupContext;
			s_MessagePopupContext.ErrorColor();
			str_copy(s_MessagePopupContext.m_aMessage, Localize("Please use a different filename"));
			Ui()->ShowPopupMessage(Ui()->MouseX(), OkButton.y + OkButton.h + 5.0f, &s_MessagePopupContext);
		}
		else
		{
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "%s/%s.demo", m_aCurrentDemoFolder, m_DemoSliceInput.GetString());
			if(Storage()->FileExists(aPath, IStorage::TYPE_SAVE))
			{
				s_ConfirmPopupContext.Reset();
				s_ConfirmPopupContext.YesNoButtons();
				str_copy(s_ConfirmPopupContext.m_aMessage, Localize("File already exists, do you want to overwrite it?"));
				Ui()->ShowPopupConfirm(Ui()->MouseX(), OkButton.y + OkButton.h + 5.0f, &s_ConfirmPopupContext);
			}
			else
				s_ConfirmPopupContext.m_Result = CUi::SConfirmPopupContext::CONFIRMED;
		}
	}

	if(s_ConfirmPopupContext.m_Result == CUi::SConfirmPopupContext::CONFIRMED)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s.demo", m_aCurrentDemoFolder, m_DemoSliceInput.GetString());
		str_copy(m_aCurrentDemoSelectionName, m_DemoSliceInput.GetString());
		if(str_endswith(m_aCurrentDemoSelectionName, ".demo"))
			m_aCurrentDemoSelectionName[str_length(m_aCurrentDemoSelectionName) - str_length(".demo")] = '\0';

		Client()->DemoSlice(aPath, CMenus::DemoFilterChat, &s_RemoveChat);
		DemolistPopulate();
		DemolistOnUpdate(false);
		m_DemoPlayerState = DEMOPLAYER_NONE;
#if defined(CONF_VIDEORECORDER)
		if(s_RenderCut)
		{
			m_Popup = POPUP_RENDER_DEMO;
			m_StartPaused = false;
			m_DemoRenderInput.Set(m_aCurrentDemoSelectionName);
			Ui()->SetActiveItem(&m_DemoRenderInput);
			if(m_DemolistStorageType != IStorage::TYPE_ALL && m_DemolistStorageType != IStorage::TYPE_SAVE)
				m_DemolistStorageType = IStorage::TYPE_ALL; // Select a storage type containing the sliced demo
		}
#endif
	}
	if(s_ConfirmPopupContext.m_Result != CUi::SConfirmPopupContext::UNSET)
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
		Item.m_Date = 0;
	}
	else
	{
		str_truncate(Item.m_aName, sizeof(Item.m_aName), pInfo->m_pName, str_length(pInfo->m_pName) - str_length(".demo"));
		Item.m_Date = pInfo->m_TimeModified;
	}
	Item.m_InfosLoaded = false;
	Item.m_Valid = false;
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	pSelf->m_vDemos.push_back(Item);

	if(time_get_nanoseconds() - pSelf->m_DemoPopulateStartTime > 500ms)
	{
		pSelf->RenderLoading(Localize("Loading demo files"), "", 0);
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
	RefreshFilteredDemos();
}

void CMenus::RefreshFilteredDemos()
{
	m_vpFilteredDemos.clear();
	for(auto &Demo : m_vDemos)
	{
		if(str_find_nocase(Demo.m_aFilename, m_DemoSearchInput.GetString()))
		{
			m_vpFilteredDemos.push_back(&Demo);
		}
	}
}

void CMenus::DemolistOnUpdate(bool Reset)
{
	if(Reset)
	{
		if(m_vpFilteredDemos.empty())
		{
			m_DemolistSelectedIndex = -1;
			m_aCurrentDemoSelectionName[0] = '\0';
		}
		else
		{
			m_DemolistSelectedIndex = 0;
			str_copy(m_aCurrentDemoSelectionName, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aName);
		}
	}
	else
	{
		RefreshFilteredDemos();

		// search for selected index
		m_DemolistSelectedIndex = -1;
		int SelectedIndex = -1;
		for(const auto &pItem : m_vpFilteredDemos)
		{
			SelectedIndex++;
			if(str_comp(m_aCurrentDemoSelectionName, pItem->m_aName) == 0)
			{
				m_DemolistSelectedIndex = SelectedIndex;
				break;
			}
		}
	}

	if(m_DemolistSelectedIndex >= 0)
		m_DemolistSelectedReveal = true;
}

bool CMenus::FetchHeader(CDemoItem &Item)
{
	if(!Item.m_InfosLoaded)
	{
		char aBuffer[IO_MAX_PATH_LENGTH];
		str_format(aBuffer, sizeof(aBuffer), "%s/%s", m_aCurrentDemoFolder, Item.m_aFilename);
		Item.m_Valid = DemoPlayer()->GetDemoInfo(Storage(), nullptr, aBuffer, Item.m_StorageType, &Item.m_Info, &Item.m_TimelineMarkers, &Item.m_MapInfo);
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

void CMenus::RenderDemoBrowser(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_DEMOS);

	CUIRect ListView, DetailsView, ButtonsView;
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);
	MainView.HSplitBottom(22.0f * 2.0f + 5.0f, &ListView, &ButtonsView);
	ListView.VSplitRight(205.0f, &ListView, &DetailsView);
	ListView.VSplitRight(5.0f, &ListView, nullptr);

	bool WasListboxItemActivated;
	RenderDemoBrowserList(ListView, WasListboxItemActivated);
	RenderDemoBrowserDetails(DetailsView);
	RenderDemoBrowserButtons(ButtonsView, WasListboxItemActivated);
}

void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)
{
	if(!m_DemoBrowserListInitialized)
	{
		DemolistPopulate();
		DemolistOnUpdate(true);
		m_DemoBrowserListInitialized = true;
	}

#if defined(CONF_VIDEORECORDER)
	if(!m_DemoRenderInput.IsEmpty())
	{
		if(DemoPlayer()->ErrorMessage()[0] == '\0')
		{
			m_Popup = POPUP_RENDER_DONE;
		}
		else
		{
			m_DemoRenderInput.Clear();
		}
	}
#endif

	struct SColumn
	{
		int m_Id;
		int m_Sort;
		const char *m_pCaption;
		int m_Direction;
		float m_Width;
		CUIRect m_Rect;
	};

	enum
	{
		COL_ICON = 0,
		COL_DEMONAME,
		COL_LENGTH,
		COL_DATE,
	};

	static CListBox s_ListBox;
	static SColumn s_aCols[] = {
		{-1, -1, "", -1, 2.0f, {0}},
		{COL_ICON, -1, "", -1, ms_ListheaderHeight, {0}},
		{-1, -1, "", -1, 2.0f, {0}},
		{COL_DEMONAME, SORT_DEMONAME, Localizable("Demo"), 0, 0.0f, {0}},
		{-1, -1, "", 1, 2.0f, {0}},
		{COL_LENGTH, SORT_LENGTH, Localizable("Length"), 1, 75.0f, {0}},
		{-1, -1, "", 1, 2.0f, {0}},
		{COL_DATE, SORT_DATE, Localizable("Date"), 1, 150.0f, {0}},
		{-1, -1, "", 1, s_ListBox.ScrollbarWidthMax(), {0}},
	};

	CUIRect Headers, ListBox;
	ListView.HSplitTop(ms_ListheaderHeight, &Headers, &ListBox);
	Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	ListBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);

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

	for(auto &Col : s_aCols)
	{
		if(Col.m_pCaption[0] != '\0' && Col.m_Sort != -1)
		{
			if(DoButton_GridHeader(&Col.m_Id, Localize(Col.m_pCaption), g_Config.m_BrDemoSort == Col.m_Sort, &Col.m_Rect))
			{
				if(g_Config.m_BrDemoSort == Col.m_Sort)
					g_Config.m_BrDemoSortOrder ^= 1;
				else
					g_Config.m_BrDemoSortOrder = 0;
				g_Config.m_BrDemoSort = Col.m_Sort;
				// Don't rescan in order to keep fetched headers, just resort
				std::stable_sort(m_vDemos.begin(), m_vDemos.end());
				DemolistOnUpdate(false);
			}
		}
	}

	if(m_DemolistSelectedReveal)
	{
		s_ListBox.ScrollToSelected();
		m_DemolistSelectedReveal = false;
	}

	s_ListBox.DoStart(ms_ListheaderHeight, m_vpFilteredDemos.size(), 1, 3, m_DemolistSelectedIndex, &ListBox, false, IGraphics::CORNER_ALL, true);

	char aBuf[64];
	int ItemIndex = -1;
	for(auto &pItem : m_vpFilteredDemos)
	{
		ItemIndex++;

		const CListboxItem ListItem = s_ListBox.DoNextItem(pItem, ItemIndex == m_DemolistSelectedIndex);
		if(!ListItem.m_Visible)
			continue;

		for(const auto &Col : s_aCols)
		{
			CUIRect Button;
			Button.x = Col.m_Rect.x;
			Button.y = ListItem.m_Rect.y;
			Button.h = ListItem.m_Rect.h;
			Button.w = Col.m_Rect.w;

			if(Col.m_Id == COL_ICON)
			{
				Button.Margin(1.0f, &Button);

				const char *pIconType;
				if(pItem->m_IsLink || str_comp(pItem->m_aFilename, "..") == 0)
					pIconType = FONT_ICON_FOLDER_TREE;
				else if(pItem->m_IsDir)
					pIconType = FONT_ICON_FOLDER;
				else
					pIconType = FONT_ICON_FILM;

				ColorRGBA IconColor;
				if(!pItem->m_IsDir && (!pItem->m_InfosLoaded || !pItem->m_Valid))
					IconColor = ColorRGBA(0.6f, 0.6f, 0.6f, 1.0f); // not loaded
				else
					IconColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

				TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
				TextRender()->TextColor(IconColor);
				TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
				Ui()->DoLabel(&Button, pIconType, 12.0f, TEXTALIGN_ML);
				TextRender()->SetRenderFlags(0);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			}
			else if(Col.m_Id == COL_DEMONAME)
			{
				SLabelProperties Props;
				Props.m_MaxWidth = Button.w;
				Props.m_EllipsisAtEnd = true;
				Props.m_EnableWidthCheck = false;
				Ui()->DoLabel(&Button, pItem->m_aName, 12.0f, TEXTALIGN_ML, Props);
			}
			else if(Col.m_Id == COL_LENGTH && !pItem->m_IsDir && pItem->m_Valid)
			{
				str_time((int64_t)pItem->Length() * 100, TIME_HOURS, aBuf, sizeof(aBuf));
				Button.VMargin(4.0f, &Button);
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_MR);
			}
			else if(Col.m_Id == COL_DATE && !pItem->m_IsDir)
			{
				str_timestamp_ex(pItem->m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
				Button.VMargin(4.0f, &Button);
				Ui()->DoLabel(&Button, aBuf, 12.0f, TEXTALIGN_MR);
			}
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(NewSelected != m_DemolistSelectedIndex)
	{
		m_DemolistSelectedIndex = NewSelected;
		if(m_DemolistSelectedIndex >= 0)
			str_copy(m_aCurrentDemoSelectionName, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aName);
		DemolistOnUpdate(false);
	}

	WasListboxItemActivated = s_ListBox.WasItemActivated();
}

void CMenus::RenderDemoBrowserDetails(CUIRect DetailsView)
{
	CUIRect Contents, Header;
	DetailsView.HSplitTop(ms_ListheaderHeight, &Header, &Contents);
	Contents.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);
	Contents.Margin(5.0f, &Contents);

	const float FontSize = 12.0f;
	CDemoItem *pItem = m_DemolistSelectedIndex >= 0 ? m_vpFilteredDemos[m_DemolistSelectedIndex] : nullptr;

	Header.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	const char *pHeaderLabel;
	if(pItem == nullptr)
		pHeaderLabel = Localize("No demo selected");
	else if(str_comp(pItem->m_aFilename, "..") == 0)
		pHeaderLabel = Localize("Parent Folder");
	else if(pItem->m_IsLink)
		pHeaderLabel = Localize("Folder Link");
	else if(pItem->m_IsDir)
		pHeaderLabel = Localize("Folder");
	else if(!FetchHeader(*pItem))
		pHeaderLabel = Localize("Invalid Demo");
	else
		pHeaderLabel = Localize("Demo");
	Ui()->DoLabel(&Header, pHeaderLabel, FontSize + 2.0f, TEXTALIGN_MC);

	if(pItem == nullptr || pItem->m_IsDir)
		return;

	char aBuf[256];
	CUIRect Left, Right;

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, Localize("Created"), FontSize, TEXTALIGN_ML);
	str_timestamp_ex(pItem->m_Date, aBuf, sizeof(aBuf), FORMAT_SPACE);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(4.0f, nullptr, &Contents);

	if(!pItem->m_Valid)
		return;

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Left.VSplitMid(&Left, &Right, 4.0f);
	Ui()->DoLabel(&Left, Localize("Type"), FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&Right, Localize("Version"), FontSize, TEXTALIGN_ML);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	Left.VSplitMid(&Left, &Right, 4.0f);
	Ui()->DoLabel(&Left, pItem->m_Info.m_aType, FontSize - 1.0f, TEXTALIGN_ML);
	str_format(aBuf, sizeof(aBuf), "%d", pItem->m_Info.m_Version);
	Ui()->DoLabel(&Right, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(4.0f, nullptr, &Contents);

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Left.VSplitMid(&Left, &Right, 4.0f);
	Ui()->DoLabel(&Left, Localize("Length"), FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&Right, Localize("Markers"), FontSize, TEXTALIGN_ML);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	Left.VSplitMid(&Left, &Right, 4.0f);
	str_time((int64_t)pItem->Length() * 100, TIME_HOURS, aBuf, sizeof(aBuf));
	Ui()->DoLabel(&Left, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	str_format(aBuf, sizeof(aBuf), "%d", pItem->NumMarkers());
	Ui()->DoLabel(&Right, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(4.0f, nullptr, &Contents);

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, Localize("Netversion"), FontSize, TEXTALIGN_ML);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, pItem->m_Info.m_aNetversion, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(16.0f, nullptr, &Contents);

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, Localize("Map"), FontSize, TEXTALIGN_ML);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, pItem->m_Info.m_aMapName, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(4.0f, nullptr, &Contents);

	Contents.HSplitTop(18.0f, &Left, &Contents);
	Ui()->DoLabel(&Left, Localize("Size"), FontSize, TEXTALIGN_ML);
	Contents.HSplitTop(18.0f, &Left, &Contents);
	const float Size = pItem->Size() / 1024.0f;
	if(Size == 0.0f)
		str_copy(aBuf, Localize("map not included", "Demo details"));
	else if(Size > 1024)
		str_format(aBuf, sizeof(aBuf), Localize("%.2f MiB"), Size / 1024.0f);
	else
		str_format(aBuf, sizeof(aBuf), Localize("%.2f KiB"), Size);
	Ui()->DoLabel(&Left, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	Contents.HSplitTop(4.0f, nullptr, &Contents);

	Contents.HSplitTop(18.0f, &Left, &Contents);
	if(pItem->m_MapInfo.m_Sha256 != SHA256_ZEROED)
	{
		Ui()->DoLabel(&Left, "SHA256", FontSize, TEXTALIGN_ML);
		Contents.HSplitTop(18.0f, &Left, &Contents);
		char aSha[SHA256_MAXSTRSIZE];
		sha256_str(pItem->m_MapInfo.m_Sha256, aSha, sizeof(aSha));
		SLabelProperties Props;
		Props.m_MaxWidth = Left.w;
		Props.m_EllipsisAtEnd = true;
		Props.m_EnableWidthCheck = false;
		Ui()->DoLabel(&Left, aSha, FontSize - 1.0f, TEXTALIGN_ML, Props);
	}
	else
	{
		Ui()->DoLabel(&Left, "CRC32", FontSize, TEXTALIGN_ML);
		Contents.HSplitTop(18.0f, &Left, &Contents);
		str_format(aBuf, sizeof(aBuf), "%08x", pItem->m_MapInfo.m_Crc);
		Ui()->DoLabel(&Left, aBuf, FontSize - 1.0f, TEXTALIGN_ML);
	}
	Contents.HSplitTop(4.0f, nullptr, &Contents);
}

void CMenus::RenderDemoBrowserButtons(CUIRect ButtonsView, bool WasListboxItemActivated)
{
	const auto &&SetIconMode = [&](bool Enable) {
		if(Enable)
		{
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		}
		else
		{
			TextRender()->SetRenderFlags(0);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		}
	};

	CUIRect ButtonBarTop, ButtonBarBottom;
	ButtonsView.HSplitTop(5.0f, nullptr, &ButtonsView);
	ButtonsView.HSplitMid(&ButtonBarTop, &ButtonBarBottom, 5.0f);

	// quick search
	{
		CUIRect DemoSearch;
		ButtonBarTop.VSplitLeft(ButtonBarBottom.h * 21.0f, &DemoSearch, &ButtonBarTop);
		ButtonBarTop.VSplitLeft(ButtonBarTop.h / 2.0f, nullptr, &ButtonBarTop);
		if(Ui()->DoEditBox_Search(&m_DemoSearchInput, &DemoSearch, 14.0f, !Ui()->IsPopupOpen() && m_pClient->m_GameConsole.IsClosed()))
		{
			RefreshFilteredDemos();
			DemolistOnUpdate(false);
		}
	}

	// refresh button
	{
		CUIRect RefreshButton;
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h * 3.0f, &RefreshButton, &ButtonBarBottom);
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h / 2.0f, nullptr, &ButtonBarBottom);
		SetIconMode(true);
		static CButtonContainer s_RefreshButton;
		if(DoButton_Menu(&s_RefreshButton, FONT_ICON_ARROW_ROTATE_RIGHT, 0, &RefreshButton) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
		{
			SetIconMode(false);
			DemolistPopulate();
			DemolistOnUpdate(false);
		}
		SetIconMode(false);
	}

	// fetch info checkbox
	{
		CUIRect FetchInfo;
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h * 7.0f, &FetchInfo, &ButtonBarBottom);
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h / 2.0f, nullptr, &ButtonBarBottom);
		if(DoButton_CheckBox(&g_Config.m_BrDemoFetchInfo, Localize("Fetch Info"), g_Config.m_BrDemoFetchInfo, &FetchInfo))
		{
			g_Config.m_BrDemoFetchInfo ^= 1;
			if(g_Config.m_BrDemoFetchInfo)
				FetchAllHeaders();
		}
	}

	// demos directory button
	if(m_DemolistSelectedIndex >= 0 && m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType != IStorage::TYPE_ALL)
	{
		CUIRect DemosDirectoryButton;
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h * 10.0f, &DemosDirectoryButton, &ButtonBarBottom);
		ButtonBarBottom.VSplitLeft(ButtonBarBottom.h / 2.0f, nullptr, &ButtonBarBottom);
		static CButtonContainer s_DemosDirectoryButton;
		if(DoButton_Menu(&s_DemosDirectoryButton, Localize("Demos directory"), 0, &DemosDirectoryButton))
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(m_DemolistSelectedIndex >= 0 ? m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType : IStorage::TYPE_SAVE, m_aCurrentDemoFolder[0] == '\0' ? "demos" : m_aCurrentDemoFolder, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
		GameClient()->m_Tooltips.DoToolTip(&s_DemosDirectoryButton, &DemosDirectoryButton, Localize("Open the directory that contains the demo files"));
	}

	// play/open button
	if(m_DemolistSelectedIndex >= 0)
	{
		CUIRect PlayButton;
		ButtonBarBottom.VSplitRight(ButtonBarBottom.h * 3.0f, &ButtonBarBottom, &PlayButton);
		ButtonBarBottom.VSplitRight(ButtonBarBottom.h, &ButtonBarBottom, nullptr);
		SetIconMode(true);
		static CButtonContainer s_PlayButton;
		if(DoButton_Menu(&s_PlayButton, (m_DemolistSelectedIndex >= 0 && m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir) ? FONT_ICON_FOLDER_OPEN : FONT_ICON_PLAY, 0, &PlayButton) || WasListboxItemActivated || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || (Input()->KeyPress(KEY_P) && m_pClient->m_GameConsole.IsClosed() && !m_DemoSearchInput.IsActive()))
		{
			SetIconMode(false);
			if(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir) // folder
			{
				m_DemoSearchInput.Clear();
				const bool ParentFolder = str_comp(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename, "..") == 0;
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
						m_DemolistStorageType = m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType;
					str_append(m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
				}
				DemolistPopulate();
				DemolistOnUpdate(!ParentFolder);
			}
			else // file
			{
				char aBuf[IO_MAX_PATH_LENGTH];
				str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
				const char *pError = Client()->DemoPlayer_Play(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType);
				m_LastPauseChange = -1.0f;
				m_LastSpeedChange = -1.0f;
				if(pError)
				{
					PopupMessage(Localize("Error loading demo"), pError, Localize("Ok"));
				}
				else
				{
					Ui()->SetActiveItem(nullptr);
					return;
				}
			}
		}
		SetIconMode(false);

		if(m_aCurrentDemoFolder[0] != '\0')
		{
			if(str_comp(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename, "..") != 0 && m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType == IStorage::TYPE_SAVE)
			{
				// rename button
				CUIRect RenameButton;
				ButtonBarBottom.VSplitRight(ButtonBarBottom.h * 3.0f, &ButtonBarBottom, &RenameButton);
				ButtonBarBottom.VSplitRight(ButtonBarBottom.h / 2.0f, &ButtonBarBottom, nullptr);
				SetIconMode(true);
				static CButtonContainer s_RenameButton;
				if(DoButton_Menu(&s_RenameButton, FONT_ICON_PENCIL, 0, &RenameButton))
				{
					SetIconMode(false);
					m_Popup = POPUP_RENAME_DEMO;
					if(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir)
					{
						m_DemoRenameInput.Set(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
					}
					else
					{
						char aNameWithoutExt[IO_MAX_PATH_LENGTH];
						fs_split_file_extension(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
						m_DemoRenameInput.Set(aNameWithoutExt);
					}
					Ui()->SetActiveItem(&m_DemoRenameInput);
					return;
				}

				// delete button
				static CButtonContainer s_DeleteButton;
				CUIRect DeleteButton;
				ButtonBarBottom.VSplitRight(ButtonBarBottom.h * 3.0f, &ButtonBarBottom, &DeleteButton);
				ButtonBarBottom.VSplitRight(ButtonBarBottom.h / 2.0f, &ButtonBarBottom, nullptr);
				if(DoButton_Menu(&s_DeleteButton, FONT_ICON_TRASH, 0, &DeleteButton) || Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE) || (Input()->KeyPress(KEY_D) && m_pClient->m_GameConsole.IsClosed() && !m_DemoSearchInput.IsActive()))
				{
					SetIconMode(false);
					char aBuf[128 + IO_MAX_PATH_LENGTH];
					str_format(aBuf, sizeof(aBuf), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? Localize("Are you sure that you want to delete the folder '%s'?") : Localize("Are you sure that you want to delete the demo '%s'?"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
					PopupConfirm(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? Localize("Delete folder") : Localize("Delete demo"), aBuf, Localize("Yes"), Localize("No"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir ? &CMenus::PopupConfirmDeleteFolder : &CMenus::PopupConfirmDeleteDemo);
					return;
				}
				SetIconMode(false);
			}

#if defined(CONF_VIDEORECORDER)
			// render demo button
			if(!m_vpFilteredDemos[m_DemolistSelectedIndex]->m_IsDir)
			{
				CUIRect RenderButton;
				ButtonBarTop.VSplitRight(ButtonBarBottom.h * 3.0f, &ButtonBarTop, &RenderButton);
				ButtonBarTop.VSplitRight(ButtonBarBottom.h, &ButtonBarTop, nullptr);
				SetIconMode(true);
				static CButtonContainer s_RenderButton;
				if(DoButton_Menu(&s_RenderButton, FONT_ICON_VIDEO, 0, &RenderButton) || (Input()->KeyPress(KEY_R) && m_pClient->m_GameConsole.IsClosed() && !m_DemoSearchInput.IsActive()))
				{
					SetIconMode(false);
					m_Popup = POPUP_RENDER_DEMO;
					m_StartPaused = false;
					char aNameWithoutExt[IO_MAX_PATH_LENGTH];
					fs_split_file_extension(m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
					m_DemoRenderInput.Set(aNameWithoutExt);
					Ui()->SetActiveItem(&m_DemoRenderInput);
					return;
				}
				SetIconMode(false);
			}
#endif
		}
	}
}

void CMenus::PopupConfirmDeleteDemo()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	if(Storage()->RemoveFile(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the demo '%s'"), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}

void CMenus::PopupConfirmDeleteFolder()
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
	if(Storage()->RemoveFolder(aBuf, m_vpFilteredDemos[m_DemolistSelectedIndex]->m_StorageType))
	{
		DemolistPopulate();
		DemolistOnUpdate(false);
	}
	else
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Unable to delete the folder '%s'. Make sure it's empty first."), m_vpFilteredDemos[m_DemolistSelectedIndex]->m_aFilename);
		PopupMessage(Localize("Error"), aError, Localize("Ok"));
	}
}
