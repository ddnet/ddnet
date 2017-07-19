#include <base/system.h>
#include <engine/storage.h>

int main(int argc, const char **argv)
{
	dbg_logger_stdout();
	IStorage *pStorage = CreateLocalStorage();
	if(argc == 1)
	{
		dbg_msg("usage", "%s FILE1 [ FILE2... ]", argv[0]);
		return -1;
	}
	for(int i = 1; i < argc; i++)
	{
		char aConfig[2048];

		size_t Len = str_length(argv[i]) + 1; // including '\0'
		if(Len > sizeof(aConfig))
		{
			dbg_msg("config_common", "can't process overlong filename '%s'", argv[i]);
			continue;
		}

		if(Len < sizeof(".map") || str_comp(argv[i] + Len - sizeof(".map"), ".map") != 0)
		{
			dbg_msg("config_common", "can't process non-map file '%s'", argv[i]);
			continue;
		}

		str_copy(aConfig, argv[i], sizeof(aConfig));
		aConfig[Len - sizeof(".map")] = 0;
		str_append(aConfig, ".cfg", sizeof(aConfig));
		dbg_msg("config_common", "processing '%s'", argv[i]);
		Process(pStorage, argv[i], aConfig);
	}
	return 0;
}
