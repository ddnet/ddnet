/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layers.h"

#include "mapitems.h"

#include <engine/map.h>

CLayers::CLayers()
{
	Unload();
}

void CLayers::Init(IMap *pMap, bool GameOnly, bool InitializeTilemapSkip)
{
	Unload();

	m_pMap = pMap;
	m_pMap->GetType(MAPITEMTYPE_GROUP, &m_GroupsStart, &m_GroupsNum);
	m_pMap->GetType(MAPITEMTYPE_LAYER, &m_LayersStart, &m_LayersNum);

	for(int GroupIndex = 0; GroupIndex < NumGroups(); GroupIndex++)
	{
		CMapItemGroup *pGroup = GetGroup(GroupIndex);
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer + LayerIndex);
			if(pLayer->m_Type != LAYERTYPE_TILES)
				continue;

			CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);

			if(pTilemap->m_Flags & TILESLAYERFLAG_GAME)
			{
				m_pGameLayer = pTilemap;
				m_pGameGroup = pGroup;

				// make sure the game group has standard settings
				m_pGameGroup->m_OffsetX = 0;
				m_pGameGroup->m_OffsetY = 0;
				m_pGameGroup->m_ParallaxX = 100;
				m_pGameGroup->m_ParallaxY = 100;

				if(m_pGameGroup->m_Version >= 2)
				{
					m_pGameGroup->m_UseClipping = 0;
					m_pGameGroup->m_ClipX = 0;
					m_pGameGroup->m_ClipY = 0;
					m_pGameGroup->m_ClipW = 0;
					m_pGameGroup->m_ClipH = 0;
				}
			}
			else if(!GameOnly)
			{
				if(pTilemap->m_Flags & TILESLAYERFLAG_TELE)
				{
					m_pTeleLayer = pTilemap;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_SPEEDUP)
				{
					m_pSpeedupLayer = pTilemap;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_FRONT)
				{
					m_pFrontLayer = pTilemap;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_SWITCH)
				{
					m_pSwitchLayer = pTilemap;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_TUNE)
				{
					m_pTuneLayer = pTilemap;
				}
			}
		}
	}

	if(InitializeTilemapSkip)
	{
		InitTilemapSkip();
	}
}

void CLayers::Unload()
{
	m_GroupsNum = 0;
	m_GroupsStart = 0;
	m_LayersNum = 0;
	m_LayersStart = 0;

	m_pGameGroup = nullptr;
	m_pGameLayer = nullptr;
	m_pMap = nullptr;

	m_pTeleLayer = nullptr;
	m_pSpeedupLayer = nullptr;
	m_pFrontLayer = nullptr;
	m_pSwitchLayer = nullptr;
	m_pTuneLayer = nullptr;
}

void CLayers::InitTilemapSkip()
{
	for(int GroupIndex = 0; GroupIndex < NumGroups(); GroupIndex++)
	{
		const CMapItemGroup *pGroup = GetGroup(GroupIndex);
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			const CMapItemLayer *pLayer = GetLayer(pGroup->m_StartLayer + LayerIndex);
			if(pLayer->m_Type != LAYERTYPE_TILES)
				continue;

			const CMapItemLayerTilemap *pTilemap = reinterpret_cast<const CMapItemLayerTilemap *>(pLayer);
			// Physics layers except the game layer store their tiles in their own data index,
			// so their m_Data is neither used nor validated when the map is loaded.
			if((pTilemap->m_Flags & (TILESLAYERFLAG_TELE | TILESLAYERFLAG_SPEEDUP | TILESLAYERFLAG_FRONT | TILESLAYERFLAG_SWITCH | TILESLAYERFLAG_TUNE)) != 0)
				continue;

			CTile *pTiles = static_cast<CTile *>(m_pMap->GetData(pTilemap->m_Data));
			if(pTiles == nullptr)
			{
				continue;
			}

			for(int y = 0; y < pTilemap->m_Height; y++)
			{
				for(int x = 1; x < pTilemap->m_Width;)
				{
					int SkippedX;
					for(SkippedX = 1; x + SkippedX < pTilemap->m_Width && SkippedX < 255; SkippedX++)
					{
						if(pTiles[y * pTilemap->m_Width + x + SkippedX].m_Index)
							break;
					}

					pTiles[y * pTilemap->m_Width + x].m_Skip = SkippedX - 1;
					x += SkippedX;
				}
			}
		}
	}
}

CMapItemGroup *CLayers::GetGroup(int Index) const
{
	return static_cast<CMapItemGroup *>(m_pMap->GetItem(m_GroupsStart + Index));
}

CMapItemLayer *CLayers::GetLayer(int Index) const
{
	return static_cast<CMapItemLayer *>(m_pMap->GetItem(m_LayersStart + Index));
}
