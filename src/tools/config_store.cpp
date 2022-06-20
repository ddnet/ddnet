#include <base/math.h>
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <game/mapitems.h>
#include <vector>

void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	IOHANDLE File = pStorage->OpenFile(pConfigName, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_ABSOLUTE);
	if(!File)
	{
		dbg_msg("config_store", "config '%s' not found", pConfigName);
		return;
	}

	CLineReader LineReader;
	LineReader.Init(File);

	char *pLine;
	int TotalLength = 0;
	std::vector<char *> vLines;
	while((pLine = LineReader.Get()))
	{
		int Length = str_length(pLine) + 1;
		char *pCopy = (char *)malloc(Length);
		mem_copy(pCopy, pLine, Length);
		vLines.push_back(pCopy);
		TotalLength += Length;
	}
	io_close(File);

	char *pSettings = (char *)malloc(maximum(1, TotalLength));
	int Offset = 0;
	for(auto &Line : vLines)
	{
		int Length = str_length(Line) + 1;
		mem_copy(pSettings + Offset, Line, Length);
		Offset += Length;
		free(Line);
	}

	CDataFileReader Reader;
	Reader.Open(pStorage, pMapName, IStorage::TYPE_ABSOLUTE);

	CDataFileWriter Writer;
	Writer.Init();

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int Type, ID;
		int *pItem = (int *)Reader.GetItem(i, &Type, &ID);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if(Type == MAPITEMTYPE_INFO && ID == 0)
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
						dbg_msg("config_store", "configs coincide, not updating map");
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
		Writer.AddItem(Type, ID, Size, pItem);
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
	if(!Writer.OpenFile(pStorage, pMapName))
	{
		dbg_msg("config_store", "couldn't open map file '%s' for writing", pMapName);
		return;
	}
	Writer.Finish();
	dbg_msg("config_store", "imported settings");
}
#include "config_common.h"
