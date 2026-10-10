#include <base/dbg.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

#include <game/mapitems.h>

static void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("config_retrieve", "error opening map '%s'", pMapName);
		return;
	}
	bool ConfigFound = false;
	int Start, Num;
	Reader.GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int Id;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)Reader.GetItem(i, nullptr, &Id);
		int ItemSize = Reader.GetItemSize(i);
		if(!pItem || Id != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		const std::optional<std::vector<const char *>> vpSettings = Reader.GetDataStringArray(pItem->m_Settings);
		if(!vpSettings.has_value())
		{
			dbg_msg("config_retrieve", "error reading settings from map '%s'", pMapName);
			break;
		}

		ConfigFound = true;
		IOHANDLE Config = pStorage->OpenFile(pConfigName, IOFLAG_WRITE, IStorage::TYPE_ABSOLUTE);
		if(!Config)
		{
			dbg_msg("config_retrieve", "error opening config for writing '%s'", pConfigName);
			Reader.Close();
			return;
		}

		for(const char *pSetting : vpSettings.value())
		{
			io_write(Config, pSetting, str_length(pSetting));
			io_write_newline(Config);
		}
		Reader.UnloadData(pItem->m_Settings);
		io_close(Config);
		break;
	}
	Reader.Close();
	if(!ConfigFound)
	{
		(void)fs_remove(pConfigName);
	}
}
#include "config_common.h"
