#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H

#include <game/client/component.h>

class CChatHelper : public CComponent
{
	class CChillerBotUX *m_pChillerBot;
	int64 m_LastGreet;

	char m_aGreetName[32];
	char m_aLastPingName[32];
	char m_aLastAfkPing[2048];

	bool LineShouldHighlight(const char *pLine, const char *pName);
	bool IsGreeting(const char *pMsg);
	void DoGreet();
	void SayFormat(const char *pMsg);
	void Get128Name(const char *pMsg, char *pName);

	void OnChatMessage(int ClientID, int Team, const char *pMsg);

	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConSayFormat(IConsole::IResult *pResult, void *pUserData);

public:
	const char *GetGreetName() { return m_aGreetName; }
	const char *GetPingName() { return m_aLastPingName; }
	const char *LastAfkPingMessage() { return m_aLastAfkPing; }
	void ClearLastAfkPingMessage() { m_aLastAfkPing[0] = '\0'; }
};

#endif
