#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	CDataFileReader Map;
	if(!Map.Open(pStorage, pMapName, IStorage::TYPE_ALL))
	{
		dbg_msg("config_retrieve", "error opening map '%s'", pMapName);
		return;
	}
	bool ConfigFound = false;
	int Start, Num;
	Map.GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)Map.GetItem(i, 0, &ItemID);
		int ItemSize = Map.GetItemSize(i) - 8;
		if(!pItem || ItemID != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		ConfigFound = true;
		IOHANDLE Config = pStorage->OpenFile(pConfigName, IOFLAG_WRITE, IStorage::TYPE_ALL);
		if(!Config)
		{
			dbg_msg("config_retrieve", "error opening config for writing '%s'", pConfigName);
			return;
		}

		int Size = Map.GetUncompressedDataSize(pItem->m_Settings);
		char *pSettings = (char *)Map.GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			io_write(Config, pNext, StrSize - 1);
			io_write_newline(Config);
			pNext += StrSize;
		}
		Map.UnloadData(pItem->m_Settings);
		io_close(Config);
		break;
	}
	Map.Close();
	if(!ConfigFound)
	{
		fs_remove(pConfigName);
	}
}
#include "config_common.h"
