/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/logger.h>
#include <base/system.h>

#include <engine/shared/datafile.h>
#include <engine/storage.h>

static const char *TOOL_NAME = "map_resave";

static int ResaveMap(const char *pSourceMap, const char *pDestinationMap, IStorage *pStorage)
{
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, pSourceMap, IStorage::TYPE_ABSOLUTE))
	{
		log_error(TOOL_NAME, "Failed to open source map '%s' for reading", pSourceMap);
		return -1;
	}

	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, pDestinationMap))
	{
		log_error(TOOL_NAME, "Failed to open destination map '%s' for writing", pDestinationMap);
		Reader.Close();
		return -1;
	}

	// add all items
	for(int Index = 0; Index < Reader.NumItems(); Index++)
	{
		int Type, Id;
		CUuid Uuid;
		const void *pPtr = Reader.GetItem(Index, &Type, &Id, &Uuid);

		// Filter ITEMTYPE_EX items, they will be automatically added again.
		if(Type == ITEMTYPE_EX)
		{
			continue;
		}

		int Size = Reader.GetItemSize(Index);
		Writer.AddItem(Type, Id, Size, pPtr, &Uuid);
	}

	// add all data
	for(int Index = 0; Index < Reader.NumData(); Index++)
	{
		const void *pPtr = Reader.GetData(Index);
		int Size = Reader.GetDataSize(Index);
		Writer.AddData(Size, pPtr);
	}

	Reader.Close();
	Writer.Finish();
	log_info(TOOL_NAME, "Resaved '%s' to '%s'", pSourceMap, pDestinationMap);
	return 0;
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc != 3)
	{
		log_error(TOOL_NAME, "Usage: %s <source map> <destination map>", TOOL_NAME);
		return -1;
	}

	IStorage *pStorage = CreateStorage(IStorage::EInitializationType::BASIC, argc, argv);
	if(!pStorage)
	{
		log_error(TOOL_NAME, "Error creating basic storage");
		return -1;
	}

	return ResaveMap(argv[1], argv[2], pStorage);
}
