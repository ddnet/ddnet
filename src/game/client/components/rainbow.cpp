#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "rainbow.h"

void CRainbow::TransformColor(unsigned char Mode, int Tick, CTeeRenderInfo *pinfo)
{
	if(!Mode)
		return;

	int DefTick = Tick % 255;

	const ColorHSLA PlayerColBody = ColorHSLA(g_Config.m_ClPlayerColorBody);
	const ColorHSLA PlayerColFeet = ColorHSLA(g_Config.m_ClPlayerColorFeet);

	pinfo->m_CustomColoredSkin = true;
	if(Mode == COLORMODE_RAINBOW)
	{
		const ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA((float)DefTick / 255.0f, 1.0f, 0.5f));
		pinfo->m_ColorBody = Col;
		pinfo->m_ColorFeet = Col;
		pinfo->m_BloodColor = Col;
	}
	else if(Mode == COLORMODE_PULSE)
	{
		pinfo->m_ColorBody.s = 1.0f;
		pinfo->m_ColorFeet.s = 1.0f;
		pinfo->m_BloodColor.s = 1.0f;
		pinfo->m_ColorBody.l = 0.5f + std::fabs(((float)DefTick / 255.0f) - 0.5f);
		pinfo->m_ColorFeet.l = 0.5f + std::fabs(((float)DefTick / 255.0f) - 0.5f);
		pinfo->m_BloodColor.l = 0.5f + std::fabs(((float)DefTick / 255.0f) - 0.5f);
		pinfo->m_ColorBody.h = (float)DefTick / 255.0f;
		pinfo->m_ColorFeet.h = (float)DefTick / 255.0f;
		pinfo->m_BloodColor.h = (float)DefTick / 255.0f;
	}
	else if(Mode == COLORMODE_DARKNESS)
	{
		pinfo->m_ColorBody = ColorRGBA(0.0f, 0.0f, 0.0f);
		pinfo->m_ColorFeet = ColorRGBA(0.0f, 0.0f, 0.0f);
		pinfo->m_BloodColor = ColorRGBA(0.0f, 0.0f, 0.0f);
	}
	else
	{
		pinfo->m_CustomColoredSkin = true;
		pinfo->m_ColorBody = color_cast<ColorRGBA>(PlayerColBody);
		pinfo->m_ColorFeet = color_cast<ColorRGBA>(PlayerColFeet);
		// pinfo->m_BloodColor = pinfo->m_BloodColor; // TODO reset blood color
	}
}

void CRainbow::OnRender()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// check if local player
		bool Local = m_pClient->m_Snap.m_LocalClientId == i;

		CTeeRenderInfo *RenderInfo = &m_pClient->m_aClients[i].m_RenderInfo;
		// check if rainbow is enabled
		if(g_Config.m_ClRainbow && Local) // rainbow is enabled and is own player
		{
			TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, RenderInfo);
		}
		else if(g_Config.m_ClRainbowOthers && !Local) // rainbow is enabled and is not own player
		{
			TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, RenderInfo);
		}
	}
}
