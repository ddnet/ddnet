#include <base/system.h>
#include <base/tl/array.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <game/mapitems.h>

void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	IOHANDLE File = pStorage->OpenFile(pConfigName, IOFLAG_READ, IStorage::TYPE_ALL);
	array<char *> aLines;
	char *pSettings = NULL;
	if(!File)
	{
		dbg_msg("config_store", "config '%s' not found", pConfigName);
		return;
	}

	CLineReader LineReader;
	LineReader.Init(File);

	char *pLine;
	int TotalLength = 0;
	while((pLine = LineReader.Get()))
	{
		int Length = str_length(pLine) + 1;
		char *pCopy = (char *)mem_alloc(Length, 1);
		mem_copy(pCopy, pLine, Length);
		aLines.add(pCopy);
		TotalLength += Length;
	}

	pSettings = (char *)mem_alloc(TotalLength, 1);
	int Offset = 0;
	for(int i = 0; i < aLines.size(); i++)
	{
		int Length = str_length(aLines[i]) + 1;
		mem_copy(pSettings + Offset, aLines[i], Length);
		Offset += Length;
		mem_free(aLines[i]);
	}

	CDataFileReader Reader;
	Reader.Open(pStorage, pMapName, IStorage::TYPE_ALL);

	CDataFileWriter Writer;
	Writer.Init();

	int SettingsIndex = Reader.NumData();
	for(int i = 0; i < Reader.NumItems(); i++)
	{
		int TypeID;
		int ItemID;
		int *pData = (int *)Reader.GetItem(i, &TypeID, &ItemID);
		// GetItemSize returns item size including header, remove that.
		int Size = Reader.GetItemSize(i) - sizeof(int) * 2;
		CMapItemInfoSettings MapInfo;
		if(TypeID == MAPITEMTYPE_INFO && ItemID == 0)
		{
			CMapItemInfoSettings *pInfo = (CMapItemInfoSettings *)pData;
			if(Size >= (int)sizeof(CMapItemInfoSettings))
			{
				MapInfo = *pInfo;
				pData = (int *)&MapInfo;
				Size = sizeof(MapInfo);
				if(pInfo->m_Settings > -1)
				{
					SettingsIndex = pInfo->m_Settings;
					char *pMapSettings = (char *)Reader.GetData(SettingsIndex);
					int DataSize = Reader.GetUncompressedDataSize(SettingsIndex);
					if(DataSize == TotalLength && mem_comp(pSettings, pMapSettings, Size) == 0)
					{
						dbg_msg("config_store", "configs coincide, not updating map");
						return;
					}
					Reader.UnloadData(pInfo->m_Settings);
				}
				else
				{
					MapInfo = *pInfo;
					MapInfo.m_Settings = SettingsIndex;
					pData = (int *)&MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo *)&MapInfo = *(CMapItemInfo *)pInfo;
				MapInfo.m_Settings = SettingsIndex;
				pData = (int *)&MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(TypeID, ItemID, Size, pData);
	}

	for(int i = 0; i < Reader.NumData() || i == SettingsIndex; i++)
	{
		if(i == SettingsIndex)
		{
			Writer.AddData(TotalLength, pSettings);
			continue;
		}
		unsigned char *pData = (unsigned char *)Reader.GetData(i);
		int Size = Reader.GetUncompressedDataSize(i);
		Writer.AddData(Size, pData);
		Reader.UnloadData(i);
	}

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
