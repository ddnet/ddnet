/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "map.h"

#include <engine/storage.h>

#include <game/mapitems.h>

CMap::CMap() = default;

void *CMap::GetData(int Index)
{
	return m_DataFile.GetData(Index);
}

int CMap::GetDataSize(int Index) const
{
	return m_DataFile.GetDataSize(Index);
}

void *CMap::GetDataSwapped(int Index)
{
	return m_DataFile.GetDataSwapped(Index);
}

void CMap::UnloadData(int Index)
{
	m_DataFile.UnloadData(Index);
}

int CMap::NumData() const
{
	return m_DataFile.NumData();
}

void *CMap::GetItem(int Index, int *pType, int *pID)
{
	return m_DataFile.GetItem(Index, pType, pID);
}

int CMap::GetItemSize(int Index)
{
	return m_DataFile.GetItemSize(Index);
}

void CMap::GetType(int Type, int *pStart, int *pNum)
{
	m_DataFile.GetType(Type, pStart, pNum);
}

void *CMap::FindItem(int Type, int ID)
{
	return m_DataFile.FindItem(Type, ID);
}

int CMap::NumItems() const
{
	return m_DataFile.NumItems();
}

bool CMap::Load(const char *pMapName)
{
	IStorage *pStorage = Kernel()->RequestInterface<IStorage>();
	if(!pStorage)
		return false;
	if(!m_DataFile.Open(pStorage, pMapName, IStorage::TYPE_ALL))
		return false;
	// check version
	const CMapItemVersion *pItem = (CMapItemVersion *)m_DataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(!pItem || pItem->m_Version != CMapItemVersion::CURRENT_VERSION)
		return false;

	// replace compressed tile layers with uncompressed ones
	int GroupsStart, GroupsNum, LayersStart, LayersNum;
	m_DataFile.GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	m_DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);
	for(int g = 0; g < GroupsNum; g++)
	{
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(m_DataFile.GetItem(GroupsStart + g));
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(m_DataFile.GetItem(LayersStart + pGroup->m_StartLayer + l));
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
				if(pTilemap->m_Version >= CMapItemLayerTilemap::TILE_SKIP_MIN_VERSION)
				{
					const size_t TilemapSize = (size_t)pTilemap->m_Width * pTilemap->m_Height * sizeof(CTile);
					CTile *pTiles = static_cast<CTile *>(malloc(TilemapSize));
					ExtractTiles(pTiles, (size_t)pTilemap->m_Width * pTilemap->m_Height, static_cast<CTile *>(m_DataFile.GetData(pTilemap->m_Data)), m_DataFile.GetDataSize(pTilemap->m_Data) / sizeof(CTile));
					m_DataFile.ReplaceData(pTilemap->m_Data, reinterpret_cast<char *>(pTiles), TilemapSize);
				}
			}
		}
	}

	return true;
}

void CMap::Unload()
{
	m_DataFile.Close();
}

bool CMap::IsLoaded() const
{
	return m_DataFile.IsOpen();
}

IOHANDLE CMap::File() const
{
	return m_DataFile.File();
}

SHA256_DIGEST CMap::Sha256() const
{
	return m_DataFile.Sha256();
}

unsigned CMap::Crc() const
{
	return m_DataFile.Crc();
}

int CMap::MapSize() const
{
	return m_DataFile.MapSize();
}

void CMap::ExtractTiles(CTile *pDest, size_t DestSize, const CTile *pSrc, size_t SrcSize)
{
	size_t DestIndex = 0;
	size_t SrcIndex = 0;
	while(DestIndex < DestSize && SrcIndex < SrcSize)
	{
		for(unsigned Counter = 0; Counter <= pSrc[SrcIndex].m_Skip && DestIndex < DestSize; Counter++)
		{
			pDest[DestIndex] = pSrc[SrcIndex];
			pDest[DestIndex].m_Skip = 0;
			DestIndex++;
		}
		SrcIndex++;
	}
}

extern IEngineMap *CreateEngineMap() { return new CMap; }
