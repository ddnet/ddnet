#include <base/dbg.h>
#include <base/io.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/map.h>
#include <engine/storage.h>

#include <game/gamecore.h>
#include <game/mapitems.h>

#include <cstdlib>
#include <memory>

static int TileDataIndex(const CMapItemLayerTilemap *pTilemap, int PhysicsLayerFlags)
{
	if(PhysicsLayerFlags & TILESLAYERFLAG_TELE)
		return pTilemap->m_Tele;
	if(PhysicsLayerFlags & TILESLAYERFLAG_SPEEDUP)
		return pTilemap->m_Speedup;
	if(PhysicsLayerFlags & TILESLAYERFLAG_FRONT)
		return pTilemap->m_Front;
	if(PhysicsLayerFlags & TILESLAYERFLAG_SWITCH)
		return pTilemap->m_Switch;
	if(PhysicsLayerFlags & TILESLAYERFLAG_TUNE)
		return pTilemap->m_Tune;
	return pTilemap->m_Data;
}

template<typename T, typename FCompare>
static bool DiffTileLayer(std::shared_ptr<IMap> apMaps[2], const char *apMapNames[2], const int aData[2], int Width, int Height, FCompare pfnCompare)
{
	const T *apTile[2];
	for(int i = 0; i < 2; ++i)
	{
		apTile[i] = (const T *)apMaps[i]->GetData(aData[i]);
	}

	if(apTile[0] == nullptr || apTile[1] == nullptr)
	{
		for(int i = 0; i < 2; ++i)
		{
			if(apTile[i] == nullptr)
				dbg_msg("map_diff", "invalid tile layer data in \"%s\"", apMapNames[i]);
		}
		for(int i = 0; i < 2; ++i)
			apMaps[i]->UnloadData(aData[i]);
		// Layers that are invalid in both maps are ignored
		return apTile[0] == apTile[1];
	}

	for(int y = 0; y < Height; y++)
	{
		for(int x = 0; x < Width; x++)
		{
			const int Pos = y * Width + x;
			pfnCompare(apTile[0][Pos], apTile[1][Pos], x, y);
		}
	}

	for(int i = 0; i < 2; ++i)
		apMaps[i]->UnloadData(aData[i]);
	return true;
}

static bool Process(IStorage *pStorage, const char *apMapNames[2])
{
	std::shared_ptr<IMap> apMaps[2];

	for(int i = 0; i < 2; ++i)
	{
		apMaps[i] = CreateMap();

		if(!apMaps[i]->Load(pStorage, apMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_diff", "error opening map '%s'", apMapNames[i]);
			return false;
		}
	}

	int aStart[2], aLayersNum[2];
	for(int i = 0; i < 2; ++i)
		apMaps[i]->GetType(MAPITEMTYPE_LAYER, &aStart[i], &aLayersNum[i]);

	// ensure basic layout
	if(aLayersNum[0] != aLayersNum[1])
	{
		dbg_msg("map_diff", "different layer numbers:");
		for(int i = 0; i < 2; ++i)
			dbg_msg("map_diff", "  \"%s\": %d layers", apMapNames[i], aLayersNum[i]);
		return false;
	}

	// compare
	for(int j = 0; j < aLayersNum[0]; ++j)
	{
		CMapItemLayer *apItem[2];
		for(int i = 0; i < 2; ++i)
			apItem[i] = (CMapItemLayer *)apMaps[i]->GetItem(aStart[i] + j);

		if(apItem[0]->m_Type != LAYERTYPE_TILES || apItem[1]->m_Type != LAYERTYPE_TILES)
			continue;

		CMapItemLayerTilemap *apTilemap[2];
		char aaName[2][sizeof(CMapItemLayerTilemap{}.m_aName)];

		for(int i = 0; i < 2; ++i)
		{
			apTilemap[i] = (CMapItemLayerTilemap *)apItem[i];
			IntsToStr(apTilemap[i]->m_aName, std::size(apTilemap[i]->m_aName), aaName[i], std::size(aaName[i]));
		}

		const int PhysicsLayerFlags = TILESLAYERFLAG_TELE | TILESLAYERFLAG_SPEEDUP | TILESLAYERFLAG_FRONT | TILESLAYERFLAG_SWITCH | TILESLAYERFLAG_TUNE;
		if(str_comp(aaName[0], aaName[1]) != 0 ||
			apTilemap[0]->m_Width != apTilemap[1]->m_Width ||
			apTilemap[0]->m_Height != apTilemap[1]->m_Height ||
			(apTilemap[0]->m_Flags & PhysicsLayerFlags) != (apTilemap[1]->m_Flags & PhysicsLayerFlags))
		{
			dbg_msg("map_diff", "different tile layers:");
			for(int i = 0; i < 2; ++i)
				dbg_msg("map_diff", " [%d:%s] (%dx%d, flags: %d)", j, aaName[i], apTilemap[i]->m_Width, apTilemap[i]->m_Height, apTilemap[i]->m_Flags);
			return false;
		}

		const int Width = apTilemap[0]->m_Width;
		const int Height = apTilemap[0]->m_Height;
		const int Flags = apTilemap[0]->m_Flags & PhysicsLayerFlags;
		const int aData[2] = {TileDataIndex(apTilemap[0], Flags), TileDataIndex(apTilemap[1], Flags)};
		bool Ok;
		if(Flags & TILESLAYERFLAG_TELE)
		{
			Ok = DiffTileLayer<CTeleTile>(apMaps, apMapNames, aData, Width, Height, [&](const CTeleTile &Tile0, const CTeleTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type)
				{
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d) != (number: %d, type: %d)",
						aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile1.m_Number, Tile1.m_Type);
				}
			});
		}
		else if(Flags & TILESLAYERFLAG_SPEEDUP)
		{
			Ok = DiffTileLayer<CSpeedupTile>(apMaps, apMapNames, aData, Width, Height, [&](const CSpeedupTile &Tile0, const CSpeedupTile &Tile1, int x, int y) {
				if(Tile0.m_Force != Tile1.m_Force || Tile0.m_MaxSpeed != Tile1.m_MaxSpeed || Tile0.m_Type != Tile1.m_Type || Tile0.m_Angle != Tile1.m_Angle)
				{
					dbg_msg("map_diff", "[%d:%s] %dx%d: (force: %d, maxspeed: %d, angle: %d, type: %d) != (force: %d, maxspeed: %d, angle: %d, type: %d)",
						aLayersNum[0], aaName[0], x, y, Tile0.m_Force, Tile0.m_MaxSpeed, Tile0.m_Angle, Tile0.m_Type, Tile1.m_Force, Tile1.m_MaxSpeed, Tile1.m_Angle, Tile1.m_Type);
				}
			});
		}
		else if(Flags & TILESLAYERFLAG_SWITCH)
		{
			Ok = DiffTileLayer<CSwitchTile>(apMaps, apMapNames, aData, Width, Height, [&](const CSwitchTile &Tile0, const CSwitchTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type || Tile0.m_Flags != Tile1.m_Flags || Tile0.m_Delay != Tile1.m_Delay)
				{
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d, flags: %d, delay: %d) != (number: %d, type: %d, flags: %d, delay: %d)",
						aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile0.m_Flags, Tile0.m_Delay, Tile1.m_Number, Tile1.m_Type, Tile1.m_Flags, Tile1.m_Delay);
				}
			});
		}
		else if(Flags & TILESLAYERFLAG_TUNE)
		{
			Ok = DiffTileLayer<CTuneTile>(apMaps, apMapNames, aData, Width, Height, [&](const CTuneTile &Tile0, const CTuneTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type)
				{
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d) != (number: %d, type: %d)",
						aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile1.m_Number, Tile1.m_Type);
				}
			});
		}
		else
		{
			// Regular, game and front layers all store CTile
			Ok = DiffTileLayer<CTile>(apMaps, apMapNames, aData, Width, Height, [&](const CTile &Tile0, const CTile &Tile1, int x, int y) {
				if(Tile0.m_Index != Tile1.m_Index || Tile0.m_Flags != Tile1.m_Flags)
				{
					dbg_msg("map_diff", "[%d:%s] %dx%d: (index: %d, flags: %d) != (index: %d, flags: %d)",
						aLayersNum[0], aaName[0], x, y, Tile0.m_Index, Tile0.m_Flags, Tile1.m_Index, Tile1.m_Flags);
				}
			});
		}

		if(!Ok)
			return false;
	}

	return true;
}

int main(int argc, const char *argv[])
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	std::vector<std::shared_ptr<ILogger>> vpLoggers;
	std::shared_ptr<ILogger> pStdoutLogger = std::shared_ptr<ILogger>(log_logger_stdout());
	if(pStdoutLogger)
	{
		vpLoggers.push_back(pStdoutLogger);
	}
	IOHANDLE LogFile = io_open("map_diff.txt", IOFLAG_WRITE);
	if(LogFile)
	{
		vpLoggers.push_back(std::shared_ptr<ILogger>(log_logger_file(LogFile)));
	}
	log_set_global_logger(log_logger_collection(std::move(vpLoggers)).release());

	if(argc != 3)
	{
		dbg_msg("usage", "%s map1 map2", argv[0]);
		return -1;
	}

	std::unique_ptr<IStorage> pStorage = CreateLocalStorage();
	if(!pStorage)
	{
		log_error("map_diff", "Error creating local storage");
		return -1;
	}

	const char *apMapNames[] = {argv[1], argv[2]};
	return Process(pStorage.get(), apMapNames) ? 0 : 1;
}
