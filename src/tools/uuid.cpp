#include <engine/shared/uuid_manager.h>
int main(int argc, const char **argv)
{
	cmdline_fix(&argc, &argv);
	dbg_logger_stdout();
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
