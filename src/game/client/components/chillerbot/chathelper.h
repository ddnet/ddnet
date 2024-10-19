#ifndef GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H
#define GAME_CLIENT_COMPONENTS_CHILLERBOT_CHATHELPER_H

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>

#include <engine/shared/chillerbot/langparser.h>

#define MAX_CHAT_BUFFER_LEN 8
#define MAX_CHAT_FILTERS 8
#define MAX_CHAT_FILTER_LEN 128
class CChatHelper : public CComponent
{
	class CChillerBotUX *m_pChillerBot;

	CLangParser m_LangParser;
	class CCommand
	{
	public:
		const char *m_pName;
		const char *m_pParams;
		char m_aParamsSlim[16];
		/*
			Variable: m_ROffset

			The index of the parameter of type r
			if no parameter is r it is -1
		*/
		int m_ROffset;
		/*
			Variable: m_OptOffset

			The index of the parameter of first
			optional parameter (indicated by ?)
			if no parameter is optional it is -1
		*/
		int m_OptOffset;

		bool operator<(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(m_pName, Other.m_pName) == 0; }

		CCommand(const char *pName, const char *pParams) :
			m_pName(pName), m_pParams(pParams)
		{
			m_ROffset = -1;
			m_OptOffset = -1;
			bool IsBrace = false;
			int k = 0;
			for(int i = 0; pParams[i]; i++)
			{
				if(IsBrace)
				{
					if(pParams[i] == ']')
						IsBrace = false;
					continue;
				}
				if(pParams[i] == '[')
				{
					IsBrace = true;
					continue;
				}
				if(pParams[i] == '?' && m_OptOffset == -1)
				{
					m_OptOffset = k;
					continue;
				}
				if(pParams[i] == 'r' && m_ROffset == -1)
					m_ROffset = k;
				m_aParamsSlim[k++] = pParams[i];
			}
		}
	};

	std::vector<CCommand> m_vCommands;

	int64_t m_NextGreetClear;
	int64_t m_NextMessageSend;

	struct CLastPing
	{
		void Reset()
		{
			m_aName[0] = '\0';
			m_aClan[0] = '\0';
			m_aMessage[0] = '\0';
			m_ReciveTime = 0;
			m_Team = 0;
		}

		CLastPing()
		{
			Reset();
		}

		char m_aName[32];
		char m_aClan[32];
		char m_aMessage[2048];
		int m_Team;
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

	void PushPing(const char *pName, const char *pClan, const char *pMessage, int Team)
	{
		// yikers is this copying a shitton of data by value?
		for(int i = PING_QUEUE_SIZE - 1; i > 0; i--)
			m_aLastPings[i] = m_aLastPings[i - 1];
		str_copy(m_aLastPings[0].m_aName, pName, sizeof(m_aLastPings[0].m_aName));
		str_copy(m_aLastPings[0].m_aClan, pClan, sizeof(m_aLastPings[0].m_aClan));
		str_copy(m_aLastPings[0].m_aMessage, pMessage, sizeof(m_aLastPings[0].m_aMessage));
		m_aLastPings[0].m_Team = Team;
		m_aLastPings[0].m_ReciveTime = time_get();
	}
	void PopPing(char *pName, int SizeOfName, char *pClan, int SizeOfClan, char *pMessage, int SizeOfMessage, int *pTeam)
	{
		str_copy(pName, m_aLastPings[0].m_aName, SizeOfName);
		str_copy(pClan, m_aLastPings[0].m_aClan, SizeOfClan);
		str_copy(pMessage, m_aLastPings[0].m_aMessage, SizeOfMessage);
		*pTeam = m_aLastPings[0].m_Team;
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aName[0] = '\0';
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aClan[0] = '\0';
		m_aLastPings[PING_QUEUE_SIZE - 1].m_aMessage[0] = '\0';
		for(int i = 0; i < PING_QUEUE_SIZE - 1; i++)
			m_aLastPings[i] = m_aLastPings[i + 1];
	}

	char m_aaChatFilter[MAX_CHAT_FILTERS][MAX_CHAT_FILTER_LEN];
	char m_aGreetName[32];
	char m_aLastAfkPing[2048];

public:
	enum
	{
		BUFFER_CHAT_ALL,
		BUFFER_CHAT_TEAM,
		NUM_BUFFERS,
	};

private:
	char m_aaaSendBuffer[NUM_BUFFERS][MAX_CHAT_BUFFER_LEN][2048];

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
	int IsSpam(int ClientId, int Team, const char *pMsg);

	void OnChatMessage(int ClientId, int Team, const char *pMsg);

	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnConsoleInit() override;
	void OnInit() override;

	static void ConReplyToLastPing(IConsole::IResult *pResult, void *pUserData);
	static void ConSayHi(IConsole::IResult *pResult, void *pUserData);
	static void ConSayFormat(IConsole::IResult *pResult, void *pUserData);
	static void ConAddChatFilter(IConsole::IResult *pResult, void *pUserData);
	static void ConListChatFilter(IConsole::IResult *pResult, void *pUserData);
	static void ConDeleteChatFilter(IConsole::IResult *pResult, void *pUserData);

public:
	CChatHelper();
	int Sizeof() const override { return sizeof(*this); }
	bool LineShouldHighlight(const char *pLine, const char *pName);
	void RegisterCommand(const char *pName, const char *pParams, int Flags, const char *pHelp);
	int Get128Name(const char *pMsg, char *pName);
	const char *GetGreetName() { return m_aGreetName; }
	const char *LastAfkPingMessage() { return m_aLastAfkPing; }
	void ClearLastAfkPingMessage() { m_aLastAfkPing[0] = '\0'; }
	bool HowToJoinClan(const char *pClan, char *pResponse, int SizeOfResponse);
	CLangParser &LangParser() { return m_LangParser; }

	/*
		Function: ChatCommandGetROffset

		Parameters:
			pCmd - name of the command without arguments

		Returns:
			Which parameter is a r parameter.
			-1 if none is r.
			r means it is the last parameter that can not be
			split into words.

			the argument format is defined in chatcommands.h
			r is one of the following types:
				s - string (split by spaces)
				i - integer (split by spaces)
				r - string (consume till end)

		Example:
			If the argument pattern of a chat command is
			"ssr" we would return 2
	*/
	int ChatCommandGetROffset(const char *pCmd);
	/*
		Function: SayBuffer
			Adds message to spam safe queue.

		Parameters:
			Team - has to be one of the buffer enums BUFFER_CHAT_ALL or BUFFER_CHAT_TEAM
			StayAfk - Do not deactivate afk mode.
	*/
	void SayBuffer(const char *pMsg, int Team, bool StayAfk = false);
	bool FilterChat(int ClientId, int Team, const char *pLine);
	bool OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB);
};

#endif
