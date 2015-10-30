#ifndef GAME_SERVER_COUNTRY_MESSAGE_H
#define GAME_SERVER_COUNTRY_MESSAGE_H

#include <engine/shared/config.h>
#include <engine/shared/memheap.h>
#include <engine/console.h>

enum
{
	MAX_COUNTRY_MESSAGE_LENGTH=256,
	MAX_COUNTRY_MESSAGES=512
};

struct CCountryMsg
{
	CCountryMsg *m_pNext;
	CCountryMsg *m_pPrev;
	char m_aMessage[MAX_COUNTRY_MESSAGE_LENGTH];
	int m_CountryID;
};

class CCountryMessages
{
public:
	CCountryMessages();
	~CCountryMessages();
	void OnConsoleInit(class CGameContext *pGameServer);

	const char *getCountryMessage(int CountryID);

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }

private:
	class CGameContext *m_pGameServer;
	class IServer *m_pServer;
	class IConsole *m_pConsole;

	static void ConSetCountryMsg(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveCountryMsg(IConsole::IResult *pResult, void *pUserData);
	static void ConResetCountryMsgs(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpCountryMsgs(IConsole::IResult *pResult, void *pUserData);

	CHeap *m_pHeap;
	int m_NumMessages;
	CCountryMsg *m_pCountryMsgFirst;
	CCountryMsg *m_pCountryMsgLast;
};

#endif
