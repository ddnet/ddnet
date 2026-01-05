/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "map.h"

#include <base/log.h>
#include <base/system.h>

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
	if(pItem == nullptr || pItem->m_Version != 1)
	{
		log_error("map/load", "Map version not supported.");
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

				const size_t TilemapCount = (size_t)pTilemap->m_Width * pTilemap->m_Height;
				const size_t TilemapSize = TilemapCount * sizeof(CTile);

				if(((int)TilemapCount / pTilemap->m_Width != pTilemap->m_Height) || (TilemapSize / sizeof(CTile) != TilemapCount))
				{
					log_error("map/load", "Map layer %d in group %d is too big (%d * %d * %d causes an integer overflow).",
						l, g, pTilemap->m_Width, pTilemap->m_Height, (int)sizeof(CTile));
					return false;
				}

				const CTile *pSavedTiles = static_cast<CTile *>(NewDataFile.GetData(pTilemap->m_Data));
				const size_t SavedTilesSize = NewDataFile.GetDataSize(pTilemap->m_Data) / sizeof(CTile);

				if(pSavedTiles == nullptr)
				{
					log_error("map/load", "Tile data of layer %d in group %d is missing or corrupted.", l, g);
					return false;
				}
				else if(pTilemap->m_Version >= CMapItemLayerTilemap::VERSION_TEEWORLDS_TILESKIP)
				{
					CTile *pTiles = static_cast<CTile *>(malloc(TilemapSize));
					if(!pTiles)
					{
						log_error("map/load", "Failed to allocate memory for layer %d in group %d (size %d * %d).",
							l, g, pTilemap->m_Width, pTilemap->m_Height);
						return false;
					}
					else if(!ExtractTiles(pTiles, (size_t)pTilemap->m_Width * pTilemap->m_Height, pSavedTiles, SavedTilesSize))
					{
						free(pTiles);
						log_error("map/load", "Failed to extract tiles of layer %d in group %d.", l, g);
						return false;
					}
					NewDataFile.ReplaceData(pTilemap->m_Data, reinterpret_cast<char *>(pTiles), TilemapSize);
				}
				else if(SavedTilesSize < TilemapCount)
				{
					log_error("map/load", "Tile data of layer %d in group %d is truncated (got %" PRIzu ", wanted %" PRIzu ").",
						l, g, SavedTilesSize, TilemapCount);
					return false;
				}
				else
				{
					for(size_t TileIndex = 0; TileIndex < TilemapCount; ++TileIndex)
					{
						if(pSavedTiles[TileIndex].m_Skip != 0)
						{
							log_error("map/load", "Tile data of layer %d in group %d contains non-zero skip value at index %" PRIzu " but version %d does not use tileskip.",
								l, g, TileIndex, CMapItemLayerTilemap::VERSION_TEEWORLDS_TILESKIP);
							return false;
						}
						if(pSavedTiles[TileIndex].m_Reserved != 0)
						{
							log_error("map/load", "Tile data of layer %d in group %d contains non-zero padding value at index %" PRIzu ".",
								l, g, TileIndex);
							return false;
						}
					}
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

bool CMap::ExtractTiles(CTile *pDest, size_t DestSize, const CTile *pSrc, size_t SrcSize)
{
	size_t DestIndex = 0;
	size_t SrcIndex = 0;
	while(DestIndex < DestSize && SrcIndex < SrcSize)
	{
		if(pSrc[SrcIndex].m_Reserved != 0)
		{
			log_error("map/load", "Tile layer data contains non-zero padding value at index %" PRIzu ".", SrcIndex);
			return false;
		}
		for(unsigned Counter = 0; Counter <= pSrc[SrcIndex].m_Skip && DestIndex < DestSize; Counter++)
		{
			pDest[DestIndex].m_Index = pSrc[SrcIndex].m_Index;
			pDest[DestIndex].m_Flags = pSrc[SrcIndex].m_Flags;
			pDest[DestIndex].m_Skip = 0;
			pDest[DestIndex].m_Reserved = 0;
			DestIndex++;
		}
		SrcIndex++;
	}
	if(DestIndex != DestSize)
	{
		log_error("map/load", "Tile layer data is truncated (got %" PRIzu ", wanted %" PRIzu ").", DestIndex, DestSize);
		return false;
	}
	return true;
}

extern IEngineMap *CreateEngineMap() { return new CMap; }
