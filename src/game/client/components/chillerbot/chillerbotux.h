#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H

#include <game/client/component.h>

class CChillerBotUX : public CComponent
{
	bool m_IsNearFinish;
	char m_aGreetName[32];
	int64 m_LastGreet;
	int64 m_AfkTill;
	int m_AfkActivity;
	char m_aLastAfkPing[2048];

	void OnChatMessage(int ClientID, int Team, const char *pMsg);
	bool LineShouldHighlight(const char *pLine, const char *pName);
	bool IsGreeting(const char *pMsg);
	void Get128Name(const char *pMsg, char *pName);
	void DoGreet();
	void GoAfk(int Minutes);
	void FinishRenameTick();

	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnConsoleInit();
	virtual void OnInit();
	virtual bool OnMouseMove(float x, float y);
	virtual bool OnInput(IInput::CEvent e);

	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConAfk(IConsole::IResult *pResult, void *pUserData);

public:
	void ReturnFromAfk();
};

#endif
