#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

// global new layers data (set by ReplaceAreaTiles and ReplaceAreaQuads)
void *g_apNewData[1024];
void *g_apNewItem[1024];
int g_aNewDataSize[1024];

class CMapObject // quad pivot or tile layer
{
public:
	static constexpr float ms_aStandardScreen[2] = {1430 / 2.f, 1050 / 2.f};

	float m_aLayerOffset[2];
	bool m_UseClipping;
	float m_aaClipArea[2][2];
	float m_aSpeed[2];
	float m_aaScreenOffset[2][2];
	float m_aaBaseArea[2][2]; // adapted to offset
	float m_aaExtendedArea[2][2]; // extended with parallax
};

bool ReplaceArea(IStorage *, const char[3][64], const float[][2][2]);
bool OpenMaps(IStorage *, const char[3][64], CDataFileReader[2], CDataFileWriter &);
void SaveOutputMap(CDataFileReader &, CDataFileWriter &);
bool CompareLayers(const char[3][64], CDataFileReader[2]);
void CompareGroups(const char[3][64], CDataFileReader[2]);
const CMapItemGroup *GetLayerGroup(CDataFileReader &, int);

void ReplaceAreaTiles(CDataFileReader[2], const float[][2][2], const CMapItemGroup *[2], CMapItemLayer *[2]);
void RemoveDestinationTiles(CMapItemLayerTilemap *, CTile *, float[2][2]);
void ReplaceDestinationTiles(CMapItemLayerTilemap *[2], CTile *[2], float[2][2][2]);
bool AdaptVisibleAreas(const float[2][2][2], const CMapObject[2], float[2][2][2]);
bool AdaptReplaceableAreas(const float[2][2][2], const float[2][2][2], const CMapObject[2], float[2][2][2]);

void ReplaceAreaQuads(CDataFileReader[2], const float[][2][2], const CMapItemGroup *[2], CMapItemLayer *[2], int);
bool RemoveDestinationQuads(const float[2][2], const CQuad *, int, const CMapItemGroup *, CQuad *, int &);
bool InsertDestinationQuads(const float[2][2][2], const CQuad *, int, const CMapItemGroup *[2], CQuad *, int &);
bool AdaptVisiblePoint(const float[2][2][2], const float[2][2], const CMapObject[2], float[2]);

CMapObject CreateMapObject(const CMapItemGroup *, int, int, int, int);
void SetExtendedArea(CMapObject &);
bool GetVisibleArea(const float[2][2], const CMapObject &, float[2][2] = nullptr);
bool GetReplaceableArea(const float[2][2], const CMapObject &, float[2][2]);

void GetGameAreaDistance(const float[2][2][2], const CMapObject[2], const float[2][2][2], float[2]);
void GetGameAreaDistance(const float[2][2][2], const CMapObject[2], const float[2][2], float[2]);
void GetSignificantScreenPos(const CMapObject &, const float[2][2], const float[2][2], float[2]);
void ConvertToTiles(const float[2][2], int[2][2]);

bool GetLineIntersection(const float[2], const float[2], float[2] = nullptr);
bool GetLineIntersection(const float[2], float);
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

	char aaMapNames[3][64];
	str_copy(aaMapNames[0], argv[1]); //from_map
	str_copy(aaMapNames[1], argv[4]); //to_map
	str_copy(aaMapNames[2], argv[9]); //output_map

	float aaaGameAreas[2][2][2];

	for(int i = 0; i < 2; i++)
	{
		aaaGameAreas[i][0][0] = str_tofloat(argv[2 + i * 3]) * 32; //x
		aaaGameAreas[i][1][0] = str_tofloat(argv[3 + i * 3]) * 32; //y
		aaaGameAreas[i][0][1] = aaaGameAreas[i][0][0] + str_tofloat(argv[7]) * 32; //x + width
		aaaGameAreas[i][1][1] = aaaGameAreas[i][1][0] + str_tofloat(argv[8]) * 32; //y + height
	}

	cmdline_free(argc, argv);

	dbg_msg("map_replace_area", "from_map='%s'; to_map='%s'; from_area='%fx,%fy'; to_area='%fx,%fy'; area_width='%fpx'; area_heigth='%fpx'; output_map='%s'",
		aaMapNames[0], aaMapNames[1], aaaGameAreas[0][0][0], aaaGameAreas[0][1][0], aaaGameAreas[1][0][0], aaaGameAreas[1][1][0],
		aaaGameAreas[0][0][1] - aaaGameAreas[0][0][0], aaaGameAreas[0][1][1] - aaaGameAreas[0][1][0], aaMapNames[2]);

	IStorage *pStorage = CreateLocalStorage();
	for(int i = 0; i < 1024; i++)
	{
		g_apNewData[i] = g_apNewItem[i] = nullptr;
		g_aNewDataSize[i] = 0;
	}

	return ReplaceArea(pStorage, aaMapNames, aaaGameAreas) ? 0 : 1;
}

bool ReplaceArea(IStorage *pStorage, const char aaMapNames[3][64], const float aaaGameAreas[][2][2])
{
	CDataFileReader aInputMaps[2];
	CDataFileWriter OutputMap;

	if(!OpenMaps(pStorage, aaMapNames, aInputMaps, OutputMap))
		return false;
	if(!CompareLayers(aaMapNames, aInputMaps))
		return false;
	CompareGroups(aaMapNames, aInputMaps);

	int aLayersStart[2], LayersCount;
	for(int i = 0; i < 2; i++)
		aInputMaps[i].GetType(MAPITEMTYPE_LAYER, &aLayersStart[i], &LayersCount);

	for(int i = 0; i < LayersCount; i++)
	{
		const CMapItemGroup *apLayerGroups[2];
		CMapItemLayer *apItem[2];
		for(int j = 0; j < 2; j++)
		{
			apLayerGroups[j] = GetLayerGroup(aInputMaps[j], i + 1);
			apItem[j] = (CMapItemLayer *)aInputMaps[j].GetItem(aLayersStart[j] + i);
		}

		if(!apLayerGroups[0] || !apLayerGroups[1])
			continue;

		if(apItem[0]->m_Type == LAYERTYPE_TILES)
			ReplaceAreaTiles(aInputMaps, aaaGameAreas, apLayerGroups, apItem);
		else if(apItem[0]->m_Type == LAYERTYPE_QUADS)
			ReplaceAreaQuads(aInputMaps, aaaGameAreas, apLayerGroups, apItem, aLayersStart[1] + i);
	}

	SaveOutputMap(aInputMaps[1], OutputMap);

	return true;
}

bool OpenMaps(IStorage *pStorage, const char aaMapNames[3][64], CDataFileReader aInputMaps[2], CDataFileWriter &OutputMap)
{
	for(int i = 0; i < 2; i++)
	{
		if(!aInputMaps[i].Open(pStorage, aaMapNames[i], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_replace_area", "ERROR: unable to open map '%s'", aaMapNames[i]);
			return false;
		}
	}

	if(!OutputMap.Open(pStorage, aaMapNames[2], IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_replace_area", "ERROR: unable to open map '%s'", aaMapNames[2]);
		return false;
	}

	return true;
}

void SaveOutputMap(CDataFileReader &InputMap, CDataFileWriter &OutputMap)
{
	for(int i = 0; i < InputMap.NumItems(); i++)
	{
		int Id, Type;
		CUuid Uuid;
		void *pItem = InputMap.GetItem(i, &Type, &Id, &Uuid);

		// Filter ITEMTYPE_EX items, they will be automatically added again.
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		if(g_apNewItem[i])
			pItem = g_apNewItem[i];

		int Size = InputMap.GetItemSize(i);
		OutputMap.AddItem(Type, Id, Size, pItem, &Uuid);
	}

	for(int i = 0; i < InputMap.NumData(); i++)
	{
		void *pData = g_apNewData[i] ? g_apNewData[i] : InputMap.GetData(i);
		int Size = g_aNewDataSize[i] ? g_aNewDataSize[i] : InputMap.GetDataSize(i);
		OutputMap.AddData(Size, pData);
	}

	OutputMap.Finish();
}

bool CompareLayers(const char aaMapNames[3][64], CDataFileReader aInputMaps[2])
{
	int aStart[2], aNum[2];
	for(int i = 0; i < 2; i++)
		aInputMaps[i].GetType(MAPITEMTYPE_LAYER, &aStart[i], &aNum[i]);

	if(aNum[0] != aNum[1])
	{
		dbg_msg("map_replace_area", "ERROR: different layers quantity");
		for(int i = 0; i < 2; i++)
			dbg_msg("map_replace_area", " \"%s\": %d layers", aaMapNames[i], aNum[i]);
		return false;
	}

	for(int i = 0; i < aNum[0]; i++)
	{
		CMapItemLayer *apItem[2];
		for(int j = 0; j < 2; j++)
			apItem[j] = (CMapItemLayer *)aInputMaps[j].GetItem(aStart[j] + i);

		if(apItem[0]->m_Type != apItem[1]->m_Type)
		{
			dbg_msg("map_replace_area", "ERROR: different types on layer #%d", i);
			for(int j = 0; j < 2; j++)
				dbg_msg("map_replace_area", " \"%s\": %s", aaMapNames[j], apItem[j]->m_Type == LAYERTYPE_TILES ? "tiles layer" : "quad layer");
			return false;
		}
	}

	return true;
}

void CompareGroups(const char aaMapNames[3][64], CDataFileReader aInputMaps[2])
{
	int aStart[2], aNum[2];
	for(int i = 0; i < 2; i++)
		aInputMaps[i].GetType(MAPITEMTYPE_GROUP, &aStart[i], &aNum[i]);

	for(int i = 0; i < std::max(aNum[0], aNum[1]); i++)
	{
		CMapItemGroup *apItem[2];
		for(int j = 0; j < 2; j++)
			apItem[j] = (CMapItemGroup *)aInputMaps[j].GetItem(aStart[j] + i);

		bool SameConfig = apItem[0]->m_ParallaxX == apItem[1]->m_ParallaxX && apItem[0]->m_ParallaxY == apItem[1]->m_ParallaxY && apItem[0]->m_OffsetX == apItem[1]->m_OffsetX && apItem[0]->m_OffsetY == apItem[1]->m_OffsetY && apItem[0]->m_UseClipping == apItem[1]->m_UseClipping && apItem[0]->m_ClipX == apItem[1]->m_ClipX && apItem[0]->m_ClipY == apItem[1]->m_ClipY && apItem[0]->m_ClipW == apItem[1]->m_ClipW && apItem[0]->m_ClipH == apItem[1]->m_ClipH;

		if(!SameConfig)
			dbg_msg("map_replace_area", "WARNING: different configuration on layergroup #%d, this might lead to unexpected results", i);
	}
}

const CMapItemGroup *GetLayerGroup(CDataFileReader &InputMap, const int LayerNumber)
{
	int Start, Num;
	InputMap.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemGroup *pItem = (CMapItemGroup *)InputMap.GetItem(Start + i);
		if(LayerNumber >= pItem->m_StartLayer && LayerNumber <= pItem->m_StartLayer + pItem->m_NumLayers)
			return pItem;
	}

	return nullptr;
}

void ReplaceAreaTiles(CDataFileReader aInputMaps[2], const float aaaGameAreas[][2][2], const CMapItemGroup *apLayerGroups[2], CMapItemLayer *apItem[2])
{
	CMapItemLayerTilemap *apTilemap[2];
	CTile *apTile[2];
	float aaaVisibleAreas[2][2][2], aaaReplaceableAreas[2][2][2];
	CMapObject aObs[2];

	for(int i = 0; i < 2; i++)
	{
		apTilemap[i] = (CMapItemLayerTilemap *)apItem[i];
		apTile[i] = (CTile *)aInputMaps[i].GetData(apTilemap[i]->m_Data);
		aObs[i] = CreateMapObject(apLayerGroups[i], 0, 0, apTilemap[i]->m_Width * 32, apTilemap[i]->m_Height * 32);
	}

	if(!GetVisibleArea(aaaGameAreas[1], aObs[1], aaaVisibleAreas[1]))
		return;

	GetReplaceableArea(aaaVisibleAreas[1], aObs[1], aaaReplaceableAreas[1]);
	RemoveDestinationTiles(apTilemap[1], apTile[1], aaaReplaceableAreas[1]);

	if(GetVisibleArea(aaaGameAreas[0], aObs[0], aaaVisibleAreas[0]) && AdaptVisibleAreas(aaaGameAreas, aObs, aaaVisibleAreas))
	{
		for(int i = 0; i < 2; i++)
			GetReplaceableArea(aaaVisibleAreas[i], aObs[i], aaaReplaceableAreas[i]);

		if(AdaptReplaceableAreas(aaaGameAreas, aaaVisibleAreas, aObs, aaaReplaceableAreas))
			ReplaceDestinationTiles(apTilemap, apTile, aaaReplaceableAreas);
	}

	g_apNewData[apTilemap[1]->m_Data] = apTile[1];
}

void RemoveDestinationTiles(CMapItemLayerTilemap *pTilemap, CTile *pTile, float aaReplaceableArea[2][2])
{
	int aaRange[2][2];
	ConvertToTiles(aaReplaceableArea, aaRange);

	CTile EmptyTile;
	EmptyTile.m_Index = EmptyTile.m_Flags = EmptyTile.m_Skip = EmptyTile.m_Reserved = 0;

	for(int y = aaRange[1][0]; y < aaRange[1][1]; y++)
		for(int x = aaRange[0][0]; x < aaRange[0][1]; x++)
			pTile[x + (y * pTilemap->m_Width)] = EmptyTile;
}

void ReplaceDestinationTiles(CMapItemLayerTilemap *apTilemap[2], CTile *apTile[2], float aaaReplaceableAreas[2][2][2])
{
	int aaaRanges[2][2][2];
	for(int i = 0; i < 2; i++)
		ConvertToTiles(aaaReplaceableAreas[i], aaaRanges[i]);

	for(int y0 = aaaRanges[0][1][0], y1 = aaaRanges[1][1][0]; y0 < aaaRanges[0][1][1] && y1 < aaaRanges[1][1][1]; y0++, y1++)
		for(int x0 = aaaRanges[0][0][0], x1 = aaaRanges[1][0][0]; x0 < aaaRanges[0][0][1] && x1 < aaaRanges[1][0][1]; x0++, x1++)
			apTile[1][x1 + (y1 * apTilemap[1]->m_Width)] = apTile[0][x0 + (y0 * apTilemap[0]->m_Width)];
}

bool AdaptVisibleAreas(const float aaaGameAreas[2][2][2], const CMapObject aObs[2], float aaaVisibleAreas[2][2][2])
{
	float aDistance[2];
	GetGameAreaDistance(aaaGameAreas, aObs, aaaVisibleAreas, aDistance);

	for(int i = 0; i < 2; i++)
	{
		if(aObs[0].m_aSpeed[i] == 1 || aObs[1].m_aSpeed[i] == 1)
			continue;

		for(int j = 0; j < 2; j++)
			aaaVisibleAreas[1][i][j] -= aDistance[i];

		if(!GetLineIntersection(aaaVisibleAreas[0][i], aaaVisibleAreas[1][i], aaaVisibleAreas[0][i]))
			return false;

		for(int j = 0; j < 2; j++)
			aaaVisibleAreas[1][i][j] = aaaVisibleAreas[0][i][j] + aDistance[i];
	}

	return true;
}

bool AdaptReplaceableAreas(const float aaaGameAreas[2][2][2], const float aaaVisibleAreas[2][2][2], const CMapObject aObs[2], float aaaReplaceableAreas[2][2][2])
{
	float aDistance[2], aScreenPos[2];
	GetGameAreaDistance(aaaGameAreas, aObs, aaaVisibleAreas, aDistance);
	GetSignificantScreenPos(aObs[0], aaaVisibleAreas[0], aaaReplaceableAreas[0], aScreenPos);

	for(int i = 0; i < 2; i++)
	{
		float aDestLine[2], aSourceLine[2], aVisibleLine[2];

		aDestLine[0] = aObs[1].m_aaBaseArea[i][0] + (aScreenPos[i] + aDistance[i]) * aObs[1].m_aSpeed[i];
		aDestLine[1] = aDestLine[0] + (aObs[1].m_aaBaseArea[i][1] - aObs[1].m_aaBaseArea[i][0]);

		if(!GetLineIntersection(aDestLine, aaaVisibleAreas[1][i], aVisibleLine))
			return false;

		aSourceLine[0] = aaaVisibleAreas[0][i][0] + aDistance[i] - aaaReplaceableAreas[0][i][0];
		aSourceLine[1] = aaaVisibleAreas[0][i][1] + aDistance[i] + aaaReplaceableAreas[0][i][1] - aaaReplaceableAreas[0][i][0];

		if(!GetLineIntersection(aSourceLine, aVisibleLine, aVisibleLine))
			return false;

		aaaReplaceableAreas[0][i][0] = aVisibleLine[0] - aSourceLine[0];
		aaaReplaceableAreas[1][i][0] = aVisibleLine[0] - aDestLine[0];
	}

	return true;
}

void ReplaceAreaQuads(CDataFileReader aInputMaps[2], const float aaaGameAreas[][2][2], const CMapItemGroup *apLayerGroups[2], CMapItemLayer *apItem[2], const int ItemNumber)
{
	CMapItemLayerQuads *apQuadLayer[2];
	for(int i = 0; i < 2; i++)
		apQuadLayer[i] = (CMapItemLayerQuads *)apItem[i];

	CQuad *apQuads[3];
	for(int i = 0; i < 2; i++)
		apQuads[i] = (CQuad *)aInputMaps[i].GetDataSwapped(apQuadLayer[i]->m_Data);

	apQuads[2] = new CQuad[apQuadLayer[0]->m_NumQuads + apQuadLayer[1]->m_NumQuads];
	int QuadsCounter = 0;

	bool DataChanged = RemoveDestinationQuads(aaaGameAreas[1], apQuads[1], apQuadLayer[1]->m_NumQuads, apLayerGroups[1], apQuads[2], QuadsCounter);
	DataChanged |= InsertDestinationQuads(aaaGameAreas, apQuads[0], apQuadLayer[0]->m_NumQuads, apLayerGroups, apQuads[2], QuadsCounter);

	if(DataChanged)
	{
		g_apNewData[apQuadLayer[1]->m_Data] = apQuads[2];
		g_aNewDataSize[apQuadLayer[1]->m_Data] = ((int)sizeof(CQuad)) * QuadsCounter;
		apQuadLayer[1]->m_NumQuads = QuadsCounter;
		g_apNewItem[ItemNumber] = apItem[1];
	}
	else
		delete[] apQuads[2];
}

bool RemoveDestinationQuads(const float aaGameArea[2][2], const CQuad *pQuads, const int NumQuads, const CMapItemGroup *pLayerGroup, CQuad *pDestQuads, int &QuadsCounter)
{
	bool DataChanged = false;

	for(int i = 0; i < NumQuads; i++)
	{
		CMapObject Ob = CreateMapObject(pLayerGroup, fx2f(pQuads[i].m_aPoints[4].x), fx2f(pQuads[i].m_aPoints[4].y), 0, 0);

		if(GetVisibleArea(aaGameArea, Ob))
		{
			DataChanged = true;
			continue;
		}

		pDestQuads[QuadsCounter] = pQuads[i];
		QuadsCounter++;
	}

	return DataChanged;
}

bool InsertDestinationQuads(const float aaaGameAreas[2][2][2], const CQuad *pQuads, const int NumQuads, const CMapItemGroup *apLayerGroups[2], CQuad *pDestQuads, int &QuadsCounter)
{
	bool DataChanged = false;

	for(int i = 0; i < NumQuads; i++)
	{
		CMapObject aObs[2];
		aObs[0] = CreateMapObject(apLayerGroups[0], fx2f(pQuads[i].m_aPoints[4].x), fx2f(pQuads[i].m_aPoints[4].y), 0, 0);
		float aaVisibleArea[2][2];

		if(GetVisibleArea(aaaGameAreas[0], aObs[0], aaVisibleArea))
		{
			float aQuadPos[2];
			aObs[1] = CreateMapObject(apLayerGroups[1], 0, 0, 0, 0);

			if(!AdaptVisiblePoint(aaaGameAreas, aaVisibleArea, aObs, aQuadPos))
				continue;

			pDestQuads[QuadsCounter] = pQuads[i];
			for(auto &Point : pDestQuads[QuadsCounter].m_aPoints)
			{
				Point.x += f2fx(aQuadPos[0]) - pDestQuads[QuadsCounter].m_aPoints[4].x;
				Point.y += f2fx(aQuadPos[1]) - pDestQuads[QuadsCounter].m_aPoints[4].y;
			}

			QuadsCounter++;
			DataChanged = true;
		}
	}

	return DataChanged;
}

bool AdaptVisiblePoint(const float aaaGameAreas[2][2][2], const float aaVisibleArea[2][2], const CMapObject aObs[2], float aPos[2])
{
	float aDistance[2], aScreenPos[2];
	GetGameAreaDistance(aaaGameAreas, aObs, aaVisibleArea, aDistance);
	GetSignificantScreenPos(aObs[0], aaVisibleArea, nullptr, aScreenPos);

	for(int i = 0; i < 2; i++)
		aPos[i] = aaVisibleArea[i][0] + aDistance[i] + aObs[1].m_aLayerOffset[i] - (aScreenPos[i] + aDistance[i]) * aObs[1].m_aSpeed[i];

	CMapObject FinalOb = aObs[1];
	for(int i = 0; i < 2; i++)
		FinalOb.m_aaBaseArea[i][0] = FinalOb.m_aaBaseArea[i][1] += aPos[i];
	SetExtendedArea(FinalOb);

	return GetVisibleArea(aaaGameAreas[1], FinalOb);
}

CMapObject CreateMapObject(const CMapItemGroup *pLayerGroup, const int PosX, const int PosY, const int Width, const int Height)
{
	CMapObject Ob;

	Ob.m_aaBaseArea[0][0] = PosX - pLayerGroup->m_OffsetX;
	Ob.m_aaBaseArea[1][0] = PosY - pLayerGroup->m_OffsetY;
	Ob.m_aaBaseArea[0][1] = Ob.m_aaBaseArea[0][0] + Width;
	Ob.m_aaBaseArea[1][1] = Ob.m_aaBaseArea[1][0] + Height;
	Ob.m_aLayerOffset[0] = pLayerGroup->m_OffsetX;
	Ob.m_aLayerOffset[1] = pLayerGroup->m_OffsetY;
	Ob.m_UseClipping = pLayerGroup->m_UseClipping;
	Ob.m_aaClipArea[0][0] = pLayerGroup->m_ClipX;
	Ob.m_aaClipArea[1][0] = pLayerGroup->m_ClipY;
	Ob.m_aaClipArea[0][1] = pLayerGroup->m_ClipX + pLayerGroup->m_ClipW;
	Ob.m_aaClipArea[1][1] = pLayerGroup->m_ClipY + pLayerGroup->m_ClipH;
	Ob.m_aSpeed[0] = 1 - (pLayerGroup->m_ParallaxX / 100.0f);
	Ob.m_aSpeed[1] = 1 - (pLayerGroup->m_ParallaxY / 100.0f);

	for(int i = 0; i < 2; i++)
	{
		Ob.m_aaScreenOffset[i][0] = -CMapObject::ms_aStandardScreen[i];
		Ob.m_aaScreenOffset[i][1] = CMapObject::ms_aStandardScreen[i];
		if(Ob.m_aSpeed[i] < 0)
			std::swap(Ob.m_aaScreenOffset[i][0], Ob.m_aaScreenOffset[i][1]);
	}

	SetExtendedArea(Ob);
	return Ob;
}

void SetExtendedArea(CMapObject &Ob)
{
	SetInexistent((float *)Ob.m_aaExtendedArea, 4);

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_aSpeed[i] == 1)
		{
			float aInspectedArea[2];
			if(GetLineIntersection(Ob.m_aaBaseArea[i], Ob.m_aaScreenOffset[i], aInspectedArea))
				mem_copy(Ob.m_aaExtendedArea[i], aInspectedArea, sizeof(float[2]));
			continue;
		}

		for(int j = 0; j < 2; j++)
			Ob.m_aaExtendedArea[i][j] = (Ob.m_aaBaseArea[i][j] + Ob.m_aaScreenOffset[i][j] * Ob.m_aSpeed[i]) / (1 - Ob.m_aSpeed[i]);

		if(Ob.m_aaExtendedArea[i][0] > Ob.m_aaExtendedArea[i][1])
			std::swap(Ob.m_aaExtendedArea[i][0], Ob.m_aaExtendedArea[i][1]);
	}
}

bool GetVisibleArea(const float aaGameArea[2][2], const CMapObject &Ob, float aaVisibleArea[2][2])
{
	if(IsInexistent((float *)Ob.m_aaExtendedArea, 4))
		return false;

	if(aaVisibleArea)
		SetInexistent((float *)aaVisibleArea, 4);

	float aaInspectedArea[2][2];
	mem_copy(aaInspectedArea, aaGameArea, sizeof(float[2][2]));

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_aSpeed[i] == 1)
		{
			mem_copy(aaInspectedArea[i], Ob.m_aaExtendedArea[i], sizeof(float[2]));
			continue;
		}

		if(Ob.m_UseClipping && !GetLineIntersection(aaInspectedArea[i], Ob.m_aaClipArea[i], aaInspectedArea[i]))
			return false;

		if(!GetLineIntersection(aaInspectedArea[i], Ob.m_aaExtendedArea[i], aaInspectedArea[i]))
			return false;
	}

	if(aaVisibleArea)
		mem_copy(aaVisibleArea, aaInspectedArea, sizeof(float[2][2]));

	return true;
}

bool GetReplaceableArea(const float aaVisibleArea[2][2], const CMapObject &Ob, float aaReplaceableArea[2][2])
{
	SetInexistent((float *)aaReplaceableArea, 4);
	if(IsInexistent((float *)aaVisibleArea, 4))
		return false;

	for(int i = 0; i < 2; i++)
	{
		if(Ob.m_aSpeed[i] == 1)
		{
			aaReplaceableArea[i][0] = aaVisibleArea[i][0] - Ob.m_aaBaseArea[i][0];
			aaReplaceableArea[i][1] = aaVisibleArea[i][1] - Ob.m_aaBaseArea[i][0];
			continue;
		}

		for(int j = 0; j < 2; j++)
		{
			float aVisibleLine[2], aReplaceableLine[2];
			int k = Ob.m_aSpeed[i] > 1 ? !j : j;

			aVisibleLine[0] = Ob.m_aaBaseArea[i][0] + (aaVisibleArea[i][j] - Ob.m_aaScreenOffset[i][k]) * Ob.m_aSpeed[i];
			aVisibleLine[1] = aVisibleLine[0] + Ob.m_aaBaseArea[i][1] - Ob.m_aaBaseArea[i][0];

			if(GetLineIntersection(aaVisibleArea[i], aVisibleLine, aReplaceableLine))
				aaReplaceableArea[i][k] = aReplaceableLine[j] - aVisibleLine[0];
			else
				aaReplaceableArea[i][k] = k * (Ob.m_aaBaseArea[i][1] - Ob.m_aaBaseArea[i][0]);
		}
	}

	return true;
}

void GetGameAreaDistance(const float aaaGameAreas[2][2][2], const CMapObject aObs[2], const float aaaVisibleAreas[2][2][2], float aDistance[2])
{
	for(int i = 0; i < 2; i++)
	{
		if(aObs[0].m_aSpeed[i] == 1 && aObs[1].m_aSpeed[i] == 1)
			aDistance[i] = 0;
		else if(aObs[0].m_aSpeed[i] == 1 && aObs[1].m_aSpeed[i] != 1)
			aDistance[i] = aaaGameAreas[1][i][0] - aaaVisibleAreas[0][i][0];
		else if(aObs[0].m_aSpeed[i] != 1 && aObs[1].m_aSpeed[i] == 1)
			aDistance[i] = aaaVisibleAreas[1][i][0] - aaaGameAreas[0][i][0];
		else
			aDistance[i] = aaaGameAreas[1][i][0] - aaaGameAreas[0][i][0];
	}
}

void GetGameAreaDistance(const float aaaGameAreas[2][2][2], const CMapObject aObs[2], const float aaVisibleArea[2][2], float aDistance[2])
{
	float aaaVisibleAreas[2][2][2];
	mem_copy(aaaVisibleAreas[0], aaVisibleArea[0], sizeof(float[2][2]));
	mem_copy(aaaVisibleAreas[1], aaVisibleArea[0], sizeof(float[2][2]));
	GetGameAreaDistance(aaaGameAreas, aObs, aaaVisibleAreas, aDistance);
}

void GetSignificantScreenPos(const CMapObject &Ob, const float aaVisibleArea[2][2], const float aaReplaceableArea[2][2], float aScreen[2])
{
	for(int i = 0; i < 2; i++)
	{
		if(!Ob.m_aSpeed[i])
		{
			aScreen[i] = aaVisibleArea[i][0] + Ob.m_aaScreenOffset[i][1];
			continue;
		}

		float BaseOffset = aaReplaceableArea ? aaReplaceableArea[i][0] : 0;
		aScreen[i] = (aaVisibleArea[i][0] - Ob.m_aaBaseArea[i][0] - BaseOffset) / Ob.m_aSpeed[i];
	}
}

void ConvertToTiles(const float aaArea[2][2], int aaTiles[2][2])
{
	for(int i = 0; i < 2; i++)
	{
		aaTiles[i][0] = std::floor((std::floor(aaArea[i][0] * 100.0f) / 100.0f) / 32.0f);
		aaTiles[i][1] = std::ceil((std::floor(aaArea[i][1] * 100.0f) / 100.0f) / 32.0f);
	}
}

bool GetLineIntersection(const float aLine1[2], const float aLine2[2], float aIntersection[2])
{
	float aBorders[2] = {
		std::max(aLine1[0], aLine2[0]),
		std::min(aLine1[1], aLine2[1])};

	if(aIntersection)
		SetInexistent(aIntersection, 2);

	if(aBorders[0] - aBorders[1] > 0.01f)
		return false;

	if(aIntersection)
		mem_copy(aIntersection, aBorders, sizeof(float[2]));

	return true;
}

bool GetLineIntersection(const float aLine[2], const float Point)
{
	return aLine[0] - Point <= 0.01f && aLine[1] - Point >= 0.01f;
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
