// Adapted from TWMapImagesRecovery by Tardo: https://github.com/Tardo/TWMapImagesRecovery
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

#include <engine/shared/image_loader.h>

bool Process(IStorage *pStorage, const char *pMapName, const char *pPathSave)
{
	CDataFileReader Map;

	if(!Map.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_extract", "error opening map '%s'", pMapName);
		return false;
	}

	// check version
	CMapItemVersion *pVersion = (CMapItemVersion *)Map.FindItem(MAPITEMTYPE_VERSION, 0);
	if(pVersion && pVersion->m_Version != 1)
		return false;

	dbg_msg("map_extract", "Make sure you have the permission to use these images and sounds in your own maps");

	CMapItemInfo *pInfo = (CMapItemInfo *)Map.FindItem(MAPITEMTYPE_INFO, 0);

	if(pInfo)
	{
		dbg_msg("map_extract", "author:  %s", (char *)Map.GetData(pInfo->m_Author));
		dbg_msg("map_extract", "version: %s", (char *)Map.GetData(pInfo->m_MapVersion));
		dbg_msg("map_extract", "credits: %s", (char *)Map.GetData(pInfo->m_Credits));
		dbg_msg("map_extract", "license: %s", (char *)Map.GetData(pInfo->m_License));
	}

	int Start, Num;

	// load images
	Map.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemImage *pItem = (CMapItemImage *)Map.GetItem(Start + i, 0, 0);
		char *pName = (char *)Map.GetData(pItem->m_ImageName);

		if(pItem->m_External)
			continue;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s/%s.png", pPathSave, pName);
		dbg_msg("map_extract", "writing image: %s (%dx%d)", aBuf, pItem->m_Width, pItem->m_Height);

		// copy image data
		IOHANDLE File = io_open(aBuf, IOFLAG_WRITE);
		if(File)
		{
			TImageByteBuffer ByteBuffer;
			SImageByteBuffer ImageByteBuffer(&ByteBuffer);

			if(SavePNG(IMAGE_FORMAT_RGBA, (const uint8_t *)Map.GetData(pItem->m_ImageData), ImageByteBuffer, pItem->m_Width, pItem->m_Height))
				io_write(File, &ByteBuffer.front(), ByteBuffer.size());
			io_close(File);
		}
	}

	// load sounds
	Map.GetType(MAPITEMTYPE_SOUND, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemSound *pItem = (CMapItemSound *)Map.GetItem(Start + i, 0, 0);
		char *pName = (char *)Map.GetData(pItem->m_SoundName);

		if(pItem->m_External)
			continue;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s/%s.opus", pPathSave, pName);
		dbg_msg("map_extract", "writing sound: %s (%d B)", aBuf, pItem->m_SoundDataSize);

		IOHANDLE Opus = io_open(aBuf, IOFLAG_WRITE);
		io_write(Opus, (unsigned char *)Map.GetData(pItem->m_SoundData), pItem->m_SoundDataSize);
		io_close(Opus);
	}

	return Map.Close();
}

int main(int argc, char *argv[])
{
	dbg_logger_stdout();

	char aMap[512];
	char aDir[512];

	IStorage *pStorage = CreateLocalStorage();

	if(argc == 2)
	{
		str_copy(aMap, argv[1], sizeof(aMap));
		str_copy(aDir, ".", sizeof(aMap));
	}
	else if(argc == 3)
	{
		str_copy(aMap, argv[1], sizeof(aMap));
		str_copy(aDir, argv[2], sizeof(aDir));
	}
	else
	{
		dbg_msg("usage", "%s map [directory]", argv[0]);
		return -1;
	}

	if(!fs_is_dir(aDir))
	{
		dbg_msg("usage", "directory '%s' does not exist", aDir);
		return -1;
	}

	return Process(pStorage, aMap, aDir) ? 0 : 1;
}
