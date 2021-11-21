#include <base/system.h>

int main(int argc, char **argv)
{
	dbg_logger_stdout();
	cmdline_init(argc, argv);

	if(cmdline_arg_num() < 1 + 2)
	{
		char aExecutablePath[IO_MAX_PATH_LENGTH];
		cmdline_arg_get(0, aExecutablePath, sizeof(aExecutablePath));
		dbg_msg("usage", "%s STR1 STR2", aExecutablePath);
		return -1;
	}

	char aStr1[128];
	char aStr2[128];
	cmdline_arg_get(1, aStr1, sizeof(aStr1));
	cmdline_arg_get(2, aStr2, sizeof(aStr2));
	dbg_msg("conf", "not_confusable=%d", str_utf8_comp_confusable(aStr1, aStr2));

	cmdline_free();
	return 0;
}
