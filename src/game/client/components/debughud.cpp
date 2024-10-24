/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/localization.h>

#include "debughud.h"

static constexpr int64_t GRAPH_MAX_VALUES = 128;

CDebugHud::CDebugHud() :
	m_RampGraph(GRAPH_MAX_VALUES),
	m_ZoomedInGraph(GRAPH_MAX_VALUES)
{
}

void CDebugHud::RenderNetCorrections()
{
	if(!g_Config.m_Debug || g_Config.m_DbgGraphs || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const float Velspeed = length(vec2(m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f, m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f)) * Client()->GameTickSpeed();
	const float VelspeedX = m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f * Client()->GameTickSpeed();
	const float VelspeedY = m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f * Client()->GameTickSpeed();
	const float Ramp = VelocityRamp(Velspeed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
	const CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterById(m_pClient->m_Snap.m_LocalClientId);

	const float FontSize = 5.0f;
	const float LineHeight = FontSize + 1.0f;

	float y = 50.0f;
	char aBuf[128];
	const auto &&RenderRow = [&](const char *pLabel, const char *pValue) {
		TextRender()->Text(Width - 100.0f, y, FontSize, pLabel);
		TextRender()->Text(Width - 10.0f - TextRender()->TextWidth(FontSize, pValue), y, FontSize, pValue);
		y += LineHeight;
	};

	TextRender()->TextColor(TextRender()->DefaultTextColor());

	str_format(aBuf, sizeof(aBuf), "%.0f Bps", Velspeed / 32);
	RenderRow("Velspeed:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%.0f Bps", VelspeedX / 32 * Ramp);
	RenderRow("Velspeed.x * Ramp:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%.0f Bps", VelspeedY / 32);
	RenderRow("Velspeed.y:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", Ramp);
	RenderRow("Ramp:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%d", pCharacter == nullptr ? -1 : pCharacter->m_TeleCheckpoint);
	RenderRow("Checkpoint:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%d / %d", pCharacter == nullptr ? -1 : pCharacter->GetPureTuneZone(), pCharacter == nullptr ? -1 : pCharacter->GetOverriddenTuneZone());
	RenderRow("Tune zone (pure / override):", aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_pClient->m_Snap.m_pLocalCharacter->m_X / 32.0f);
	RenderRow("Pos.x:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_pClient->m_Snap.m_pLocalCharacter->m_Y / 32.0f);
	RenderRow("Pos.y:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%d", m_pClient->m_Snap.m_pLocalCharacter->m_Angle);
	RenderRow("Angle:", aBuf);

	str_format(aBuf, sizeof(aBuf), "%d", m_pClient->NetobjNumCorrections());
	RenderRow("Netobj corrections", aBuf);
	RenderRow(" on:", m_pClient->NetobjCorrectedOn());
}

void CDebugHud::RenderTuning()
{
	enum
	{
		DBG_TUNING_OFF = 0,
		DBG_TUNING_SHOW_CHANGED,
		DBG_TUNING_SHOW_ALL,
	};

	if(g_Config.m_DbgTuning == DBG_TUNING_OFF)
		return;

	const CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterById(m_pClient->m_Snap.m_LocalClientId);

	const CTuningParams StandardTuning;
	const CTuningParams *pGlobalTuning = m_pClient->GetTuning(0);
	const CTuningParams *pZoneTuning = !m_pClient->m_GameWorld.m_WorldConfig.m_UseTuneZones || pCharacter == nullptr ? nullptr : m_pClient->GetTuning(pCharacter->GetOverriddenTuneZone());
	const CTuningParams *pActiveTuning = pZoneTuning == nullptr ? pGlobalTuning : pZoneTuning;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const float FontSize = 5.0f;

	const float StartY = 50.0f;
	float y = StartY;
	float StartX = 30.0f;
	const auto &&RenderRow = [&](const char *pCol1, const char *pCol2, const char *pCol3) {
		float x = StartX;
		TextRender()->Text(x - TextRender()->TextWidth(FontSize, pCol1), y, FontSize, pCol1);

		x += 30.0f;
		TextRender()->Text(x - TextRender()->TextWidth(FontSize, pCol2), y, FontSize, pCol2);

		x += 10.0f;
		TextRender()->Text(x, y, FontSize, pCol3);

		y += FontSize + 1.0f;

		if(y >= Height - 80.0f)
		{
			y = StartY;
			StartX += 130.0f;
		}
	};

	for(int i = 0; i < CTuningParams::Num(); i++)
	{
		float CurrentGlobal, CurrentZone, Standard;
		pGlobalTuning->Get(i, &CurrentGlobal);
		if(pZoneTuning == nullptr)
			CurrentZone = 0.0f;
		else
			pZoneTuning->Get(i, &CurrentZone);
		StandardTuning.Get(i, &Standard);

		if(g_Config.m_DbgTuning == DBG_TUNING_SHOW_CHANGED && Standard == CurrentGlobal && (pZoneTuning == nullptr || Standard == CurrentZone))
			continue; // skip unchanged params

		if(y == StartY)
		{
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			RenderRow("Standard", "Current", "Tuning");
		}

		ColorRGBA TextColor;
		if(g_Config.m_DbgTuning == DBG_TUNING_SHOW_ALL && Standard == CurrentGlobal && (pZoneTuning == nullptr || Standard == CurrentZone))
			TextColor = ColorRGBA(0.75f, 0.75f, 0.75f, 1.0f); // grey: value unchanged globally and in current zone
		else if(Standard == CurrentGlobal && pZoneTuning != nullptr && Standard != CurrentZone)
			TextColor = ColorRGBA(0.6f, 0.6f, 1.0f, 1.0f); // blue: value changed only in current zone
		else if(Standard != CurrentGlobal && pZoneTuning != nullptr && Standard == CurrentZone)
			TextColor = ColorRGBA(0.4f, 1.0f, 0.4f, 1.0f); // green: value changed globally but reset to default by tune zone
		else
			TextColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f); // red: value changed globally
		TextRender()->TextColor(TextColor);

		char aBufStandard[32];
		str_format(aBufStandard, sizeof(aBufStandard), "%.2f", Standard);
		char aBufCurrent[32];
		str_format(aBufCurrent, sizeof(aBufCurrent), "%.2f", pZoneTuning == nullptr ? CurrentGlobal : CurrentZone);
		RenderRow(aBufStandard, aBufCurrent, CTuningParams::Name(i));
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	if(g_Config.m_DbgTuning == DBG_TUNING_SHOW_CHANGED)
		return;

	// Render Velspeed.X * Ramp Graphs
	Graphics()->MapScreen(0.0f, 0.0f, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float GraphSpacing = Graphics()->ScreenWidth() / 100.0f;
	const float GraphW = Graphics()->ScreenWidth() / 4.0f;
	const float GraphH = Graphics()->ScreenHeight() / 6.0f;
	const float GraphX = GraphW;
	const float GraphY = Graphics()->ScreenHeight() - GraphH - GraphSpacing;

	const int StepSizeRampGraph = 270;
	const int StepSizeZoomedInGraph = 14;
	if(m_OldVelrampStart != pActiveTuning->m_VelrampStart || m_OldVelrampRange != pActiveTuning->m_VelrampRange || m_OldVelrampCurvature != pActiveTuning->m_VelrampCurvature)
	{
		m_OldVelrampStart = pActiveTuning->m_VelrampStart;
		m_OldVelrampRange = pActiveTuning->m_VelrampRange;
		m_OldVelrampCurvature = pActiveTuning->m_VelrampCurvature;

		m_RampGraph.Init(0.0f, 0.0f);
		m_SpeedTurningPoint = 0;
		float PreviousRampedSpeed = 1.0f;
		for(int64_t i = 0; i < GRAPH_MAX_VALUES; i++)
		{
			// This is a calculation of the speed values per second on the X axis, from 270 to 34560 in steps of 270
			const float Speed = (i + 1) * StepSizeRampGraph;
			const float Ramp = VelocityRamp(Speed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
			const float RampedSpeed = Speed * Ramp;
			if(RampedSpeed >= PreviousRampedSpeed)
			{
				m_RampGraph.InsertAt(i, RampedSpeed / 32, ColorRGBA(0.0f, 1.0f, 0.0f, 0.75f));
				m_SpeedTurningPoint = Speed;
			}
			else
			{
				m_RampGraph.InsertAt(i, RampedSpeed / 32, ColorRGBA(1.0f, 0.0f, 0.0f, 0.75f));
			}
			PreviousRampedSpeed = RampedSpeed;
		}
		m_RampGraph.Scale(GRAPH_MAX_VALUES - 1);

		m_ZoomedInGraph.Init(0.0f, 0.0f);
		PreviousRampedSpeed = 1.0f;
		MiddleOfZoomedInGraph = m_SpeedTurningPoint;
		for(int64_t i = 0; i < GRAPH_MAX_VALUES; i++)
		{
			// This is a calculation of the speed values per second on the X axis, from (MiddleOfZoomedInGraph - 64 * StepSize) to (MiddleOfZoomedInGraph + 64 * StepSize)
			const float Speed = MiddleOfZoomedInGraph - 64 * StepSizeZoomedInGraph + i * StepSizeZoomedInGraph;
			const float Ramp = VelocityRamp(Speed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
			const float RampedSpeed = Speed * Ramp;
			if(RampedSpeed >= PreviousRampedSpeed)
			{
				m_ZoomedInGraph.InsertAt(i, RampedSpeed / 32, ColorRGBA(0.0f, 1.0f, 0.0f, 0.75f));
				m_SpeedTurningPoint = Speed;
			}
			else
			{
				m_ZoomedInGraph.InsertAt(i, RampedSpeed / 32, ColorRGBA(1.0f, 0.0f, 0.0f, 0.75f));
			}
			if(i == 0)
			{
				m_ZoomedInGraph.SetMin(RampedSpeed / 32);
			}
			PreviousRampedSpeed = RampedSpeed;
		}
		m_ZoomedInGraph.Scale(GRAPH_MAX_VALUES - 1);
	}

	const float GraphFontSize = 12.0f;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Velspeed.X * Ramp in Bps (Velspeed %d to %d)", StepSizeRampGraph / 32, 128 * StepSizeRampGraph / 32);
	m_RampGraph.Render(Graphics(), TextRender(), GraphX, GraphY, GraphW, GraphH, aBuf);
	str_format(aBuf, sizeof(aBuf), "Max Velspeed before it ramps off:  %.2f Bps", m_SpeedTurningPoint / 32);
	TextRender()->Text(GraphX, GraphY - GraphFontSize, GraphFontSize, aBuf);
	str_format(aBuf, sizeof(aBuf), "Zoomed in on turning point (Velspeed %d to %d)", ((int)MiddleOfZoomedInGraph - 64 * StepSizeZoomedInGraph) / 32, ((int)MiddleOfZoomedInGraph + 64 * StepSizeZoomedInGraph) / 32);
	m_ZoomedInGraph.Render(Graphics(), TextRender(), GraphX + GraphW + GraphSpacing, GraphY, GraphW, GraphH, aBuf);
}

void CDebugHud::RenderHint()
{
	if(!g_Config.m_Debug)
		return;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const float FontSize = 5.0f;
	const float Spacing = 5.0f;

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->Text(Spacing, Height - FontSize - Spacing, FontSize, Localize("Debug mode enabled. Press Ctrl+Shift+D to disable debug mode."));
}

void CDebugHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	RenderTuning();
	RenderNetCorrections();
	RenderHint();
}
