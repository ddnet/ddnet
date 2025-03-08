#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

bool Process(IStorage *pStorage, const char **pMapNames)
{
	CDataFileReader aMaps[2];

	for(int i = 0; i < 2; ++i)
	{
		if(!aMaps[i].Open(pStorage, pMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_compare", "error opening map '%s'", pMapNames[i]);
			return false;
		}

		const CMapItemVersion *pVersion = static_cast<CMapItemVersion *>(aMaps[i].FindItem(MAPITEMTYPE_VERSION, 0));
		if(pVersion == nullptr || pVersion->m_Version != 1)
		{
			dbg_msg("map_compare", "unsupported map version '%s'", pMapNames[i]);
			return false;
		}
	}

	int aStart[2], aNum[2];
	for(int i = 0; i < 2; ++i)
		aMaps[i].GetType(MAPITEMTYPE_LAYER, &aStart[i], &aNum[i]);

	// ensure basic layout
	if(aNum[0] != aNum[1])
	{
		dbg_msg("map_compare", "different layer numbers:");
		for(int i = 0; i < 2; ++i)
			dbg_msg("map_compare", "  \"%s\": %d layers", pMapNames[i], aNum[i]);
		return false;
	}

	// preload data
	for(int j = 0; j < aNum[0]; ++j)
	{
		for(int i = 0; i < 2; ++i)
		{
			CMapItemLayer *pItem = (CMapItemLayer *)aMaps[i].GetItem(aStart[i] + j);
			if(pItem->m_Type == LAYERTYPE_TILES)
				(void)aMaps[i].GetData(((CMapItemLayerTilemap *)pItem)->m_Data);
		}
	}

	// compare
	for(int j = 0; j < aNum[0]; ++j)
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

		if(str_comp(aaName[0], aaName[1]) != 0 || apTilemap[0]->m_Width != apTilemap[1]->m_Width || apTilemap[0]->m_Height != apTilemap[1]->m_Height)
		{
			dbg_msg("map_compare", "different tile layers:");
			for(int i = 0; i < 2; ++i)
				dbg_msg("map_compare", "  \"%s\" (%dx%d)", aaName[i], apTilemap[i]->m_Width, apTilemap[i]->m_Height);
			return false;
		}
		CTile *apTile[2];
		for(int i = 0; i < 2; ++i)
			apTile[i] = (CTile *)aMaps[i].GetData(apTilemap[i]->m_Data);

		for(int y = 0; y < apTilemap[0]->m_Height; y++)
		{
			for(int x = 0; x < apTilemap[0]->m_Width; x++)
			{
				int Pos = y * apTilemap[0]->m_Width + x;
				if(apTile[0][Pos].m_Index != apTile[1][Pos].m_Index || apTile[0][Pos].m_Flags != apTile[1][Pos].m_Flags)
				{
					dbg_msg("map_compare", "[%d:%s] %dx%d: (index: %d, flags: %d) != (index: %d, flags: %d)", aNum[0], aaName[0], x, y, apTile[0][Pos].m_Index, apTile[0][Pos].m_Flags, apTile[1][Pos].m_Index, apTile[0][Pos].m_Flags);
				}
			}
		}
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

	IStorage *pStorage = CreateLocalStorage();
	if(!pStorage)
		return -1;

	return Process(pStorage, &argv[1]) ? 0 : 1;
}
