#include <game/mapitems.h>
#include <game/gamecore.h>
#include <base/system.h>
#include <engine/external/pnglite/pnglite.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>

bool Process(IStorage *pStorage, char **pMapNames)
{
	CDataFileReader Maps[2];

	for(int i = 0; i < 2; ++i)
	{
		if(!Maps[i].Open(pStorage, pMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_compare", "error opening map '%s'", pMapNames[i]);
			return false;
		}

		CDataFileReader *pMap = &Maps[i];
		// check version
		CMapItemVersion *pVersion = (CMapItemVersion *)pMap->FindItem(MAPITEMTYPE_VERSION, 0);
		if(pVersion && pVersion->m_Version != 1)
			return false;
	}

	int Start[2], Num[2];

	Maps[0].GetType(MAPITEMTYPE_LAYER, &Start[0], &Num[0]);
	Maps[1].GetType(MAPITEMTYPE_LAYER, &Start[1], &Num[1]);

	// ensure basic layout
	if(Num[0] != Num[1])
	{
		dbg_msg("map_compare", "different layer numbers:");
		for(int i = 0; i < 2; ++i)
			dbg_msg("map_compare", "  \"%s\": %d layers", pMapNames[i], Num[i]);
		return false;
	}

	// preload data
	for(int j = 0; j < Num[0]; ++j)
	{
		CMapItemLayer *pItem[2];
		CMapItemLayerTilemap *pTilemap[2];
		for(int i = 0; i < 2; ++i)
		{
			pItem[i] = (CMapItemLayer *)Maps[i].GetItem(Start[i]+j, 0, 0);
			pTilemap[i] = (CMapItemLayerTilemap *)pItem[i];
			(void)(CTile *)Maps[i].GetData(pTilemap[i]->m_Data);
		}
	}

	// compare
	for(int j = 0; j < Num[0]; ++j)
	{
		CMapItemLayer *pItem[2];
		for(int i = 0; i < 2; ++i)
			pItem[i] = (CMapItemLayer *)Maps[i].GetItem(Start[i]+j, 0, 0);

		if(pItem[0]->m_Type != LAYERTYPE_TILES)
			continue;

		CMapItemLayerTilemap *pTilemap[2];
		char aName[2][16];

		for(int i = 0; i < 2; ++i)
		{
			pTilemap[i] = (CMapItemLayerTilemap *)pItem[i];
			IntsToStr(pTilemap[i]->m_aName, sizeof(pTilemap[i]->m_aName)/sizeof(int), aName[i]);
		}

		if(str_comp_num(aName[0], aName[1], sizeof(aName[0])) != 0 || pTilemap[0]->m_Width != pTilemap[1]->m_Width || pTilemap[0]->m_Height != pTilemap[1]->m_Height)
		{
			dbg_msg("map_compare", "different tile layers:");
			for(int i = 0; i < 2; ++i)
				dbg_msg("map_compare", "  \"%s\" (%dx%d)", aName[i], pTilemap[i]->m_Width, pTilemap[i]->m_Height);
			return false;
		}
		CTile *pTile[2];
		for(int i = 0; i < 2; ++i)
			pTile[i] = (CTile *)Maps[i].GetData(pTilemap[i]->m_Data);

		for(int y = 0; y < pTilemap[0]->m_Height; y++)
		{
			for(int x = 0; x < pTilemap[0]->m_Width; x++)
			{
				int pos = y * pTilemap[0]->m_Width + x;
				if(pTile[0][pos].m_Index != pTile[1][pos].m_Index || pTile[0][pos].m_Flags != pTile[1][pos].m_Flags)
				{
					dbg_msg("map_compare", "[%d:%s] %dx%d: (index: %d, flags: %d) != (index: %d, flags: %d)", Num[0], aName[0], x, y, pTile[0][pos].m_Index, pTile[0][pos].m_Flags, pTile[1][pos].m_Index, pTile[0][pos].m_Flags);
				}
			}
		}
	}

	return true;
}

int main(int argc, char* argv[])
{
	dbg_logger_stdout();
	dbg_logger_file("map_diff.txt");

	IStorage *pStorage = CreateLocalStorage();

	if(argc == 3)
	{
		return Process(pStorage, &argv[1]) ? 0 : 1;
	}
	else
	{
		dbg_msg("usage", "%s map1 map2", argv[0]);
		return -1;
	}
}
