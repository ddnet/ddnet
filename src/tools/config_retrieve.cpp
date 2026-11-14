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

	const CMapItemInfoSettings *pSettingsItem = static_cast<const CMapItemInfoSettings *>(pMap->FindItem(MAPITEMTYPE_INFO, 0));
	if(pSettingsItem == nullptr)
	{
		(void)fs_remove(pConfigName);
	}
	else
	{
		const std::optional<std::vector<const char *>> vpSettings = pMap->GetDataStringArray(pSettingsItem->m_Settings);
		if(!vpSettings.has_value())
		{
			log_error(TOOL_NAME, "Failed to read settings from map '%s'", pMapName);
			return;
		}
		if(vpSettings.value().empty())
		{
			(void)fs_remove(pConfigName);
		}
		else
		{
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
			pMap->UnloadData(pSettingsItem->m_Settings);
			io_close(Config);
		}
	}
	pMap->Unload();
}
#include "config_common.h"
