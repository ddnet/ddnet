#include <base/system.h>

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
	m_pBackgroundLayers = m_pLayers->m_pLayers;
	m_pImages = new CMapImages;
	m_pBackgroundImages = m_pImages;
	m_pMap = CreateEngineMap();
	m_pBackgroundMap = m_pMap;
	m_Loaded = false;
	m_aMapName[0] = '\0';
}

CBackground::~CBackground()
{
	if(m_pLayers->m_pLayers != GameClient()->Layers())
	{
		delete m_pLayers->m_pLayers;
		delete m_pLayers;
		delete m_pImages;
	}
}

void CBackground::OnInit()
{
	m_pImages->m_pClient = GameClient();
	m_pLayers->m_pClient = GameClient();
	Kernel()->ReregisterInterface(static_cast<IEngineMap*>(m_pMap));
	str_format(m_aMapName, sizeof(m_aMapName), "%s", g_Config.m_ClBackgroundEntities);
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT))
		LoadBackground();
}

void CBackground::LoadBackground()
{
	if(time_get()-m_LastLoad < 10*time_freq())
		return;

	if(m_Loaded && m_pMap == m_pBackgroundMap)
		m_pMap->Unload();

	m_Loaded = false;
	m_pMap = m_pBackgroundMap;
	m_pLayers->m_pLayers = m_pBackgroundLayers;
	m_pImages = m_pBackgroundImages;

	str_format(m_aMapName, sizeof(m_aMapName), "%s", g_Config.m_ClBackgroundEntities);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "maps/%s", g_Config.m_ClBackgroundEntities);
	if(m_pMap->Load(aBuf))
	{
		m_pLayers->m_pLayers->InitBackground(m_pMap);
		m_pImages->LoadBackground(m_pMap);
		RenderTools()->RenderTilemapGenerateSkip(m_pLayers->m_pLayers);
		m_Loaded = true;
	}
	else if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT) == 0)
	{
		m_pMap = Kernel()->RequestInterface<IEngineMap>();
		m_pLayers->m_pLayers = GameClient()->Layers();
		m_pImages = GameClient()->m_pMapimages;
		m_Loaded = true;
	}

	m_LastLoad = time_get();
}

void CBackground::OnMapLoad()
{
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT) == 0 || str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
		LoadBackground();
}

//code is from CMapLayers::OnRender()
void CBackground::OnRender()
{
	//probably not the best place for this
	if(str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
		LoadBackground();

	if(!m_Loaded)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_ClOverlayEntities != 100)
		return;

	CUIRect Screen;
	Graphics()->GetScreen(&Screen.x, &Screen.y, &Screen.w, &Screen.h);

	vec2 Center = m_pClient->m_pCamera->m_Center;

	bool PassedGameLayer = false;

	for(int g = 0; g < m_pLayers->m_pLayers->NumGroups() && !PassedGameLayer; g++)
	{
		CMapItemGroup *pGroup = m_pLayers->m_pLayers->GetGroup(g);

		if(!pGroup)
		{
			dbg_msg("MapLayers", "Error:Group was null, Group Number = %d, Total Groups = %d", g, m_pLayers->m_pLayers->NumGroups());
			dbg_msg("MapLayers", "This is here to prevent a crash but the source of this is unknown, please report this for it to get fixed");
			dbg_msg("MapLayers", "we need mapname and crc and the map that caused this if possible, and anymore info you think is relevant");
			continue;
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

		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = m_pLayers->m_pLayers->GetLayer(pGroup->m_StartLayer+l);
			// skip rendering if detail layers if not wanted
			if(pLayer->m_Flags&LAYERFLAG_DETAIL && !g_Config.m_GfxHighDetail)
				continue;

			if(pLayer == (CMapItemLayer*)m_pLayers->m_pLayers->GameLayer())
			{
				PassedGameLayer = true;
				break;
			}

			if(pLayer->m_Type == LAYERTYPE_TILES && g_Config.m_ClBackgroundShowTilesLayers)
			{
				CMapItemLayerTilemap *pTMap = (CMapItemLayerTilemap *)pLayer;
				if(pTMap->m_Image == -1)
					Graphics()->TextureSet(-1);
				else
					Graphics()->TextureSet(m_pImages->Get(pTMap->m_Image));

				CTile *pTiles = (CTile *)m_pMap->GetData(pTMap->m_Data);
				unsigned int Size = m_pMap->GetUncompressedDataSize(pTMap->m_Data);

				if (Size >= pTMap->m_Width*pTMap->m_Height*sizeof(CTile))
				{
					Graphics()->BlendNone();
					vec4 Color = vec4(pTMap->m_Color.r/255.0f, pTMap->m_Color.g/255.0f, pTMap->m_Color.b/255.0f, pTMap->m_Color.a/255.0f);
					RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE,
													m_pLayers->EnvelopeEval, m_pLayers, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
					Graphics()->BlendNormal();
					RenderTools()->RenderTilemap(pTiles, pTMap->m_Width, pTMap->m_Height, 32.0f, Color, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT,
													m_pLayers->EnvelopeEval, m_pLayers, pTMap->m_ColorEnv, pTMap->m_ColorEnvOffset);
				}
			}
			else if(pLayer->m_Type == LAYERTYPE_QUADS && g_Config.m_ClShowQuads)
			{
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
		if(!g_Config.m_GfxNoclip)
			Graphics()->ClipDisable();
	}

	// reset the screen like it was before
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
}
