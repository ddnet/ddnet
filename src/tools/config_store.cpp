#include <base/dbg.h>
#include <base/io.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/map.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/mapitems.h>

#include <algorithm>
#include <vector>

static constexpr const char *const TOOL_NAME = "config_store";

static void Process(IStorage *pStorage, const char *pMapName, const char *pConfigName)
{
	CLineReader LineReader;
	if(!LineReader.OpenFile(pStorage->OpenFile(pConfigName, IOFLAG_READ, IStorage::TYPE_ABSOLUTE)))
	{
		log_error(TOOL_NAME, "Failed to import settings from '%s': failed to open config for reading", pConfigName);
		return;
	}
	std::vector<const char *> vpConfigLines;
	while(const char *pLine = LineReader.Get())
	{
		vpConfigLines.push_back(pLine);
	}

	std::shared_ptr<IMap> pMap = CreateMap();
	if(!pMap->Load(pStorage, pMapName, IStorage::TYPE_ABSOLUTE))
	{
		log_error(TOOL_NAME, "Failed to import settings from '%s': failed to open map '%s' for reading", pConfigName, pMapName);
		return;
	}

	const CMapItemInfoSettings *pSettingsItem = static_cast<CMapItemInfoSettings *>(pMap->FindItem(MAPITEMTYPE_INFO, 0));
	if(pSettingsItem == nullptr)
	{
		if(vpConfigLines.empty())
		{
			log_info(TOOL_NAME, "Configs coincide, not updating map");
			return;
		}
	}
	else
	{
		const std::optional<std::vector<const char *>> vpMapLines = pMap->GetDataStringArray(pSettingsItem->m_Settings);
		if(!vpMapLines.has_value())
		{
			log_error(TOOL_NAME, "Failed to read settings from map '%s'", pMapName);
			return;
		}
		if((vpConfigLines.empty() && vpMapLines.value().size() == 1 && vpMapLines.value().front()[0] == '\0') ||
			std::equal(vpConfigLines.begin(), vpConfigLines.end(), vpMapLines.value().begin(), vpMapLines.value().end(),
				[&](const char *pConfigLine, const char *pMapLine) { return str_comp(pConfigLine, pMapLine) == 0; }))
		{
			log_info(TOOL_NAME, "Configs coincide, not updating map");
			return;
		}
		pMap->UnloadData(pSettingsItem->m_Settings);
		if(vpConfigLines.empty())
		{
			// An empty string array would be packed as index -1, but we need to preserve the
			// original data indices below so we instead pack an array of an empty string.
			vpConfigLines = {""};
		}
	}

	CDataFileWriter Writer;

	for(int ItemIndex = 0; ItemIndex < pMap->NumItems(); ItemIndex++)
	{
		int Type, Id;
		CUuid Uuid;
		const void *pItem = pMap->GetItem(ItemIndex, &Type, &Id, &Uuid);

		// Filter ITEMTYPE_EX items, they will be automatically added again.
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		const int Size = pMap->GetItemSize(ItemIndex);
		Writer.AddItem(Type, Id, Size, pItem, &Uuid);
	}

	const int NewSettingsDataIndex = pMap->NumData();
	if(pSettingsItem == nullptr)
	{
		CMapItemInfoSettings Info;
		Info.m_Version = 1;
		Info.m_Author = -1;
		Info.m_MapVersion = -1;
		Info.m_Credits = -1;
		Info.m_License = -1;
		Info.m_Settings = NewSettingsDataIndex;
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
	}

	for(int DataIndex = 0; DataIndex < pMap->NumData() || DataIndex == NewSettingsDataIndex; DataIndex++)
	{
		if(DataIndex == NewSettingsDataIndex)
		{
			dbg_assert(Writer.AddDataStringArray(vpConfigLines) == DataIndex, "Data indices mismatch");
			continue;
		}
		const void *pData = pMap->GetData(DataIndex);
		const int Size = pMap->GetDataSize(DataIndex);
		dbg_assert(Writer.AddData(Size, pData) == DataIndex, "Data indices mismatch");
		pMap->UnloadData(DataIndex);
	}

	pMap->Unload();

	if(!Writer.Open(pStorage, pMapName))
	{
		log_error(TOOL_NAME, "Failed to import settings from '%s': failed to open map '%s' for writing", pConfigName, pMapName);
		return;
	}
	Writer.Finish();
	log_info(TOOL_NAME, "Imported settings from '%s' into '%s'", pConfigName, pMapName);
}
#include "config_common.h"
