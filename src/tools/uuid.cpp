#include <base/logger.h>
#include <base/os.h>

#include <engine/shared/uuid_manager.h>

static constexpr const char *const TOOL_NAME = "uuid";

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc != 2)
	{
		log_error(TOOL_NAME, "Usage: %s <name>", argc > 0 ? argv[0] : TOOL_NAME);
		return -1;
	}

	const CUuid Uuid = CalculateUuid(argv[1]);
	char aUuidStr[UUID_MAXSTRSIZE];
	FormatUuid(Uuid, aUuidStr, sizeof(aUuidStr));
	log_info(TOOL_NAME, "%s", aUuidStr);
	return 0;
}
