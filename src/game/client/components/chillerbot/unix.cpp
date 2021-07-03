#include "unix.h"

void CUnix::OnInit()
{
}

void CUnix::OnConsoleInit()
{
	Console()->Register("ls", "", CFGFLAG_CLIENT, ConLs, this, "List files in current directory");
}

void CUnix::ConLs(IConsole::IResult *pResult, void *pUserData)
{
	((CUnix *)pUserData)->ls();
}

int CUnix::listDirCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser)
{
	CUnix *pSelf = (CUnix *)pUser;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "binds", pName);
	return 0;
}

void CUnix::ls()
{
	Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, ".", listDirCallback, this);
}
