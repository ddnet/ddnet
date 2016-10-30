#include <base/system.h>

int main(int argc, const char **argv) // ignore_convention
{
	dbg_logger_stdout();
	if(argc < 1 + 2)
	{
		dbg_msg("usage", "%s STR1 STR2", argv[0] ? argv[0] : "confusables");
		return -1;
	}
	dbg_msg("conf", "not_confusable=%d", str_utf8_comp_confusable(argv[1], argv[2]));
	return 0;
}
