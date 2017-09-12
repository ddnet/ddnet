#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/graphics.h>

#include <game/client/components/camera.h>
#include <game/client/components/maplayers.h>
#include <game/client/components/mapimages.h>

#include "background.h"

CBackground::CBackground() : CMapLayers(CMapLayers::TYPE_BACKGROUND_FORCE)
{
	m_pLayers = new CLayers;
	m_pBackgroundLayers = m_pLayers;
	m_pImages = new CMapImages;
	m_pBackgroundImages = m_pImages;
	m_pMap = CreateEngineMap();
	m_pBackgroundMap = m_pMap;
	m_Loaded = false;
	m_aMapName[0] = '\0';
}

CBackground::~CBackground()
{
	delete m_pBackgroundLayers;
	delete m_pBackgroundImages;
}

void CBackground::OnInit()
{
	m_pImages->m_pClient = GameClient();
	Kernel()->ReregisterInterface(static_cast<IEngineMap*>(m_pMap));
	if(g_Config.m_ClBackgroundEntities[0] != '\0' && str_comp(g_Config.m_ClBackgroundEntities, CURRENT))
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
	m_pLayers = m_pBackgroundLayers;
	m_pImages = m_pBackgroundImages;

	str_copy(m_aMapName, g_Config.m_ClBackgroundEntities, sizeof(m_aMapName));
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "maps/%s", g_Config.m_ClBackgroundEntities);
	if(m_pMap->Load(aBuf))
	{
		m_pLayers->InitBackground(m_pMap);
		m_pImages->LoadBackground(m_pMap);
		RenderTools()->RenderTilemapGenerateSkip(m_pLayers);
		m_Loaded = true;
	}
	else if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT) == 0)
	{
		m_pMap = Kernel()->RequestInterface<IEngineMap>();
		m_pLayers = GameClient()->Layers();
		m_pImages = GameClient()->m_pMapimages;
		m_Loaded = true;
	}
	
	if(m_Loaded) 
		CMapLayers::OnMapLoad();

	m_LastLoad = time_get();
}

void CBackground::OnMapLoad()
{
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT) == 0 || str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
		LoadBackground();
}

void CBackground::OnRender()
{
	//probably not the best place for this
	if(g_Config.m_ClBackgroundEntities[0] != '\0' && str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
		LoadBackground();

	if(!m_Loaded)
		return;

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(g_Config.m_ClOverlayEntities != 100)
		return;
	
	CMapLayers::OnRender();
}
