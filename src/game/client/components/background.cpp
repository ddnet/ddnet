#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/graphics.h>

#include <game/client/components/camera.h>
#include <game/client/components/maplayers.h>
#include <game/client/components/mapimages.h>

#include "background.h"

CBackground::CBackground()
{
	m_pLayers = new CMapLayers(CMapLayers::TYPE_BACKGROUND);
	m_pLayers->m_pLayers = new CLayers;
	m_pImages = new CMapImages;
	m_pMap = CreateEngineMap();
	m_Loaded = false;
	m_aMapName[0] = '\0';
}

CBackground::~CBackground()
{
	delete m_pLayers->m_pLayers;
	delete m_pLayers;
	delete m_pImages;
}

void CBackground::OnInit()
{
	m_Loaded = false;
	m_pImages->m_pClient = GameClient();
	m_pLayers->m_pClient = GameClient();
	str_format(m_aMapName, sizeof(m_aMapName), "%s", g_Config.m_ClBackgroundEntities);
	if(Kernel()->ReregisterInterface(static_cast<IEngineMap*>(m_pMap)))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "maps/%s", g_Config.m_ClBackgroundEntities);
		if(m_pMap->Load(aBuf))
		{
			m_pLayers->m_pLayers->InitBackground(m_pMap);
			m_pImages->LoadBackground(m_pMap);
			str_format(m_aMapName, sizeof(m_aMapName), "%s", g_Config.m_ClBackgroundEntities);
			m_Loaded = true;
		}
		else if(!str_comp(g_Config.m_ClBackgroundEntities, "%current%"))
		{
			str_format(aBuf, sizeof(aBuf), "downloadedmaps/%s_%08x.map", Client()->GetCurrentMap(), Client()->GetCurrentMapCrc());
			if(m_pMap->Load(aBuf))
			{
				m_pLayers->m_pLayers->InitBackground(m_pMap);
				m_pImages->LoadBackground(m_pMap);
				str_format(m_aMapName, sizeof(m_aMapName), "%s", "%current%");
				m_Loaded = true;
			}
		}
	}
}

void CBackground::OnMapLoad()
{
	if(str_comp(g_Config.m_ClBackgroundEntities, "%current%") == 0)
		OnInit();
}

//code is from CMapLayers::OnRender()
void CBackground::OnRender()
{
	//probably not the best place for this
	if(str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
		OnInit();
		
	if(!m_Loaded)
		return;
		
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_ClOverlayEntities != 100)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = m_pClient->m_pCamera->m_Center;

	for(int g = 0; g < m_pLayers->m_pLayers->NumGroups(); g++)
	{
		CMapItemGroup *pGroup = m_pLayers->m_pLayers->GetGroup(g);
		
		if(!pGroup)
		{
			dbg_msg("MapLayers", "Error:Group was null, Group Number = %d, Total Groups = %d", g, m_pLayers->m_pLayers->NumGroups());
			dbg_msg("MapLayers", "This is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("MapLayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
		}
		
		// load group name
		if(pGroup->m_Version >= 3)
		{
			char Buf[32];
			IntsToStr(pGroup->m_aName, sizeof(Buf)/sizeof(int), Buf);
			if(str_comp(Buf, "Game") == 0)
				return;
		}

		if(!g_Config.m_GfxNoclip && pGroup->m_Version >= 2 && pGroup->m_UseClipping)
		{
			// set clipping
			float Points[4];
			m_pLayers->MapScreenToGroup(Center.x, Center.y, m_pLayers->m_pLayers->GameGroup(), m_pClient->m_pCamera->m_Zoom);
			Graphics()->GetScreen(&Points[0], &Points[1], &Points[2], &Points[3]);
			float x0 = (pGroup->m_ClipX - Points[0]) / (Points[2]-Points[0]);
			float y0 = (pGroup->m_ClipY - Points[1]) / (Points[3]-Points[1]);
			float x1 = ((pGroup->m_ClipX+pGroup->m_ClipW) - Points[0]) / (Points[2]-Points[0]);
			float y1 = ((pGroup->m_ClipY+pGroup->m_ClipH) - Points[1]) / (Points[3]-Points[1]);

			Graphics()->ClipEnable((int)(x0*Graphics()->ScreenWidth()), (int)(y0*Graphics()->ScreenHeight()),
				(int)((x1-x0)*Graphics()->ScreenWidth()), (int)((y1-y0)*Graphics()->ScreenHeight()));
		}

		if(!g_Config.m_ClZoomBackgroundLayers && !pGroup->m_ParallaxX && !pGroup->m_ParallaxY)
			m_pLayers->MapScreenToGroup(Center.x, Center.y, pGroup, 1.0);
		else
			m_pLayers->MapScreenToGroup(Center.x, Center.y, pGroup, m_pClient->m_pCamera->m_Zoom);
		
		if(g_Config.m_ClOverlayEntities == 100 && g_Config.m_ClBackgroundEntities)
		{
			for(int l = 0; l < pGroup->m_NumLayers; l++)
			{
				CMapItemLayer *pLayer = m_pLayers->m_pLayers->GetLayer(pGroup->m_StartLayer+l);
				// skip rendering if detail layers if not wanted
				if(pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail)
					continue;
					
				if(pLayer->m_Type == LAYERTYPE_TILES)
					continue;
				
				CMapItemLayerQuads *pQLayer = (CMapItemLayerQuads *)pLayer;
				if(pQLayer->m_Image == -1)
					Graphics()->TextureSet(-1);
				else
					Graphics()->TextureSet(m_pImages->Get(pQLayer->m_Image));

				CQuad *pQuads = (CQuad *)m_pMap->GetDataSwapped(pQLayer->m_Data);
				
				Graphics()->BlendNone();
				RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_OPAQUE, m_pLayers->EnvelopeEval, m_pLayers);
				Graphics()->BlendNormal();
				RenderTools()->ForceRenderQuads(pQuads, pQLayer->m_NumQuads, LAYERRENDERFLAG_TRANSPARENT, m_pLayers->EnvelopeEval, m_pLayers);
			}
		}
	}
	
	if(!g_Config.m_GfxNoclip)
		Graphics()->ClipDisable();

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}