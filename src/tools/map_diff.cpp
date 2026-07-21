#include <base/dbg.h>
#include <base/io.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/gamecore.h>
#include <game/mapitems.h>

template<typename T, typename FCompare>
static bool DiffTileLayer(CDataFileReader aMaps[2], const char **pMapNames, const int aData[2], int Width, int Height, FCompare pfnCompare)
{
	const T *apTile[2];
	for(int i = 0; i < 2; ++i)
	{
		apTile[i] = (const T *)aMaps[i].GetData(aData[i]);
		if(apTile[i] == nullptr || (size_t)aMaps[i].GetDataSize(aData[i]) < (size_t)Width * Height * sizeof(T))
		{
			dbg_msg("map_diff", "invalid tile layer data in \"%s\"", pMapNames[i]);
			return false;
		}
	}

	for(int y = 0; y < Height; y++)
	{
		for(int x = 0; x < Width; x++)
		{
			const int Pos = y * Width + x;
			pfnCompare(apTile[0][Pos], apTile[1][Pos], x, y);
		}
	}
	return true;
}

static bool Process(IStorage *pStorage, const char **pMapNames)
{
	CDataFileReader aMaps[2];

	for(int i = 0; i < 2; ++i)
	{
		if(!aMaps[i].Open(pStorage, pMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_diff", "error opening map '%s'", pMapNames[i]);
			return false;
		}

		const CMapItemVersion *pVersion = static_cast<CMapItemVersion *>(aMaps[i].FindItem(MAPITEMTYPE_VERSION, 0));
		if(pVersion == nullptr || pVersion->m_Version != 1)
		{
			dbg_msg("map_diff", "unsupported map version '%s'", pMapNames[i]);
			return false;
		}
	}

	int aStart[2], aLayersNum[2];
	for(int i = 0; i < 2; ++i)
		aMaps[i].GetType(MAPITEMTYPE_LAYER, &aStart[i], &aLayersNum[i]);

	// ensure basic layout
	if(aLayersNum[0] != aLayersNum[1])
	{
		dbg_msg("map_diff", "different layer numbers:");
		for(int i = 0; i < 2; ++i)
			dbg_msg("map_diff", "  \"%s\": %d layers", pMapNames[i], aLayersNum[i]);
		return false;
	}

	// preload data
	for(int j = 0; j < aLayersNum[0]; ++j)
	{
		for(int i = 0; i < 2; ++i)
		{
			CMapItemLayer *pItem = (CMapItemLayer *)aMaps[i].GetItem(aStart[i] + j);
			if(pItem->m_Type == LAYERTYPE_TILES)
				(void)aMaps[i].GetData(((CMapItemLayerTilemap *)pItem)->m_Data);
		}
	}

	// compare
	for(int j = 0; j < aLayersNum[0]; ++j)
	{
		CMapItemLayer *apItem[2];
		for(int i = 0; i < 2; ++i)
			apItem[i] = (CMapItemLayer *)aMaps[i].GetItem(aStart[i] + j);

		if(apItem[0]->m_Type != LAYERTYPE_TILES || apItem[1]->m_Type != LAYERTYPE_TILES)
			continue;

		CMapItemLayerTilemap *apTilemap[2];
		char aaName[2][12];

		for(int i = 0; i < 2; ++i)
		{
			apTilemap[i] = (CMapItemLayerTilemap *)apItem[i];
			IntsToStr(apTilemap[i]->m_aName, std::size(apTilemap[i]->m_aName), aaName[i], std::size(aaName[i]));
		}

		const int PhysicsLayerFlags = TILESLAYERFLAG_TELE | TILESLAYERFLAG_SPEEDUP | TILESLAYERFLAG_FRONT | TILESLAYERFLAG_SWITCH | TILESLAYERFLAG_TUNE;
		if(str_comp(aaName[0], aaName[1]) != 0 || apTilemap[0]->m_Width != apTilemap[1]->m_Width || apTilemap[0]->m_Height != apTilemap[1]->m_Height || (apTilemap[0]->m_Flags & PhysicsLayerFlags) != (apTilemap[1]->m_Flags & PhysicsLayerFlags))
		{
			dbg_msg("map_diff", "different tile layers:");
			for(int i = 0; i < 2; ++i)
				dbg_msg("map_diff", "  \"%s\" (%dx%d, flags: %d)", aaName[i], apTilemap[i]->m_Width, apTilemap[i]->m_Height, apTilemap[i]->m_Flags);
			return false;
		}

		const int Width = apTilemap[0]->m_Width;
		const int Height = apTilemap[0]->m_Height;
		const int Flags = apTilemap[0]->m_Flags & PhysicsLayerFlags;
		bool Ok;
		if(Flags & TILESLAYERFLAG_TELE)
		{
			const int aData[2] = {apTilemap[0]->m_Tele, apTilemap[1]->m_Tele};
			Ok = DiffTileLayer<CTeleTile>(aMaps, pMapNames, aData, Width, Height, [&](const CTeleTile &Tile0, const CTeleTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type)
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d) != (number: %d, type: %d)", aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile1.m_Number, Tile1.m_Type);
			});
		}
		else if(Flags & TILESLAYERFLAG_SPEEDUP)
		{
			const int aData[2] = {apTilemap[0]->m_Speedup, apTilemap[1]->m_Speedup};
			Ok = DiffTileLayer<CSpeedupTile>(aMaps, pMapNames, aData, Width, Height, [&](const CSpeedupTile &Tile0, const CSpeedupTile &Tile1, int x, int y) {
				if(Tile0.m_Force != Tile1.m_Force || Tile0.m_MaxSpeed != Tile1.m_MaxSpeed || Tile0.m_Type != Tile1.m_Type || Tile0.m_Angle != Tile1.m_Angle)
					dbg_msg("map_diff", "[%d:%s] %dx%d: (force: %d, maxspeed: %d, angle: %d, type: %d) != (force: %d, maxspeed: %d, angle: %d, type: %d)", aLayersNum[0], aaName[0], x, y, Tile0.m_Force, Tile0.m_MaxSpeed, Tile0.m_Angle, Tile0.m_Type, Tile1.m_Force, Tile1.m_MaxSpeed, Tile1.m_Angle, Tile1.m_Type);
			});
		}
		else if(Flags & TILESLAYERFLAG_SWITCH)
		{
			const int aData[2] = {apTilemap[0]->m_Switch, apTilemap[1]->m_Switch};
			Ok = DiffTileLayer<CSwitchTile>(aMaps, pMapNames, aData, Width, Height, [&](const CSwitchTile &Tile0, const CSwitchTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type || Tile0.m_Flags != Tile1.m_Flags || Tile0.m_Delay != Tile1.m_Delay)
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d, flags: %d, delay: %d) != (number: %d, type: %d, flags: %d, delay: %d)", aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile0.m_Flags, Tile0.m_Delay, Tile1.m_Number, Tile1.m_Type, Tile1.m_Flags, Tile1.m_Delay);
			});
		}
		else if(Flags & TILESLAYERFLAG_TUNE)
		{
			const int aData[2] = {apTilemap[0]->m_Tune, apTilemap[1]->m_Tune};
			Ok = DiffTileLayer<CTuneTile>(aMaps, pMapNames, aData, Width, Height, [&](const CTuneTile &Tile0, const CTuneTile &Tile1, int x, int y) {
				if(Tile0.m_Number != Tile1.m_Number || Tile0.m_Type != Tile1.m_Type)
					dbg_msg("map_diff", "[%d:%s] %dx%d: (number: %d, type: %d) != (number: %d, type: %d)", aLayersNum[0], aaName[0], x, y, Tile0.m_Number, Tile0.m_Type, Tile1.m_Number, Tile1.m_Type);
			});
		}
		else
		{
			// Regular/game layers use m_Data, the front layer uses m_Front, both as CTile.
			const int aData[2] = {
				(Flags & TILESLAYERFLAG_FRONT) ? apTilemap[0]->m_Front : apTilemap[0]->m_Data,
				(Flags & TILESLAYERFLAG_FRONT) ? apTilemap[1]->m_Front : apTilemap[1]->m_Data};
			Ok = DiffTileLayer<CTile>(aMaps, pMapNames, aData, Width, Height, [&](const CTile &Tile0, const CTile &Tile1, int x, int y) {
				if(Tile0.m_Index != Tile1.m_Index || Tile0.m_Flags != Tile1.m_Flags)
					dbg_msg("map_diff", "[%d:%s] %dx%d: (index: %d, flags: %d) != (index: %d, flags: %d)", aLayersNum[0], aaName[0], x, y, Tile0.m_Index, Tile0.m_Flags, Tile1.m_Index, Tile1.m_Flags);
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

	return Process(pStorage.get(), &argv[1]) ? 0 : 1;
}
