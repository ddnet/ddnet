#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include <cmath>

#include "base/color.h"
#include "rainbow.h"

void CRainbow::TransformColor(unsigned char Mode, int Tick, CTeeRenderInfo *pInfo)
{
	if(!Mode)
		return;

	int Deftick = Tick % 255 + 1;
	if(Mode == COLORMODE_RAINBOW)
	{
		const ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA((float)Deftick / 255.0f, 1.0f, 0.5f));
		pInfo->m_CustomColoredSkin = true;
		pInfo->m_ColorBody = Col;
		pInfo->m_ColorFeet = Col;
		pInfo->m_BloodColor = Col;
		return;
	}
	else if(Mode == COLORMODE_PULSE)
	{
		float Light = 0.5f + (std::sin(((float)Deftick / 255.0f) * 2 * pi) + 1.f) / 4.f;
		pInfo->m_CustomColoredSkin = true;
		ColorHSLA Body = color_cast<ColorHSLA>(pInfo->m_ColorBody);
		Body.l = Light;
		pInfo->m_ColorBody = color_cast<ColorRGBA>(Body);
		pInfo->m_BloodColor = pInfo->m_ColorBody;
		ColorHSLA Feet = color_cast<ColorHSLA>(pInfo->m_ColorFeet);
		Feet.l = Light;
		pInfo->m_ColorFeet = color_cast<ColorRGBA>(Feet);
		return;
	}
	else if(Mode == COLORMODE_DARKNESS)
	{
		pInfo->m_CustomColoredSkin = true;
		pInfo->m_ColorBody = ColorRGBA(0.0f, 0.0f, 0.0f);
		pInfo->m_ColorFeet = ColorRGBA(0.0f, 0.0f, 0.0f);
		pInfo->m_BloodColor = ColorRGBA(0.0f, 0.0f, 0.0f);
		return;
	}
}

void CRainbow::OnRender()
{
	if(g_Config.m_ClRainbowOthers)
		for(auto &Client : m_pClient->m_aClients)
			TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, &Client.m_RenderInfo);
	else if(g_Config.m_ClRainbow)
		TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, &m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientId].m_RenderInfo);
}
