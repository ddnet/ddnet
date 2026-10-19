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
		int Id;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, nullptr, &Id);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || Id != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		ConfigFound = true;
		IOHANDLE Config = pStorage->OpenFile(pConfigName, IOFLAG_WRITE, IStorage::TYPE_ABSOLUTE);
		if(!Config)
		{
			log_error(TOOL_NAME, "Failed to open config '%s' for writing", pConfigName);
			return;
		}

		int Size = pMap->GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			io_write(Config, pNext, StrSize - 1);
			io_write_newline(Config);
			pNext += StrSize;
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
