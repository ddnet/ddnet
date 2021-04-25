#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H

#include <game/client/component.h>
#include <game/client/lineinput.h>

#define MAX_CHAT_BUFFER_LEN 8

class CChatHelper : public CComponent
{
	class CChillerBotUX *m_pChillerBot;

	struct CCommand
	{
		const char *pName;
		const char *pParams;

		bool operator<(const CCommand &Other) const { return str_comp(pName, Other.pName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(pName, Other.pName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(pName, Other.pName) == 0; }
	};

	sorted_array<CCommand> m_Commands;

	int64 m_NextGreetClear;
	int64 m_NextMessageSend;
	int64 m_NextPingMsgClear;

	char m_aGreetName[32];
	char m_aLastPingName[32];
	char m_aLastAfkPing[2048];
	char m_aLastPingMessage[2048];
	char m_aSendBuffer[MAX_CHAT_BUFFER_LEN][2048];

	bool LineShouldHighlight(const char *pLine, const char *pName);
	bool IsGreeting(const char *pMsg);
	void DoGreet();
	void SayFormat(const char *pMsg);

	void OnChatMessage(int ClientID, int Team, const char *pMsg);

	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void OnConsoleInit();
	virtual void OnInit();

	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConSayFormat(IConsole::IResult *pResult, void *pUserData);

public:
	CChatHelper();
	void RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp);
	void Get128Name(const char *pMsg, char *pName);
	const char *GetGreetName() { return m_aGreetName; }
	const char *GetPingName() { return m_aLastPingName; }
	const char *LastAfkPingMessage() { return m_aLastAfkPing; }
	void ClearLastAfkPingMessage() { m_aLastAfkPing[0] = '\0'; }
	/*
		Function: SayBuffer
			Adds message to spam safe queue.

		Parameters:
			StayAfk - Do not deactivate afk mode.
	*/
	void SayBuffer(const char *pMsg, bool StayAfk = false);
	bool OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB);
};

#endif
