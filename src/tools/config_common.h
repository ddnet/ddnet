#include <base/fs.h>
#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

#include <engine/storage.h>

struct SListDirectoryContext
{
	const char *m_pPath;
	IStorage *m_pStorage;
};

inline void ProcessItem(const char *pItemName, IStorage *pStorage)
{
	char aConfig[2048];

	size_t Len = (size_t)str_length(pItemName) + 1; // including '\0'
	if(Len > sizeof(aConfig))
	{
		log_error(TOOL_NAME, "can't process overlong filename '%s'", pItemName);
		return;
	}

	if(!str_endswith(pItemName, ".map"))
	{
		log_error(TOOL_NAME, "can't process non-map file '%s'", pItemName);
		return;
	}

	str_copy(aConfig, pItemName);
	aConfig[Len - sizeof(".map")] = 0;
	str_append(aConfig, ".cfg");
	log_info(TOOL_NAME, "processing '%s'", pItemName);
	Process(pStorage, pItemName, aConfig);
}

static int ListdirCallback(const char *pItemName, int IsDir, int StorageType, void *pUser)
{
	if(!IsDir)
	{
		SListDirectoryContext Context = *((SListDirectoryContext *)pUser);
		char aName[2048];
		str_format(aName, sizeof(aName), "%s/%s", Context.m_pPath, pItemName);
		ProcessItem(aName, Context.m_pStorage);
	}

	return 0;
}

int main(int argc, const char **argv) // NOLINT(misc-definitions-in-headers)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	std::unique_ptr<IStorage> pStorage = CreateLocalStorage();
	if(!pStorage)
	{
		log_error(TOOL_NAME, "Error creating local storage");
		return -1;
	}

	if(argc == 1)
	{
		log_error(TOOL_NAME, "Usage: %s FILE1 [ FILE2... ]", argc > 0 ? argv[0] : TOOL_NAME);
		log_error(TOOL_NAME, "Usage: %s DIRECTORY", argc > 0 ? argv[0] : TOOL_NAME);
		return -1;
	}
	else if(argc == 2 && fs_is_dir(argv[1]))
	{
		SListDirectoryContext Context = {argv[1], pStorage.get()};
		pStorage->ListDirectory(IStorage::TYPE_ALL, argv[1], ListdirCallback, &Context);
	}

	for(int i = 1; i < argc; i++)
	{
		ProcessItem(argv[i], pStorage.get());
	}
	return 0;
}
