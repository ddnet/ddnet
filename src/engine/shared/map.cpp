/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "map.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/storage.h>

#include <game/gamecore.h>
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

void *CMap::GetItem(int Index, int *pType, int *pId, CUuid *pUuid)
{
	return m_DataFile.GetItem(Index, pType, pId, pUuid);
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

static bool AtMostOneBitSet(int Flags)
{
	// https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
	return (Flags & (Flags - 1)) == 0;
}

bool CMap::Load(const char *pFullName, IStorage *pStorage, const char *pPath, int StorageType)
{
	// Ensure current datafile is not left in an inconsistent state if loading fails,
	// by loading the new datafile separately first.
	CDataFileReader NewDataFile;
	if(!NewDataFile.Open(pFullName, pStorage, pPath, StorageType))
		return false;

	if(!ValidateMapVersion(NewDataFile))
	{
		NewDataFile.Close();
		return false;
	}

	int GroupsStart, GroupsNum, LayersStart, LayersNum;
	NewDataFile.GetType(MAPITEMTYPE_GROUP, &GroupsStart, &GroupsNum);
	NewDataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

	// Replace map items for old versions with items compatible with latest version to avoid version checks when using the map items.
	// Ensure that we have a game layer and game group.
	const CMapItemLayerTilemap *pGameLayer = nullptr;
	std::set<int> UsedLayerItemIndices;
	for(int GroupIndex = 0; GroupIndex < GroupsNum; GroupIndex++)
	{
		const size_t GroupItemSize = NewDataFile.GetItemSize(GroupsStart + GroupIndex);
		if(GroupItemSize < sizeof(CMapItemGroup_v1))
		{
			log_error("map/load", "Group %d is truncated (size %" PRIzu ").", GroupIndex, GroupItemSize);
			return false;
		}
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(NewDataFile.GetItem(GroupsStart + GroupIndex));
		if(pGroup->m_StartLayer < 0 || pGroup->m_NumLayers < 0 ||
			(int64_t)pGroup->m_StartLayer + pGroup->m_NumLayers > LayersNum)
		{
			log_error("map/load", "Group %d uses invalid layers %d to %d (the map contains %d layers).",
				GroupIndex, pGroup->m_StartLayer, pGroup->m_StartLayer + pGroup->m_NumLayers - 1, LayersNum);
			return false;
		}
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			const int LayerItemIndex = LayersStart + pGroup->m_StartLayer + LayerIndex;
			const auto &[_, LayerUnique] = UsedLayerItemIndices.emplace(LayerItemIndex);
			if(!LayerUnique)
			{
				log_error("map/load", "Layer %d in group %d is also being used by another group.", LayerIndex, GroupIndex);
				return false;
			}
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(NewDataFile.GetItem(LayerItemIndex));
			const size_t LayerItemSize = NewDataFile.GetItemSize(LayerItemIndex);
			if(LayerItemSize < sizeof(CMapItemLayer))
			{
				log_error("map/load", "Layer %d in group %d is truncated (size %" PRIzu ").", LayerIndex, GroupIndex, LayerItemSize);
				return false;
			}

			if(pLayer->m_Version != 0)
			{
				log_debug("map/load", "Layer %d in group %d has unused version set to %d. Resetting to 0.", LayerIndex, GroupIndex, pLayer->m_Version);
				pLayer->m_Version = 0;
			}

			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				if(!UpgradeAndValidateTilesLayerItem(NewDataFile, GroupIndex, LayerIndex, reinterpret_cast<CMapItemLayerTilemap *>(pLayer), LayerItemIndex, LayerItemSize))
				{
					return false;
				}
				// The item may have been replaced, so the pointer must be determined again.
				const CMapItemLayerTilemap *pLayerTilemap = static_cast<CMapItemLayerTilemap *>(NewDataFile.GetItem(LayerItemIndex));
				if(pLayerTilemap->m_Flags & TILESLAYERFLAG_GAME)
				{
					pGameLayer = pLayerTilemap;
				}
			}
		}
	}
	if(pGameLayer == nullptr)
	{
		log_error("map/load", "Game layer is missing.");
		return false;
	}

	// Lazily validate data and replace compressed tile layers with uncompressed ones.
	std::set<int> UsedDataIndices;
	for(int GroupIndex = 0; GroupIndex < GroupsNum; GroupIndex++)
	{
		const CMapItemGroup *pGroup = static_cast<CMapItemGroup *>(NewDataFile.GetItem(GroupsStart + GroupIndex));
		for(int LayerIndex = 0; LayerIndex < pGroup->m_NumLayers; LayerIndex++)
		{
			CMapItemLayer *pLayer = static_cast<CMapItemLayer *>(NewDataFile.GetItem(LayersStart + pGroup->m_StartLayer + LayerIndex));
			if(pLayer->m_Type == LAYERTYPE_TILES)
			{
				if(!ValidateAndUnpackTilesLayerData(NewDataFile, GroupIndex, LayerIndex, reinterpret_cast<const CMapItemLayerTilemap *>(pLayer), *pGameLayer, UsedDataIndices))
				{
					return false;
				}
			}
		}
	}

	// Load and implicitly validate game layer tile data immediately because we need it.
	// Do not preload other data to avoid excessive memory usage.
	if(NewDataFile.GetData(pGameLayer->m_Data) == nullptr)
	{
		log_error("map/load", "Game layer data is invalid.");
		return false;
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

bool CMap::ValidateMapVersion(CDataFileReader &NewDataFile)
{
	const int VersionItemIndex = NewDataFile.FindItemIndex(MAPITEMTYPE_VERSION, 0);
	if(VersionItemIndex < 0)
	{
		log_error("map/load", "Map version item is missing.");
		return false;
	}
	const size_t VersionItemSize = NewDataFile.GetItemSize(VersionItemIndex);
	if(VersionItemSize < sizeof(CMapItemVersion))
	{
		log_error("map/load", "Map version item is truncated (size %" PRIzu ").", VersionItemSize);
		return false;
	}
	const CMapItemVersion *pVersionItem = static_cast<CMapItemVersion *>(NewDataFile.GetItem(VersionItemIndex));
	if(pVersionItem->m_Version != 1)
	{
		log_error("map/load", "Map version %d is not supported.", pVersionItem->m_Version);
		return false;
	}
	return true;
}

bool CMap::ExtractTiles(CTile *pDest, size_t DestSize, const CTile *pSrc, size_t SrcSize)
{
	size_t DestIndex = 0;
	size_t SrcIndex = 0;
	while(DestIndex < DestSize && SrcIndex < SrcSize)
	{
		if(pSrc[SrcIndex].m_MustBe0 != 0)
		{
			log_error("map/load", "Tile layer data contains non-zero padding value %d at index %" PRIzu ".",
				pSrc[SrcIndex].m_MustBe0, SrcIndex);
			return false;
		}
		for(unsigned Counter = 0; Counter <= pSrc[SrcIndex].m_Skip && DestIndex < DestSize; Counter++)
		{
			pDest[DestIndex].m_Index = pSrc[SrcIndex].m_Index;
			pDest[DestIndex].m_Flags = pSrc[SrcIndex].m_Flags;
			pDest[DestIndex].m_Skip = 0;
			pDest[DestIndex].m_MustBe0 = 0;
			DestIndex++;
		}
		SrcIndex++;
	}
	if(DestIndex != DestSize)
	{
		log_error("map/load", "Tile layer data is truncated (got %" PRIzu ", wanted %" PRIzu ").",
			DestIndex, DestSize);
		return false;
	}
	if(SrcIndex != SrcSize)
	{
		log_error("map/load", "Too much tile layer data (read %" PRIzu ", total %" PRIzu ").",
			SrcIndex, SrcSize);
		return false;
	}
	return true;
}

static bool EnsureTileLayerProperties(int GroupIndex, int LayerIndex, CMapItemLayerTilemap &LayerTilemap)
{
	if(LayerTilemap.m_Width < 2)
	{
		log_error("map/load", "Tile layer %d in group %d has invalid width %d.",
			LayerIndex, GroupIndex, LayerTilemap.m_Width);
		return false;
	}

	if(LayerTilemap.m_Height < 2)
	{
		log_error("map/load", "Tile layer %d in group %d has invalid height %d.",
			LayerIndex, GroupIndex, LayerTilemap.m_Height);
		return false;
	}

	const auto &&EnsureValidName = [&](const char *pExpectedName) {
		char aCurrentName[sizeof(LayerTilemap.m_aName)];
		if(!IntsToStr(LayerTilemap.m_aName, std::size(LayerTilemap.m_aName), aCurrentName, std::size(aCurrentName)))
		{
			log_error("map/load", "Tile layer %d in group %d has invalid name.",
				LayerIndex, GroupIndex);
			return false;
		}
		else if(pExpectedName != nullptr && str_comp(aCurrentName, pExpectedName) != 0)
		{
			log_debug("map/load", "Physics tile layer %d in group %d has unexpected name '%s'. Resetting to '%s'.",
				LayerIndex, GroupIndex, aCurrentName, pExpectedName);
			StrToInts(LayerTilemap.m_aName, std::size(LayerTilemap.m_aName), pExpectedName);
		}
		return true;
	};

	const auto &&EnsureDefaultColor = [&]() {
		const CColor DefaultColor = CColor{255, 255, 255, 255};
		if(LayerTilemap.m_Color != DefaultColor)
		{
			log_debug("map/load", "Physics tile layer %d in group %d has unexpected color (%d, %d, %d, %d). Resetting to default.",
				LayerIndex, GroupIndex, LayerTilemap.m_Color.r, LayerTilemap.m_Color.g, LayerTilemap.m_Color.b, LayerTilemap.m_Color.a);
			LayerTilemap.m_Color = DefaultColor;
		}
	};

	const auto &&EnsureNoDetailFlag = [&]() {
		if(LayerTilemap.m_Layer.m_Flags & LAYERFLAG_DETAIL)
		{
			log_debug("map/load", "Physics tile layer %d in group %d has detail flag set. Resetting to non-detail.",
				LayerIndex, GroupIndex);
			LayerTilemap.m_Layer.m_Flags &= ~LAYERFLAG_DETAIL;
		}
	};

	if(LayerTilemap.m_Flags & TILESLAYERFLAG_GAME)
	{
		if(!EnsureValidName("Game"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else if(LayerTilemap.m_Flags & TILESLAYERFLAG_TELE)
	{
		if(!EnsureValidName("Tele"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else if(LayerTilemap.m_Flags & TILESLAYERFLAG_SPEEDUP)
	{
		if(!EnsureValidName("Speedup"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else if(LayerTilemap.m_Flags & TILESLAYERFLAG_FRONT)
	{
		if(!EnsureValidName("Front"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else if(LayerTilemap.m_Flags & TILESLAYERFLAG_SWITCH)
	{
		if(!EnsureValidName("Switch"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else if(LayerTilemap.m_Flags & TILESLAYERFLAG_TUNE)
	{
		if(!EnsureValidName("Tune"))
		{
			return false;
		}
		EnsureDefaultColor();
		EnsureNoDetailFlag();
	}
	else
	{
		if(!EnsureValidName(nullptr))
		{
			return false;
		}

		if(!in_range(LayerTilemap.m_Color.r, 0, 255) ||
			!in_range(LayerTilemap.m_Color.g, 0, 255) ||
			!in_range(LayerTilemap.m_Color.b, 0, 255) ||
			!in_range(LayerTilemap.m_Color.a, 0, 255))
		{
			log_error("map/load", "Tile layer %d in group %d has invalid color (%d, %d, %d, %d).",
				LayerIndex, GroupIndex, LayerTilemap.m_Color.r, LayerTilemap.m_Color.g, LayerTilemap.m_Color.b, LayerTilemap.m_Color.a);
			return false;
		}
	}

	const auto &&EnsureUnsetPhysicsData = [&](int TilesLayerFlag, int *pDataIndex, const char *pName) {
		if((LayerTilemap.m_Flags & TilesLayerFlag) == 0 && *pDataIndex != -1)
		{
			log_debug("map/load", "Tile layer %d in group %d has unused %s data index %d. Resetting to -1.",
				LayerIndex, GroupIndex, pName, *pDataIndex);
			*pDataIndex = -1;
		}
	};
	EnsureUnsetPhysicsData(TILESLAYERFLAG_TELE, &LayerTilemap.m_Tele, "tele");
	EnsureUnsetPhysicsData(TILESLAYERFLAG_SPEEDUP, &LayerTilemap.m_Speedup, "speedup");
	EnsureUnsetPhysicsData(TILESLAYERFLAG_FRONT, &LayerTilemap.m_Front, "front");
	EnsureUnsetPhysicsData(TILESLAYERFLAG_SWITCH, &LayerTilemap.m_Switch, "switch");
	EnsureUnsetPhysicsData(TILESLAYERFLAG_TUNE, &LayerTilemap.m_Tune, "tune");

	return true;
}

bool CMap::UpgradeAndValidateTilesLayerItem(
	CDataFileReader &NewDataFile, int GroupIndex, int LayerIndex,
	CMapItemLayerTilemap_v2 *pLayerTilemapBase, int LayerItemIndex, size_t LayerItemSize)
{
	if(LayerItemSize < sizeof(CMapItemLayerTilemap_v2))
	{
		log_error("map/load", "Tile layer %d in group %d is truncated (size %" PRIzu ").",
			LayerIndex, GroupIndex, LayerItemSize);
		return false;
	}

	if(!in_range(pLayerTilemapBase->m_Version, 2, 4))
	{
		log_error("map/load", "Tile layer %d in group %d has unsupported version %d.",
			LayerIndex, GroupIndex, pLayerTilemapBase->m_Version);
		return false;
	}

	if(!AtMostOneBitSet(pLayerTilemapBase->m_Flags & (TILESLAYERFLAG_GAME | TILESLAYERFLAG_TELE | TILESLAYERFLAG_SPEEDUP | TILESLAYERFLAG_FRONT | TILESLAYERFLAG_SWITCH | TILESLAYERFLAG_TUNE)))
	{
		log_error("map/load", "Tile layer %d in group %d has invalid combination of flags %d. At most one physics tile layer flag can be set.",
			LayerIndex, GroupIndex, pLayerTilemapBase->m_Flags);
		return false;
	}

	const auto &&UnpackPhysicsLayerDataIndex = [&](int TilesLayerFlag, int *pTargetDataIndex, const int *pSourceDataIndex, const CMapItemLayerTilemap_v2 *pSourceTileLayer, const char *pName) {
		// We have to check the size individually for each tile data index because old maps were created
		// containing only some prefix of the physics tile data indices without incrementing the version.
		if(LayerItemSize < reinterpret_cast<const uint8_t *>(pSourceDataIndex) - reinterpret_cast<const uint8_t *>(pSourceTileLayer) + sizeof(*pSourceDataIndex))
		{
			if(pSourceTileLayer->m_Flags & TilesLayerFlag)
			{
				log_error("map/load", "%s layer %d in group %d is truncated (version %d, size %" PRIzu ").",
					pName, LayerIndex, GroupIndex, pSourceTileLayer->m_Version, LayerItemSize);
				return false;
			}
			*pTargetDataIndex = -1;
		}
		else
		{
			*pTargetDataIndex = *pSourceDataIndex;
		}
		return true;
	};

	if(pLayerTilemapBase->m_Version == 2)
	{
		const CMapItemLayerTilemap_v2Legacy *pLayerTilemapLegacy = static_cast<const CMapItemLayerTilemap_v2Legacy *>(pLayerTilemapBase);
		CMapItemLayerTilemap OverriddenLayerTilemap;
		mem_copy(&OverriddenLayerTilemap, pLayerTilemapLegacy, sizeof(CMapItemLayerTilemap_v2));

		// Version 2 items have no name. Default to empty string. We fix the name of physics layers later.
		StrToInts(OverriddenLayerTilemap.m_aName, std::size(OverriddenLayerTilemap.m_aName), "");

		if(!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_TELE, &OverriddenLayerTilemap.m_Tele, &pLayerTilemapLegacy->m_Tele, pLayerTilemapBase, "Tele") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_SPEEDUP, &OverriddenLayerTilemap.m_Speedup, &pLayerTilemapLegacy->m_Speedup, pLayerTilemapBase, "Speedup") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_FRONT, &OverriddenLayerTilemap.m_Front, &pLayerTilemapLegacy->m_Front, pLayerTilemapBase, "Front") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_SWITCH, &OverriddenLayerTilemap.m_Switch, &pLayerTilemapLegacy->m_Switch, pLayerTilemapBase, "Switch") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_TUNE, &OverriddenLayerTilemap.m_Tune, &pLayerTilemapLegacy->m_Tune, pLayerTilemapBase, "Tune"))
		{
			return false;
		}
		if(!EnsureTileLayerProperties(GroupIndex, LayerIndex, OverriddenLayerTilemap))
		{
			return false;
		}
		if(!NewDataFile.OverrideItemData(LayerItemIndex, &OverriddenLayerTilemap, sizeof(OverriddenLayerTilemap)))
		{
			return false;
		}
	}
	else if(LayerItemSize < sizeof(CMapItemLayerTilemap_v3Teeworlds))
	{
		// Only the physics layer data indices added by DDRace may be truncated in
		// version 3 and 4 items, the layer name must always be complete.
		log_error("map/load", "Tile layer %d in group %d is truncated (version %d, size %" PRIzu ").",
			LayerIndex, GroupIndex, pLayerTilemapBase->m_Version, LayerItemSize);
		return false;
	}
	else if(LayerItemSize < sizeof(CMapItemLayerTilemap))
	{
		const CMapItemLayerTilemap *pLayerTilemapLegacy = static_cast<const CMapItemLayerTilemap *>(pLayerTilemapBase);
		CMapItemLayerTilemap OverriddenLayerTilemap;
		mem_copy(&OverriddenLayerTilemap, pLayerTilemapLegacy, sizeof(CMapItemLayerTilemap_v3Teeworlds));

		if(!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_TELE, &OverriddenLayerTilemap.m_Tele, &pLayerTilemapLegacy->m_Tele, pLayerTilemapBase, "Tele") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_SPEEDUP, &OverriddenLayerTilemap.m_Speedup, &pLayerTilemapLegacy->m_Speedup, pLayerTilemapBase, "Speedup") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_FRONT, &OverriddenLayerTilemap.m_Front, &pLayerTilemapLegacy->m_Front, pLayerTilemapBase, "Front") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_SWITCH, &OverriddenLayerTilemap.m_Switch, &pLayerTilemapLegacy->m_Switch, pLayerTilemapBase, "Switch") ||
			!UnpackPhysicsLayerDataIndex(TILESLAYERFLAG_TUNE, &OverriddenLayerTilemap.m_Tune, &pLayerTilemapLegacy->m_Tune, pLayerTilemapBase, "Tune"))
		{
			return false;
		}
		if(!EnsureTileLayerProperties(GroupIndex, LayerIndex, OverriddenLayerTilemap))
		{
			return false;
		}
		if(!NewDataFile.OverrideItemData(LayerItemIndex, &OverriddenLayerTilemap, sizeof(OverriddenLayerTilemap)))
		{
			return false;
		}
	}
	else // latest version, whole CMapItemLayerTilemap available
	{
		if(!EnsureTileLayerProperties(GroupIndex, LayerIndex, *static_cast<CMapItemLayerTilemap *>(pLayerTilemapBase)))
		{
			return false;
		}
	}

	return true;
}

bool CMap::ValidateAndUnpackTilesLayerData(CDataFileReader &NewDataFile, int GroupIndex, int LayerIndex, const CMapItemLayerTilemap *pLayerTilemap, const CMapItemLayerTilemap &GameLayer, std::set<int> &UsedDataIndices)
{
	size_t TileSize;
	int DataIndex;
	int LayerType;
	if(pLayerTilemap->m_Flags & TILESLAYERFLAG_GAME)
	{
		TileSize = sizeof(CTile);
		DataIndex = pLayerTilemap->m_Data;
		LayerType = LAYERTYPE_GAME;
	}
	else if(pLayerTilemap->m_Flags & TILESLAYERFLAG_TELE)
	{
		TileSize = sizeof(CTeleTile);
		DataIndex = pLayerTilemap->m_Tele;
		LayerType = LAYERTYPE_TELE;
	}
	else if(pLayerTilemap->m_Flags & TILESLAYERFLAG_SPEEDUP)
	{
		TileSize = sizeof(CSpeedupTile);
		DataIndex = pLayerTilemap->m_Speedup;
		LayerType = LAYERTYPE_SPEEDUP;
	}
	else if(pLayerTilemap->m_Flags & TILESLAYERFLAG_FRONT)
	{
		TileSize = sizeof(CTile);
		DataIndex = pLayerTilemap->m_Front;
		LayerType = LAYERTYPE_FRONT;
	}
	else if(pLayerTilemap->m_Flags & TILESLAYERFLAG_SWITCH)
	{
		TileSize = sizeof(CSwitchTile);
		DataIndex = pLayerTilemap->m_Switch;
		LayerType = LAYERTYPE_SWITCH;
	}
	else if(pLayerTilemap->m_Flags & TILESLAYERFLAG_TUNE)
	{
		TileSize = sizeof(CTuneTile);
		DataIndex = pLayerTilemap->m_Tune;
		LayerType = LAYERTYPE_TUNE;
	}
	else
	{
		TileSize = sizeof(CTile);
		DataIndex = pLayerTilemap->m_Data;
		LayerType = LAYERTYPE_TILES;
	}

	if(DataIndex < 0 || DataIndex >= NewDataFile.NumData())
	{
		log_error("map/load", "Tile data index %d of layer %d in group %d is invalid.", DataIndex, LayerIndex, GroupIndex);
		return false;
	}

	const auto &[_, DataUnique] = UsedDataIndices.emplace(DataIndex);
	if(!DataUnique)
	{
		log_error("map/load", "Tile data index %d of layer %d in group %d is not unique.", DataIndex, LayerIndex, GroupIndex);
		return false;
	}

	const size_t TilemapCount = (size_t)pLayerTilemap->m_Width * pLayerTilemap->m_Height;
	const size_t TilemapSize = TilemapCount * TileSize;

	if(((int)TilemapCount / pLayerTilemap->m_Width != pLayerTilemap->m_Height) || (TilemapSize / TileSize != TilemapCount))
	{
		log_error("map/load", "Tile layer %d in group %d is too big (%d * %d * %" PRIzu " causes an integer overflow).",
			LayerIndex, GroupIndex, pLayerTilemap->m_Width, pLayerTilemap->m_Height, TileSize);
		return false;
	}

	// The collision uses the size of the game layer for the data of all physics layers,
	// so physics layers must contain at least as many tiles as the game layer.
	if(LayerType != LAYERTYPE_TILES && LayerType != LAYERTYPE_GAME &&
		TilemapCount < (size_t)GameLayer.m_Width * GameLayer.m_Height)
	{
		log_error("map/load", "Physics layer %d in group %d is smaller than the game layer (%d * %d < %d * %d).",
			LayerIndex, GroupIndex, pLayerTilemap->m_Width, pLayerTilemap->m_Height, GameLayer.m_Width, GameLayer.m_Height);
		return false;
	}

	NewDataFile.AddDataProcessor(DataIndex, [pLayerTilemap, TileSize, LayerType, GroupIndex, LayerIndex, TilemapCount, TilemapSize](void *pData, size_t Size) -> std::pair<void *, size_t> {
		const size_t SavedTilesSize = Size / TileSize;
		if(pLayerTilemap->m_Version >= 4)
		{
			// CMapItemLayerTilemap with this version are only written to maps in upstream Teeworlds.
			// The tile data of tilemaps using this version must be unpacked by repeating tiles
			// according to the CTile::m_Skip values of the packed tile data.
			if(LayerType != LAYERTYPE_TILES && LayerType != LAYERTYPE_GAME)
			{
				log_error("map/load", "Layer %d in group %d uses tileskip but this is only supported for tiles and game layers.",
					LayerIndex, GroupIndex);
				free(pData);
				return std::make_pair(nullptr, 0);
			}
			CTile *pTiles = static_cast<CTile *>(malloc(TilemapSize));
			if(pTiles == nullptr)
			{
				log_error("map/load", "Failed to allocate memory for layer %d in group %d (size %d * %d).",
					LayerIndex, GroupIndex, pLayerTilemap->m_Width, pLayerTilemap->m_Height);
				free(pData);
				return std::make_pair(nullptr, 0);
			}
			else if(!ExtractTiles(pTiles, (size_t)pLayerTilemap->m_Width * pLayerTilemap->m_Height, static_cast<const CTile *>(pData), SavedTilesSize))
			{
				log_error("map/load", "Failed to extract tiles of layer %d in group %d.",
					LayerIndex, GroupIndex);
				free(pTiles);
				free(pData);
				return std::make_pair(nullptr, 0);
			}
			free(pData);
			return std::make_pair(pTiles, TilemapSize);
		}
		else if(SavedTilesSize < TilemapCount)
		{
			log_error("map/load", "Tile data of layer %d in group %d is truncated (got %" PRIzu ", wanted %" PRIzu ").",
				LayerIndex, GroupIndex, SavedTilesSize, TilemapCount);
			free(pData);
			return std::make_pair(nullptr, 0);
		}
		else if(LayerType == LAYERTYPE_TILES || LayerType == LAYERTYPE_GAME || LayerType == LAYERTYPE_FRONT)
		{
			const CTile *pTileData = static_cast<const CTile *>(pData);
			for(size_t TileIndex = 0; TileIndex < TilemapCount; ++TileIndex)
			{
				if(pTileData[TileIndex].m_Skip != 0)
				{
					log_error("map/load", "Tile data of layer %d in group %d contains non-zero skip value %d at index %" PRIzu " but version %d does not use tileskip.",
						LayerIndex, GroupIndex, pTileData[TileIndex].m_Skip, TileIndex, pLayerTilemap->m_Version);
					free(pData);
					return std::make_pair(nullptr, 0);
				}
				if(pTileData[TileIndex].m_MustBe0 != 0)
				{
					log_error("map/load", "Tile data of layer %d in group %d contains non-zero padding value %d at index %" PRIzu ".",
						LayerIndex, GroupIndex, pTileData[TileIndex].m_MustBe0, TileIndex);
					free(pData);
					return std::make_pair(nullptr, 0);
				}
			}
		}
		else if(LayerType == LAYERTYPE_SPEEDUP)
		{
			const CSpeedupTile *pSpeedupData = static_cast<const CSpeedupTile *>(pData);
			for(size_t TileIndex = 0; TileIndex < TilemapCount; ++TileIndex)
			{
				if(pSpeedupData[TileIndex].m_MustBe0 != 0)
				{
					log_error("map/load", "Speedup tile data of layer %d in group %d contains non-zero padding value %d at index %" PRIzu ".",
						LayerIndex, GroupIndex, pSpeedupData[TileIndex].m_MustBe0, TileIndex);
					free(pData);
					return std::make_pair(nullptr, 0);
				}
			}
		}
		return std::make_pair(pData, Size);
	});

	return true;
}

extern std::unique_ptr<IMap> CreateMap()
{
	return std::make_unique<CMap>();
}
