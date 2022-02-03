#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_UNIX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_UNIX_H

#include <game/client/component.h>

class CUnix : public CComponent
{
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;

	void ls();

	static int listDirCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);

	static void ConLs(IConsole::IResult *pResult, void *pUserData);

public:
	virtual int Sizeof() const override { return sizeof(*this); }
};

#endif
