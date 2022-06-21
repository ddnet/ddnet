#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

// global new layers data (set by ReplaceAreaTiles and ReplaceAreaQuads)
void *g_pNewData[1024];
void *g_pNewItem[1024];
int g_pNewDataSize[1024];

struct MapObject // quad pivot or tile layer
{
	static constexpr float m_pStandardScreen[2] = {1430 / 2, 1050 / 2};

	float m_pLayerOffset[2];
	bool m_UseClipping;
	float m_pClipArea[2][2];
	float m_pSpeed[2];
	float m_pScreenOffset[2][2];
	float m_pBaseArea[2][2]; // adapted to offset
	float m_pExtendedArea[2][2]; // extended with parallax
};

bool ReplaceArea(IStorage *, const char[][64], const float[][2][2]);
bool OpenMaps(IStorage *, const char[][64], CDataFileReader[], CDataFileWriter &);
void SaveOutputMap(CDataFileReader &, CDataFileWriter &);
bool CompareLayers(const char[][64], CDataFileReader[]);
void CompareGroups(const char[][64], CDataFileReader[]);
const CMapItemGroup *GetLayerGroup(CDataFileReader &, int);

void ReplaceAreaTiles(CDataFileReader[], const float[][2][2], const CMapItemGroup *[], CMapItemLayer *[]);
void RemoveDestinationTiles(CMapItemLayerTilemap *, CTile *, float[][2]);
void ReplaceDestinationTiles(CMapItemLayerTilemap *[], CTile *[], float[][2][2]);
bool AdaptVisibleAreas(const float[][2][2], const MapObject[], float[][2][2]);
bool AdaptReplaceableAreas(const float[][2][2], const float[][2][2], const MapObject[], float[][2][2]);

void ReplaceAreaQuads(CDataFileReader[], const float[][2][2], const CMapItemGroup *[], CMapItemLayer *[], int);
bool RemoveDestinationQuads(const float[][2], const CQuad *, int, const CMapItemGroup *, CQuad *, int &);
bool InsertDestinationQuads(const float[][2][2], const CQuad *, int, const CMapItemGroup *[], CQuad *, int &);
bool AdaptVisiblePoint(const float[][2][2], const float[][2], const MapObject[], float[]);

MapObject CreateMapObject(const CMapItemGroup *, int, int, int, int);
void SetExtendedArea(MapObject &);
bool GetVisibleArea(const float[][2], MapObject, float[][2] = 0x0);
bool GetReplaceableArea(const float[][2], MapObject, float[][2]);

void GetGameAreaDistance(const float[][2][2], const MapObject[], const float[][2][2], float[]);
void GetGameAreaDistance(const float[][2][2], const MapObject[], const float[][2], float[]);
void GetSignificantScreenPos(MapObject, const float[][2], const float[][2], float[]);
void ConvertToTiles(const float[][2], int[][2]);

bool GetLineIntersection(const float[], const float[], float[] = 0x0);
bool GetLineIntersection(const float[], float);
void SetInexistent(float *, int);
bool IsInexistent(const float *, int);
bool IsInexistent(float);

int main(int argc, const char *argv[])
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc != 10)
	{
		dbg_msg("map_replace_area", "Invalid arguments");
		dbg_msg("map_replace_area", "Usage: %s <from_map> <from_x> <from_y> <to_map> <to_x> <to_y> <width> <height> <output_map>", argv[0]);
		dbg_msg("map_replace_area", "Note: use game layer tiles as a reference for both coordinates and sizes");

		return -1;
	}

	char pMapNames[3][64];
	snprintf(pMapNames[0], 64, "%s", argv[1]); //from_map
	snprintf(pMapNames[1], 64, "%s", argv[4]); //to_map
	snprintf(pMapNames[2], 64, "%s", argv[9]); //output_map

	float pGameAreas[2][2][2];

	for(int i = 0; i < 2; i++)
	{
		pGameAreas[i][0][0] = atof(argv[2 + i * 3]) * 32; //x
		pGameAreas[i][1][0] = atof(argv[3 + i * 3]) * 32; //y
		pGameAreas[i][0][1] = pGameAreas[i][0][0] + atof(argv[7]) * 32; //x + width
		pGameAreas[i][1][1] = pGameAreas[i][1][0] + atof(argv[8]) * 32; //y + height
	}

	cmdline_free(argc, argv);

	dbg_msg("map_replace_area", "from_map='%s'; to_map='%s'; from_area='%fx,%fy'; to_area='%fx,%fy'; area_width='%fpx'; area_heigth='%fpx'; output_map='%s'",
		pMapNames[0], pMapNames[1], pGameAreas[0][0][0], pGameAreas[0][1][0], pGameAreas[1][0][0], pGameAreas[1][1][0],
		pGameAreas[0][0][1] - pGameAreas[0][0][0], pGameAreas[0][1][1] - pGameAreas[0][1][0], pMapNames[2]);

	IStorage *pStorage = CreateLocalStorage();
	for(int i = 0; i < 1024; i++)
	{
		g_pNewData[i] = g_pNewItem[i] = 0;
		g_pNewDataSize[i] = 0;
	}

	return ReplaceArea(pStorage, pMapNames, pGameAreas) ? 0 : 1;
}

bool ReplaceArea(IStorage *pStorage, const char pMapNames[3][64], const float pGameAreas[][2][2])
{
	CDataFileReader InputMaps[2];
	CDataFileWriter OutputMap;

	if(!OpenMaps(pStorage, pMapNames, InputMaps, OutputMap))
		return false;
	if(!CompareLayers(pMapNames, InputMaps))
		return false;
	CompareGroups(pMapNames, InputMaps);

	int LayersStart[2], LayersCount;
	for(int i = 0; i < 2; i++)
		InputMaps[i].GetType(MAPITEMTYPE_LAYER, &LayersStart[i], &LayersCount);

	for(int i = 0; i < LayersCount; i++)
	{
		const CMapItemGroup *pLayerGroups[2];
		CMapItemLayer *pItem[2];
		for(int j = 0; j < 2; j++)
		{
			pLayerGroups[j] = GetLayerGroup(InputMaps[j], i + 1);
			pItem[j] = (CMapItemLayer *)InputMaps[j].GetItem(LayersStart[j] + i, 0, 0);
		}

		if(!pLayerGroups[0] || !pLayerGroups[1])
			continue;

		if(pItem[0]->m_Type == LAYERTYPE_TILES)
			ReplaceAreaTiles(InputMaps, pGameAreas, pLayerGroups, pItem);
		else if(pItem[0]->m_Type == LAYERTYPE_QUADS)
			ReplaceAreaQuads(InputMaps, pGameAreas, pLayerGroups, pItem, LayersStart[1] + i);
	}

	SaveOutputMap(InputMaps[1], OutputMap);

	return true;
}

bool OpenMaps(IStorage *pStorage, const char pMapNames[3][64], CDataFileReader InputMaps[2], CDataFileWriter &OutputMap)
{
	for(int i = 0; i < 2; i++)
	{
		if(!InputMaps[i].Open(pStorage, pMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_replace_area", "ERROR: unable to open map '%s'", pMapNames[i]);
			return false;
		}
	}

	if(!OutputMap.Open(pStorage, pMapNames[2], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_replace_area", "ERROR: unable to open map '%s'", pMapNames[2]);
		return false;
	}

	return true;
}

void SaveOutputMap(CDataFileReader &InputMap, CDataFileWriter &OutputMap)
{
	for(int i = 0; i < InputMap.NumItems(); i++)
	{
		int ID, Type;
		void *pItem = InputMap.GetItem(i, &Type, &ID);

		if(Type == ITEMTYPE_EX)
			continue;
		if(g_pNewItem[i])
			pItem = g_pNewItem[i];

		int Size = InputMap.GetItemSize(i);
		OutputMap.AddItem(Type, ID, Size, pItem);
	}

	for(int i = 0; i < InputMap.NumData(); i++)
	{
		void *pData = g_pNewData[i] ? g_pNewData[i] : InputMap.GetData(i);
		int Size = g_pNewDataSize[i] ? g_pNewDataSize[i] : InputMap.GetDataSize(i);
		OutputMap.AddData(Size, pData);
	}

	OutputMap.Finish();
}

bool CompareLayers(const char pMapNames[3][64], CDataFileReader InputMaps[2])
{
	int Start[2], Num[2];
	for(int i = 0; i < 2; i++)
		InputMaps[i].GetType(MAPITEMTYPE_LAYER, &Start[i], &Num[i]);

	if(Num[0] != Num[1])
	{
		dbg_msg("map_replace_area", "ERROR: different layers quantity");
		for(int i = 0; i < 2; i++)
			dbg_msg("map_replace_area", " \"%s\": %d layers", pMapNames[i], Num[i]);
		return false;
	}

	for(int i = 0; i < Num[0]; i++)
	{
		CMapItemLayer *pItem[2];
		for(int j = 0; j < 2; j++)
			pItem[j] = (CMapItemLayer *)InputMaps[j].GetItem(Start[j] + i, 0, 0);

		if(pItem[0]->m_Type != pItem[1]->m_Type)
		{
			dbg_msg("map_replace_area", "ERROR: different types on layer #%d", i);
			for(int j = 0; j < 2; j++)
				dbg_msg("map_replace_area", " \"%s\": %s", pMapNames[j], pItem[j]->m_Type == LAYERTYPE_TILES ? "tiles layer" : "quad layer");
			return false;
		}
	}

	return true;
}

void CompareGroups(const char pMapNames[3][64], CDataFileReader InputMaps[2])
{
	int Start[2], Num[2];
	for(int i = 0; i < 2; i++)
		InputMaps[i].GetType(MAPITEMTYPE_GROUP, &Start[i], &Num[i]);

	for(int i = 0; i < std::max(Num[0], Num[1]); i++)
	{
		CMapItemGroup *pItem[2];
		for(int j = 0; j < 2; j++)
			pItem[j] = (CMapItemGroup *)InputMaps[j].GetItem(Start[j] + i, 0, 0);

		bool bSameConfig = pItem[0]->m_ParallaxX == pItem[1]->m_ParallaxX && pItem[0]->m_ParallaxY == pItem[1]->m_ParallaxY && pItem[0]->m_OffsetX == pItem[1]->m_OffsetX && pItem[0]->m_OffsetY == pItem[1]->m_OffsetY && pItem[0]->m_UseClipping == pItem[1]->m_UseClipping && pItem[0]->m_ClipX == pItem[1]->m_ClipX && pItem[0]->m_ClipY == pItem[1]->m_ClipY && pItem[0]->m_ClipW == pItem[1]->m_ClipW && pItem[0]->m_ClipH == pItem[1]->m_ClipH;

		if(!bSameConfig)
			dbg_msg("map_replace_area", "WARNING: different configuration on layergroup #%d, this might lead to unexpected results", i);
	}
}

const CMapItemGroup *GetLayerGroup(CDataFileReader &InputMap, const int LayerNumber)
{
	int Start, Num;
	InputMap.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemGroup *pItem = (CMapItemGroup *)InputMap.GetItem(Start + i, 0, 0);
		if(LayerNumber >= pItem->m_StartLayer && LayerNumber <= pItem->m_StartLayer + pItem->m_NumLayers)
			return pItem;
	}

	return 0x0;
}

void ReplaceAreaTiles(CDataFileReader InputMaps[2], const float pGameAreas[][2][2], const CMapItemGroup *pLayerGroups[2], CMapItemLayer *pItem[2])
{
	CMapItemLayerTilemap *pTilemap[2];
	CTile *pTile[2];
	float pVisibleAreas[2][2][2], pReplaceableAreas[2][2][2];
	MapObject Obs[2];

	for(int i = 0; i < 2; i++)
	{
		pTilemap[i] = (CMapItemLayerTilemap *)pItem[i];
		pTile[i] = (CTile *)InputMaps[i].GetData(pTilemap[i]->m_Data);
		Obs[i] = CreateMapObject(pLayerGroups[i], 0, 0, pTilemap[i]->m_Width * 32, pTilemap[i]->m_Height * 32);
	}

	if(!GetVisibleArea(pGameAreas[1], Obs[1], pVisibleAreas[1]))
		return;

	GetReplaceableArea(pVisibleAreas[1], Obs[1], pReplaceableAreas[1]);
	RemoveDestinationTiles(pTilemap[1], pTile[1], pReplaceableAreas[1]);

	if(GetVisibleArea(pGameAreas[0], Obs[0], pVisibleAreas[0]) && AdaptVisibleAreas(pGameAreas, Obs, pVisibleAreas))
	{
		for(int i = 0; i < 2; i++)
			GetReplaceableArea(pVisibleAreas[i], Obs[i], pReplaceableAreas[i]);

		if(AdaptReplaceableAreas(pGameAreas, pVisibleAreas, Obs, pReplaceableAreas))
			ReplaceDestinationTiles(pTilemap, pTile, pReplaceableAreas);
	}

	g_pNewData[pTilemap[1]->m_Data] = pTile[1];
}

void RemoveDestinationTiles(CMapItemLayerTilemap *pTilemap, CTile *pTile, float pReplaceableArea[2][2])
{
	int pRange[2][2];
	ConvertToTiles(pReplaceableArea, pRange);

	CTile EmptyTile;
	EmptyTile.m_Index = EmptyTile.m_Flags = EmptyTile.m_Skip = EmptyTile.m_Reserved = 0;

	for(int y = pRange[1][0]; y < pRange[1][1]; y++)
		for(int x = pRange[0][0]; x < pRange[0][1]; x++)
			pTile[x + (y * pTilemap->m_Width)] = EmptyTile;
}

void ReplaceDestinationTiles(CMapItemLayerTilemap *pTilemap[2], CTile *pTile[2], float pReplaceableAreas[2][2][2])
{
	int pRanges[2][2][2];
	for(int i = 0; i < 2; i++)
		ConvertToTiles(pReplaceableAreas[i], pRanges[i]);

	for(int y0 = pRanges[0][1][0], y1 = pRanges[1][1][0]; y0 < pRanges[0][1][1] && y1 < pRanges[1][1][1]; y0++, y1++)
		for(int x0 = pRanges[0][0][0], x1 = pRanges[1][0][0]; x0 < pRanges[0][0][1] && x1 < pRanges[1][0][1]; x0++, x1++)
			pTile[1][x1 + (y1 * pTilemap[1]->m_Width)] = pTile[0][x0 + (y0 * pTilemap[0]->m_Width)];
}

bool AdaptVisibleAreas(const float pGameAreas[2][2][2], const MapObject Obs[2], float pVisibleAreas[2][2][2])
{
	float pDistance[2];
	GetGameAreaDistance(pGameAreas, Obs, pVisibleAreas, pDistance);

	for(int i = 0; i < 2; i++)
	{
		if(Obs[0].m_pSpeed[i] == 1 || Obs[1].m_pSpeed[i] == 1)
			continue;

		for(int j = 0; j < 2; j++)
			pVisibleAreas[1][i][j] -= pDistance[i];

		if(!GetLineIntersection(pVisibleAreas[0][i], pVisibleAreas[1][i], pVisibleAreas[0][i]))
			return false;

		for(int j = 0; j < 2; j++)
			pVisibleAreas[1][i][j] = pVisibleAreas[0][i][j] + pDistance[i];
	}

	return true;
}

bool AdaptReplaceableAreas(const float pGameAreas[2][2][2], const float pVisibleAreas[2][2][2], const MapObject Obs[2], float pReplaceableAreas[2][2][2])
{
	float pDistance[2], pScreenPos[2];
	GetGameAreaDistance(pGameAreas, Obs, pVisibleAreas, pDistance);
	GetSignificantScreenPos(Obs[0], pVisibleAreas[0], pReplaceableAreas[0], pScreenPos);

	for(int i = 0; i < 2; i++)
	{
		float pDestLine[2], pSourceLine[2], pVisibleLine[2];

		pDestLine[0] = Obs[1].m_pBaseArea[i][0] + (pScreenPos[i] + pDistance[i]) * Obs[1].m_pSpeed[i];
		pDestLine[1] = pDestLine[0] + (Obs[1].m_pBaseArea[i][1] - Obs[1].m_pBaseArea[i][0]);

		if(!GetLineIntersection(pDestLine, pVisibleAreas[1][i], pVisibleLine))
			return false;

		pSourceLine[0] = pVisibleAreas[0][i][0] + pDistance[i] - pReplaceableAreas[0][i][0];
		pSourceLine[1] = pVisibleAreas[0][i][1] + pDistance[i] + pReplaceableAreas[0][i][1] - pReplaceableAreas[0][i][0];

		if(!GetLineIntersection(pSourceLine, pVisibleLine, pVisibleLine))
			return false;

		pReplaceableAreas[0][i][0] = pVisibleLine[0] - pSourceLine[0];
		pReplaceableAreas[1][i][0] = pVisibleLine[0] - pDestLine[0];
	}

	return true;
}

void ReplaceAreaQuads(CDataFileReader InputMaps[2], const float pGameAreas[][2][2], const CMapItemGroup *pLayerGroups[2], CMapItemLayer *pItem[2], const int ItemNumber)
{
	CMapItemLayerQuads *pQuadLayer[2];
	for(int i = 0; i < 2; i++)
		pQuadLayer[i] = (CMapItemLayerQuads *)pItem[i];

	CQuad *pQuads[3];
	for(int i = 0; i < 2; i++)
		pQuads[i] = (CQuad *)InputMaps[i].GetDataSwapped(pQuadLayer[i]->m_Data);

	pQuads[2] = new CQuad[pQuadLayer[0]->m_NumQuads + pQuadLayer[1]->m_NumQuads];
	int QuadsCounter = 0;

	bool bDataChanged = RemoveDestinationQuads(pGameAreas[1], pQuads[1], pQuadLayer[1]->m_NumQuads, pLayerGroups[1], pQuads[2], QuadsCounter);
	bDataChanged |= InsertDestinationQuads(pGameAreas, pQuads[0], pQuadLayer[0]->m_NumQuads, pLayerGroups, pQuads[2], QuadsCounter);

	if(bDataChanged)
	{
		g_pNewData[pQuadLayer[1]->m_Data] = pQuads[2];
		g_pNewDataSize[pQuadLayer[1]->m_Data] = ((int)sizeof(CQuad)) * QuadsCounter;
		pQuadLayer[1]->m_NumQuads = QuadsCounter;
		g_pNewItem[ItemNumber] = pItem[1];
	}
	else
		delete[] pQuads[2];
}

bool RemoveDestinationQuads(const float pGameArea[2][2], const CQuad *pQuads, const int NumQuads, const CMapItemGroup *pLayerGroup, CQuad *pDestQuads, int &QuadsCounter)
{
	bool bDataChanged = false;

	for(int i = 0; i < NumQuads; i++)
	{
		MapObject Ob = CreateMapObject(pLayerGroup, fx2f(pQuads[i].m_aPoints[4].x), fx2f(pQuads[i].m_aPoints[4].y), 0, 0);

		if(GetVisibleArea(pGameArea, Ob))
		{
			bDataChanged = true;
			continue;
		}

		pDestQuads[QuadsCounter] = pQuads[i];
		QuadsCounter++;
	}

	return bDataChanged;
}

bool InsertDestinationQuads(const float pGameAreas[2][2][2], const CQuad *pQuads, const int NumQuads, const CMapItemGroup *pLayerGroups[2], CQuad *pDestQuads, int &QuadsCounter)
{
	bool bDataChanged = false;

	for(int i = 0; i < NumQuads; i++)
	{
		MapObject Obs[2];
		Obs[0] = CreateMapObject(pLayerGroups[0], fx2f(pQuads[i].m_aPoints[4].x), fx2f(pQuads[i].m_aPoints[4].y), 0, 0);
		float pVisibleArea[2][2];

		if(GetVisibleArea(pGameAreas[0], Obs[0], pVisibleArea))
		{
			float pQuadPos[2];
			Obs[1] = CreateMapObject(pLayerGroups[1], 0, 0, 0, 0);

			if(!AdaptVisiblePoint(pGameAreas, pVisibleArea, Obs, pQuadPos))
				continue;

			pDestQuads[QuadsCounter] = pQuads[i];
			for(auto &Point : pDestQuads[QuadsCounter].m_aPoints)
			{
				Point.x += f2fx(pQuadPos[0]) - pDestQuads[QuadsCounter].m_aPoints[4].x;
				Point.y += f2fx(pQuadPos[1]) - pDestQuads[QuadsCounter].m_aPoints[4].y;
			}

			QuadsCounter++;
			bDataChanged = true;
		}
	}

	return bDataChanged;
}

bool AdaptVisiblePoint(const float pGameAreas[2][2][2], const float pVisibleArea[2][2], const MapObject Obs[2], float pPos[2])
{
	float pDistance[2], pScreenPos[2];
	GetGameAreaDistance(pGameAreas, Obs, pVisibleArea, pDistance);
	GetSignificantScreenPos(Obs[0], pVisibleArea, 0x0, pScreenPos);

	for(int i = 0; i < 2; i++)
		pPos[i] = pVisibleArea[i][0] + pDistance[i] + Obs[1].m_pLayerOffset[i] - (pScreenPos[i] + pDistance[i]) * Obs[1].m_pSpeed[i];

	MapObject FinalOb = Obs[1];
	for(int i = 0; i < 2; i++)
		FinalOb.m_pBaseArea[i][0] = FinalOb.m_pBaseArea[i][1] += pPos[i];
	SetExtendedArea(FinalOb);

	return GetVisibleArea(pGameAreas[1], FinalOb);
}

MapObject CreateMapObject(const CMapItemGroup *pLayerGroup, const int PosX, const int PosY, const int Width, const int Height)
{
	MapObject Ob;

	Ob.m_pBaseArea[0][0] = PosX - pLayerGroup->m_OffsetX;
	Ob.m_pBaseArea[1][0] = PosY - pLayerGroup->m_OffsetY;
	Ob.m_pBaseArea[0][1] = Ob.m_pBaseArea[0][0] + Width;
	Ob.m_pBaseArea[1][1] = Ob.m_pBaseArea[1][0] + Height;
	Ob.m_pLayerOffset[0] = pLayerGroup->m_OffsetX;
	Ob.m_pLayerOffset[1] = pLayerGroup->m_OffsetY;
	Ob.m_UseClipping = pLayerGroup->m_UseClipping;
	Ob.m_pClipArea[0][0] = pLayerGroup->m_ClipX;
	Ob.m_pClipArea[1][0] = pLayerGroup->m_ClipY;
	Ob.m_pClipArea[0][1] = pLayerGroup->m_ClipX + pLayerGroup->m_ClipW;
	Ob.m_pClipArea[1][1] = pLayerGroup->m_ClipY + pLayerGroup->m_ClipH;
	Ob.m_pSpeed[0] = 1 - (pLayerGroup->m_ParallaxX / 100.0f);
	Ob.m_pSpeed[1] = 1 - (pLayerGroup->m_ParallaxY / 100.0f);

	for(int i = 0; i < 2; i++)
	{
		Ob.m_pScreenOffset[i][0] = -Ob.m_pStandardScreen[i];
		Ob.m_pScreenOffset[i][1] = Ob.m_pStandardScreen[i];
		if(Ob.m_pSpeed[i] < 0)
			std::swap(Ob.m_pScreenOffset[i][0], Ob.m_pScreenOffset[i][1]);
	}

	SetExtendedArea(Ob);
	return Ob;
}

void SetExtendedArea(MapObject &Ob)
{
	SetInexistent((float *)Ob.m_pExtendedArea, 4);

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_pSpeed[i] == 1)
		{
			float pInspectedArea[2];
			if(GetLineIntersection(Ob.m_pBaseArea[i], Ob.m_pScreenOffset[i], pInspectedArea))
				memcpy(Ob.m_pExtendedArea[i], pInspectedArea, sizeof(float[2]));
			continue;
		}

		for(int j = 0; j < 2; j++)
			Ob.m_pExtendedArea[i][j] = (Ob.m_pBaseArea[i][j] + Ob.m_pScreenOffset[i][j] * Ob.m_pSpeed[i]) / (1 - Ob.m_pSpeed[i]);

		if(Ob.m_pExtendedArea[i][0] > Ob.m_pExtendedArea[i][1])
			std::swap(Ob.m_pExtendedArea[i][0], Ob.m_pExtendedArea[i][1]);
	}
}

bool GetVisibleArea(const float pGameArea[2][2], const MapObject Ob, float pVisibleArea[2][2])
{
	if(IsInexistent((float *)Ob.m_pExtendedArea, 4))
		return false;

	if(pVisibleArea)
		SetInexistent((float *)pVisibleArea, 4);

	float pInspectedArea[2][2];
	memcpy(pInspectedArea, pGameArea, sizeof(float[2][2]));

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_pSpeed[i] == 1)
		{
			memcpy(pInspectedArea[i], Ob.m_pExtendedArea[i], sizeof(float[2]));
			continue;
		}

		if(Ob.m_UseClipping && !GetLineIntersection(pInspectedArea[i], Ob.m_pClipArea[i], pInspectedArea[i]))
			return false;

		if(!GetLineIntersection(pInspectedArea[i], Ob.m_pExtendedArea[i], pInspectedArea[i]))
			return false;
	}

	if(pVisibleArea)
		memcpy(pVisibleArea, pInspectedArea, sizeof(float[2][2]));

	return true;
}

bool GetReplaceableArea(const float pVisibleArea[2][2], const MapObject Ob, float pReplaceableArea[2][2])
{
	SetInexistent((float *)pReplaceableArea, 4);
	if(IsInexistent((float *)pVisibleArea, 4))
		return false;

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_pSpeed[i] == 1)
		{
			pReplaceableArea[i][0] = pVisibleArea[i][0] - Ob.m_pBaseArea[i][0];
			pReplaceableArea[i][1] = pVisibleArea[i][1] - Ob.m_pBaseArea[i][0];
			continue;
		}

		for(int j = 0; j < 2; j++)
		{
			float pVisibleLine[2], pReplaceableLine[2];
			int k = Ob.m_pSpeed[i] > 1 ? !j : j;

			pVisibleLine[0] = Ob.m_pBaseArea[i][0] + (pVisibleArea[i][j] - Ob.m_pScreenOffset[i][k]) * Ob.m_pSpeed[i];
			pVisibleLine[1] = pVisibleLine[0] + Ob.m_pBaseArea[i][1] - Ob.m_pBaseArea[i][0];

			if(GetLineIntersection(pVisibleArea[i], pVisibleLine, pReplaceableLine))
				pReplaceableArea[i][k] = pReplaceableLine[j] - pVisibleLine[0];
			else
				pReplaceableArea[i][k] = k * (Ob.m_pBaseArea[i][1] - Ob.m_pBaseArea[i][0]);
		}
	}

	return true;
}

void GetGameAreaDistance(const float pGameAreas[2][2][2], const MapObject Obs[2], const float pVisibleAreas[2][2][2], float pDistance[2])
{
	for(int i = 0; i < 2; i++)
	{
		if(Obs[0].m_pSpeed[i] == 1 && Obs[1].m_pSpeed[i] == 1)
			pDistance[i] = 0;
		else if(Obs[0].m_pSpeed[i] == 1 && Obs[1].m_pSpeed[i] != 1)
			pDistance[i] = pGameAreas[1][i][0] - pVisibleAreas[0][i][0];
		else if(Obs[0].m_pSpeed[i] != 1 && Obs[1].m_pSpeed[i] == 1)
			pDistance[i] = pVisibleAreas[1][i][0] - pGameAreas[0][i][0];
		else
			pDistance[i] = pGameAreas[1][i][0] - pGameAreas[0][i][0];
	}
}

void GetGameAreaDistance(const float pGameAreas[2][2][2], const MapObject Obs[2], const float pVisibleArea[2][2], float pDistance[2])
{
	float pVisibleAreas[2][2][2];
	memcpy(pVisibleAreas[0], pVisibleArea[0], sizeof(float[2][2]));
	memcpy(pVisibleAreas[1], pVisibleArea[0], sizeof(float[2][2]));
	GetGameAreaDistance(pGameAreas, Obs, pVisibleAreas, pDistance);
}

void GetSignificantScreenPos(const MapObject Ob, const float pVisibleArea[2][2], const float pReplaceableArea[2][2], float pScreen[2])
{
	for(int i = 0; i < 2; i++)
	{
		if(!Ob.m_pSpeed[i])
		{
			pScreen[i] = pVisibleArea[i][0] + Ob.m_pScreenOffset[i][1];
			continue;
		}

		float BaseOffset = pReplaceableArea ? pReplaceableArea[i][0] : 0;
		pScreen[i] = (pVisibleArea[i][0] - Ob.m_pBaseArea[i][0] - BaseOffset) / Ob.m_pSpeed[i];
	}
}

void ConvertToTiles(const float pArea[2][2], int pTiles[2][2])
{
	for(int i = 0; i < 2; i++)
	{
		pTiles[i][0] = floor((floor(pArea[i][0] * 100.0f) / 100.0f) / 32.0f);
		pTiles[i][1] = ceil((floor(pArea[i][1] * 100.0f) / 100.0f) / 32.0f);
	}
}

bool GetLineIntersection(const float pLine1[2], const float pLine2[2], float pIntersection[2])
{
	float pBorders[2] = {
		std::max(pLine1[0], pLine2[0]),
		std::min(pLine1[1], pLine2[1])};

	if(pIntersection)
		SetInexistent((float *)pIntersection, 2);

	if(pBorders[0] - pBorders[1] > 0.01f)
		return false;

	if(pIntersection)
		memcpy(pIntersection, pBorders, sizeof(float[2]));

	return true;
}

bool GetLineIntersection(const float pLine[2], const float Point)
{
	return pLine[0] - Point <= 0.01f && pLine[1] - Point >= 0.01f;
}

void SetInexistent(float *pArray, const int Count)
{
	for(int i = 0; i < Count; i++)
		pArray[i] = std::numeric_limits<float>::max();
}

bool IsInexistent(const float *pArray, const int Count)
{
	for(int i = 0; i < Count; i++)
		if(pArray[i] == std::numeric_limits<float>::max())
			return true;
	return false;
}

bool IsInexistent(const float Value)
{
	return Value == std::numeric_limits<float>::max();
}
