#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/mapitems.h>

#include <vector>

void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pConfigName, IOFLAG_READ, IStorage::TYPE_ABSOLUTE)))
	{
		log_error("config_store", "Failed to import settings from '%s': could not open config for reading", pConfigName);
		return;
	}

	std::vector<const char *> vpLines;
	int TotalLength = 0;
	while(const char *pLine = LineReader.Get())
	{
		vpLines.push_back(pLine);
		TotalLength += str_length(pLine) + 1;
	}

	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		log_error("config_store", "Failed to import settings from '%s': failed to open map '%s' for reading", pConfigName, pMapName);
		return;
	}

	char *pSettings = (char *)malloc(maximum(1, TotalLength));
	int Offset = 0;
	for(const char *pLine : vpLines)
	{
		int Length = str_length(pLine) + 1;
		mem_copy(pSettings + Offset, pLine, Length);
		Offset += Length;
	}

	CDataFileWriter Writer;

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int Type, Id;
		int *pItem = (int *)Reader.GetItem(i, &Type, &Id);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if(Type == MAPITEMTYPE_INFO && Id == 0)
		{
			FoundInfo = true;
			CMapItemInfoSettings *pInfo = (CMapItemInfoSettings *)pItem;
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				MapInfo = *pInfo;
				pItem = (int *)&MapInfo;
				Size = sizeof(MapInfo);
				if(pInfo->m_Settings > -1)
				{
					SettingsIndex = pInfo->m_Settings;
					char *pMapSettings = (char *)Reader.GetData(SettingsIndex);
					int DataSize = Reader.GetDataSize(SettingsIndex);
					if(DataSize == TotalLength && mem_comp(pSettings, pMapSettings, DataSize) == 0)
					{
						log_info("config_store", "Configs coincide, not updating map");
						free(pSettings);
						return;
					}
					Reader.UnloadData(pInfo->m_Settings);
				}
				else
				{
					MapInfo = *pInfo;
					MapInfo.m_Settings = SettingsIndex;
					pItem = (int *)&MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo *)&MapInfo = *(CMapItemInfo *)pInfo;
				MapInfo.m_Settings = SettingsIndex;
				pItem = (int *)&MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(Type, Id, Size, pItem);
	}

	if(!FoundInfo)
	{
		CMapItemInfoSettings Info;
		Info.m_Version = 1;
		Info.m_Author = -1;
		Info.m_MapVersion = -1;
		Info.m_Credits = -1;
		Info.m_License = -1;
		Info.m_Settings = SettingsIndex;
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
	}

	for(int i = 0; i < Reader.NumData() || i == SettingsIndex; i++)
	{
		if(i == SettingsIndex)
		{
			Writer.AddData(TotalLength, pSettings);
			continue;
		}
		unsigned char *pData = (unsigned char *)Reader.GetData(i);
		int Size = Reader.GetDataSize(i);
		Writer.AddData(Size, pData);
		Reader.UnloadData(i);
	}

	free(pSettings);
	Reader.Close();
	if(!Writer.Open(pStorage, pMapName))
	{
		log_error("config_store", "Failed to import settings from '%s': failed to open map '%s' for writing", pConfigName, pMapName);
		return;
	}
	Writer.Finish();
	log_info("config_store", "Imported settings from '%s' into '%s'", pConfigName, pMapName);
}
#include "config_common.h"
