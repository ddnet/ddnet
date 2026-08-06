#include <base/dbg.h>
#include <base/hash.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/mapitems.h>

#include <zlib.h>

static const char *TOOL_NAME = "dummy_map";

static void CreateEmptyMap(IStorage *pStorage)
{
	const char *pMapName = "maps/dummy3.map";

	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, pMapName))
	{
		log_error(TOOL_NAME, "Failed to open map '%s' for writing", pMapName);
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
	std::fill(std::begin(aTiles), std::end(aTiles), CTile{.m_Index = TILE_SOLID, .m_Flags = 0, .m_Skip = 0, .m_MustBe0 = 0});

	CMapItemLayerTilemap GameLayer;
	GameLayer.m_Layer.m_Version = 0; // Not set by the official client.
	GameLayer.m_Layer.m_Type = LAYERTYPE_TILES;
	GameLayer.m_Layer.m_Flags = 0;
	GameLayer.m_Version = 2;
	GameLayer.m_Width = LayerWidth;
	GameLayer.m_Height = LayerHeight;
	GameLayer.m_Flags = TILESLAYERFLAG_GAME;
	GameLayer.m_Color.r = 255;
	GameLayer.m_Color.g = 255;
	GameLayer.m_Color.b = 255;
	GameLayer.m_Color.a = 255;
	GameLayer.m_ColorEnv = -1;
	GameLayer.m_ColorEnvOffset = 0;
	GameLayer.m_Image = -1;
	GameLayer.m_Data = Writer.AddData(sizeof(aTiles), &aTiles);
	Writer.AddItem(MAPITEMTYPE_LAYER, 0, sizeof(GameLayer) - sizeof(GameLayer.m_aName) - sizeof(GameLayer.m_Tele) - sizeof(GameLayer.m_Speedup) - sizeof(GameLayer.m_Front) - sizeof(GameLayer.m_Switch) - sizeof(GameLayer.m_Tune), &GameLayer);

	CMapItemLayerTilemap Layer;
	Layer.m_Layer.m_Version = 0;
	Layer.m_Layer.m_Type = LAYERTYPE_TILES;
	Layer.m_Layer.m_Flags = 0;
	Layer.m_Version = 2;
	Layer.m_Width = LayerWidth;
	Layer.m_Height = LayerHeight;
	Layer.m_Flags = 0;
	Layer.m_Color.r = 255;
	Layer.m_Color.g = 255;
	Layer.m_Color.b = 255;
	Layer.m_Color.a = 255;
	Layer.m_ColorEnv = -1;
	Layer.m_ColorEnvOffset = 0;
	Layer.m_Image = -1;
	Layer.m_Data = Writer.AddData(sizeof(aTiles), &aTiles);
	Writer.AddItem(MAPITEMTYPE_LAYER, 1, sizeof(Layer) - sizeof(Layer.m_aName) - sizeof(Layer.m_Tele) - sizeof(Layer.m_Speedup) - sizeof(Layer.m_Front) - sizeof(Layer.m_Switch) - sizeof(Layer.m_Tune), &Layer);

	Writer.Finish();

	log_info(TOOL_NAME, "Dummy map written to '%s'", pMapName);

	void *pData;
	unsigned DataSize;
	if(!pStorage->ReadFile(pMapName, IStorage::TYPE_ALL, &pData, &DataSize))
	{
		log_error(TOOL_NAME, "Failed to open map file '%s' for reading", pMapName);
		return;
	}
	unsigned char *pDataChar = static_cast<unsigned char *>(pData);

	unsigned Crc = crc32(0, pDataChar, DataSize);
	SHA256_DIGEST Sha256 = sha256(pDataChar, DataSize);

	char aMapSha[SHA256_MAXSTRSIZE];
	sha256_str(Sha256, aMapSha, sizeof(aMapSha));
	log_info(TOOL_NAME, "CRC32 %08X, SHA256 %s", Crc, aMapSha);

	const unsigned HexSize = 6 * DataSize + 1;
	char *pHex = static_cast<char *>(malloc(HexSize));
	str_hex_cstyle(pHex, HexSize, pDataChar, DataSize);
	log_info(TOOL_NAME, "Data %s", pHex);
	free(pHex);

	free(pDataChar);
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	std::unique_ptr<IStorage> pStorage = std::unique_ptr<IStorage>(CreateStorage(IStorage::EInitializationType::SERVER, argc, argv));
	if(!pStorage)
	{
		log_error(TOOL_NAME, "Error creating server storage");
		return -1;
	}

	CreateEmptyMap(pStorage.get());
	return 0;
}
