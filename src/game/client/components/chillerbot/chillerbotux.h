#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHILLERBOTUX_H

#include <game/client/component.h>

class CChillerBotUX : public CComponent
{
	bool m_IsNearFinish;
	char m_aGreetName[32];
	int64 m_LastGreet;

	void OnChatMessage(int ClientID, int Team, const char *pMsg);
	bool LineShouldHighlight(const char *pLine, const char *pName);
	bool IsGreeting(const char *pMsg);
	void Get128Name(const char *pMsg, char *pName);
	void DoGreet();
	void FinishRenameTick();

	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
};

#endif
