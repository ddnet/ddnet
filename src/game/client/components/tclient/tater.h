#ifndef GAME_CLIENT_COMPONENTS_TATER_H
#define GAME_CLIENT_COMPONENTS_TATER_H
#include <game/client/component.h>

class CTater : public CComponent
{
	static void ConRandomTee(IConsole::IResult *pResult, void *pUserData);
	static void ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void RandomBodyColor();
	static void RandomFeetColor();
	static void RandomSkin(void *pUserData);
	static void RandomFlag(void *pUserData);

public:
	CTater();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;

	virtual void OnConsoleInit() override;
};

#endif
