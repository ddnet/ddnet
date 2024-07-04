/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "map.h"

#include <base/log.h>

#include <engine/storage.h>

#include <game/mapitems.h>

CMap::CMap() = default;

int CMap::GetDataSize(int Index) const
{
	return m_DataFile.GetDataSize(Index);
}

void *CMap::GetData(int Index)
{
	return m_DataFile.GetData(Index);
}

void *CMap::GetDataSwapped(int Index)
{
	return m_DataFile.GetDataSwapped(Index);
}

const char *CMap::GetDataString(int Index)
{
	return m_DataFile.GetDataString(Index);
}

void CMap::UnloadData(int Index)
{
	m_DataFile.UnloadData(Index);
}

int CMap::NumData() const
{
	return m_DataFile.NumData();
}

int CMap::GetItemSize(int Index)
{
	return m_DataFile.GetItemSize(Index);
}

void *CMap::GetItem(int Index, int *pType, int *pId)
{
	return m_DataFile.GetItem(Index, pType, pId);
}

void CMap::GetType(int Type, int *pStart, int *pNum)
{
	m_DataFile.GetType(Type, pStart, pNum);
}

int CMap::FindItemIndex(int Type, int Id)
{
	return m_DataFile.FindItemIndex(Type, Id);
}

void *CMap::FindItem(int Type, int Id)
{
	return m_DataFile.FindItem(Type, Id);
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

	// Ensure current datafile is not left in an inconsistent state if loading fails,
	// by loading the new datafile separately first.
	CDataFileReader NewDataFile;
	if(!NewDataFile.Open(pStorage, pMapName, IStorage::TYPE_ALL))
		return false;

	// Check version
	const CMapItemVersion *pItem = (CMapItemVersion *)NewDataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(pItem == nullptr || pItem->m_Version != CMapItemVersion::CURRENT_VERSION)
	{
		log_error("map/load", "Error: map version not supported.");
		NewDataFile.Close();
		return false;
	}

	// Replace compressed tile layers with uncompressed ones
	int GroupsStart, GroupsNum, LayersStart, LayersNum;
	NewDataFile.GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	NewDataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);
	for(int g = 0; g < GroupsNum; g++)
	{
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(NewDataFile.GetItem(GroupsStart + g));
		for(int l = 0; l < pGroup->m_NumLayers; l++)
		{
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(NewDataFile.GetItem(LayersStart + pGroup->m_StartLayer + l));
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);
				if(pTilemap->m_Version >= CMapItemLayerTilemap::TILE_SKIP_MIN_VERSION)
				{
					const size_t TilemapCount = (size_t)pTilemap->m_Width * pTilemap->m_Height;
					const size_t TilemapSize = TilemapCount * sizeof(CTile);

					if(((int)TilemapCount / pTilemap->m_Width != pTilemap->m_Height) || (TilemapSize / sizeof(CTile) != TilemapCount))
					{
						log_error("map/load", "map layer too big (%d * %d * %d causes an integer overflow)", pTilemap->m_Width, pTilemap->m_Height, sizeof(CTile));
						return false;
					}
					CTile *pTiles = static_cast<CTile *>(malloc(TilemapSize));
					if(!pTiles)
						return false;
					ExtractTiles(pTiles, (size_t)pTilemap->m_Width * pTilemap->m_Height, static_cast<CTile *>(NewDataFile.GetData(pTilemap->m_Data)), NewDataFile.GetDataSize(pTilemap->m_Data) / sizeof(CTile));
					NewDataFile.ReplaceData(pTilemap->m_Data, reinterpret_cast<char *>(pTiles), TilemapSize);
				}
			}
		}
	}

	// Replace existing datafile with new datafile
	m_DataFile.Close();
	m_DataFile = std::move(NewDataFile);
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
