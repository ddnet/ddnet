#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
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

		ConfigFound = true;
		IOHANDLE Config = pStorage->OpenFile(pConfigName, IOFLAG_WRITE, IStorage::TYPE_ABSOLUTE);
		if(!Config)
		{
			dbg_msg("config_retrieve", "error opening config for writing '%s'", pConfigName);
			Reader.Close();
			return;
		}

		int Size = Reader.GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)Reader.GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			io_write(Config, pNext, StrSize - 1);
			io_write_newline(Config);
			pNext += StrSize;
		}
		Reader.UnloadData(pItem->m_Settings);
		io_close(Config);
		break;
	}
	Reader.Close();
	if(!ConfigFound)
	{
		fs_remove(pConfigName);
	}
}
#include "config_common.h"
