#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

struct EnvelopedQuad
{
	int m_GroupID;
	int m_LayerID;
	int m_TilePosX;
	int m_TilePosY;
};

bool OpenMap(const char pMapName[64], CDataFileReader &InputMap)
{
	IStorage *pStorage = CreateLocalStorage();

	if(!InputMap.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_find_env", "ERROR: unable to open map '%s'", pMapName);
		return false;
	}
	return true;
}

bool GetLayerGroupIDs(CDataFileReader &InputMap, const int LayerNumber, int &GroupID, int &LayerRelativeID)
{
	int Start, Num;
	InputMap.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemGroup *pItem = (CMapItemGroup *)InputMap.GetItem(Start + i, 0, 0);
		if(LayerNumber >= pItem->m_StartLayer && LayerNumber <= pItem->m_StartLayer + pItem->m_NumLayers)
		{
			GroupID = i;
			LayerRelativeID = LayerNumber - pItem->m_StartLayer - 1;
			return true;
		}
	}

	return false;
}

int FxToTilePos(const int FxPos)
{
	return (int)floor(fx2f(FxPos) / 32);
}

bool GetEnvelopedQuads(const CQuad *pQuads, const int NumQuads, const int EnvID, const int GroupID, const int LayerID, int &QuadsCounter, EnvelopedQuad pEnvQuads[1024])
{
	bool bFound = false;
	for(int i = 0; i < NumQuads; i++)
	{
		if(pQuads[i].m_PosEnv != EnvID && pQuads[i].m_ColorEnv != EnvID)
			continue;

		pEnvQuads[QuadsCounter].m_GroupID = GroupID;
		pEnvQuads[QuadsCounter].m_LayerID = LayerID;
		pEnvQuads[QuadsCounter].m_TilePosX = FxToTilePos(pQuads[i].m_aPoints[4].x);
		pEnvQuads[QuadsCounter].m_TilePosY = FxToTilePos(pQuads[i].m_aPoints[4].y);

		QuadsCounter++;
		bFound = true;
	}

	return bFound;
}

void PrintEnvelopedQuads(const EnvelopedQuad pEnvQuads[1024], const int EnvID, const int QuadsCounter)
{
	if(!QuadsCounter)
	{
		dbg_msg("map_find_env", "No quads found with env number #%d", EnvID + 1);
		return;
	}

	dbg_msg("map_find_env", "Found %d quads with env number #%d:", QuadsCounter, EnvID + 1);
	for(int i = 0; i < QuadsCounter; i++)
		dbg_msg("map_find_env", "%*d. Group: #%d - Layer: #%d - Pos: %d,%d", (int)(log10(abs(QuadsCounter))) + 1, i + 1, pEnvQuads[i].m_GroupID, pEnvQuads[i].m_LayerID, pEnvQuads[i].m_TilePosX, pEnvQuads[i].m_TilePosY);
}

bool FindEnv(const char aFilename[64], const int EnvID)
{
	CDataFileReader InputMap;
	if(!OpenMap(aFilename, InputMap))
		return false;

	int LayersStart, LayersCount, QuadsCounter = 0;
	InputMap.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersCount);
	EnvelopedQuad pEnvQuads[1024];

	for(int i = 0; i < LayersCount; i++)
	{
		CMapItemLayer *pItem;
		pItem = (CMapItemLayer *)InputMap.GetItem(LayersStart + i, 0, 0);

		if(pItem->m_Type != LAYERTYPE_QUADS)
			continue;

		CMapItemLayerQuads *pQuadLayer = (CMapItemLayerQuads *)pItem;
		CQuad *pQuads = (CQuad *)InputMap.GetDataSwapped(pQuadLayer->m_Data);

		int GroupID = 0, LayerRelativeID = 0;
		if(!GetLayerGroupIDs(InputMap, i + 1, GroupID, LayerRelativeID))
			return false;

		GetEnvelopedQuads(pQuads, pQuadLayer->m_NumQuads, EnvID, GroupID, LayerRelativeID, QuadsCounter, pEnvQuads);
	}

	PrintEnvelopedQuads(pEnvQuads, EnvID, QuadsCounter);

	return true;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc < 3)
	{
		dbg_msg("map_find_env", "Invalid arguments");
		dbg_msg("map_find_env", "Usage: %s <input_map> <env_number>", argv[0]);
		dbg_msg("map_find_env", "Note: returned quads positions are relative to their layers");

		return -1;
	}

	char aFilename[64];
	snprintf(aFilename, 64, "%s", argv[1]);
	int EnvID = atoi(argv[2]) - 1;
	dbg_msg("map_find_env", "input_map='%s'; env_number='#%d';", aFilename, EnvID + 1);

	return FindEnv(aFilename, EnvID);
}
