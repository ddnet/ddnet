// ChillerDragon 2020 - chillerbot ux

#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chillerbotux.h>
#include <game/client/gameclient.h>

#include <cinttypes>

#include "chathelper.h"

CChatHelper::CChatHelper()
{
	mem_zero(m_aaChatFilter, sizeof(m_aaChatFilter));
#define CHILLERBOT_CHAT_CMD(name, params, flags, callback, userdata, help) RegisterCommand(name, params, flags, help);
#include "chatcommands.h"
	m_Commands.sort_range();
}

void CChatHelper::RegisterCommand(const char *pName, const char *pParams, int flags, const char *pHelp)
{
	m_Commands.add_unsorted(CCommand{pName, pParams});
}

void CChatHelper::OnInit()
{
	m_pChillerBot = &m_pClient->m_ChillerBotUX;

	m_aGreetName[0] = '\0';
	m_NextGreetClear = 0;
	m_NextMessageSend = 0;
	m_NextPingMsgClear = 0;
	m_aLastAfkPing[0] = '\0';
	m_aLastPingMessage[0] = '\0';
	m_aLastPingName[0] = '\0';
	mem_zero(m_aSendBuffer, sizeof(m_aSendBuffer));
}

void CChatHelper::OnRender()
{
	int64_t time_now = time_get();
	if(time_now % 10 == 0)
	{
		if(m_NextGreetClear < time_now)
		{
			m_NextGreetClear = 0;
			m_aGreetName[0] = '\0';
		}
		if(m_NextPingMsgClear < time_now)
		{
			m_NextPingMsgClear = 0;
			m_aLastPingMessage[0] = '\0';
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
	((CChatHelper *)pUserData)->ReplyToLastPing();
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

bool CChatHelper::IsGreeting(const char *pMsg)
{
	const char aGreetings[][128] = {
		"hi",
		"hay",
		"hey",
		"heey",
		"heeey",
		"heeeey",
		"haay",
		"haaay",
		"haaaay",
		"haaaaay",
		"henlo",
		"helo",
		"hello",
		"hallo",
		"hellu",
		"hallu",
		"helu",
		"henlu",
		"hemnlo",
		"herro",
		"ahoi",
		"moin",
		"servus",
		"guten tag",
		"priviet",
		"ola",
		"ay",
		"ayy",
		"ayyy",
		"ayyyy",
		"aayyy",
		"aaay",
		"aaaay",
		"yo",
		"yoo",
		"yooo",
		"salut",
		"slt",
		"sup",
		"szia",
		"selam"};
	for(const auto &aGreeting : aGreetings)
	{
		const char *pHL = str_find_nocase(pMsg, aGreeting);
		while(pHL)
		{
			int Length = str_length(aGreeting);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aGreeting);
		}
	}
	return false;
}

bool CChatHelper::IsBye(const char *pMsg)
{
	const char aByes[][128] = {
		"bb",
		"see you",
		"leaving",
		"quit",
		"bye"};
	for(const auto &aBye : aByes)
	{
		const char *pHL = str_find_nocase(pMsg, aBye);
		while(pHL)
		{
			int Length = str_length(aBye);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '1' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, aBye);
		}
	}
	return false;
}

bool CChatHelper::IsQuestionWhy(const char *pMsg)
{
	const char aWhys[][128] = {
		"warum",
		"whyy",
		"whyyy",
		"whyyyy",
		"w hyyyy",
		"whhy",
		"whhyy",
		"whhyyy",
		"wtf?",
		"why"};
	for(const auto &pWhy : aWhys)
	{
		const char *pHL = str_find_nocase(pMsg, pWhy);
		while(pHL)
		{
			int Length = str_length(pWhy);

			if((pMsg == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == pHL[Length - 1]))
				return true;
			pHL = str_find_nocase(pHL + 1, pWhy);
		}
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
				str_append(aBuf, m_aLastPingName, sizeof(aBuf));
				buf_i += str_length(m_aLastPingName);
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

void CChatHelper::ReplyToLastPing()
{
	if(m_aLastPingName[0] == '\0')
		return;
	if(m_aLastPingMessage[0] == '\0')
		return;

	char aBuf[128];
	int MsgLen = str_length(m_aLastPingMessage);
	int NameLen = 0;
	const char *pName = m_pClient->m_aClients[m_pClient->m_LocalIDs[0]].m_aName;
	const char *pDummyName = m_pClient->m_aClients[m_pClient->m_LocalIDs[1]].m_aName;

	if(LineShouldHighlight(m_aLastPingMessage, pName))
		NameLen = str_length(pName);
	else if(m_pClient->Client()->DummyConnected() && LineShouldHighlight(m_aLastPingMessage, pDummyName))
		NameLen = str_length(pDummyName);

	// ping without further context
	if(MsgLen < NameLen + 2)
	{
		str_format(aBuf, sizeof(aBuf), "%s ?", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// greetings
	if(IsGreeting(m_aLastPingMessage))
	{
		str_format(aBuf, sizeof(aBuf), "hi %s", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	if(IsBye(m_aLastPingMessage))
	{
		str_format(aBuf, sizeof(aBuf), "bye %s", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// why?
	if(IsQuestionWhy(m_aLastPingMessage) || (str_find(m_aLastPingMessage, "?") && MsgLen < NameLen + 4))
	{
		char aWarReason[128];
		if(m_pClient->m_WarList.IsWarlist(m_aLastPingName) || m_pClient->m_WarList.IsTraitorlist(m_aLastPingName))
		{
			m_pClient->m_WarList.GetWarReason(m_aLastPingName, aWarReason, sizeof(aWarReason));
			if(aWarReason[0])
				str_format(aBuf, sizeof(aBuf), "%s has war because: %s", m_aLastPingName, aWarReason);
			else
				str_format(aBuf, sizeof(aBuf), "%s you are on my warlist.", m_aLastPingName);
			m_pClient->m_Chat.Say(0, aBuf);
			m_aLastPingMessage[0] = '\0';
			return;
		}
		else if(m_pClient->m_WarList.IsWarClanlist(m_aLastPingClan))
		{
			str_format(aBuf, sizeof(aBuf), "%s your clan is on my warlist.", m_aLastPingName);
			m_pClient->m_Chat.Say(0, aBuf);
			m_aLastPingMessage[0] = '\0';
			return;
		}
	}
	// spec me
	if(str_find_nocase(m_aLastPingMessage, "spec") || str_find_nocase(m_aLastPingMessage, "watch") || (str_find_nocase(m_aLastPingMessage, "look") && !str_find_nocase(m_aLastPingMessage, "looks")) || str_find_nocase(m_aLastPingMessage, "schau"))
	{
		str_format(aBuf, sizeof(aBuf), "/pause %s", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		str_format(aBuf, sizeof(aBuf), "%s ok i am watching you", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// no u
	if(MsgLen < NameLen + 8 && (str_find_nocase(m_aLastPingMessage, "no u") || str_find_nocase(m_aLastPingMessage, "no you") || str_find_nocase(m_aLastPingMessage, "bad")))
	{
		str_format(aBuf, sizeof(aBuf), "%s no u", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// wanna? (always say no automated if motivated to do something type yes manually)
	if(str_find_nocase(m_aLastPingMessage, "wanna") || str_find_nocase(m_aLastPingMessage, "want"))
	{
		// TODO: fix tone
		// If you get asked to be given something "no sorry" sounds weird
		// If you are being asked to do something together "no thanks" sounds weird
		// the generic "no" might be a bit dry
		str_format(aBuf, sizeof(aBuf), "%s no", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// help
	if(str_find_nocase(m_aLastPingMessage, "help"))
	{
		str_format(aBuf, sizeof(aBuf), "%s where? what?", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// small talk
	if(str_find_nocase(m_aLastPingMessage, "how are you") ||
		str_find_nocase(m_aLastPingMessage, "how r u") ||
		str_find_nocase(m_aLastPingMessage, "how r you") ||
		str_find_nocase(m_aLastPingMessage, "how are u") ||
		str_find_nocase(m_aLastPingMessage, "how is it going") ||
		str_find_nocase(m_aLastPingMessage, "ca va"))
	{
		str_format(aBuf, sizeof(aBuf), "%s good, and you? :)", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	if(str_find_nocase(m_aLastPingMessage, "wie gehts") || str_find_nocase(m_aLastPingMessage, "wie geht es") || str_find_nocase(m_aLastPingMessage, "was geht"))
	{
		str_format(aBuf, sizeof(aBuf), "%s gut, und dir? :)", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	if(str_find_nocase(m_aLastPingMessage, "about you") || str_find_nocase(m_aLastPingMessage, "and you") || str_find_nocase(m_aLastPingMessage, "and u") ||
		(str_find_nocase(m_aLastPingMessage, "u?") && MsgLen < NameLen + 5) ||
		(str_find_nocase(m_aLastPingMessage, "wbu") && MsgLen < NameLen + 8) ||
		(str_find_nocase(m_aLastPingMessage, "hbu") && MsgLen < NameLen + 8))
	{
		str_format(aBuf, sizeof(aBuf), "%s good", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// advertise chillerbot
	if(str_find_nocase(m_aLastPingMessage, "what client") || str_find_nocase(m_aLastPingMessage, "which client") || str_find_nocase(m_aLastPingMessage, "wat client") ||
		str_find_nocase(m_aLastPingMessage, "good client") ||
		((str_find_nocase(m_aLastPingMessage, "ddnet") || str_find_nocase(m_aLastPingMessage, "vanilla")) && str_find_nocase(m_aLastPingMessage, "?")))
	{
		str_format(aBuf, sizeof(aBuf), "%s I use chillerbot-ux ( https://chillerbot.github.io )", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// compliments
	if(str_find_nocase(m_aLastPingMessage, "good") ||
		str_find_nocase(m_aLastPingMessage, "happy") ||
		str_find_nocase(m_aLastPingMessage, "congrats") ||
		str_find_nocase(m_aLastPingMessage, "nice") ||
		str_find_nocase(m_aLastPingMessage, "pro"))
	{
		str_format(aBuf, sizeof(aBuf), "%s thanks", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
	// impatient
	if(str_find_nocase(m_aLastPingMessage, "answer") || str_find_nocase(m_aLastPingMessage, "ignore") || str_find_nocase(m_aLastPingMessage, "antwort") || str_find_nocase(m_aLastPingMessage, "ignorier"))
	{
		str_format(aBuf, sizeof(aBuf), "%s i am currently busy (automated reply)", m_aLastPingName);
		m_pClient->m_Chat.Say(0, aBuf);
		m_aLastPingMessage[0] = '\0';
		return;
	}
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

void CChatHelper::Get128Name(const char *pMsg, char *pName)
{
	for(int i = 0; pMsg[i] && i < 17; i++)
	{
		if(pMsg[i] == ':' && pMsg[i + 1] == ' ')
		{
			str_copy(pName, pMsg, i + 1);
			return;
		}
	}
	str_copy(pName, " ", 2);
}

void CChatHelper::OnChatMessage(int ClientID, int Team, const char *pMsg)
{
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
	str_copy(m_aLastPingName, aName, sizeof(m_aLastPingName));
	str_copy(m_aLastPingClan, m_pClient->m_aClients[ClientID].m_aClan, sizeof(m_aLastPingClan));
	if(IsGreeting(pMsg))
	{
		str_copy(m_aGreetName, aName, sizeof(m_aGreetName));
		m_NextGreetClear = time_get() + time_freq() * 10;
	}
	// ignore duplicated messages
	if(!str_comp(m_aLastPingMessage, pMsg))
		return;
	str_copy(m_aLastPingMessage, pMsg, sizeof(m_aLastPingMessage));
	m_NextPingMsgClear = time_get() + time_freq() * 60;
	int64_t AfkTill = m_pChillerBot->GetAfkTime();
	if(AfkTill && m_pChillerBot->GetAfkActivity() < 25)
	{
		char aBuf[256];
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

bool CChatHelper::FilterChat(int ClientID, int Team, const char *pLine)
{
	for(auto &aChatFilter : m_aaChatFilter)
	{
		if(aChatFilter[0] == '\0')
			continue;
		if(str_find(pLine, aChatFilter))
			return true;
	}
	return false;
}

bool CChatHelper::OnAutocomplete(CLineInput *pInput, const char *pCompletionBuffer, int PlaceholderOffset, int PlaceholderLength, int *pOldChatStringLength, int *pCompletionChosen, bool ReverseTAB)
{
	if(pCompletionBuffer[0] != '.')
		return false;

	CCommand *pCompletionCommand = 0;

	const size_t NumCommands = m_Commands.size();

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

		auto &Command = m_Commands[Index];

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
