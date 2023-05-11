#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "rainbow.h"

void CRainbow::TransformColor(unsigned char mode, int tick, CTeeRenderInfo *pinfo)
{
    if (!mode)
        return;

    int deftick = tick % 255;
 
    const ColorHSLA playercolbody = ColorHSLA(g_Config.m_ClPlayerColorBody);
    const ColorHSLA playercolfeet = ColorHSLA(g_Config.m_ClPlayerColorFeet);
 
    const ColorRGBA col = color_cast<ColorRGBA>(ColorHSLA((float)deftick / 255.0f, 1.0f, 0.5f));
    if (mode == COLORMODE_RAINBOW)
    {
        pinfo->m_CustomColoredSkin = true;
        pinfo->m_ColorBody = col;
        pinfo->m_ColorFeet = col;
        pinfo->m_BloodColor = col;
        return;
    }else if (mode == COLORMODE_PULSE)
    {
        pinfo->m_CustomColoredSkin = true;
        pinfo->m_ColorBody.s = 1.0f;
        pinfo->m_ColorFeet.s = 1.0f;
        pinfo->m_BloodColor.s = 1.0f;
        pinfo->m_ColorBody.l = 0.5f + fabs(((float)deftick / 255.0f) - 0.5f);
        pinfo->m_ColorFeet.l = 0.5f + fabs(((float)deftick / 255.0f) - 0.5f);
        pinfo->m_BloodColor.l = 0.5f + fabs(((float)deftick / 255.0f) - 0.5f);
        
        pinfo->m_ColorBody.h = (float)deftick / 255.0f;
        pinfo->m_ColorFeet.h = (float)deftick / 255.0f;
        pinfo->m_BloodColor.h = (float)deftick / 255.0f;

        return;
    }else if (mode == COLORMODE_DARKNESS)
    {
        pinfo->m_CustomColoredSkin = true;
        pinfo->m_ColorBody = ColorRGBA(0.0f, 0.0f, 0.0f);
        pinfo->m_ColorFeet = ColorRGBA(0.0f, 0.0f, 0.0f);
        pinfo->m_BloodColor = ColorRGBA(0.0f, 0.0f, 0.0f);
        return;
    }else{
        pinfo->m_CustomColoredSkin = true;
        pinfo->m_ColorBody = color_cast<ColorRGBA>(playercolbody);
        pinfo->m_ColorFeet = color_cast<ColorRGBA>(playercolfeet);
        pinfo->m_BloodColor = pinfo->m_BloodColor;
        return;
    }
}

void CRainbow::OnRender()
{

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // check if local player
        bool Local = m_pClient->m_Snap.m_LocalClientID == i;

        CTeeRenderInfo *RenderInfo = &m_pClient->m_aClients[i].m_RenderInfo;
        // check if rainbow is enabled
        if (g_Config.m_ClRainbow && Local) // rainbow is enabled and is own player
        {
            TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, RenderInfo);
        }else if(g_Config.m_ClRainbowOthers && !Local) // rainbow is enabled and is not own player
        {
            TransformColor(g_Config.m_ClRainbowMode, m_pClient->m_GameWorld.m_GameTick, RenderInfo);
        }
       
    }

}
