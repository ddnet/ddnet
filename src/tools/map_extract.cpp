// Adapted from TWMapImagesRecovery by Tardo: https://github.com/Tardo/TWMapImagesRecovery
#include <base/logger.h>
#include <base/system.h>
#include <engine/gfx/image_loader.h>
#include <engine/graphics.h>
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

	const CMapItemVersion *pVersion = static_cast<CMapItemVersion *>(Reader.FindItem(MAPITEMTYPE_VERSION, 0));
	if(pVersion == nullptr || pVersion->m_Version != CMapItemVersion::CURRENT_VERSION)
	{
		dbg_msg("map_extract", "unsupported map version '%s'", pMapName);
		return false;
	}

	dbg_msg("map_extract", "Make sure you have the permission to use these images and sounds in your own maps");

	CMapItemInfo *pInfo = (CMapItemInfo *)Reader.FindItem(MAPITEMTYPE_INFO, 0);

	if(pInfo)
	{
		const char *pAuthor = Reader.GetDataString(pInfo->m_Author);
		dbg_msg("map_extract", "author:  %s", pAuthor == nullptr ? "(error)" : pAuthor);
		const char *pMapVersion = Reader.GetDataString(pInfo->m_MapVersion);
		dbg_msg("map_extract", "version: %s", pMapVersion == nullptr ? "(error)" : pMapVersion);
		const char *pCredits = Reader.GetDataString(pInfo->m_Credits);
		dbg_msg("map_extract", "credits: %s", pCredits == nullptr ? "(error)" : pCredits);
		const char *pLicense = Reader.GetDataString(pInfo->m_License);
		dbg_msg("map_extract", "license: %s", pLicense == nullptr ? "(error)" : pLicense);
	}

	int Start, Num;

	// load images
	Reader.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemImage_v2 *pItem = (CMapItemImage_v2 *)Reader.GetItem(Start + i);
		if(pItem->m_External)
			continue;

		const char *pName = Reader.GetDataString(pItem->m_ImageName);
		if(pName == nullptr || pName[0] == '\0')
		{
			dbg_msg("map_extract", "failed to load name of image %d", i);
			continue;
		}

		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.png", pPathSave, pName);
		dbg_msg("map_extract", "writing image: %s (%dx%d)", aBuf, pItem->m_Width, pItem->m_Height);

		const int Format = pItem->m_Version < CMapItemImage_v2::CURRENT_VERSION ? CImageInfo::FORMAT_RGBA : pItem->m_Format;
		EImageFormat OutputFormat;
		if(Format == CImageInfo::FORMAT_RGBA)
			OutputFormat = IMAGE_FORMAT_RGBA;
		else if(Format == CImageInfo::FORMAT_RGB)
			OutputFormat = IMAGE_FORMAT_RGB;
		else
		{
			dbg_msg("map_extract", "ignoring image '%s' with unknown format %d", aBuf, Format);
			continue;
		}

		// copy image data
		IOHANDLE File = io_open(aBuf, IOFLAG_WRITE);
		if(File)
		{
			TImageByteBuffer ByteBuffer;
			SImageByteBuffer ImageByteBuffer(&ByteBuffer);

			if(SavePNG(OutputFormat, (const uint8_t *)Reader.GetData(pItem->m_ImageData), ImageByteBuffer, pItem->m_Width, pItem->m_Height))
				io_write(File, &ByteBuffer.front(), ByteBuffer.size());
			io_close(File);
		}
	}

	// load sounds
	Reader.GetType(MAPITEMTYPE_SOUND, &Start, &Num);

	for(int i = 0; i < Num; i++)
	{
		CMapItemSound *pItem = (CMapItemSound *)Reader.GetItem(Start + i);
		if(pItem->m_External)
			continue;

		const char *pName = Reader.GetDataString(pItem->m_SoundName);
		if(pName == nullptr || pName[0] == '\0')
		{
			dbg_msg("map_extract", "failed to load name of sound %d", i);
			continue;
		}

		const int SoundDataSize = Reader.GetDataSize(pItem->m_SoundData);
		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.opus", pPathSave, pName);
		dbg_msg("map_extract", "writing sound: %s (%d B)", aBuf, SoundDataSize);

		IOHANDLE Opus = io_open(aBuf, IOFLAG_WRITE);
		io_write(Opus, Reader.GetData(pItem->m_SoundData), SoundDataSize);
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
