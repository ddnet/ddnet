#include <engine/shared/uuid_manager.h>
int main(int argc, char **argv)
{
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
}
