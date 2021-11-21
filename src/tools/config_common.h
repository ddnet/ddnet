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

int main(int argc, char **argv) // NOLINT(misc-definitions-in-headers)
{
	dbg_logger_stdout();
	cmdline_init(argc, argv);

	IStorage *pStorage = CreateLocalStorage();
	if(cmdline_arg_num() == 1)
	{
		char aExecutablePath[IO_MAX_PATH_LENGTH];
		cmdline_arg_get(0, aExecutablePath, sizeof(aExecutablePath));
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", aExecutablePath);
		dbg_msg("usage", "%s DIRECTORY", aExecutablePath);
		return -1;
	}
	else if(cmdline_arg_num() == 2)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		cmdline_arg_get(1, aPath, sizeof(aPath));
		if(fs_is_dir(aPath))
		{
			ListDirectoryContext Context = {
				aPath,
				pStorage};
			pStorage->ListDirectory(IStorage::TYPE_ALL, aPath, ListdirCallback, &Context);
		}
	}

	for(int i = 1; i < cmdline_arg_num(); i++)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		cmdline_arg_get(i, aPath, sizeof(aPath));
		ProcessItem(aPath, pStorage);
	}

	cmdline_free();
	return 0;
}
