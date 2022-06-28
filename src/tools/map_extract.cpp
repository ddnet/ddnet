// Adapted from TWMapImagesRecovery by Tardo: https://github.com/Tardo/TWMapImagesRecovery
#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

bool Process(IStorage *pStorage, const char *pMapName, const char *pPathSave)
{
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_extract", "error opening map '%s'", pMapName);
		return false;
	}

	// check version
	CMapItemVersion *pVersion = (CMapItemVersion *)Reader.FindItem(MAPITEMTYPE_VERSION, 0);
	if(pVersion && pVersion->m_Version != 1)
		return false;

	dbg_msg("map_extract", "Make sure you have the permission to use these images and sounds in your own maps");

	CMapItemInfo *pInfo = (CMapItemInfo *)Reader.FindItem(MAPITEMTYPE_INFO, 0);

	if(pInfo)
	{
		dbg_msg("map_extract", "author:  %s", (char *)Reader.GetData(pInfo->m_Author));
		dbg_msg("map_extract", "version: %s", (char *)Reader.GetData(pInfo->m_MapVersion));
		dbg_msg("map_extract", "credits: %s", (char *)Reader.GetData(pInfo->m_Credits));
		dbg_msg("map_extract", "license: %s", (char *)Reader.GetData(pInfo->m_License));
	}

	int Start, Num;

	// load images
	Reader.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemImage *pItem = (CMapItemImage *)Reader.GetItem(Start + i, nullptr, nullptr);
		char *pName = (char *)Reader.GetData(pItem->m_ImageName);

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

			if(SavePNG(IMAGE_FORMAT_RGBA, (const uint8_t *)Reader.GetData(pItem->m_ImageData), ImageByteBuffer, pItem->m_Width, pItem->m_Height))
				io_write(File, &ByteBuffer.front(), ByteBuffer.size());
			io_close(File);
		}
	}

	// load sounds
	Reader.GetType(MAPITEMTYPE_SOUND, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemSound *pItem = (CMapItemSound *)Reader.GetItem(Start + i, nullptr, nullptr);
		char *pName = (char *)Reader.GetData(pItem->m_SoundName);

		if(pItem->m_External)
			continue;

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%s/%s.opus", pPathSave, pName);
		dbg_msg("map_extract", "writing sound: %s (%d B)", aBuf, pItem->m_SoundDataSize);

		IOHANDLE Opus = io_open(aBuf, IOFLAG_WRITE);
		io_write(Opus, (unsigned char *)Reader.GetData(pItem->m_SoundData), pItem->m_SoundDataSize);
		io_close(Opus);
	}

	return Reader.Close();
}

int main(int argc, const char *argv[])
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	IStorage *pStorage = CreateLocalStorage();
	if(!pStorage)
		return -1;

	const char *pDir;
	if(argc == 2)
	{
		pDir = ".";
	}
	else if(argc == 3)
	{
		pDir = argv[2];
	}
	else
	{
		dbg_msg("usage", "%s map [directory]", argv[0]);
		return -1;
	}

	if(!fs_is_dir(pDir))
	{
		dbg_msg("usage", "directory '%s' does not exist", pDir);
		return -1;
	}

	int Result = Process(pStorage, argv[1], pDir) ? 0 : 1;
	return Result;
}
