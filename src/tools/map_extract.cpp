// Adapted from TWMapImagesRecovery by Tardo: https://github.com/Tardo/TWMapImagesRecovery

#include <base/fs.h>
#include <base/io.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/gfx/image_loader.h>
#include <engine/map.h>
#include <engine/storage.h>

#include <game/mapitems.h>

static void PrintMapInfo(IMap *pMap)
{
	const CMapItemInfo *pInfo = static_cast<CMapItemInfo *>(pMap->FindItem(MAPITEMTYPE_INFO, 0));
	if(pInfo)
	{
		const char *pAuthor = pMap->GetDataString(pInfo->m_Author);
		log_info("map_extract", "author:  %s", pAuthor == nullptr ? "(error)" : pAuthor);
		const char *pMapVersion = pMap->GetDataString(pInfo->m_MapVersion);
		log_info("map_extract", "version: %s", pMapVersion == nullptr ? "(error)" : pMapVersion);
		const char *pCredits = pMap->GetDataString(pInfo->m_Credits);
		log_info("map_extract", "credits: %s", pCredits == nullptr ? "(error)" : pCredits);
		const char *pLicense = pMap->GetDataString(pInfo->m_License);
		log_info("map_extract", "license: %s", pLicense == nullptr ? "(error)" : pLicense);
	}
}

static void ExtractMapImages(IMap *pMap, const char *pPathSave)
{
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
	for(int i = 0; i < Num; i++)
	{
		const CMapItemImage_v2 *pItem = static_cast<CMapItemImage_v2 *>(pMap->GetItem(Start + i));
		if(pItem->m_External)
			continue;

		const char *pName = pMap->GetDataString(pItem->m_ImageName);
		if(pName == nullptr || pName[0] == '\0')
		{
			log_error("map_extract", "failed to load name of image %d", i);
			continue;
		}

		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.png", pPathSave, pName);
		pMap->UnloadData(pItem->m_ImageName);

		if(pItem->m_MustBe1 != 1)
		{
			log_error("map_extract", "ignoring image '%s' with unknown format %d", aBuf, pItem->m_MustBe1);
			continue;
		}

		if(pItem->m_Width <= 0 || pItem->m_Height <= 0)
		{
			log_error("map_extract", "ignoring image '%s' with invalid dimensions %dx%d", aBuf, pItem->m_Width, pItem->m_Height);
			continue;
		}

		CImageInfo Image;
		Image.m_Width = pItem->m_Width;
		Image.m_Height = pItem->m_Height;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.m_pData = static_cast<uint8_t *>(pMap->GetData(pItem->m_ImageData));

		log_info("map_extract", "writing image: %s (%dx%d)", aBuf, pItem->m_Width, pItem->m_Height);
		if(!CImageLoader::SavePng(io_open(aBuf, IOFLAG_WRITE), aBuf, Image))
		{
			log_error("map_extract", "failed to write image file. filename='%s'", aBuf);
		}
		pMap->UnloadData(pItem->m_ImageData);
	}
}

static void ExtractMapSounds(IMap *pMap, const char *pPathSave)
{
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_SOUND, &Start, &Num);
	for(int i = 0; i < Num; i++)
	{
		const CMapItemSound *pItem = static_cast<CMapItemSound *>(pMap->GetItem(Start + i));
		if(pItem->m_External)
			continue;

		const char *pName = pMap->GetDataString(pItem->m_SoundName);
		if(pName == nullptr || pName[0] == '\0')
		{
			log_error("map_extract", "failed to load name of sound %d", i);
			continue;
		}

		const int SoundDataSize = pMap->GetDataSize(pItem->m_SoundData);
		char aBuf[IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), "%s/%s.opus", pPathSave, pName);
		pMap->UnloadData(pItem->m_SoundName);

		IOHANDLE Opus = io_open(aBuf, IOFLAG_WRITE);
		if(Opus)
		{
			log_info("map_extract", "writing sound: %s (%d B)", aBuf, SoundDataSize);
			io_write(Opus, pMap->GetData(pItem->m_SoundData), SoundDataSize);
			io_close(Opus);
			pMap->UnloadData(pItem->m_SoundData);
		}
		else
		{
			log_error("map_extract", "failed to open sound file for writing. filename='%s'", aBuf);
		}
	}
}

static bool ExtractMap(IStorage *pStorage, const char *pMapName, const char *pPathSave)
{
	std::unique_ptr<IMap> pMap = CreateMap();
	if(!pMap->Load(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		log_error("map_extract", "error opening map '%s'", pMapName);
		return false;
	}

	log_info("map_extract", "Make sure you have the permission to use these images and sounds in your own maps");

	PrintMapInfo(pMap.get());
	ExtractMapImages(pMap.get(), pPathSave);
	ExtractMapSounds(pMap.get(), pPathSave);

	pMap->Unload();
	return true;
}

int main(int argc, const char *argv[])
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	std::unique_ptr<IStorage> pStorage = CreateLocalStorage();
	if(!pStorage)
	{
		log_error("map_extract", "Error creating local storage");
		return -1;
	}

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

	return ExtractMap(pStorage.get(), argv[1], pDir) ? 0 : 1;
}
