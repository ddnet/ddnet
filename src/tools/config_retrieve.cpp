#include <base/fs.h>
#include <base/io.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/map.h>
#include <engine/storage.h>

#include <game/mapitems.h>

static constexpr const char *const TOOL_NAME = "config_retrieve";

static void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	std::shared_ptr<IMap> pMap = CreateMap();
	if(!pMap->Load(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		log_error(TOOL_NAME, "Failed to open map '%s' for reading", pMapName);
		return;
	}
	bool ConfigFound = false;
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemId;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, nullptr, &ItemId);
		if(ItemId != 0)
			continue;

		const std::optional<std::vector<const char *>> vpSettings = pMap->GetDataStringArray(pItem->m_Settings);
		if(!vpSettings.has_value())
		{
			log_error(TOOL_NAME, "Failed to read settings from map '%s'", pMapName);
			return;
		}
		if(vpSettings.value().empty())
		{
			break;
		}

		ConfigFound = true;
		IOHANDLE Config = pStorage->OpenFile(pConfigName, IOFLAG_WRITE, IStorage::TYPE_ABSOLUTE);
		if(!Config)
		{
			log_error(TOOL_NAME, "Failed to open config '%s' for writing", pConfigName);
			return;
		}
		for(const char *pSetting : vpSettings.value())
		{
			io_write(Config, pSetting, str_length(pSetting));
			io_write_newline(Config);
		}
		pMap->UnloadData(pItem->m_Settings);
		io_close(Config);
		break;
	}
	pMap->Unload();
	if(!ConfigFound)
	{
		(void)fs_remove(pConfigName);
	}
}
#include "config_common.h"
