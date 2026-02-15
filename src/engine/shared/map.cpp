/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "map.h"

#include <base/log.h>
#include <base/system.h>

#include <engine/storage.h>

#include <game/mapitems.h>

CMap::CMap() = default;

CMap::~CMap()
{
	Unload();
}

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

bool CMap::Load(const char *pFullName, IStorage *pStorage, const char *pPath, int StorageType)
{
	// Ensure current datafile is not left in an inconsistent state if loading fails,
	// by loading the new datafile separately first.
	CDataFileReader NewDataFile;
	if(!NewDataFile.Open(pFullName, pStorage, pPath, StorageType))
		return false;

	// Check version
	const CMapItemVersion *pItem = (CMapItemVersion *)NewDataFile.FindItem(MAPITEMTYPE_VERSION, 0);
	if(pItem == nullptr || pItem->m_Version != 1)
	{
		log_error("map/load", "Error: map version not supported.");
		NewDataFile.Close();
		return false;
	}

	// Replace compressed tile layers with uncompressed ones, and validate layer data.
	int GroupsStart, GroupsNum, LayersStart, LayersNum;
	NewDataFile.GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	NewDataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);
	for(int GroupIndex = 0; GroupIndex < GroupsNum; GroupIndex++)
	{
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(NewDataFile.GetItem(GroupsStart + GroupIndex));
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(NewDataFile.GetItem(LayersStart + pGroup->m_StartLayer + LayerIndex));
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				CMapItemLayerTilemap *pTilemap = reinterpret_cast<CMapItemLayerTilemap *>(pLayer);

				size_t TileSize;
				int DataIndex;
				int LayerType;
				if(pTilemap->m_Flags & TILESLAYERFLAG_GAME)
				{
					TileSize = sizeof(CTile);
					DataIndex = pTilemap->m_Data;
					LayerType = LAYERTYPE_GAME;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_TELE)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Tele = *((const int *)(pTilemap) + 15);
					}
					TileSize = sizeof(CTeleTile);
					DataIndex = pTilemap->m_Tele;
					LayerType = LAYERTYPE_TELE;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_SPEEDUP)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Speedup = *((const int *)(pTilemap) + 16);
					}
					TileSize = sizeof(CSpeedupTile);
					DataIndex = pTilemap->m_Speedup;
					LayerType = LAYERTYPE_SPEEDUP;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_FRONT)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Front = *((const int *)(pTilemap) + 17);
					}
					TileSize = sizeof(CTile);
					DataIndex = pTilemap->m_Front;
					LayerType = LAYERTYPE_FRONT;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_SWITCH)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Switch = *((const int *)(pTilemap) + 18);
					}
					TileSize = sizeof(CSwitchTile);
					DataIndex = pTilemap->m_Switch;
					LayerType = LAYERTYPE_SWITCH;
				}
				else if(pTilemap->m_Flags & TILESLAYERFLAG_TUNE)
				{
					if(pTilemap->m_Version <= 2)
					{
						pTilemap->m_Tune = *((const int *)(pTilemap) + 19);
					}
					TileSize = sizeof(CTuneTile);
					DataIndex = pTilemap->m_Tune;
					LayerType = LAYERTYPE_TUNE;
				}
				else
				{
					TileSize = sizeof(CTile);
					DataIndex = pTilemap->m_Data;
					LayerType = LAYERTYPE_TILES;
				}

				const size_t TilemapCount = (size_t)pTilemap->m_Width * pTilemap->m_Height;
				const size_t TilemapSize = TilemapCount * TileSize;

				if(((int)TilemapCount / pTilemap->m_Width != pTilemap->m_Height) || (TilemapSize / TileSize != TilemapCount))
				{
					log_error("map/load", "Tiles layer %d in group %d is too big (%d * %d * %d causes an integer overflow).",
						LayerIndex, GroupIndex, pTilemap->m_Width, pTilemap->m_Height, (int)TileSize);
					return false;
				}

				const void *pRawData = NewDataFile.GetData(DataIndex);
				const size_t SavedTilesSize = NewDataFile.GetDataSize(DataIndex) / TileSize;

				if(pRawData == nullptr)
				{
					log_error("map/load", "Tile data of layer %d in group %d is missing or corrupted.", LayerIndex, GroupIndex);
					return false;
				}
				else if(pTilemap->m_Version >= CMapItemLayerTilemap::VERSION_TEEWORLDS_TILESKIP)
				{
					if(LayerType != LAYERTYPE_TILES && LayerType != LAYERTYPE_GAME)
					{
						log_error("map/load", "Layer %d in group %d uses tileskip but this is only supported for tiles and game layers.", LayerIndex, GroupIndex);
						return false;
					}
					CTile *pTiles = static_cast<CTile *>(malloc(TilemapSize));
					if(pTiles == nullptr)
					{
						log_error("map/load", "Failed to allocate memory for layer %d in group %d (size %d * %d).",
							LayerIndex, GroupIndex, pTilemap->m_Width, pTilemap->m_Height);
						return false;
					}
					else if(!ExtractTiles(pTiles, (size_t)pTilemap->m_Width * pTilemap->m_Height, static_cast<const CTile *>(pRawData), SavedTilesSize))
					{
						free(pTiles);
						log_error("map/load", "Failed to extract tiles of layer %d in group %d.", LayerIndex, GroupIndex);
						return false;
					}
					NewDataFile.ReplaceData(DataIndex, reinterpret_cast<char *>(pTiles), TilemapSize);
				}
				else if(SavedTilesSize < TilemapCount)
				{
					log_error("map/load", "Tile data of layer %d in group %d is truncated (got %" PRIzu ", wanted %" PRIzu ").",
						LayerIndex, GroupIndex, SavedTilesSize, TilemapCount);
					return false;
				}
				else if(LayerType == LAYERTYPE_TILES || LayerType == LAYERTYPE_GAME || LayerType == LAYERTYPE_FRONT)
				{
					const CTile *pTileData = static_cast<const CTile *>(pRawData);
					for(size_t TileIndex = 0; TileIndex < TilemapCount; ++TileIndex)
					{
						if(pTileData[TileIndex].m_Skip != 0)
						{
							log_error("map/load", "Tile data of layer %d in group %d contains non-zero skip value %d at index %" PRIzu " but version %d does not use tileskip.",
								LayerIndex, GroupIndex, pTileData[TileIndex].m_Skip, TileIndex, pTilemap->m_Version);
							return false;
						}
						if(pTileData[TileIndex].m_Reserved != 0)
						{
							log_error("map/load", "Tile data of layer %d in group %d contains non-zero padding value %d at index %" PRIzu ".",
								LayerIndex, GroupIndex, pTileData[TileIndex].m_Reserved, TileIndex);
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

bool CMap::Load(IStorage *pStorage, const char *pPath, int StorageType)
{
	char aFilename[IO_MAX_PATH_LENGTH];
	fs_split_file_extension(fs_filename(pPath), aFilename, sizeof(aFilename));
	return Load(aFilename, pStorage, pPath, StorageType);
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

const char *CMap::FullName() const
{
	return m_DataFile.FullName();
}

const char *CMap::BaseName() const
{
	return m_DataFile.BaseName();
}

const char *CMap::Path() const
{
	return m_DataFile.Path();
}

SHA256_DIGEST CMap::Sha256() const
{
	return m_DataFile.Sha256();
}

unsigned CMap::Crc() const
{
	return m_DataFile.Crc();
}

int CMap::Size() const
{
	return m_DataFile.Size();
}

bool CMap::ExtractTiles(CTile *pDest, size_t DestSize, const CTile *pSrc, size_t SrcSize)
{
	size_t DestIndex = 0;
	size_t SrcIndex = 0;
	while(DestIndex < DestSize && SrcIndex < SrcSize)
	{
		if(pSrc[SrcIndex].m_Reserved != 0)
		{
			log_error("map/load", "Tile layer data contains non-zero padding value %d at index %" PRIzu ".", pSrc[SrcIndex].m_Reserved, SrcIndex);
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

extern std::unique_ptr<IMap> CreateMap()
{
	return std::make_unique<CMap>();
}
