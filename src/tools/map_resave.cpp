/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>

int main(int argc, char **argv)
{
	dbg_logger_stdout();
	cmdline_init(argc, argv);

	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC);
	if(!pStorage)
		return -1;
	if(cmdline_arg_num() != 3)
	{
		dbg_msg("map_resave", "usage: map_resave existing.map newname.map");
		return -1;
	}

	char aInputPath[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(1, aInputPath, sizeof(aInputPath));
	CDataFileReader Reader;
	if(!Reader.Open(pStorage, aInputPath, IStorage::TYPE_ABSOLUTE))
	{
		dbg_msg("map_resave", "failed to open input map '%s'", aInputPath);
		return -1;
	}

	char aOutputPath[IO_MAX_PATH_LENGTH];
	cmdline_arg_get(2, aOutputPath, sizeof(aOutputPath));
	CDataFileWriter Writer;
	if(!Writer.Open(pStorage, aOutputPath))
	{
		dbg_msg("map_resave", "failed to open output map '%s'", aOutputPath);
		return -1;
	}

	// add all items
	for(int Index = 0; Index < Reader.NumItems(); Index++)
	{
		int Type, ID;
		void *pPtr = Reader.GetItem(Index, &Type, &ID);
		int Size = Reader.GetItemSize(Index);

		// filter ITEMTYPE_EX items, they will be automatically added again
		if(Type == ITEMTYPE_EX)
			continue;

		Writer.AddItem(Type, ID, Size, pPtr);
	}

	// add all data
	for(int Index = 0; Index < Reader.NumData(); Index++)
	{
		void *pPtr = Reader.GetData(Index);
		int Size = Reader.GetDataSize(Index);
		Writer.AddData(Size, pPtr);
	}

	Reader.Close();
	Writer.Finish();

	cmdline_free();
	return 0;
}
