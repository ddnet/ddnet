// Adapted from TWMapImagesRecovery by Tardo: https://github.com/Tardo/TWMapImagesRecovery

#include <base/logger.h>
#include <base/system.h>

#include <engine/gfx/image_loader.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/mapitems.h>

static void PrintMapInfo(CDataFileReader &Reader)
{
	const CMapItemInfo *pInfo = static_cast<CMapItemInfo *>(Reader.FindItem(MAPITEMTYPE_INFO, 0));
	if(pInfo)
	{
		const char *pAuthor = Reader.GetDataString(pInfo->m_Author);
		log_info("map_extract", "author:  %s", pAuthor == nullptr ? "(error)" : pAuthor);
		const char *pMapVersion = Reader.GetDataString(pInfo->m_MapVersion);
		log_info("map_extract", "version: %s", pMapVersion == nullptr ? "(error)" : pMapVersion);
		const char *pCredits = Reader.GetDataString(pInfo->m_Credits);
		log_info("map_extract", "credits: %s", pCredits == nullptr ? "(error)" : pCredits);
		const char *pLicense = Reader.GetDataString(pInfo->m_License);
		log_info("map_extract", "license: %s", pLicense == nullptr ? "(error)" : pLicense);
	}
}

static void ExtractMapImages(CDataFileReader &Reader, const char *pPathSave)
{
	int Start, Num;
	Reader.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
	for(int i = 0; i < Num; i++)
	{
		const CMapItemImage_v2 *pItem = static_cast<CMapItemImage_v2 *>(Reader.GetItem(Start + i));
		if(pItem->m_External)
			continue;

		const char *pName = Reader.GetDataString(pItem->m_ImageName);
		if(pName == nullptr || pName[0] == '\0')
		{
			log_error("map_extract", "failed to load name of image %d", i);
			continue;
		}

		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.png", pPathSave, pName);
		Reader.UnloadData(pItem->m_ImageName);

		if(pItem->m_Version >= 2 && pItem->m_MustBe1 != 1)
		{
			log_error("map_extract", "ignoring image '%s' with unknown format %d", aBuf, pItem->m_MustBe1);
			continue;
		}

		CImageInfo Image;
		Image.m_Width = pItem->m_Width;
		Image.m_Height = pItem->m_Height;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.m_pData = static_cast<uint8_t *>(Reader.GetData(pItem->m_ImageData));

		log_info("map_extract", "writing image: %s (%dx%d)", aBuf, pItem->m_Width, pItem->m_Height);
		if(!CImageLoader::SavePng(io_open(aBuf, IOFLAG_WRITE), aBuf, Image))
		{
			log_error("map_extract", "failed to write image file. filename='%s'", aBuf);
		}
		Reader.UnloadData(pItem->m_ImageData);
	}
}

static void ExtractMapSounds(CDataFileReader &Reader, const char *pPathSave)
{
	int Start, Num;
	Reader.GetType(MAPITEMTYPE_SOUND, &Start, &Num);
	for(int i = 0; i < Num; i++)
	{
		const CMapItemSound *pItem = static_cast<CMapItemSound *>(Reader.GetItem(Start + i));
		if(pItem->m_External)
			continue;

		const char *pName = Reader.GetDataString(pItem->m_SoundName);
		if(pName == nullptr || pName[0] == '\0')
		{
			log_error("map_extract", "failed to load name of sound %d", i);
			continue;
		}

		const int SoundDataSize = Reader.GetDataSize(pItem->m_SoundData);
		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.opus", pPathSave, pName);
		Reader.UnloadData(pItem->m_SoundName);

		IOHANDLE Opus = io_open(aBuf, IOFLAG_WRITE);
		if(Opus)
		{
			log_info("map_extract", "writing sound: %s (%d B)", aBuf, SoundDataSize);
			io_write(Opus, Reader.GetData(pItem->m_SoundData), SoundDataSize);
			io_close(Opus);
			Reader.UnloadData(pItem->m_SoundData);
		}
		else
		{
			log_error("map_extract", "failed to open sound file for writing. filename='%s'", aBuf);
		}
	}
}

static bool ExtractMap(IStorage *pStorage, const char *pMapName, const char *pPathSave)
{
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		log_error("map_extract", "error opening map '%s'", pMapName);
		return false;
	}

	const CMapItemVersion *pVersion = static_cast<CMapItemVersion *>(Reader.FindItem(MAPITEMTYPE_VERSION, 0));
	if(pVersion == nullptr || pVersion->m_Version != 1)
	{
		log_error("map_extract", "unsupported map version '%s'", pMapName);
		return false;
	}

	log_info("map_extract", "Make sure you have the permission to use these images and sounds in your own maps");

	PrintMapInfo(Reader);
	ExtractMapImages(Reader, pPathSave);
	ExtractMapSounds(Reader, pPathSave);

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
		log_error("map_extract", "usage: %s <map> [directory]", argv[0]);
		return -1;
	}

	if(!fs_is_dir(pDir))
	{
		log_error("map_extract", "directory '%s' does not exist", pDir);
		return -1;
	}

	int Result = ExtractMap(pStorage, argv[1], pDir) ? 0 : 1;
	return Result;
}
