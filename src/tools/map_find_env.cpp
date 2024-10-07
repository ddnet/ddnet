#include <base/logger.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

class CEnvelopedQuad
{
public:
	int m_GroupId;
	int m_LayerId;
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

bool GetLayerGroupIds(CDataFileReader &InputMap, const int LayerNumber, int &GroupId, int &LayerRelativeId)
{
	int Start, Num;
	InputMap.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemGroup *pItem = (CMapItemGroup *)InputMap.GetItem(Start + i);
		if(LayerNumber >= pItem->m_StartLayer && LayerNumber <= pItem->m_StartLayer + pItem->m_NumLayers)
		{
			GroupId = i;
			LayerRelativeId = LayerNumber - pItem->m_StartLayer - 1;
			return true;
		}
	}

	return false;
}

int FxToTilePos(const int FxPos)
{
	return std::floor(fx2f(FxPos) / 32);
}

bool GetEnvelopedQuads(const CQuad *pQuads, const int NumQuads, const int EnvId, const int GroupId, const int LayerId, int &QuadsCounter, CEnvelopedQuad pEnvQuads[1024])
{
	bool Found = false;
	for(int i = 0; i < NumQuads; i++)
	{
		if(pQuads[i].m_PosEnv != EnvId && pQuads[i].m_ColorEnv != EnvId)
			continue;

		pEnvQuads[QuadsCounter].m_GroupId = GroupId;
		pEnvQuads[QuadsCounter].m_LayerId = LayerId;
		pEnvQuads[QuadsCounter].m_TilePosX = FxToTilePos(pQuads[i].m_aPoints[4].x);
		pEnvQuads[QuadsCounter].m_TilePosY = FxToTilePos(pQuads[i].m_aPoints[4].y);

		QuadsCounter++;
		Found = true;
	}

	return Found;
}

void PrintEnvelopedQuads(const CEnvelopedQuad pEnvQuads[1024], const int EnvId, const int QuadsCounter)
{
	if(!QuadsCounter)
	{
		dbg_msg("map_find_env", "No quads found with env number #%d", EnvId + 1);
		return;
	}

	dbg_msg("map_find_env", "Found %d quads with env number #%d:", QuadsCounter, EnvId + 1);
	for(int i = 0; i < QuadsCounter; i++)
		dbg_msg("map_find_env", "%*d. Group: #%d - Layer: #%d - Pos: %d,%d", (int)(std::log10(absolute(QuadsCounter))) + 1, i + 1, pEnvQuads[i].m_GroupId, pEnvQuads[i].m_LayerId, pEnvQuads[i].m_TilePosX, pEnvQuads[i].m_TilePosY);
}

bool FindEnv(const char aFilename[64], const int EnvId)
{
	CDataFileReader InputMap;
	if(!OpenMap(aFilename, InputMap))
		return false;

	int LayersStart, LayersCount, QuadsCounter = 0;
	InputMap.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersCount);
	CEnvelopedQuad pEnvQuads[1024];

	for(int i = 0; i < LayersCount; i++)
	{
		CMapItemLayer *pItem;
		pItem = (CMapItemLayer *)InputMap.GetItem(LayersStart + i);

		if(pItem->m_Type != LAYERTYPE_QUADS)
			continue;

		CMapItemLayerQuads *pQuadLayer = (CMapItemLayerQuads *)pItem;
		CQuad *pQuads = (CQuad *)InputMap.GetDataSwapped(pQuadLayer->m_Data);

		int GroupId = 0, LayerRelativeId = 0;
		if(!GetLayerGroupIds(InputMap, i + 1, GroupId, LayerRelativeId))
			return false;

		GetEnvelopedQuads(pQuads, pQuadLayer->m_NumQuads, EnvId, GroupId, LayerRelativeId, QuadsCounter, pEnvQuads);
	}

	PrintEnvelopedQuads(pEnvQuads, EnvId, QuadsCounter);

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
	str_copy(aFilename, argv[1]);
	int EnvId = str_toint(argv[2]) - 1;
	dbg_msg("map_find_env", "input_map='%s'; env_number='#%d';", aFilename, EnvId + 1);

	return FindEnv(aFilename, EnvId);
}
