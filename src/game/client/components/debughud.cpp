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

void CDebugHud::RenderNetCorrections()
{
	if(!g_Config.m_Debug || g_Config.m_DbgGraphs || !m_pClient->m_Snap.m_pLocalCharacter || !m_pClient->m_Snap.m_pLocalPrevCharacter)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);

	const float TicksPerSecond = 50.0f;
	float Velspeed = length(vec2(m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f, m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f)) * TicksPerSecond;
	float VelspeedX = m_pClient->m_Snap.m_pLocalCharacter->m_VelX / 256.0f * TicksPerSecond;
	float VelspeedY = m_pClient->m_Snap.m_pLocalCharacter->m_VelY / 256.0f * TicksPerSecond;
	float Ramp = VelocityRamp(Velspeed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);

	static const char *s_apStrings[] = {"velspeed:", "velspeed.x*ramp:", "velspeed.y:", "ramp:", "checkpoint:", "Pos", " x:", " y:", "angle:", "netobj corrections", " num:", " on:"};
	const int Num = std::size(s_apStrings);
	const float LineHeight = 6.0f;
	const float Fontsize = 5.0f;

	float x = Width - 100.0f, y = 50.0f;
	for(int i = 0; i < Num; ++i)
		TextRender()->Text(0, x, y + i * LineHeight, Fontsize, s_apStrings[i], -1.0f);

	x = Width - 10.0f;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%.0f Bps", Velspeed / 32);
	float w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.0f Bps", VelspeedX / 32 * Ramp);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.0f Bps", VelspeedY / 32);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.2f", Ramp);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	const CCharacter *pCharacter = m_pClient->m_GameWorld.GetCharacterByID(m_pClient->m_Snap.m_LocalClientID);
	if(pCharacter)
		str_format(aBuf, sizeof(aBuf), "%d", pCharacter->m_TeleCheckpoint);
	else
		str_copy(aBuf, "-1");
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += 2 * LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.2f", m_pClient->m_Snap.m_pLocalCharacter->m_X / 32.0f);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%.2f", m_pClient->m_Snap.m_pLocalCharacter->m_Y / 32.0f);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	str_format(aBuf, sizeof(aBuf), "%d", m_pClient->m_Snap.m_pLocalCharacter->m_Angle);
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += 2 * LineHeight;
	str_format(aBuf, sizeof(aBuf), "%d", m_pClient->NetobjNumCorrections());
	w = TextRender()->TextWidth(0, Fontsize, aBuf, -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, aBuf, -1.0f);
	y += LineHeight;
	w = TextRender()->TextWidth(0, Fontsize, m_pClient->NetobjCorrectedOn(), -1, -1.0f);
	TextRender()->Text(0, x - w, y, Fontsize, m_pClient->NetobjCorrectedOn(), -1.0f);
}

void CDebugHud::RenderTuning()
{
	// render tuning debugging
	if(!g_Config.m_DbgTuning)
		return;

	CTuningParams StandardTuning;

	Graphics()->MapScreen(0, 0, 300 * Graphics()->ScreenAspect(), 300);

	float y = 27.0f;
	int Count = 0;
	for(int i = 0; i < CTuningParams::Num(); i++)
	{
		char aBuf[128];
		float Current, Standard;
		m_pClient->m_aTuning[g_Config.m_ClDummy].Get(i, &Current);
		StandardTuning.Get(i, &Standard);

		if(Standard == Current)
			TextRender()->TextColor(1, 1, 1, 1.0f);
		else
			TextRender()->TextColor(1, 0.25f, 0.25f, 1.0f);

		float w;
		float x = 5.0f;

		str_format(aBuf, sizeof(aBuf), "%.2f", Standard);
		x += 20.0f;
		w = TextRender()->TextWidth(0, 5, aBuf, -1, -1.0f);
		TextRender()->Text(0x0, x - w, y + Count * 6, 5, aBuf, -1.0f);

		str_format(aBuf, sizeof(aBuf), "%.2f", Current);
		x += 20.0f;
		w = TextRender()->TextWidth(0, 5, aBuf, -1, -1.0f);
		TextRender()->Text(0x0, x - w, y + Count * 6, 5, aBuf, -1.0f);

		x += 5.0f;
		TextRender()->Text(0x0, x, y + Count * 6, 5, CTuningParams::Name(i), -1.0f);

		Count++;
	}

	// Rander Velspeed.X*Ramp Graphs
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	float GraphW = Graphics()->ScreenWidth() / 4.0f;
	float GraphH = Graphics()->ScreenHeight() / 6.0f;
	float sp = Graphics()->ScreenWidth() / 100.0f;
	float GraphX = GraphW;
	float GraphY = Graphics()->ScreenHeight() - GraphH - sp;

	CTuningParams *pClientTuning = &m_pClient->m_aTuning[g_Config.m_ClDummy];
	const int StepSizeRampGraph = 270;
	const int StepSizeZoomedInGraph = 14;
	if(m_OldVelrampStart != pClientTuning->m_VelrampStart || m_OldVelrampRange != pClientTuning->m_VelrampRange || m_OldVelrampCurvature != pClientTuning->m_VelrampCurvature)
	{
		m_OldVelrampStart = pClientTuning->m_VelrampStart;
		m_OldVelrampRange = pClientTuning->m_VelrampRange;
		m_OldVelrampCurvature = pClientTuning->m_VelrampCurvature;

		m_RampGraph.Init(0.0f, 0.0f);
		m_SpeedTurningPoint = 0;
		float pv = 1;
		for(size_t i = 0; i < CGraph::MAX_VALUES; i++)
		{
			// This is a calculation of the speed values per second on the X axis, from 270 to 34560 in steps of 270
			float Speed = (i + 1) * StepSizeRampGraph;
			float Ramp = VelocityRamp(Speed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
			float RampedSpeed = Speed * Ramp;
			if(RampedSpeed >= pv)
			{
				m_RampGraph.InsertAt(i, RampedSpeed / 32, 0, 1, 0);
				m_SpeedTurningPoint = Speed;
			}
			else
			{
				m_RampGraph.InsertAt(i, RampedSpeed / 32, 1, 0, 0);
			}
			pv = RampedSpeed;
		}
		m_RampGraph.Scale();

		m_ZoomedInGraph.Init(0.0f, 0.0f);
		pv = 1;
		MiddleOfZoomedInGraph = m_SpeedTurningPoint;
		for(size_t i = 0; i < CGraph::MAX_VALUES; i++)
		{
			// This is a calculation of the speed values per second on the X axis, from (MiddleOfZoomedInGraph - 64 * StepSize) to (MiddleOfZoomedInGraph + 64 * StepSize)
			float Speed = MiddleOfZoomedInGraph - 64 * StepSizeZoomedInGraph + i * StepSizeZoomedInGraph;
			float Ramp = VelocityRamp(Speed, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampStart, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampRange, m_pClient->m_aTuning[g_Config.m_ClDummy].m_VelrampCurvature);
			float RampedSpeed = Speed * Ramp;
			if(RampedSpeed >= pv)
			{
				m_ZoomedInGraph.InsertAt(i, RampedSpeed / 32, 0, 1, 0);
				m_SpeedTurningPoint = Speed;
			}
			else
			{
				m_ZoomedInGraph.InsertAt(i, RampedSpeed / 32, 1, 0, 0);
			}
			if(i == 0)
			{
				m_ZoomedInGraph.SetMin(RampedSpeed);
			}
			pv = RampedSpeed;
		}
		m_ZoomedInGraph.Scale();
	}
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Velspeed.X*Ramp in Bps (Velspeed %d to %d)", StepSizeRampGraph / 32, 128 * StepSizeRampGraph / 32);
	m_RampGraph.Render(Graphics(), Client()->GetDebugFont(), GraphX, GraphY - GraphH - sp, GraphW, GraphH, aBuf);
	str_format(aBuf, sizeof(aBuf), "Max Velspeed before it ramps off:  %.2f Bps", m_SpeedTurningPoint / 32);
	TextRender()->Text(0x0, GraphX, GraphY - sp - GraphH - 12, 12, aBuf, -1.0f);
	str_format(aBuf, sizeof(aBuf), "Zoomed in on turning point (Velspeed %d to %d)", ((int)MiddleOfZoomedInGraph - 64 * StepSizeZoomedInGraph) / 32, ((int)MiddleOfZoomedInGraph + 64 * StepSizeZoomedInGraph) / 32);
	m_ZoomedInGraph.Render(Graphics(), Client()->GetDebugFont(), GraphX, GraphY, GraphW, GraphH, aBuf);
	TextRender()->TextColor(1, 1, 1, 1);
}

void CDebugHud::RenderHint()
{
	if(!g_Config.m_Debug)
		return;

	float Width = 300 * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0, 0, Width, 300);
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->Text(0x0, 5, 290, 5, Localize("Debug mode enabled. Press Ctrl+Shift+D to disable debug mode."), -1.0f);
}

void CDebugHud::OnRender()
{
	RenderTuning();
	RenderNetCorrections();
	RenderHint();
}
