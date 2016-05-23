#include <base/system.h>
#include <base/tl/array.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

void CreateEmptyMap(IStorage *pStorage)
{
	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, "dummy.map"))
	{
		dbg_msg("empty_map", "couldn't open map file 'dummy.map' for writing");
		return;
	}
	CMapItemGroup_v1 Group;
	Group.m_Version = 1;
	Group.m_OffsetX = 0;
	Group.m_OffsetY = 0;
	Group.m_ParallaxX = 0;
	Group.m_ParallaxY = 0;
	Group.m_StartLayer = 0;
	Group.m_NumLayers = 1;
	Writer.AddItem(MAPITEMTYPE_GROUP, 0, sizeof(Group), &Group);

	CMapItemLayerTilemap Layer;
	Layer.m_Layer.m_Version = 0; // Not set by the official client.
	Layer.m_Layer.m_Type = LAYERTYPE_TILES;
	Layer.m_Layer.m_Flags = 0;
	Layer.m_Version = 2;
	Layer.m_Width = 2;
	Layer.m_Height = 2;
	Layer.m_Flags = TILESLAYERFLAG_GAME;
	Layer.m_Color.r = 0;
	Layer.m_Color.g = 0;
	Layer.m_Color.b = 0;
	Layer.m_Color.a = 0;
	Layer.m_ColorEnv = -1;
	Layer.m_ColorEnvOffset = 0;
	Layer.m_Image = -1;
	Layer.m_Data = 0;
	Writer.AddItem(MAPITEMTYPE_LAYER, 0, sizeof(Layer) - sizeof(Layer.m_aName), &Layer);

	CTile Tiles[4] = { 0 };
	Writer.AddData(sizeof(Tiles), &Tiles);

	Writer.Finish();
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv);
	CreateEmptyMap(pStorage);
	return 0;
}
