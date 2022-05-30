// ChillerDragon 2020 - chillerbot ux

#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>

#include <cinttypes>
#include <engine/shared/protocol.h>

#include <game/client/components/chat.h>

#include "chathelper.h"

CChatHelper::CChatHelper()
{
	mem_zero(m_aaChatFilter, sizeof(m_aaChatFilter));
#define CHILLERBOT_CHAT_CMD(name, params, flags, callback, userdata, help) RegisterCommand(name, params, flags, help);
#include "chatcommands.h"
	std::sort(m_vCommands.begin(), m_vCommands.end());
}

void CChatHelper::RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp)
{
	m_vCommands.push_back(CCommand{pName, pParams});
}

void CChatHelper::OnInit()
{
	m_pChillerBot = &m_pClient->m_ChillerBotUX;

	m_aGreetName[0] = '\0';
	m_NextGreetClear = 0;
	m_NextMessageSend = 0;
	m_aLastAfkPing[0] = '\0';
	mem_zero(m_aSendBuffer, sizeof(m_aSendBuffer));
}

void CChatHelper::OnRender()
{
	int64_t time_now = time_get();
	if(time_now % 10 == 0)
	{
		for(auto &Ping : m_aLastPings)
		{
			if(!Ping.m_ReciveTime)
				continue;
			if(Ping.m_ReciveTime < time_now - time_freq() * 60)
			{
				Ping.m_aName[0] = '\0';
				Ping.m_aClan[0] = '\0';
				Ping.m_aMessage[0] = '\0';
				Ping.m_ReciveTime = 0;
			}
		}
		if(m_NextGreetClear < time_now)
		{
			m_NextGreetClear = 0;
			m_aGreetName[0] = '\0';
		}
		if(m_NextMessageSend < time_now)
		{
			if(m_aSendBuffer[0][0])
			{
				m_pClient->m_Chat.Say(0, m_aSendBuffer[0]);
				for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
					str_copy(m_aSendBuffer[i], m_aSendBuffer[i + 1], sizeof(m_aSendBuffer[i]));
				m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1][0] = '\0';
				m_NextMessageSend = time_now + time_freq() * 5;
			}
		}
	}
}

void CChatHelper::SayBuffer(const char *pMsg, bool StayAfk)
{
	if(StayAfk)
		m_pClient->m_ChillerBotUX.m_IgnoreChatAfk++;
	// append at end
	for(auto &buf : m_aSendBuffer)
	{
		if(buf[0])
			continue;
		str_copy(buf, pMsg, sizeof(buf));
		return;
	}
	// full -> shift buffer and overwrite oldest element (index 0)
	for(int i = 0; i < MAX_CHAT_BUFFER_LEN - 1; i++)
		str_copy(m_aSendBuffer[i], m_aSendBuffer[i + 1], sizeof(m_aSendBuffer[i]));
	str_copy(m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1], pMsg, sizeof(m_aSendBuffer[MAX_CHAT_BUFFER_LEN - 1]));
}

void CChatHelper::OnConsoleInit()
{
	Console()->Register("reply_to_last_ping", "", CFGFLAG_CLIENT, ConReplyToLastPing, this, "Respond to the last ping in chat");
	Console()->Register("say_hi", "", CFGFLAG_CLIENT, ConSayHi, this, "Respond to the last greeting in chat");
	Console()->Register("say_format", "s[message]", CFGFLAG_CLIENT, ConSayFormat, this, "send message replacing %n with last ping name");
	Console()->Register("chat_filter_add", "s[text]", CFGFLAG_CLIENT, ConAddChatFilter, this, "Add string to chat filter. All messages containing that string will be ignored.");
	Console()->Register("chat_filter_list", "", CFGFLAG_CLIENT, ConListChatFilter, this, "list all active filters");
	Console()->Register("chat_filter_delete", "i[index]", CFGFLAG_CLIENT, ConDeleteChatFilter, this, "");
}

void CChatHelper::ConReplyToLastPing(IConsole::IResult *pResult, void *pUserData)
{
	CChatHelper *pSelf = (CChatHelper *)pUserData;
	char aResponse[1024];
	char aName[32];
	char aClan[32];
	char aMessage[2048];
	aMessage[0] = 'x';
	// pop message stack until reached the end
	// abort as soon as a response is found
	// this makes sure we always respond to something
	// given there is any respondable message is still in the stack
	while(aMessage[0])
	{
		pSelf->PopPing(aName, sizeof(aName), aClan, sizeof(aClan), aMessage, sizeof(aMessage));
		CReplyToPing ReplyToPing = CReplyToPing(pSelf, aName, aClan, aMessage, aResponse, sizeof(aResponse));
		if(ReplyToPing.Reply())
		{
			if(aResponse[0])
			{
				pSelf->m_pClient->m_Chat.Say(0, aResponse);
				break;
			}
		}
	}
}

void CChatHelper::ConSayHi(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->DoGreet();
}

void CChatHelper::ConSayFormat(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->SayFormat(pResult->GetString(0));
}

void CChatHelper::ConAddChatFilter(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->AddChatFilter(pResult->GetString(0));
}

void CChatHelper::ConListChatFilter(IConsole::IResult *pResult, void *pUserData)
{
	((CChatHelper *)pUserData)->ListChatFilter();
}

void CChatHelper::ConDeleteChatFilter(IConsole::IResult *pResult, void *pUserData)
{
}

bool CChatHelper::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_find_nocase(pLine, pName);

	if(pHL)
	{
		int Length = str_length(pName);

		if((pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}

	return false;
}

void CChatHelper::SayFormat(const char *pMsg)
{
	char aBuf[1028] = {0};
	long unsigned int i = 0;
	long unsigned int buf_i = 0;
	for(i = 0; pMsg[i] && i < sizeof(aBuf); i++)
	{
		if(pMsg[i] == '%' && pMsg[maximum((int)i - 1, 0)] != '\\')
		{
			if(pMsg[i + 1] == 'n')
			{
				str_append(aBuf, m_aLastPings[0].m_aName, sizeof(aBuf));
				buf_i += str_length(m_aLastPings[0].m_aName);
				i++;
				continue;
			}
			else if(pMsg[i + 1] == 'g')
			{
				str_append(aBuf, m_aGreetName, sizeof(aBuf));
				buf_i += str_length(m_aGreetName);
				i++;
				continue;
			}
		}
		aBuf[buf_i++] = pMsg[i];
	}
	aBuf[minimum((unsigned long)sizeof(aBuf) - 1, buf_i)] = '\0';
	m_pClient->m_Chat.Say(0, aBuf);
}

bool CChatHelper::HowToJoinClan(const char *pClan, char *pResponse, int SizeOfResponse)
{
	if(!pResponse)
		return false;
	pResponse[0] = '\0';
	if(!str_comp(pClan, "Chilli.*"))
		str_copy(pResponse, "Chilli.* is a fun clan everybody that uses the skin greensward can join", SizeOfResponse);
	else if(!str_comp(pClan, "|*KoG*|"))
		str_copy(pResponse, "If you want to join the gores clan |*KoG*| visit their website kog.tw", SizeOfResponse);
	else if(!str_comp(pClan, "χron"))
		str_copy(pResponse, "If you want to join the vanilla clan χron visit their website aeon.teewars.com", SizeOfResponse);
	else if(!str_comp(pClan, "ÆON"))
		str_copy(pResponse, "If you want to join the vanilla clan ÆON visit their website aeon.teewars.com", SizeOfResponse);
	else
		return false;
	return true;
}

void CChatHelper::DoGreet()
{
	if(m_aGreetName[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "hi %s", m_aGreetName);
		m_pClient->m_Chat.Say(0, aBuf);
		return;
	}
	m_pClient->m_Chat.Say(0, "hi");
}

int CChatHelper::Get128Name(const char *pMsg, char *pName)
{
	int i = 0;
	for(i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':' && pMsg[i + 1] == ' ')
		{
			str_copy(pName, pMsg, i + 1);
			return i;
		}
	}
	str_copy(pName, " ", 2);
	return -1;
}

void CChatHelper::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
	if(ClientID < 0 || ClientID > MAX_CLIENTS)
		return;
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		Highlighted = true;
	if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		Highlighted = true;
	if(Team == 3) // whisper recv
		Highlighted = true;
	if(!Highlighted)
		return;
	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return;
	if(m_LangParser.IsGreeting(pMsg))
	{
		str_copy(m_aGreetName, aName, sizeof(m_aGreetName));
		m_NextGreetClear = time_get() + time_freq() * 10;
	}
	// could iterate over ping history and also ignore older duplicates
	// ignore duplicated messages
	if(!str_comp(m_aLastPings[0].m_aMessage, pMsg))
		return;
	char aBuf[2048];
	PushPing(aName, m_pClient->m_aClients[ClientID].m_aClan, pMsg);
	int64_t AfkTill = m_pChillerBot->GetAfkTime();
	if(m_pChillerBot->IsAfk())
	{
		char aNote[128];
		str_format(aBuf, sizeof(aBuf), "%s: I am currently afk.", aName);
		if(AfkTill > time_get() + time_freq() * 61)
			str_format(aBuf, sizeof(aBuf), "%s: I am currently afk. Estimated return in %" PRId64 " minutes.", aName, (AfkTill - time_get()) / time_freq() / 60);
		else if(AfkTill > time_get() + time_freq() * 10)
			str_format(aBuf, sizeof(aBuf), "%s: I am currently afk. Estimated return in %" PRId64 " seconds.", aName, (AfkTill - time_get()) / time_freq());
		if(m_pChillerBot->GetAfkMessage()[0])
		{
			str_format(aNote, sizeof(aNote), " (%s)", m_pChillerBot->GetAfkMessage());
			str_append(aBuf, aNote, sizeof(aBuf));
		}
		if(aBuf[0] == '/' || aBuf[0] == '.' || aBuf[0] == '!')
		{
			char aEscape[256];
			str_format(aEscape, sizeof(aEscape), ".%s", aBuf);
			SayBuffer(aEscape, true);
		}
		else
		{
			SayBuffer(aBuf, true);
		}
		str_format(m_aLastAfkPing, sizeof(m_aLastAfkPing), "%s: %s", m_pClient->m_aClients[ClientID].m_aName, pMsg);
		m_pChillerBot->SetComponentNoteLong("afk", m_aLastAfkPing);
		return;
	}
	if(g_Config.m_ClShowLastPing)
	{
		str_format(aBuf, sizeof(aBuf), "%s: %s", m_pClient->m_aClients[ClientID].m_aName, pMsg);
		m_pChillerBot->SetComponentNoteLong("last ping", aBuf);
	}
	if(g_Config.m_ClTabbedOutMsg)
	{
		IEngineGraphics *pGraphics = ((IEngineGraphics *)Kernel()->RequestInterface<IEngineGraphics>());
		if(pGraphics && !pGraphics->WindowActive() && Graphics())
		{
			str_format(aBuf, sizeof(aBuf), "%s: I am currently tabbed out", aName);
			SayBuffer(aBuf, true);
			return;
		}
	}
	if(g_Config.m_ClAutoReply)
		SayFormat(g_Config.m_ClAutoReplyMsg);
}

void CChatHelper::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		OnChatMessage(pMsg->m_ClientID, pMsg->m_Team, pMsg->m_pMessage);
	}
}

void CChatHelper::ListChatFilter()
{
	for(int i = 0; i < MAX_CHAT_FILTERS; i++)
	{
		if(m_aaChatFilter[i][0] == '\0')
			continue;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%d. '%s'", i, m_aaChatFilter[i]);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
}

void CChatHelper::AddChatFilter(const char *pFilter)
{
	for(auto &aChatFilter : m_aaChatFilter)
	{
		if(aChatFilter[0] == '\0')
		{
			str_copy(aChatFilter, pFilter, sizeof(aChatFilter));
			return;
		}
	}
}

int CChatHelper::IsSpam(int ClientID, int Team, const char *pMsg)
{
	if(!g_Config.m_ClChatSpamFilter)
		return SPAM_NONE;
	int MsgLen = str_length(pMsg);
	int NameLen = 0;
	const char *pName = m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName;
	bool Highlighted = false;
	if(LineShouldHighlight(pMsg, pName))
	{
		Highlighted = true;
		NameLen = str_length(pName);
	}
	else if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(pMsg, pDummyName))
	{
		Highlighted = true;
		NameLen = str_length(pDummyName);
	}
	if(Team == 3) // whisper recv
		Highlighted = true;
	if(g_Config.m_ClChatSpamFilterInsults && m_LangParser.IsInsult(pMsg))
		return SPAM_INSULT;
	if(!Highlighted)
		return SPAM_NONE;
	char aName[64];
	str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
	if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
	{
		Get128Name(pMsg, aName);
		MsgLen -= str_length(aName) + 2;
		// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
	}
	// ignore own and dummys messages
	if(!str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName))
		return SPAM_NONE;
	if(Client()->DummyConnected() && !str_comp(aName, m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName))
		return SPAM_NONE;

	// ping without further context
	if(MsgLen < NameLen + 2)
		return SPAM_OTHER;
	else if(m_LangParser.IsAskToAsk(pMsg))
		return SPAM_OTHER;
	else if(!str_comp(aName, "nameless tee") || !str_comp(aName, "brainless tee") || str_find(aName, ")nameless tee") || str_find(aName, ")brainless te"))
		return SPAM_OTHER;
	else if(str_find(pMsg, "bro, check out this client: krxclient.pages.dev"))
		return SPAM_OTHER;
	else if((str_find_nocase(pMsg, "help") || str_find_nocase(pMsg, "hilfe")) && MsgLen < NameLen + 16)
		return SPAM_OTHER;
	else if((str_find_nocase(pMsg, "give") || str_find_nocase(pMsg, "need") || str_find_nocase(pMsg, "want") || str_find_nocase(pMsg, "please") || str_find_nocase(pMsg, "pls") || str_find_nocase(pMsg, "plz")) &&
		(str_find_nocase(pMsg, "rcon") || str_find_nocase(pMsg, "password") || str_find_nocase(pMsg, "admin") || str_find_nocase(pMsg, "helper") || str_find_nocase(pMsg, "mod") || str_find_nocase(pMsg, "money") || str_find_nocase(pMsg, "moni") || str_find_nocase(pMsg, "flag")))
	{
		// "I give you money"
		// "Do you want me to give you the flag"
		// "I give money back chillerdragon"
		// "ChillerDragon: Can you please post a tutorial on how to download the DDNet++ mod"
		if((str_find_nocase(pMsg, " i ") && str_find_nocase(pMsg, "you")) || str_find_nocase(pMsg, "give you") || m_LangParser.StrFindOrder(pMsg, 2, "give", "back") ||
			(str_find_nocase(pMsg, "mod") && (str_find_nocase(pMsg, "tutorial") || str_find_nocase(pMsg, "code") || str_find_nocase(pMsg, "download"))))
			return SPAM_NONE;
		else
			return SPAM_OTHER;
	}
	return SPAM_NONE;
}

bool CChatHelper::FilterChat(int ClientID, int Team, const char *pLine)
{
	for(auto &aChatFilter : m_aaChatFilter)
	{
		if(aChatFilter[0] == '\0')
			continue;
		if(str_find(pLine, aChatFilter))
			return true;
	}
	int Spam = IsSpam(ClientID, Team, pLine);
	if(Spam)
	{
		if(g_Config.m_ClChatSpamFilter != 2)
			return true;
		if(Spam >= SPAM_INSULT)
			return true;

		// if not afk auto respond to pings
		if(!m_pChillerBot->IsAfk())
		{
			char aName[64];
			str_copy(aName, m_pClient->m_aClients[ClientID].m_aName, sizeof(aName));
			if(ClientID == 63 && !str_comp_num(m_pClient->m_aClients[ClientID].m_aName, " ", 2))
			{
				Get128Name(pLine, aName);
				// dbg_msg("chillerbot", "fixname 128 player '%s' -> '%s'", m_pClient->m_aClients[ClientID].m_aName, aName);
			}
			char aResponse[1024];
			// if(m_pReplyToPing->ReplyToLastPing(aName, m_pClient->m_aClients[ClientID].m_aClan, pLine, aResponse, sizeof(aResponse)))
			CReplyToPing ReplyToPing = CReplyToPing(this, aName, m_pClient->m_aClients[ClientID].m_aClan, pLine, aResponse, sizeof(aResponse));
			if(ReplyToPing.Reply())
			{
				if(aResponse[0])
					SayBuffer(aResponse);
			}
			else
			{
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "%s your message got spam filtered", aName);
				SayBuffer(aBuf);
			}
			// TODO: do we need PopPing() here? Can't we omit the pLine parameter then from this func?
			// who is called first FilterChat() or OnChatMessage()
			// m_aLastPingMessage[0] = '\0';
		}
		return true;
	}
	return false;
}

bool CChatHelper::OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB)
{
	if(pCompletionBuffer[0] != '.')
		return false;

	CCommand *pCompletionCommand = 0;

	const size_t NumCommands = m_vCommands.size();

	if(ReverseTAB)
		*pCompletionChosen = (*pCompletionChosen - 1 + 2 * NumCommands) % (2 * NumCommands);
	else
		*pCompletionChosen = (*pCompletionChosen + 1) % (2 * NumCommands);

	const char *pCommandStart = pCompletionBuffer + 1;
	for(size_t i = 0; i < 2 * NumCommands; ++i)
	{
		int SearchType;
		int Index;

		if(ReverseTAB)
		{
			SearchType = ((*pCompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen - i + NumCommands) % NumCommands;
		}
		else
		{
			SearchType = ((*pCompletionChosen + i) % (2 * NumCommands)) / NumCommands;
			Index = (*pCompletionChosen + i) % NumCommands;
		}

		auto &Command = m_vCommands[Index];

		if(str_comp_nocase_num(Command.pName, pCommandStart, str_length(pCommandStart)) == 0)
		{
			pCompletionCommand = &Command;
			*pCompletionChosen = Index + SearchType * NumCommands;
			break;
		}
	}
	if(!pCompletionCommand)
		return false;

	char aBuf[256];
	// add part before the name
	str_truncate(aBuf, sizeof(aBuf), pInput->GetString(), PlaceholderOffset);

	// add the command
	str_append(aBuf, ".", sizeof(aBuf));
	str_append(aBuf, pCompletionCommand->pName, sizeof(aBuf));

	// add separator
	const char *pSeparator = pCompletionCommand->pParams[0] == '\0' ? "" : " ";
	str_append(aBuf, pSeparator, sizeof(aBuf));
	if(*pSeparator)
		str_append(aBuf, pSeparator, sizeof(aBuf));

	// add part after the name
	str_append(aBuf, pInput->GetString() + PlaceholderOffset + PlaceholderLength, sizeof(aBuf));

	PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->pName) + 1;
	*pOldChatStringLength = pInput->GetLength();
	pInput->Set(aBuf); // TODO: Use Add instead
	pInput->SetCursorOffset(PlaceholderOffset + PlaceholderLength);
	return true;
}
