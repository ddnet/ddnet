#include "gamecontext.h"
#include "countrymessage.h"


CCountryMessages::CCountryMessages()
{
	m_NumMessages = 0;
	m_pCountryMsgFirst = 0;
	m_pCountryMsgLast = 0;
}

void CCountryMessages::OnConsoleInit(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();
	m_pConsole = m_pGameServer->Console();
	m_pHeap = new CHeap();

	Console()->Register("set_country_msg", "is", CFGFLAG_SERVER, ConSetCountryMsg, this, "Sets a message to be displayed for players from specific countrys");
	Console()->Register("remove_country_msg", "i", CFGFLAG_SERVER, ConRemoveCountryMsg, this, "removes countrymessage");
	Console()->Register("reset_country_msgs", "", CFGFLAG_SERVER, ConResetCountryMsgs, this, "reset all countrymessages");
	Console()->Register("dump_country_msgs", "", CFGFLAG_SERVER, ConDumpCountryMsgs, this, "dump all countrymessages");
}

const char *CCountryMessages::getCountryMessage(int CountryID)
{
	CCountryMsg *pCountryMsg = m_pCountryMsgFirst;
	while(pCountryMsg)
	{
		if(pCountryMsg->m_CountryID == CountryID)
		{
			return pCountryMsg->m_aMessage;
		}
		pCountryMsg = pCountryMsg->m_pNext;
	}
	return 0;
}

void CCountryMessages::ConSetCountryMsg(IConsole::IResult *pResult, void *pUserData)
{
	CCountryMessages *pSelf = (CCountryMessages *)pUserData;
	if(pSelf->m_NumMessages == MAX_COUNTRY_MESSAGES)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of countrymessages reached");
		return;
	}

	int CountryID = pResult->GetInteger(0);

	CCountryMsg *pCountryMsg = pSelf->m_pCountryMsgFirst;
	while(pCountryMsg)
	{
		if(pCountryMsg->m_CountryID == CountryID)
		{
			str_copy(pCountryMsg->m_aMessage, pResult->GetString(1), MAX_COUNTRY_MESSAGE_LENGTH);

			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "changed countrymessage ID: %d to '%s'", pCountryMsg->m_CountryID, pCountryMsg->m_aMessage);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

			return;
		}
		pCountryMsg = pCountryMsg->m_pNext;
	}

	++pSelf->m_NumMessages;
	pCountryMsg = (CCountryMsg *)pSelf->m_pHeap->Allocate(sizeof(CCountryMsg));

	pCountryMsg->m_pNext = 0;
	pCountryMsg->m_pPrev = pSelf->m_pCountryMsgLast;
	if(pCountryMsg->m_pPrev)
		pCountryMsg->m_pPrev->m_pNext = pCountryMsg;
	pSelf->m_pCountryMsgLast = pCountryMsg;
	if(!pSelf->m_pCountryMsgFirst)
		pSelf->m_pCountryMsgFirst = pCountryMsg;

	pCountryMsg->m_CountryID = CountryID;
	str_copy(pCountryMsg->m_aMessage, pResult->GetString(1), MAX_COUNTRY_MESSAGE_LENGTH);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "added countrymessage ID: %d '%s'", pCountryMsg->m_CountryID, pCountryMsg->m_aMessage);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CCountryMessages::ConRemoveCountryMsg(IConsole::IResult *pResult, void *pUserData)
{
	CCountryMessages *pSelf = (CCountryMessages *)pUserData;

	CCountryMsg *pCountryMsg = pSelf->m_pCountryMsgFirst;
	while(pCountryMsg)
	{
		if(pCountryMsg->m_CountryID == pResult->GetInteger(0))
			break;
		pCountryMsg = pCountryMsg->m_pNext;
	}
	if(!pCountryMsg)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "countrymessage %d does not exist", pResult->GetInteger(0));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	--pSelf->m_NumMessages;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "removed countrymessage ID: %d '%s'", pCountryMsg->m_CountryID, pCountryMsg->m_aMessage);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pCountryMsgsHeap = new CHeap();
	CCountryMsg *pCountryMsgFirst = 0;
	CCountryMsg *pCountryMsgLast = 0;

	for(CCountryMsg *pSrc = pSelf->m_pCountryMsgFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pCountryMsg)
			continue;

		// copy
		CCountryMsg *pDst = (CCountryMsg *)pCountryMsgsHeap->Allocate(sizeof(CCountryMsg));
		pDst->m_pNext = 0;
		pDst->m_pPrev = pCountryMsgLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pCountryMsgLast = pDst;
		if(!pCountryMsgFirst)
			pCountryMsgFirst = pDst;

		str_copy(pDst->m_aMessage, pSrc->m_aMessage, sizeof(pDst->m_aMessage));
		pDst->m_CountryID = pSrc->m_CountryID;
	}

	// clean up
	delete pSelf->m_pHeap;
	pSelf->m_pHeap = pCountryMsgsHeap;
	pSelf->m_pCountryMsgFirst = pCountryMsgFirst;
	pSelf->m_pCountryMsgLast = pCountryMsgLast;
}

void CCountryMessages::ConResetCountryMsgs(IConsole::IResult *pResult, void *pUserData)
{
	CCountryMessages *pSelf = (CCountryMessages *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "resetted countrymessages");
	pSelf->m_pHeap->Reset();
	pSelf->m_pCountryMsgFirst = 0;
	pSelf->m_pCountryMsgLast = 0;
	pSelf->m_NumMessages = 0;
}

void CCountryMessages::ConDumpCountryMsgs(IConsole::IResult *pResult, void *pUserData)
{
	CCountryMessages *pSelf = (CCountryMessages *)pUserData;

	CCountryMsg *pCountryMsg = pSelf->m_pCountryMsgFirst;

	while (pCountryMsg)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "ID: %d %s", pCountryMsg->m_CountryID, pCountryMsg->m_aMessage);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		pCountryMsg = pCountryMsg->m_pNext;
	}
}

CCountryMessages::~CCountryMessages()
{
	delete m_pHeap;
}
