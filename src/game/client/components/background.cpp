#include <base/system.h>

#include <engine/graphics.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <game/client/components/camera.h>
#include <game/client/components/mapimages.h>
#include <game/client/components/maplayers.h>

#include "background.h"

CBackground::CBackground(int MapType, bool OnlineOnly) :
	CMapLayers(MapType, OnlineOnly)
{
	m_pLayers = new CLayers;
	m_pBackgroundLayers = m_pLayers;
	m_pImages = new CMapImages;
	m_pBackgroundImages = m_pImages;
	m_Loaded = false;
	m_aMapName[0] = '\0';
	m_LastLoad = 0;
}

CBackground::~CBackground()
{
	delete m_pBackgroundLayers;
	delete m_pBackgroundImages;
}

CBackgroundEngineMap *CBackground::CreateBGMap()
{
	return new CBackgroundEngineMap;
}

void CBackground::OnInit()
{
	m_pBackgroundMap = CreateBGMap();
	m_pMap = m_pBackgroundMap;

	m_pImages->m_pClient = GameClient();
	Kernel()->RegisterInterface(m_pBackgroundMap);
	if(g_Config.m_ClBackgroundEntities[0] != '\0' && str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP))
		LoadBackground();
}

void CBackground::LoadBackground()
{
	if(time_get() - m_LastLoad < 10 * time_freq())
		return;

	if(m_Loaded && m_pMap == m_pBackgroundMap)
		m_pMap->Unload();

	m_Loaded = false;
	m_pMap = m_pBackgroundMap;
	m_pLayers = m_pBackgroundLayers;
	m_pImages = m_pBackgroundImages;

	bool NeedImageLoading = false;

	str_copy(m_aMapName, g_Config.m_ClBackgroundEntities, sizeof(m_aMapName));
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "maps/%s", g_Config.m_ClBackgroundEntities);
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0)
	{
		m_pMap = Kernel()->RequestInterface<IEngineMap>();
		if(m_pMap->IsLoaded())
		{
			m_pLayers = GameClient()->Layers();
			m_pImages = GameClient()->m_pMapimages;
			m_Loaded = true;
		}
	}
	else if(m_pMap->Load(aBuf))
	{
		m_pLayers->InitBackground(m_pMap);
		RenderTools()->RenderTilemapGenerateSkip(m_pLayers);
		NeedImageLoading = true;
		m_Loaded = true;
	}

	if(m_Loaded)
	{
		CMapLayers::OnMapLoad();
		if(NeedImageLoading)
			m_pImages->LoadBackground(m_pLayers, m_pMap);
	}

	m_LastLoad = time_get();
}

void CBackground::OnMapLoad()
{
	if(str_comp(g_Config.m_ClBackgroundEntities, CURRENT_MAP) == 0 || str_comp(g_Config.m_ClBackgroundEntities, m_aMapName))
	{
		m_LastLoad = 0;
		LoadBackground();
	}
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
