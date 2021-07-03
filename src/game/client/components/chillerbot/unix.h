#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_UNIX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_UNIX_H

#include <game/client/component.h>

class CUnix : public CComponent
{
	virtual void OnConsoleInit();
	virtual void OnInit();

	void ls();

	static int listDirCallback(const char *pName, time_t Date, int IsDir, int StorageType, void *pUser);

	static void ConLs(IConsole::IResult *pResult, void *pUserData);
};

#endif
