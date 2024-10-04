#include <base/hash.h>
#include <base/logger.h>
#include <base/system.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/mapitems.h>

#include <zlib.h>

void CreateEmptyMap(IStorage *pStorage)
{
	const char *pMapName = "maps/dummy3.map";

	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, pMapName))
	{
		dbg_msg("dummy_map", "couldn't open map file '%s' for writing", pMapName);
		return;
	}
	CMapItemGroup_v1 Group;
	Group.m_Version = 1;
	Group.m_OffsetX = 0;
	Group.m_OffsetY = 0;
	Group.m_ParallaxX = 0;
	Group.m_ParallaxY = 0;
	Group.m_StartLayer = 0;
	Group.m_NumLayers = 2;
	Writer.AddItem(MAPITEMTYPE_GROUP, 0, sizeof(Group), &Group);

	constexpr int LayerWidth = 2;
	constexpr int LayerHeight = 2;
	CTile aTiles[LayerWidth * LayerHeight];
	for(auto &Tile : aTiles)
	{
		Tile.m_Index = 1;
		Tile.m_Flags = 0;
		Tile.m_Skip = 0;
		Tile.m_Reserved = 0;
	}
	const int TilesData = Writer.AddData(sizeof(aTiles), &aTiles);

	CMapItemLayerTilemap GameLayer;
	GameLayer.m_Layer.m_Version = 0; // Not set by the official client.
	GameLayer.m_Layer.m_Type = LAYERTYPE_TILES;
	GameLayer.m_Layer.m_Flags = 0;
	GameLayer.m_Version = 2;
	GameLayer.m_Width = LayerWidth;
	GameLayer.m_Height = LayerHeight;
	GameLayer.m_Flags = TILESLAYERFLAG_GAME;
	GameLayer.m_Color.r = 0;
	GameLayer.m_Color.g = 0;
	GameLayer.m_Color.b = 0;
	GameLayer.m_Color.a = 0;
	GameLayer.m_ColorEnv = -1;
	GameLayer.m_ColorEnvOffset = 0;
	GameLayer.m_Image = -1;
	GameLayer.m_Data = TilesData;
	Writer.AddItem(MAPITEMTYPE_LAYER, 0, sizeof(GameLayer) - sizeof(GameLayer.m_aName) - sizeof(GameLayer.m_Tele) - sizeof(GameLayer.m_Speedup) - sizeof(GameLayer.m_Front) - sizeof(GameLayer.m_Switch) - sizeof(GameLayer.m_Tune), &GameLayer);

	CMapItemLayerTilemap Layer;
	Layer.m_Layer.m_Version = 0;
	Layer.m_Layer.m_Type = LAYERTYPE_TILES;
	Layer.m_Layer.m_Flags = 0;
	Layer.m_Version = 2;
	Layer.m_Width = LayerWidth;
	Layer.m_Height = LayerHeight;
	Layer.m_Flags = 0;
	Layer.m_Color.r = 0;
	Layer.m_Color.g = 0;
	Layer.m_Color.b = 0;
	Layer.m_Color.a = 255;
	Layer.m_ColorEnv = -1;
	Layer.m_ColorEnvOffset = 0;
	Layer.m_Image = -1;
	Layer.m_Data = TilesData;
	Writer.AddItem(MAPITEMTYPE_LAYER, 1, sizeof(Layer) - sizeof(Layer.m_aName) - sizeof(Layer.m_Tele) - sizeof(Layer.m_Speedup) - sizeof(Layer.m_Front) - sizeof(Layer.m_Switch) - sizeof(Layer.m_Tune), &Layer);

	Writer.Finish();

	dbg_msg("dummy_map", "dummy map written to '%s'", pMapName);

	void *pData;
	unsigned DataSize;
	if(!pStorage->ReadFile(pMapName, IStorage::TYPE_ALL, &pData, &DataSize))
	{
		dbg_msg("dummy_map", "couldn't open map file '%s' for reading", pMapName);
		return;
	}
	unsigned char *pDataChar = static_cast<unsigned char *>(pData);

	unsigned Crc = crc32(0, pDataChar, DataSize);
	SHA256_DIGEST Sha256 = sha256(pDataChar, DataSize);

	char aMapSha[SHA256_MAXSTRSIZE];
	sha256_str(Sha256, aMapSha, sizeof(aMapSha));
	dbg_msg("dummy_map", "crc32 %08X, sha256 %s", Crc, aMapSha);

	const unsigned HexSize = 6 * DataSize + 1;
	char *pHex = static_cast<char *>(malloc(HexSize));
	str_hex_cstyle(pHex, HexSize, pDataChar, DataSize);
	dbg_msg("dummy_map", "data %s", pHex);
	free(pHex);

	free(pDataChar);
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();
	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_SERVER, argc, argv);
	if(!pStorage)
		return -1;
	CreateEmptyMap(pStorage);
	return 0;
}
