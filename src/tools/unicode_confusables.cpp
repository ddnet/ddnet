#include <base/logger.h>
#include <base/os.h>
#include <base/str.h>

static constexpr const char *const TOOL_NAME = "unicode_confusables";

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(argc < 1 + 2)
	{
		log_error(TOOL_NAME, "Usage: %s <string 1> <string 2>", argc > 0 ? argv[0] : TOOL_NAME);
		return -1;
	}

	log_info(TOOL_NAME, "not_confusable=%d", str_utf8_comp_confusable(argv[1], argv[2]));
	return 0;
}
