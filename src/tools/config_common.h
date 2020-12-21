#include <base/system.h>
#include <engine/storage.h>

struct ListDirectoryContext
{
	const char *pPath;
	IStorage *pStorage;
};

inline void ProcessItem(const char *pItemName, IStorage *pStorage)
{
	char aConfig[2048];

	size_t Len = (size_t)str_length(pItemName) + 1; // including '\0'
	if(Len > sizeof(aConfig))
	{
		dbg_msg("config_common", "can't process overlong filename '%s'", pItemName);
		return;
	}

	if(!str_endswith(pItemName, ".map"))
	{
		dbg_msg("config_common", "can't process non-map file '%s'", pItemName);
		return;
	}

	str_copy(aConfig, pItemName, sizeof(aConfig));
	aConfig[Len - sizeof(".map")] = 0;
	str_append(aConfig, ".cfg", sizeof(aConfig));
	dbg_msg("config_common", "processing '%s'", pItemName);
	Process(pStorage, pItemName, aConfig);
}

static int ListdirCallback(const char *pItemName, int IsDir, int StorageType, void *pUser)
{
	if(!IsDir)
	{
		ListDirectoryContext Context = *((ListDirectoryContext *)pUser);
		char aName[2048];
		str_format(aName, sizeof(aName), "%s/%s", Context.pPath, pItemName);
		ProcessItem(aName, Context.pStorage);
	}

	return 0;
}

int main(int argc, const char **argv) // NOLINT(misc-definitions-in-headers)
{
	dbg_logger_stdout();
	IStorage *pStorage = CreateLocalStorage();
	if(argc == 1)
	{
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", argv[0]);
		dbg_msg("usage", "%s DIRECTORY", argv[0]);
		return -1;
	}
	else if(argc == 2 && fs_is_dir(argv[1]))
	{
		ListDirectoryContext Context = {argv[1], pStorage};
		pStorage->ListDirectory(IStorage::TYPE_ALL, argv[1], ListdirCallback, &Context);
	}

	for(int i = 1; i < argc; i++)
	{
		ProcessItem(argv[i], pStorage);
	}
	return 0;
}
