#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "rainbow.h"

template<typename T>
T color_lerp(T a, T b, float c)
{
	T result;
	for(size_t i = 0; i < 4; ++i)
		result[i] = a[i] + c * (b[i] - a[i]);
	return result;
}

void CRainbow::OnRender()
{
	if(!g_Config.m_ClRainbowTees && !g_Config.m_ClRainbowWeapon && !g_Config.m_ClRainbowHook)
		return;

	if(g_Config.m_ClRainbowMode == 0)
		return;

	static float Time = 0.0f;
	Time += Client()->RenderFrameTime() * ((float)g_Config.m_ClRainbowSpeed / 100.0f);
	float DefTick = std::fmod(Time, 1.0f);
	ColorRGBA Col;

	switch(g_Config.m_ClRainbowMode)
	{
	case COLORMODE_RAINBOW:
		Col = color_cast<ColorRGBA>(ColorHSLA(DefTick, 1.0f, 0.5f));
		break;
	case COLORMODE_PULSE:
		Col = color_cast<ColorRGBA>(ColorHSLA(std::fmod(std::floor(Time) * 0.1f, 1.0f), 1.0f, 0.5f + std::fabs(DefTick - 0.5f)));
		break;
	case COLORMODE_DARKNESS:
		Col = ColorRGBA(0.0f, 0.0f, 0.0f);
		break;
	case COLORMODE_RANDOM:
		static ColorHSLA Col1 = ColorHSLA(0.0f, 0.0f, 0.0f, 0.0f), Col2 = ColorHSLA(0.0f, 0.0f, 0.0f, 0.0f);
		if(Col2.a == 0.0f) // Create first target
			Col2 = ColorHSLA((float)rand() / (float)RAND_MAX, 1.0f, (float)rand() / (float)RAND_MAX, 1.0f);
		static float LastSwap = -INFINITY;
		if(Time - LastSwap > 1.0f) // Shift target to source, create new target
		{
			LastSwap = Time;
			Col1 = Col2;
			Col2 = ColorHSLA((float)rand() / (float)RAND_MAX, 1.0f, (float)rand() / (float)RAND_MAX, 1.0f);
		}
		Col = color_cast<ColorRGBA>(color_lerp(Col1, Col2, DefTick));
		break;
	}

	m_RainbowColor = Col;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;
		// check if local player
		bool Local = m_pClient->m_Snap.m_LocalClientId == i;
		CTeeRenderInfo *RenderInfo = &m_pClient->m_aClients[i].m_RenderInfo;

		// check if rainbow is enabled
		if(Local ? g_Config.m_ClRainbowTees : (g_Config.m_ClRainbowTees && g_Config.m_ClRainbowOthers))
		{
			RenderInfo->m_BloodColor = Col;
			RenderInfo->m_ColorBody = Col;
			RenderInfo->m_ColorFeet = Col;
			RenderInfo->m_CustomColoredSkin = true;
		}
	}
}
