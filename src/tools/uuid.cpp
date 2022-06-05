#include <base/logger.h>
#include <engine/config.h>
#include <engine/shared/uuid_manager.h>
int main(int argc, const char **argv)
{
	IConfigManager *pConfigManager = CreateConfigManager();
	if(!pConfigManager)
		return -1;
	pConfigManager->Reset();

	cmdline_fix(&argc, &argv);
	log_set_global_logger_default();
	if(argc != 2)
	{
		dbg_msg("usage", "uuid <NAME>");
		return -1;
	}
	CUuid Uuid = CalculateUuid(argv[1]);
	char aBuf[UUID_MAXSTRSIZE];
	FormatUuid(Uuid, aBuf, sizeof(aBuf));
	dbg_msg("uuid", "%s", aBuf);
	cmdline_free(argc, argv);
	return 0;
}
