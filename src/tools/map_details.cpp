#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/mapitems.h>

void Process(IStorage *pStorage, const char *pMapName, const char *pMapDetailsName)
{
	CDataFileReader Map;
	if(!Map.Open(pStorage, pMapName, IStorage::TYPE_ALL))
	{
		dbg_msg("map_details", "error opening map '%s'", pMapName);
		return;
	}

	int Start, Num;
	Map.GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)Map.GetItem(i, 0, &ItemID);
		if(!pItem || ItemID != 0)
			continue;

		IOHANDLE MapDetails = pStorage->OpenFile(pMapDetailsName, IOFLAG_WRITE, IStorage::TYPE_ALL);
		if(!MapDetails)
		{
			dbg_msg("map_details", "error opening config for writing '%s'", pMapDetailsName);
			return;
		}

		char aAuthor[32];
		char aVersion[16];
		char aCredits[128];
		char aLicense[32];

		str_copy(aAuthor, pItem->m_Author > -1 ? (char *)Map.GetData(pItem->m_Author) : "", sizeof(aAuthor));
		str_copy(aVersion, pItem->m_MapVersion > -1 ? (char *)Map.GetData(pItem->m_MapVersion) : "", sizeof(aVersion));
		str_copy(aCredits, pItem->m_Credits > -1 ? (char *)Map.GetData(pItem->m_Credits) : "", sizeof(aCredits));
		str_copy(aLicense, pItem->m_License > -1 ? (char *)Map.GetData(pItem->m_License) : "", sizeof(aLicense));

		io_write(MapDetails, aAuthor, str_length(aAuthor));
		io_write_newline(MapDetails);
		io_write(MapDetails, aVersion, str_length(aVersion));
		io_write_newline(MapDetails);
		io_write(MapDetails, aCredits, str_length(aCredits));
		io_write_newline(MapDetails);
		io_write(MapDetails, aLicense, str_length(aLicense));
		io_write_newline(MapDetails);

		io_close(MapDetails);
	}

	Map.Close();
}

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	IStorage *pStorage = CreateLocalStorage();
	if(argc == 1)
	{
		dbg_msg("Usage", "%s FILE1 [ FILE2... ]", argv[0]);
		return -1;
	}
	for(int i = 1; i < argc; i++)
	{
		char aMapDetails[2048];

		size_t Len = str_length(argv[i]) + 1; // including '\0'
		if(Len > sizeof(aMapDetails))
		{
			dbg_msg("map_details", "can't process overlong filename '%s'", argv[i]);
			continue;
		}

		if(Len < sizeof(".txt") || str_comp(argv[i] + Len - sizeof(".map"), ".map") != 0)
		{
			dbg_msg("map_details", "can't process non-map file '%s'", argv[i]);
			continue;
		}

		str_copy(aMapDetails, argv[i], sizeof(aMapDetails));
		aMapDetails[Len - sizeof(".map")] = 0;
		str_append(aMapDetails, ".txt", sizeof(aMapDetails));
		dbg_msg("map_details", "processing '%s'", argv[i]);
		Process(pStorage, argv[i], aMapDetails);
	}
	return 0;
}
