#include <game/client/gameclient.h>

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

int CUnix::listDirCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CUnix *pSelf = (CUnix *)pUser;
	char aBuf[IO_MAX_PATH_LENGTH + 128];
	char aTimestamp[256];
	str_timestamp_ex(pInfo->m_TimeModified, aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);
	str_format(aBuf, sizeof(aBuf), "%s %s", aTimestamp, pInfo->m_pName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "unix", aBuf);
	return 0;
}

void CUnix::ls()
{
	Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, ".", listDirCallback, this);
}
