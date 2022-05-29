#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H

#include <game/client/component.h>
#include <game/client/lineinput.h>

#include "chathelper/replytoping.h"

#include <engine/shared/chillerbot/langparser.h>

#define MAX_CHAT_BUFFER_LEN 8
#define MAX_CHAT_FILTERS 8
#define MAX_CHAT_FILTER_LEN 128
class CChatHelper : public CComponent
{
	class CChillerBotUX *m_pChillerBot;

	CLangParser m_LangParser;
	struct CCommand
	{
		const char *pName;
		const char *pParams;

		bool operator<(const CCommand &Other) const { return str_comp(pName, Other.pName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(pName, Other.pName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(pName, Other.pName) == 0; }
	};

	std::vector<CCommand> m_vCommands;

	int64_t m_NextGreetClear;
	int64_t m_NextMessageSend;

	struct CLastPing
	{
		CLastPing()
		{
			m_aName[0] = '\0';
			m_aClan[0] = '\0';
			m_aMessage[0] = '\0';
			m_ReciveTime = 0;
		}
		char m_aName[32];
		char m_aClan[32];
		char m_aMessage[2048];
		int64_t m_ReciveTime;
	};

	enum
	{
		PING_QUEUE_SIZE = 16
	};

	/*
		m_aLastPings

		A stack holding the most recent 16 pings in chat.
		Index 0 will be the latest message.
		Popping of the stack will always give you the most recent message.
	*/
	CLastPing m_aLastPings[PING_QUEUE_SIZE];

	void PushPing(const char *pName, const char *pClan, const char *pMessage)
	{
		// yikers is this copying a shitton of data by value?
		for(int i = PING_QUEUE_SIZE - 1; i > 0; i--)
			m_aLastPings[i] = m_aLastPings[i - 1];
		str_copy(m_aLastPings[0].m_aName, pName, sizeof(m_aLastPings[0].m_aName));
		str_copy(m_aLastPings[0].m_aClan, pClan, sizeof(m_aLastPings[0].m_aClan));
		str_copy(m_aLastPings[0].m_aMessage, pMessage, sizeof(m_aLastPings[0].m_aMessage));
		m_aLastPings[0].m_ReciveTime = time_get();
	}
	void PopPing(char *pName, int SizeOfName, char *pClan, int SizeOfClan, char *pMessage, int SizeOfMessage)
	{
		str_copy(pName, m_aLastPings[0].m_aName, SizeOfName);
		str_copy(pClan, m_aLastPings[0].m_aClan, SizeOfClan);
		str_copy(pMessage, m_aLastPings[0].m_aMessage, SizeOfMessage);
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aName[0] = '\0';
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aClan[0] = '\0';
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aMessage[0] = '\0';
		for(int i = 0; i < PING_QUEUE_SIZE - 1; i++)
			m_aLastPings[i] = m_aLastPings[i + 1];
	}

	char m_aGreetName[32];
	char m_aLastAfkPing[2048];
	char m_aSendBuffer[MAX_CHAT_BUFFER_LEN][2048];
	char m_aaChatFilter[MAX_CHAT_FILTERS][MAX_CHAT_FILTER_LEN];

	void DoGreet();
	void SayFormat(const char *pMsg);
	void AddChatFilter(const char *pFilter);
	void ListChatFilter();
	enum
	{
		SPAM_NONE,
		SPAM_OTHER,
		SPAM_INSULT
	};
	int IsSpam(int ClientID, int Team, const char *pMsg);

	void OnChatMessage(int ClientID, int Team, const char *pMsg);

	virtual void OnRender() override;
	virtual void OnMessage(int MsgType, void *pRawMsg) override;
	virtual void OnConsoleInit() override;
	virtual void OnInit() override;

	static void ConReplyToLastPing(IConsole::IResult *pResult, void *pUserData);
	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConSayFormat(IConsole::IResult *pResult, void *pUserData);
	static void ConAddChatFilter(IConsole::IResult *pResult, void *pUserData);
	static void ConListChatFilter(IConsole::IResult *pResult, void *pUserData);
	static void ConDeleteChatFilter(IConsole::IResult *pResult, void *pUserData);

public:
	CChatHelper();
	virtual int Sizeof() const override { return sizeof(*this); }
	bool LineShouldHighlight(const char *pLine, const char *pName);
	void RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp);
	int Get128Name(const char *pMsg, char *pName);
	const char *GetGreetName() { return m_aGreetName; }
	const char *LastAfkPingMessage() { return m_aLastAfkPing; }
	void ClearLastAfkPingMessage() { m_aLastAfkPing[0] = '\0'; }
	bool HowToJoinClan(const char *pClan, char *pResponse, int SizeOfResponse);
	CLangParser &LangParser() { return m_LangParser; }
	/*
		Function: SayBuffer
			Adds message to spam safe queue.

		Parameters:
			StayAfk - Do not deactivate afk mode.
	*/
	void SayBuffer(const char *pMsg, bool StayAfk = false);
	bool FilterChat(int ClientID, int Team, const char *pLine);
	bool OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB);
};

#endif
