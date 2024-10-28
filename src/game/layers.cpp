/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "layers.h"

#include "mapitems.h"

#include <engine/map.h>

CLayers::CLayers()
{
	Unload();
}

void CLayers::Init(IMap *pMap, bool GameOnly)
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
			bool IsEntities = false;

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

				IsEntities = true;
			}

			if(!GameOnly)
			{
				if(pTilemap->m_Flags & TILESLAYERFLAG_TELE)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Tele = *((int *)(pTilemap) + 15);
					}
					m_pTeleLayer = pTilemap;
					IsEntities = true;
				}

				if(pTilemap->m_Flags & TILESLAYERFLAG_SPEEDUP)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Speedup = *((int *)(pTilemap) + 16);
					}
					m_pSpeedupLayer = pTilemap;
					IsEntities = true;
				}

				if(pTilemap->m_Flags & TILESLAYERFLAG_FRONT)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Front = *((int *)(pTilemap) + 17);
					}
					m_pFrontLayer = pTilemap;
					IsEntities = true;
				}

				if(pTilemap->m_Flags & TILESLAYERFLAG_SWITCH)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Switch = *((int *)(pTilemap) + 18);
					}
					m_pSwitchLayer = pTilemap;
					IsEntities = true;
				}

				if(pTilemap->m_Flags & TILESLAYERFLAG_TUNE)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Tune = *((int *)(pTilemap) + 19);
					}
					m_pTuneLayer = pTilemap;
					IsEntities = true;
				}
			}

			if(IsEntities)
			{
				// Ensure default color for entities layers
				pTilemap->m_Color = CColor(255, 255, 255, 255);
			}
		}
	}

	InitTilemapSkip();
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
			CTile *pTiles = static_cast<CTile *>(m_pMap->GetData(pTilemap->m_Data));
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
