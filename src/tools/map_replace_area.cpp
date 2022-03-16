#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

// global new layers data (set by ReplaceAreaTiles and ReplaceAreaQuads)
void *g_pNewData[1024];
void *g_pNewItem[1024];
int g_NewDataSize[1024];

bool ReplaceArea(IStorage*, const char[][64], const int[][2]);
bool OpenMaps(IStorage*, const char[][64], CDataFileReader[], CDataFileWriter&);
bool CompareLayers(const char[][64], CDataFileReader[]);
void ReplaceAreaTiles(CDataFileReader[], const int[][2], CMapItemLayer*[]);
void ReplaceAreaQuads(CDataFileReader[], const int[][2], CMapItemLayer*[], const int);
bool IsQuadInsideArea(const int[][2], const int, CPoint&);
void SaveOutputMap(CDataFileReader[], CDataFileWriter&);

int main(int argc, const char *argv[])
{
	cmdline_fix(&argc, &argv);
	dbg_logger_stdout();

	if(argc != 10)
	{
		dbg_msg("map_replace_area", "Invalid arguments");
		dbg_msg("map_replace_area", "Usage: %s <from_map> <from_x> <from_y> <to_map> <to_x> <to_y> <width> <height> <output_map>", argv[0]);
		return -1;
	}

	char pMapNames[3][64];
	strcpy(pMapNames[0], argv[1]); //from_map
	strcpy(pMapNames[1], argv[4]); //to_map
	strcpy(pMapNames[2], argv[9]); //output_map

	int pAreaData[3][2];
	pAreaData[0][0] = atoi(argv[2]); //from_x
	pAreaData[0][1] = atoi(argv[3]); //from_y
	pAreaData[1][0] = atoi(argv[5]); //to_x
	pAreaData[1][1] = atoi(argv[6]); //to_y
	pAreaData[2][0] = atoi(argv[7]); //width
	pAreaData[2][1] = atoi(argv[8]); //height

	cmdline_free(argc, argv);

	IStorage *pStorage = CreateLocalStorage();
	for(int i = 0; i < 1024; i++)
	{
		g_pNewData[i] = g_pNewItem[i] = 0;
		g_NewDataSize[i] = 0;
	}

	dbg_msg("map_replace_area", "from_map='%s'; to_map='%s'; from_area='(%dx,%dy)'; to_area='%dx,%dy'; area_width='%d'; area_heigth='%d'; output_map='%s'",
		pMapNames[0], pMapNames[1], pAreaData[0][0], pAreaData[0][1], pAreaData[1][0], pAreaData[1][1], pAreaData[2][0], pAreaData[2][1], pMapNames[2]);

        return ReplaceArea(pStorage, pMapNames, pAreaData) ? 0 : 1;
}

bool ReplaceArea(IStorage *pStorage, const char pMapNames[3][64], const int pAreaData[2][2])
{

	CDataFileReader InputMaps[2];
        CDataFileWriter OutputMap;

        if(!OpenMaps(pStorage, pMapNames, InputMaps, OutputMap))
        	return false;

        if(!CompareLayers(pMapNames, InputMaps))
        	return false;

	int LayersOffset[2], LayersCount;
	InputMaps[0].GetType(MAPITEMTYPE_LAYER, &LayersOffset[0], &LayersCount);
	InputMaps[1].GetType(MAPITEMTYPE_LAYER, &LayersOffset[1], &LayersCount);

	for(int j = 0; j < LayersCount; j++)
	{
		CMapItemLayer *pItem[2];
		for(int i = 0; i < 2; i++)
			pItem[i] = (CMapItemLayer *)InputMaps[i].GetItem(LayersOffset[i] + j, 0, 0);

		if(pItem[0]->m_Type == LAYERTYPE_TILES)
			ReplaceAreaTiles(InputMaps, pAreaData, pItem);
		else if (pItem[0]->m_Type == LAYERTYPE_QUADS)
			ReplaceAreaQuads(InputMaps, pAreaData, pItem, LayersOffset[1] + j);
	}

	SaveOutputMap(InputMaps, OutputMap);

	return true;
}


bool OpenMaps(IStorage *pStorage, const char pMapNames[3][64], CDataFileReader InputMaps[2], CDataFileWriter& OutputMap)
{
	for(int i = 0; i < 2; i++)
	{
		if(!InputMaps[i].Open(pStorage, pMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_replace_area", "Error: unable to open map '%s'", pMapNames[i]);
			return false;
		}
	}

	if(!OutputMap.Open(pStorage, pMapNames[2], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_replace_area", "Error: unable to open map '%s'", pMapNames[2]);
		return false;
	}
	
	return true;
}

bool CompareLayers(const char pMapNames[3][64], CDataFileReader InputMaps[2])
{
	int Start[2], Num[2];
	InputMaps[0].GetType(MAPITEMTYPE_LAYER, &Start[0], &Num[0]);
	InputMaps[1].GetType(MAPITEMTYPE_LAYER, &Start[1], &Num[1]);

	if(Num[0] != Num[1])
	{
		dbg_msg("map_replace_area", "Error: different layers number");
		for(int i = 0; i < 2; i++)
			dbg_msg("map_replace_area", " \"%s\": %d layers", pMapNames[i], Num[i]);
		return false;
	}

	for(int j = 0; j < Num[0]; j++)
	{
		CMapItemLayer *pItem[2];
		for(int i = 0; i < 2; i++)
			pItem[i] = (CMapItemLayer *)InputMaps[i].GetItem(Start[i] + j, 0, 0);

		if(pItem[0]->m_Type != pItem[1]->m_Type)
		{
			dbg_msg("map_replace_area", "Error: different types on layer number %d", j);
			for(int i = 0; i < 2; i++)
			{
				char aLayerType[16];
				strcpy(aLayerType, pItem[i]->m_Type == LAYERTYPE_TILES ? "tiles layer" : "quad layer");
				dbg_msg("map_replace_area", " \"%s\": %s", pMapNames[i], aLayerType);
			}
			return false;
		}
	}

	return true;
}

void ReplaceAreaTiles(CDataFileReader InputMaps[2], const int pAreaData[2][2], CMapItemLayer* pItem[2])
{
	CMapItemLayerTilemap *pTilemap[2];
	for(int i = 0; i < 2; ++i)
		pTilemap[i] = (CMapItemLayerTilemap *)pItem[i];

	CTile *pTile[2];
	for(int i = 0; i < 2; ++i)
		pTile[i] = (CTile *)InputMaps[i].GetData(pTilemap[i]->m_Data);

	bool bDataChanged = false;
	for(int y = 0; y < pAreaData[2][1]; y++)
	{
		if(y + pAreaData[0][1] >= pTilemap[0]->m_Height || y + pAreaData[1][1] >= pTilemap[1]->m_Height)
			continue;

		for(int x = 0; x < pAreaData[2][0]; x++)
		{
			if(x + pAreaData[0][0] >= pTilemap[1]->m_Width || x + pAreaData[1][0] >= pTilemap[1]->m_Width)
				continue;

			int pos[2];
			for(int i = 0; i < 2; i++)
				pos[i] = (y + pAreaData[i][1]) * pTilemap[i]->m_Width + x + pAreaData[i][0];

			pTile[1][pos[1]] = pTile[0][pos[0]];
			bDataChanged = true;
		}
	}

	if(bDataChanged)
		g_pNewData[pTilemap[1]->m_Data] = pTile[1];
}

void ReplaceAreaQuads(CDataFileReader InputMaps[2], const int pAreaData[2][2], CMapItemLayer* pItem[2], const int ItemNumber)
{

	CMapItemLayerQuads *pQuadLayer[2];
	for(int i = 0; i < 2; i++)
		pQuadLayer[i] = (CMapItemLayerQuads *)pItem[i];

        CQuad *pQuads[3];
        for(int i = 0; i < 2; i++)
		pQuads[i] = (CQuad *)InputMaps[i].GetDataSwapped(pQuadLayer[i]->m_Data);

	pQuads[3] = new CQuad[maximum(pQuadLayer[0]->m_NumQuads, pQuadLayer[1]->m_NumQuads)];
	int QuadsCounter = 0;

	bool bDataChanged = false;
	for(int i = 0; i < pQuadLayer[1]->m_NumQuads; i++)
	{
		if(IsQuadInsideArea(pAreaData, 1, pQuads[1][i].m_aPoints[4]))
		{
			bDataChanged = true;
			continue;
		}

		pQuads[3][QuadsCounter] = pQuads[1][i];
		QuadsCounter++;
	}

	for(int i = 0; i < pQuadLayer[0]->m_NumQuads; i++)
	{
		if(IsQuadInsideArea(pAreaData, 0, pQuads[0][i].m_aPoints[4]))
		{
			pQuads[3][QuadsCounter] = pQuads[0][i];

			float Distancex = (pAreaData[0][0] - pAreaData[1][0]) * 32;
			float Distancey = (pAreaData[0][1] - pAreaData[1][1]) * 32;

			for(int j = 0; j < 5; j++)
			{
				pQuads[3][QuadsCounter].m_aPoints[j].x -= f2fx(Distancex);
				pQuads[3][QuadsCounter].m_aPoints[j].y -= f2fx(Distancey);
			}

			QuadsCounter++;
			bDataChanged = true;
		}
	}

	if(bDataChanged)
	{
		g_pNewData[pQuadLayer[1]->m_Data] = pQuads[3];
		g_NewDataSize[pQuadLayer[1]->m_Data] = ((int)sizeof(CQuad)) * QuadsCounter;
		pQuadLayer[1]->m_NumQuads = QuadsCounter;
		g_pNewItem[ItemNumber] = pItem[1];
	}

}

bool IsQuadInsideArea(const int pAreaData[2][2], const int AreaIndex, CPoint& QuadPivot)
{
	return fx2f(QuadPivot.x) / 32 >= pAreaData[AreaIndex][0]
		&& fx2f(QuadPivot.x) / 32 <= pAreaData[AreaIndex][0] + pAreaData[2][0]
		&& fx2f(QuadPivot.y) / 32 >= pAreaData[AreaIndex][1]
		&& fx2f(QuadPivot.y) / 32 <= pAreaData[AreaIndex][1] + pAreaData[2][1];
}

void SaveOutputMap(CDataFileReader InputMaps[2], CDataFileWriter& OutputMap)
{
	for(int i = 0; i < InputMaps[1].NumItems(); i++)
	{
		int ID, Type;
		int Size = InputMaps[1].GetItemSize(i);
		void *pItem = InputMaps[1].GetItem(i, &Type, &ID);
		if(g_pNewItem[i])
			pItem = g_pNewItem[i];

		// filter ITEMTYPE_EX items, they will be automatically added again
		if(Type == ITEMTYPE_EX)
			continue;

		OutputMap.AddItem(Type, ID, Size, pItem);
	}

	for(int i = 0; i < InputMaps[1].NumData(); i++)
	{
		void *pData = g_pNewData[i] ? g_pNewData[i] : InputMaps[1].GetData(i);
		int Size = g_NewDataSize[i] ? g_NewDataSize[i] : InputMaps[1].GetDataSize(i);
		OutputMap.AddData(Size, pData);
	}

	OutputMap.Finish();
}
