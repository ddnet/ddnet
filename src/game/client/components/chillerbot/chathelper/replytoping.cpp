// chillerbot-ux reply to ping

#include "engine/shared/protocol.h"
#include <engine/client/notifications.h>
#include <engine/config.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/textrender.h>
#include <game/client/animstate.h>
#include <game/client/components/camera.h>
#include <game/client/components/chat.h>
#include <game/client/components/chillerbot/chathelper.h>
#include <game/client/components/chillerbot/version.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/race.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>
#include <game/version.h>

#include "replytoping.h"

CLangParser &CReplyToPing::LangParser() { return ChatHelper()->LangParser(); }

CReplyToPing::CReplyToPing(CChatHelper *pChatHelper, const char *pMessageAuthor, const char *pMessageAuthorClan, const char *pMessage, char *pResponse, long unsigned int SizeOfResponse)
{
	m_pChatHelper = pChatHelper;

	m_pMessageAuthor = pMessageAuthor;
	m_pMessageAuthorClan = pMessageAuthorClan;
	m_pMessage = pMessage;
	m_pResponse = pResponse;
	m_SizeOfResponse = SizeOfResponse;
}

bool CReplyToPing::WhyWar(const char *pVictim, bool IsCheck)
{
	if(!pVictim)
		return false;

	bool HasWar = true;
	// aVictim also has to hold the full own name to match the chop off
	char aVictim[MAX_NAME_LENGTH + 3 + MAX_NAME_LENGTH];
	str_copy(aVictim, pVictim, sizeof(aVictim));
	if(!ChatHelper()->GameClient()->m_WarList.IsWarlist(aVictim) && !ChatHelper()->GameClient()->m_WarList.IsTraitorlist(aVictim))
	{
		HasWar = false;
		while(str_endswith(aVictim, "?")) // cut off the question marks from the victim name
			aVictim[str_length(aVictim) - 1] = '\0';
		while(str_endswith(aVictim, " ")) // cut off spaces from victim name 'why war foo ?' -> 'foo ?' -> 'foo ' -> 'foo'
			aVictim[str_length(aVictim) - 1] = '\0';
		// cut off own name from the victime name if question in this format "why do you war foo (your name)"
		char aOwnName[MAX_NAME_LENGTH + 3];
		// main tee
		str_format(aOwnName, sizeof(aOwnName), " %s", ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aName);
		if(str_endswith_nocase(aVictim, aOwnName))
			aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
		if(ChatHelper()->GameClient()->Client()->DummyConnected())
		{
			str_format(aOwnName, sizeof(aOwnName), " %s", ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aName);
			if(str_endswith_nocase(aVictim, aOwnName))
				aVictim[str_length(aVictim) - str_length(aOwnName)] = '\0';
		}

		// cut off descriptions like this
		// why do you block foo he is new here!
		// why do you block foo she is my friend!!
		for(int i = 0; i < str_length(aVictim); i++)
		{
			// c++ be like...
			if(i < 2)
				continue;
			if(aVictim[i - 1] != ' ')
				continue;
			if((aVictim[i] != 'h' || !aVictim[i + 1] || aVictim[i + 1] != 'e' || !aVictim[i + 2] || aVictim[i + 2] != ' ') &&
				(aVictim[i] != 's' || !aVictim[i + 1] || aVictim[i + 1] != 'h' || !aVictim[i + 2] || aVictim[i + 2] != 'e' || !aVictim[i + 3] || aVictim[i + 3] != ' '))
				continue;

			aVictim[i - 1] = '\0';
			break;
		}

		// do not kill my friend foo
		const char *pFriend = NULL;
		if((pFriend = str_find_nocase(aVictim, " friend ")))
			pFriend += str_length(" friend ");
		else if((pFriend = str_find_nocase(aVictim, " frint ")))
			pFriend += str_length(" frint ");
		else if((pFriend = str_find_nocase(aVictim, " mate ")))
			pFriend += str_length(" mate ");
		else if((pFriend = str_find_nocase(aVictim, " bff ")))
			pFriend += str_length(" bff ");
		else if((pFriend = str_find_nocase(aVictim, " girlfriend ")))
			pFriend += str_length(" girlfriend ");
		else if((pFriend = str_find_nocase(aVictim, " boyfriend ")))
			pFriend += str_length(" boyfriend ");
		else if((pFriend = str_find_nocase(aVictim, " dog ")))
			pFriend += str_length(" dog ");
		else if((pFriend = str_find_nocase(aVictim, " gf ")))
			pFriend += str_length(" gf ");
		else if((pFriend = str_find_nocase(aVictim, " bf ")))
			pFriend += str_length(" bf ");

		if(pFriend)
			str_copy(aVictim, pFriend, sizeof(aVictim));
	}

	char aWarReason[128];
	if(HasWar || ChatHelper()->GameClient()->m_WarList.IsWarlist(aVictim) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(aVictim))
	{
		ChatHelper()->GameClient()->m_WarList.GetWarReason(aVictim, aWarReason, sizeof(aWarReason));
		if(aWarReason[0])
			str_format(m_pResponse, m_SizeOfResponse, "%s: %s has war because: %s", m_pMessageAuthor, aVictim, aWarReason);
		else
			str_format(m_pResponse, m_SizeOfResponse, "%s: the name %s is on my warlist.", m_pMessageAuthor, aVictim);
		return true;
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		auto &Client = ChatHelper()->GameClient()->m_aClients[i];
		if(!Client.m_Active)
			continue;
		if(str_comp(Client.m_aName, aVictim))
			continue;

		if(ChatHelper()->GameClient()->m_WarList.IsWarClanlist(Client.m_aClan))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s i war %s because his clan %s is on my warlist.", m_pMessageAuthor, aVictim, Client.m_aClan);
			return true;
		}
		if(ChatHelper()->GameClient()->m_WarList.IsWarClanmate(i))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s i might kill %s because I war member from his clan %s", m_pMessageAuthor, aVictim, Client.m_aClan);
			return true;
		}
	}
	if(IsCheck)
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s: '%s' is not on my warlist.", m_pMessageAuthor, aVictim);
		return true;
	}
	return false;
}

int CReplyToPing::GetSuffixLen(const char *pStr, const char *pSuffix)
{
	if(str_endswith(pStr, pSuffix))
		return str_length(pSuffix);
	return 0;
}

void CReplyToPing::StripSpacesAndPunctuationAndOwnName(const char *pStr, char *pStripped, int SizeOfStripped)
{
	dbg_assert(SizeOfStripped < 512, "too big to strip");
	if(pStr == pStripped)
	{
		pStripped[0] = '\0';
		return;
	}
	if(!pStr)
	{
		pStripped[0] = '\0';
		return;
	}
	if(!pStripped)
	{
		pStripped[0] = '\0';
		return;
	}
	if(SizeOfStripped < 1)
	{
		pStripped[0] = '\0';
		return;
	}
	char aBuf[512];
	str_copy(aBuf, pStr, sizeof(aBuf));
	while(str_endswith(aBuf, "?")) // cut off the question marks
		aBuf[str_length(aBuf) - 1] = '\0';
	while(str_endswith(aBuf, " ")) // cut off spaces
		aBuf[str_length(aBuf) - 1] = '\0';
	int Offset = 0;
	const char *pName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aName;
	const char *pDummyName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aName;
	char aName[128];
	str_format(aName, sizeof(aName), "%s: ", pName);
	if(!Offset && str_startswith(pStr, aName))
		Offset = str_length(aName);
	str_format(aName, sizeof(aName), "%s ", pName);
	if(!Offset && str_startswith(pStr, aName))
		Offset = str_length(aName);
	str_format(aName, sizeof(aName), "%s", pName);
	if(!Offset && str_startswith(pStr, aName))
		Offset = str_length(aName);
	if(ChatHelper()->GameClient()->Client()->DummyConnected())
	{
		str_format(aName, sizeof(aName), "%s: ", pDummyName);
		if(!Offset && str_startswith(pStr, aName))
			Offset = str_length(aName);
		str_format(aName, sizeof(aName), "%s ", pDummyName);
		if(!Offset && str_startswith(pStr, aName))
			Offset = str_length(aName);
		str_format(aName, sizeof(aName), "%s", pDummyName);
		if(!Offset && str_startswith(pStr, aName))
			Offset = str_length(aName);
	}
	if(Offset >= str_length(aBuf))
		pStripped[0] = '\0';
	else
		str_copy(pStripped, aBuf + Offset, SizeOfStripped);
}

bool CReplyToPing::IsEmptyStr(const char *pStr)
{
	if(!pStr)
		return true;
	for(int i = 0; pStr[i] != '\0'; i++)
		if(pStr[i] != ' ' && pStr[i] != 0x7)
		{
			dbg_msg("chiller", "'%c' != ' ' (%d)", pStr[i], pStr[i]);
			return false;
		}
	return true;
}

bool CReplyToPing::Reply()
{
	if(!m_pResponse)
		return false;
	m_pResponse[0] = '\0';
	if(m_pMessageAuthor[0] == '\0')
		return false;
	if(m_pMessage[0] == '\0')
		return false;

	int MsgLen = str_length(m_pMessage);
	int NameLen = 0;
	const char *pName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aName;
	const char *pDummyName = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aName;
	const char *pClan = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[0]].m_aClan;
	const char *pDummyClan = ChatHelper()->GameClient()->m_aClients[ChatHelper()->GameClient()->m_LocalIDs[1]].m_aClan;

	if(ChatHelper()->LineShouldHighlight(m_pMessage, pName))
		NameLen = str_length(pName);
	else if(ChatHelper()->GameClient()->Client()->DummyConnected() && ChatHelper()->LineShouldHighlight(m_pMessage, pDummyName))
		NameLen = str_length(pDummyName);

	// ping without further context
	if(MsgLen < NameLen + 2)
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s ?", m_pMessageAuthor);
		return true;
	}
	// greetings
	if(LangParser().IsGreeting(m_pMessage))
	{
		str_format(m_pResponse, m_SizeOfResponse, "hi %s", m_pMessageAuthor);
		return true;
	}
	if(LangParser().IsBye(m_pMessage))
	{
		str_format(m_pResponse, m_SizeOfResponse, "bye %s", m_pMessageAuthor);
		return true;
	}
	// can i join your clan?
	if(str_find_nocase(m_pMessage, "clan") &&
		(str_find_nocase(m_pMessage, "enter") ||
			str_find_nocase(m_pMessage, "join") ||
			str_find_nocase(m_pMessage, "let me") ||
			str_find_nocase(m_pMessage, "beitreten") ||
			str_find_nocase(m_pMessage, " in ") ||
			str_find_nocase(m_pMessage, "can i") ||
			str_find_nocase(m_pMessage, "can me") ||
			str_find_nocase(m_pMessage, "me you") ||
			str_find_nocase(m_pMessage, "me is") ||
			str_find_nocase(m_pMessage, "into")))
	{
		char aResponse[1024];
		if(ChatHelper()->HowToJoinClan(pClan, aResponse, sizeof(aResponse)) || (ChatHelper()->GameClient()->Client()->DummyConnected() && ChatHelper()->HowToJoinClan(pDummyClan, aResponse, m_SizeOfResponse)))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s %s", m_pMessageAuthor, aResponse);
			return true;
		}
	}
	// check if a player has war or not
	const char *pDoYou = nullptr;
	if(!pDoYou && (pDoYou = str_find_nocase(m_pMessage, "you war ")))
		pDoYou = pDoYou + str_length("you war ");
	if(!pDoYou && (pDoYou = str_find_nocase(m_pMessage, "you in war with ")))
		pDoYou = pDoYou + str_length("you in war with ");
	// "hast du war mit"
	// "hast du eig war mit"
	// "hast du eigentlich war mit"
	// "hast du überhaupt war mit"
	// "hast du einen war mit"
	if(!pDoYou && (pDoYou = LangParser().StrFindOrder(m_pMessage, 2, "hast du ", "war mit ")))
		pDoYou = pDoYou + str_length("war mit ");
	if(pDoYou)
		if(WhyWar(pDoYou, true))
			return true;

	// check war reason for others
	const char *pWhy = str_find_nocase(m_pMessage, "why has ");
	if(pWhy)
		pWhy = pWhy + str_length("why has ");
	if(!pWhy)
		if((pWhy = str_find_nocase(m_pMessage, "why")))
			pWhy = pWhy + str_length("why");
	if(!pWhy)
		if((pWhy = str_find_nocase(m_pMessage, "warum hat ")))
			pWhy = pWhy + str_length("warum hat ");
	if(!pWhy)
		if((pWhy = str_find_nocase(m_pMessage, "warum")))
			pWhy = pWhy + str_length("warum");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "stop");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "do not");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "don't");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "do u");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "do you");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "is u");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "is you");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "are u");
	if(!pWhy)
		pWhy = str_find_nocase(m_pMessage, "are you");
	if(pWhy)
	{
		const char *pKill = NULL;
		pKill = str_find_nocase(pWhy, "kill "); // why do you kill foo?
		if(pKill)
			pKill = pKill + str_length("kill ");
		else if((pKill = str_find_nocase(pWhy, "kil "))) // why kil foo?
			pKill = pKill + str_length("kil ");
		else if((pKill = str_find_nocase(pWhy, "killed "))) // why killed foo?
			pKill = pKill + str_length("killed ");
		else if((pKill = str_find_nocase(pWhy, "block "))) // why do you block foo?
			pKill = pKill + str_length("block ");
		else if((pKill = str_find_nocase(pWhy, "blocked "))) // why do you blocked foo?
			pKill = pKill + str_length("blocked ");
		else if((pKill = str_find_nocase(pWhy, "help "))) // why no help foo?
			pKill = pKill + str_length("help ");
		else if((pKill = str_find_nocase(pWhy, "war with "))) // why do you have war with foo?
			pKill = pKill + str_length("war with ");
		else if((pKill = str_find_nocase(pWhy, "war "))) // why war foo?
			pKill = pKill + str_length("war ");
		else if((pKill = str_find_nocase(pWhy, "wared "))) // why wared foo?
			pKill = pKill + str_length("wared ");
		else if((pKill = str_find_nocase(pWhy, "waring "))) // why waring foo?
			pKill = pKill + str_length("waring ");
		else if((pKill = str_find_nocase(pWhy, "bully "))) // why bully foo?
			pKill = pKill + str_length("bully ");
		else if((pKill = str_find_nocase(pWhy, "bullying "))) // why bullying foo?
			pKill = pKill + str_length("bullying ");
		else if((pKill = str_find_nocase(pWhy, "bullied "))) // why bullied foo?
			pKill = pKill + str_length("bullied ");
		else if((pKill = str_find_nocase(pWhy, "freeze "))) // why freeze foo?
			pKill = pKill + str_length("freeze ");
		else if((pKill = str_find_nocase(pWhy, "warlist "))) // is warlist foo?
			pKill = pKill + str_length("warlist ");
		else if((pKill = str_find_nocase(pWhy, "enemi "))) // is enemi foo?
			pKill = pKill + str_length("enemi ");
		else if((pKill = str_find_nocase(pWhy, "enemy "))) // is enemy foo?
			pKill = pKill + str_length("enemy ");

		if(WhyWar(pKill))
			return true;

		// "why foo war?"
		// chop off the "war" at the end
		char aWhy[128];
		str_copy(aWhy, pWhy, sizeof(aWhy));

		int CutOffWar = -1;
		if((CutOffWar = LangParser().StrFindIndex(aWhy, " war")) != -1)
			aWhy[CutOffWar] = '\0';
		else if((CutOffWar = LangParser().StrFindIndex(aWhy, " kill")) != -1)
			aWhy[CutOffWar] = '\0';

		// trim
		int trim = 0;
		while(aWhy[trim] == ' ')
			trim++;

		if(CutOffWar != -1)
			if(WhyWar(aWhy + trim))
				return true;

		char aStripped[128];
		StripSpacesAndPunctuationAndOwnName(pKill, aStripped, sizeof(aStripped));
		if(!IsEmptyStr(aStripped))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s: '%s' is not on my warlist. axaxaxax", m_pMessageAuthor, aStripped);
			return true;
		}
	}
	// why? (check war for self)
	if(LangParser().IsQuestionWhy(m_pMessage) || (str_find(m_pMessage, "?") && MsgLen < NameLen + 4) ||
		((str_find(m_pMessage, "stop") || str_find_nocase(m_pMessage, "help")) && (ChatHelper()->GameClient()->m_WarList.IsWarlist(m_pMessageAuthor) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(m_pMessageAuthor))))
	{
		char aWarReason[128];
		if(ChatHelper()->GameClient()->m_WarList.IsWarlist(m_pMessageAuthor) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(m_pMessageAuthor))
		{
			ChatHelper()->GameClient()->m_WarList.GetWarReason(m_pMessageAuthor, aWarReason, sizeof(aWarReason));
			if(aWarReason[0])
				str_format(m_pResponse, m_SizeOfResponse, "%s has war because: %s", m_pMessageAuthor, aWarReason);
			else
				str_format(m_pResponse, m_SizeOfResponse, "%s you are on my warlist.", m_pMessageAuthor);
			return true;
		}
		else if(ChatHelper()->GameClient()->m_WarList.IsWarClanlist(m_pMessageAuthorClan))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s your clan is on my warlist.", m_pMessageAuthor);
			return true;
		}
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			auto &Client = ChatHelper()->GameClient()->m_aClients[i];
			if(!Client.m_Active)
				continue;
			if(str_comp(Client.m_aName, m_pMessageAuthor))
				continue;

			if(ChatHelper()->GameClient()->m_WarList.IsWarClanmate(i))
			{
				str_format(m_pResponse, m_SizeOfResponse, "%s i might kill you because i war member of your clan", m_pMessageAuthor);
				return true;
			}
		}
	}
	// still check war for others but now different order
	// also cover "name is war?" in addition to "is war name?"
	int ChopEnding = 0;
	char aStrippedMsg[256];
	StripSpacesAndPunctuationAndOwnName(m_pMessage, aStrippedMsg, sizeof(aStrippedMsg));
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " is ur war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " is u war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " is your war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " is you war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " ur war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " u war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " your war");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on your warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on you warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on ur warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on u warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on your war list");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on you war list");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on ur war list");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " on u war list");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " in your warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " in you warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " in ur warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " in u warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " your warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " you warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " ur warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " u warlist");
	if(!ChopEnding)
		ChopEnding = GetSuffixLen(aStrippedMsg, " warlist");

	if(ChopEnding)
	{
		unsigned int Cut = str_length(aStrippedMsg) - ChopEnding;
		if(Cut > 0 && Cut < sizeof(aStrippedMsg))
		{
			aStrippedMsg[str_length(aStrippedMsg) - ChopEnding] = '\0';
			if(!WhyWar(aStrippedMsg))
				str_format(m_pResponse, m_SizeOfResponse, "%s: '%s' is not on my warlist.", m_pMessageAuthor, aStrippedMsg);
			return true;
		}
	}

	// check all wars "who is on your warlist?"
	const char *pWho = str_find_nocase(m_pMessage, "who ");
	if(pWho)
		pWho = pWho + str_length("who ");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "which")))
			pWho = pWho + str_length("which");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "got any")))
			pWho = pWho + str_length("got any");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "have any")))
			pWho = pWho + str_length("have any");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "wer ")))
			pWho = pWho + str_length("wer ");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "wie viele")))
			pWho = pWho + str_length("wie viele");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, " how ")))
			pWho = pWho + str_length(" how ");
	if(!pWho)
		pWho = str_startswith_nocase(m_pMessage, "how ");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "howm"))) // howmani howmany howm any
			pWho = pWho + str_length("howm");
	if(!pWho)
		if((pWho = str_find_nocase(m_pMessage, "u got")))
			pWho = pWho + str_length("u got");
	if(pWho)
	{
		const char *pEnemy = NULL;
		pEnemy = str_find_nocase(pWho, "enemi");
		if(pEnemy)
			pEnemy = pEnemy + str_length("enemi");
		else if((pEnemy = str_find_nocase(pWho, "enemy")))
			pEnemy = pEnemy + str_length("enemy");
		else if((pEnemy = str_find_nocase(pWho, "gegner")))
			pEnemy = pEnemy + str_length("gegner");
		else if((pEnemy = str_find_nocase(pWho, "against")))
			pEnemy = pEnemy + str_length("against");
		else if((pEnemy = str_find_nocase(pWho, "opponent")))
			pEnemy = pEnemy + str_length("opponent");
		else if((pEnemy = str_find_nocase(pWho, "raid")))
			pEnemy = pEnemy + str_length("raid");
		else if((pEnemy = str_find_nocase(pWho, "block")))
			pEnemy = pEnemy + str_length("block");
		else if((pEnemy = str_find_nocase(pWho, "kill")))
			pEnemy = pEnemy + str_length("kill");
		else if((pEnemy = str_find_nocase(pWho, "spaik")))
			pEnemy = pEnemy + str_length("spaik");
		else if((pEnemy = str_find_nocase(pWho, "spike")))
			pEnemy = pEnemy + str_length("spike");
		else if((pEnemy = str_find_nocase(pWho, "bad")))
			pEnemy = pEnemy + str_length("bad");
		else if((pEnemy = str_find_nocase(pWho, "war")))
			pEnemy = pEnemy + str_length("war");
		if(pEnemy)
		{
			char aEnemyList[256]; // 255 max msg len
			aEnemyList[0] = '\0';
			int NumEnemies = 0;
			for(auto &Client : ChatHelper()->GameClient()->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(!ChatHelper()->GameClient()->m_WarList.IsWarlist(Client.m_aName) &&
					!ChatHelper()->GameClient()->m_WarList.IsWarClanlist(Client.m_aClan))
					continue;

				NumEnemies++;
				if(aEnemyList[0])
				{
					str_append(aEnemyList, ", ", sizeof(aEnemyList));
				}
				str_append(aEnemyList, Client.m_aName, sizeof(aEnemyList));
			}
			if(NumEnemies)
				str_format(m_pResponse, m_SizeOfResponse, "%s %d of my %d enemies are online: %s", m_pMessageAuthor, NumEnemies, ChatHelper()->GameClient()->m_WarList.NumEnemies(), aEnemyList);
			else
				str_format(m_pResponse, m_SizeOfResponse, "%s currently 0 of my %d enemies are connected", m_pMessageAuthor, ChatHelper()->GameClient()->m_WarList.NumEnemies());
			return true;
		}
		const char *pFriend = NULL;
		pFriend = str_find_nocase(pWho, "team");
		if(pFriend)
			pFriend = pFriend + str_length("team");
		else if((pFriend = str_find_nocase(pWho, "friend")))
			pFriend = pFriend + str_length("friend");
		else if((pFriend = str_find_nocase(pWho, "frint")))
			pFriend = pFriend + str_length("frint");
		else if((pFriend = str_find_nocase(pWho, "peace")))
			pFriend = pFriend + str_length("peace");
		else if((pFriend = str_find_nocase(pWho, "like")))
			pFriend = pFriend + str_length("like");
		else if((pFriend = str_find_nocase(pWho, "good")))
			pFriend = pFriend + str_length("good");
		else if((pFriend = str_find_nocase(pWho, "help")))
			pFriend = pFriend + str_length("help");
		else if((pFriend = str_find_nocase(pWho, "side")))
			pFriend = pFriend + str_length("side");
		else if((pFriend = str_find_nocase(pWho, "with u")))
			pFriend = pFriend + str_length("with u");
		else if((pFriend = str_find_nocase(pWho, "with y")))
			pFriend = pFriend + str_length("with y");
		else if((pFriend = str_find_nocase(pWho, "u with")))
			pFriend = pFriend + str_length("u with");
		if(pFriend)
		{
			char aFriendList[256]; // 255 max msg len
			aFriendList[0] = '\0';
			int NumFriends = 0;
			for(auto &Client : ChatHelper()->GameClient()->m_aClients)
			{
				if(!Client.m_Active)
					continue;
				if(!ChatHelper()->GameClient()->m_WarList.IsTeamlist(Client.m_aName) &&
					!ChatHelper()->GameClient()->m_WarList.IsTeamClanlist(Client.m_aClan))
					continue;

				NumFriends++;
				if(aFriendList[0])
				{
					str_append(aFriendList, ", ", sizeof(aFriendList));
				}
				str_append(aFriendList, Client.m_aName, sizeof(aFriendList));
			}
			if(NumFriends)
				str_format(m_pResponse, m_SizeOfResponse, "%s %d of my %d friends are online: %s", m_pMessageAuthor, NumFriends, ChatHelper()->GameClient()->m_WarList.NumTeam(), aFriendList);
			else
				str_format(m_pResponse, m_SizeOfResponse, "%s currently 0 of my %d friends are connected", m_pMessageAuthor, ChatHelper()->GameClient()->m_WarList.NumTeam());
			return true;
		}
	}

	// intentionally check for being on warlist
	// also expecting an no if not
	if(str_find_nocase(m_pMessage, "?") ||
		str_find_nocase(m_pMessage, "do you") ||
		str_find_nocase(m_pMessage, "are we") ||
		str_find_nocase(m_pMessage, "am i") ||
		str_find_nocase(m_pMessage, "is i") ||
		str_find_nocase(m_pMessage, "im your") ||
		str_find_nocase(m_pMessage, "what") ||
		str_find_nocase(m_pMessage, "show") ||
		str_find_nocase(m_pMessage, "wat"))
	{
		if(str_find_nocase(m_pMessage, "me enem") || str_find_nocase(m_pMessage, "i enem") || str_find_nocase(m_pMessage, "me is enem") || str_find_nocase(m_pMessage, "i is enem") ||
			str_find_nocase(m_pMessage, "i u enem") || str_find_nocase(m_pMessage, "i ur enem") || str_find_nocase(m_pMessage, "i your enem") ||
			str_find_nocase(m_pMessage, "im u enem") || str_find_nocase(m_pMessage, "im ur enem") || str_find_nocase(m_pMessage, "im your enem") ||
			str_find_nocase(m_pMessage, "i am u enem") || str_find_nocase(m_pMessage, "i am ur enem") || str_find_nocase(m_pMessage, "i am your enem") ||
			str_find_nocase(m_pMessage, "me war") || str_find_nocase(m_pMessage, "i war") || str_find_nocase(m_pMessage, "me is war") || str_find_nocase(m_pMessage, "i is war") ||
			str_find_nocase(m_pMessage, "peace") ||
			str_find_nocase(m_pMessage, "me friend") || str_find_nocase(m_pMessage, "i friend") || str_find_nocase(m_pMessage, "me is friend") || str_find_nocase(m_pMessage, "i is friend") ||
			str_find_nocase(m_pMessage, "me frint") || str_find_nocase(m_pMessage, "i frint") || str_find_nocase(m_pMessage, "me is frint") || str_find_nocase(m_pMessage, "i is frint") ||
			str_find_nocase(m_pMessage, "are we in war") || str_find_nocase(m_pMessage, "we war") || str_find_nocase(m_pMessage, "we peace") || str_find_nocase(m_pMessage, "we good") ||
			str_find_nocase(m_pMessage, "i enem") || str_find_nocase(m_pMessage, "i peace") || str_find_nocase(m_pMessage, "i frien") || str_find_nocase(m_pMessage, "i frin") ||
			str_find_nocase(m_pMessage, "me enem") || str_find_nocase(m_pMessage, "me peace") || str_find_nocase(m_pMessage, "me frien") || str_find_nocase(m_pMessage, "me frin") ||
			str_find_nocase(m_pMessage, "colo") || str_find_nocase(m_pMessage, "cole") || str_find_nocase(m_pMessage, "collo") || str_find_nocase(m_pMessage, "colla") || str_find_nocase(m_pMessage, "cola") ||
			str_find_nocase(m_pMessage, "red") || str_find_nocase(m_pMessage, "green") || str_find_nocase(m_pMessage, "orange") || str_find_nocase(m_pMessage, "black") || str_find_nocase(m_pMessage, "reason"))
		{
			char aWarReason[128];
			if(ChatHelper()->GameClient()->m_WarList.IsWarlist(m_pMessageAuthor) || ChatHelper()->GameClient()->m_WarList.IsTraitorlist(m_pMessageAuthor))
			{
				ChatHelper()->GameClient()->m_WarList.GetWarReason(m_pMessageAuthor, aWarReason, sizeof(aWarReason));
				if(aWarReason[0])
					str_format(m_pResponse, m_SizeOfResponse, "%s you have war because: %s", m_pMessageAuthor, aWarReason);
				else
					str_format(m_pResponse, m_SizeOfResponse, "%s you are on my warlist.", m_pMessageAuthor);
				return true;
			}
			else if(ChatHelper()->GameClient()->m_WarList.IsWarClanlist(m_pMessageAuthorClan))
			{
				str_format(m_pResponse, m_SizeOfResponse, "%s your clan is on my warlist.", m_pMessageAuthor);
				return true;
			}
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				auto &Client = ChatHelper()->GameClient()->m_aClients[i];
				if(!Client.m_Active)
					continue;
				if(str_comp(Client.m_aName, m_pMessageAuthor))
					continue;

				if(ChatHelper()->GameClient()->m_WarList.IsWarClanmate(i))
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i might kill you because i war member of your clan", m_pMessageAuthor);
					return true;
				}
			}
			str_format(m_pResponse, m_SizeOfResponse, "%s your name is neither on my friend nor enemy list.", m_pMessageAuthor);
			return true;
		}
	}

	// where are you
	if(str_find_nocase(m_pMessage, "where are you") || str_find_nocase(m_pMessage, "where r u") || (str_find_nocase(m_pMessage, "where r yo") && !str_find_nocase(m_pMessage, "wo bist")))
	{
		for(auto &Client : ChatHelper()->GameClient()->m_aClients)
		{
			if(!Client.m_Active)
				continue;
			if(str_comp(Client.m_aName, m_pMessageAuthor))
				continue;

			// TODO: dont get current dummy but the pinged dummy
			CCharacter *pChar = ChatHelper()->GameClient()->m_GameWorld.GetCharacterByID(ChatHelper()->GameClient()->m_LocalIDs[g_Config.m_ClDummy]);
			if(pChar)
				continue;
			vec2 Self = pChar->m_Pos;
			vec2 Other = Client.m_RenderPos;
			float distY = abs(Self.y - Other.y);
			float distX = abs(Self.x - Other.x);
			if(distX < 5 * 32 && distY < 5 * 32)
			{
				if(distX > distY)
				{
					if(Self.x > Other.x)
					{
						str_format(m_pResponse, m_SizeOfResponse, "%s i am literally next to you (on your right)", m_pMessageAuthor);
						return true;
					}
					else
					{
						str_format(m_pResponse, m_SizeOfResponse, "%s i am literally next to you (on your left)", m_pMessageAuthor);
						return true;
					}
				}
				else
				{
					if(Self.y > Other.y)
					{
						str_format(m_pResponse, m_SizeOfResponse, "%s i am literally next to you (below you)", m_pMessageAuthor);
						return true;
					}
					else
					{
						str_format(m_pResponse, m_SizeOfResponse, "%s i am literally next to you (above you)", m_pMessageAuthor);
						return true;
					}
				}
			}
			else if(distY > distX * 8)
			{
				if(Self.y > Other.y)
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s below you (%d tiles)", m_pMessageAuthor, (int)(distY / 32));
					return true;
				}
				else
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s above you (%d tiles)", m_pMessageAuthor, (int)(distY / 32));
					return true;
				}
			}
			else if(Self.x > Other.x)
			{
				if(distY < 15 * 32)
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your right (%d tiles away)", m_pMessageAuthor, (int)(distX / 32));
					return true;
				}
				else if(Self.y > Other.y)
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your right (%d tiles away) and %d tiles below you", m_pMessageAuthor, (int)(distX / 32), (int)(distY / 32));
					return true;
				}
				else
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your right (%d tiles away) and %d tiles above you", m_pMessageAuthor, (int)(distX / 32), (int)(distY / 32));
					return true;
				}
			}
			else
			{
				if(distY < 15 * 32)
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your left (%d tiles away)", m_pMessageAuthor, (int)(distX / 32));
					return true;
				}
				else if(Self.y > Other.y)
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your left (%d tiles away) and %d tiles below you", m_pMessageAuthor, (int)(distX / 32), (int)(distY / 32));
					return true;
				}
				else
				{
					str_format(m_pResponse, m_SizeOfResponse, "%s i am on your left (%d tiles away) and %d tiles above you", m_pMessageAuthor, (int)(distX / 32), (int)(distY / 32));
					return true;
				}
			}
		}
		str_format(m_pResponse, m_SizeOfResponse, "%s no idea. Where are you?", m_pMessageAuthor);
		return true;
	}

	// spec me
	if(str_find_nocase(m_pMessage, "spec") || str_find_nocase(m_pMessage, "watch") || (str_find_nocase(m_pMessage, "look") && !str_find_nocase(m_pMessage, "looks")) || str_find_nocase(m_pMessage, "schau"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "/pause %s", m_pMessageAuthor);
		ChatHelper()->GameClient()->m_Chat.Say(0, m_pResponse);
		str_format(m_pResponse, m_SizeOfResponse, "%s ok i am watching you", m_pMessageAuthor);
		return true;
	}
	// wanna? (always say no automated if motivated to do something type yes manually)
	if(str_find_nocase(m_pMessage, "wanna") || str_find_nocase(m_pMessage, "want"))
	{
		// TODO: fix tone
		// If you get asked to be given something "no sorry" sounds weird
		// If you are being asked to do something together "no thanks" sounds weird
		// the generic "no" might be a bit dry
		str_format(m_pResponse, m_SizeOfResponse, "%s no", m_pMessageAuthor);
		return true;
	}
	// help
	if(str_find_nocase(m_pMessage, "help") || str_find_nocase(m_pMessage, "hilfe"))
	{
		if(!str_find_nocase(m_pMessage, "helper"))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s where? what?", m_pMessageAuthor);
			return true;
		}
	}
	// small talk
	if(str_find_nocase(m_pMessage, "how are you") ||
		str_find_nocase(m_pMessage, "how r u") ||
		str_find_nocase(m_pMessage, "how ru ") ||
		str_find_nocase(m_pMessage, "how ru?") ||
		str_find_nocase(m_pMessage, "how r you") ||
		str_find_nocase(m_pMessage, "how are u") ||
		str_find_nocase(m_pMessage, "how is it going") ||
		str_find_nocase(m_pMessage, "ca va") ||
		(str_find_nocase(m_pMessage, "как") && str_find_nocase(m_pMessage, "дела")))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s good, and you? :)", m_pMessageAuthor);
		return true;
	}
	if(str_find_nocase(m_pMessage, "wie gehts") || str_find_nocase(m_pMessage, "wie geht es") || str_find_nocase(m_pMessage, "was geht"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s gut, und dir? :)", m_pMessageAuthor);
		return true;
	}
	if(str_find_nocase(m_pMessage, "about you") || str_find_nocase(m_pMessage, "and you") || str_find_nocase(m_pMessage, "and u") ||
		(str_find_nocase(m_pMessage, "u?") && MsgLen < NameLen + 5) ||
		(str_find_nocase(m_pMessage, "wbu") && MsgLen < NameLen + 8) ||
		(str_find_nocase(m_pMessage, "hbu") && MsgLen < NameLen + 8))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s good", m_pMessageAuthor);
		return true;
	}
	// chillerbot-ux features
	if(LangParser().IsQuestionHow(m_pMessage))
	{
		// feature: auto_drop_money
		if(str_find_nocase(m_pMessage, "drop") && (str_find_nocase(m_pMessage, "money") || str_find_nocase(m_pMessage, "moni") || str_find_nocase(m_pMessage, "coin") || str_find_nocase(m_pMessage, "cash") || str_find_nocase(m_pMessage, "geld")))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s I auto drop money using \"auto_drop_money\" in chillerbot-ux", m_pMessageAuthor);
			return true;
		}
		// feature: auto reply
		if((str_find_nocase(m_pMessage, "reply") && str_find_nocase(m_pMessage, "chat")) || (str_find_nocase(m_pMessage, "auto chat") || str_find_nocase(m_pMessage, "autochat")) ||
			str_find_nocase(m_pMessage, "message") ||
			((str_find_nocase(m_pMessage, "fast") || str_find_nocase(m_pMessage, "quick")) && str_find_nocase(m_pMessage, "chat")))
		{
			str_format(m_pResponse, m_SizeOfResponse, "%s I bound the chillerbot-ux command \"reply_to_last_ping\" to automate chat", m_pMessageAuthor);
			return true;
		}
	}
	// advertise chillerbot
	if(str_find_nocase(m_pMessage, "what client") || str_find_nocase(m_pMessage, "which client") || str_find_nocase(m_pMessage, "wat client") ||
		str_find_nocase(m_pMessage, "good client") || str_find_nocase(m_pMessage, "download client") || str_find_nocase(m_pMessage, "link client") || str_find_nocase(m_pMessage, "get client") ||
		str_find_nocase(m_pMessage, "where chillerbot") || str_find_nocase(m_pMessage, "download chillerbot") || str_find_nocase(m_pMessage, "link chillerbot") || str_find_nocase(m_pMessage, "get chillerbot") ||
		str_find_nocase(m_pMessage, "chillerbot url") || str_find_nocase(m_pMessage, "chillerbot download") || str_find_nocase(m_pMessage, "chillerbot link") || str_find_nocase(m_pMessage, "chillerbot website") ||
		((str_find_nocase(m_pMessage, "ddnet") || str_find_nocase(m_pMessage, "vanilla")) && str_find_nocase(m_pMessage, "?")))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s I use chillerbot-ux ( https://chillerbot.github.io )", m_pMessageAuthor);
		return true;
	}
	// whats your setting (mousesense, distance, dyn)
	if((str_find_nocase(m_pMessage, "?") ||
		   str_find_nocase(m_pMessage, "what") ||
		   str_find_nocase(m_pMessage, "which") ||
		   str_find_nocase(m_pMessage, "wat") ||
		   str_find_nocase(m_pMessage, "much") ||
		   str_find_nocase(m_pMessage, "many") ||
		   str_find_nocase(m_pMessage, "viel") ||
		   str_find_nocase(m_pMessage, "hoch")) &&
		(str_find_nocase(m_pMessage, "sens") || str_find_nocase(m_pMessage, "sesn") || str_find_nocase(m_pMessage, "snse") || str_find_nocase(m_pMessage, "senes") || str_find_nocase(m_pMessage, "inp") || str_find_nocase(m_pMessage, "speed")))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s my current inp_mousesens is %d", m_pMessageAuthor, g_Config.m_InpMousesens);
		return true;
	}
	if((str_find_nocase(m_pMessage, "?") || str_find_nocase(m_pMessage, "what") || str_find_nocase(m_pMessage, "which") || str_find_nocase(m_pMessage, "wat") || str_find_nocase(m_pMessage, "much") || str_find_nocase(m_pMessage, "many")) &&
		str_find_nocase(m_pMessage, "dist"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s my current cl_mouse_max_distance is %d", m_pMessageAuthor, g_Config.m_ClMouseMaxDistance);
		return true;
	}
	if((str_find_nocase(m_pMessage, "?") || str_find_nocase(m_pMessage, "do you") || str_find_nocase(m_pMessage, "do u")) &&
		str_find_nocase(m_pMessage, "dyn"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s my dyncam is currently %s", m_pMessageAuthor, g_Config.m_ClDyncam ? "on" : "off");
		return true;
	}
	// compliments
	if(str_find_nocase(m_pMessage, "good") ||
		str_find_nocase(m_pMessage, "happy") ||
		str_find_nocase(m_pMessage, "congrats") ||
		str_find_nocase(m_pMessage, "nice") ||
		str_find_nocase(m_pMessage, "pro"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s thanks", m_pMessageAuthor);
		return true;
	}
	// impatient
	if(str_find_nocase(m_pMessage, "answer") || str_find_nocase(m_pMessage, "ignore") || str_find_nocase(m_pMessage, "antwort") || str_find_nocase(m_pMessage, "ignorier"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s i am currently busy (automated reply)", m_pMessageAuthor);
		return true;
	}
	// ask to ask
	if(LangParser().IsAskToAsk(m_pMessage, m_pMessageAuthor, m_pResponse, m_SizeOfResponse))
		return true;
	// got weapon?
	if(str_find_nocase(m_pMessage, "got") || str_find_nocase(m_pMessage, "have") || str_find_nocase(m_pMessage, "hast"))
	{
		int Weapon = -1;
		if(str_find_nocase(m_pMessage, "hammer"))
			Weapon = WEAPON_HAMMER;
		else if(str_find_nocase(m_pMessage, "gun"))
			Weapon = WEAPON_GUN;
		else if(str_find_nocase(m_pMessage, "sg") || str_find_nocase(m_pMessage, "shotgun") || str_find_nocase(m_pMessage, "shotty"))
			Weapon = WEAPON_SHOTGUN;
		else if(str_find_nocase(m_pMessage, "nade") || str_find_nocase(m_pMessage, "rocket") || str_find_nocase(m_pMessage, "bazooka"))
			Weapon = WEAPON_GRENADE;
		else if(str_find_nocase(m_pMessage, "rifle") || str_find_nocase(m_pMessage, "laser") || str_find_nocase(m_pMessage, "sniper"))
			Weapon = WEAPON_LASER;
		CCharacter *pChar = ChatHelper()->GameClient()->m_GameWorld.GetCharacterByID(ChatHelper()->GameClient()->m_LocalIDs[g_Config.m_ClDummy]);
		if(pChar && Weapon != -1)
		{
			char aWeapons[1024];
			aWeapons[0] = '\0';
			if(pChar->GetWeaponGot(WEAPON_HAMMER))
				str_append(aWeapons, "hammer", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GUN))
				str_append(aWeapons, aWeapons[0] ? ", gun" : "gun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_SHOTGUN))
				str_append(aWeapons, aWeapons[0] ? ", shotgun" : "shotgun", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_GRENADE))
				str_append(aWeapons, aWeapons[0] ? ", grenade" : "grenade", sizeof(aWeapons));
			if(pChar->GetWeaponGot(WEAPON_LASER))
				str_append(aWeapons, aWeapons[0] ? ", rifle" : "rifle", sizeof(aWeapons));

			if(pChar->GetWeaponGot(Weapon))
				str_format(m_pResponse, m_SizeOfResponse, "%s Yes I got those weapons: %s", m_pMessageAuthor, aWeapons);
			else
				str_format(m_pResponse, m_SizeOfResponse, "%s No I got those weapons: %s", m_pMessageAuthor, aWeapons);
			return true;
		}
	}
	// weeb
	if(str_find_nocase(m_pMessage, "uwu"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s OwO", m_pMessageAuthor);
		return true;
	}
	if(str_find_nocase(m_pMessage, "owo"))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s UwU", m_pMessageAuthor);
		return true;
	}
	// no u
	if(MsgLen < NameLen + 8 && (str_find_nocase(m_pMessage, "no u") ||
					   str_find_nocase(m_pMessage, "no you") ||
					   str_find_nocase(m_pMessage, "noob") ||
					   str_find_nocase(m_pMessage, "nob") ||
					   str_find_nocase(m_pMessage, "nuub") ||
					   str_find_nocase(m_pMessage, "nub") ||
					   str_find_nocase(m_pMessage, "bad")))
	{
		str_format(m_pResponse, m_SizeOfResponse, "%s no u", m_pMessageAuthor);
		return true;
	}
	return false;
}
