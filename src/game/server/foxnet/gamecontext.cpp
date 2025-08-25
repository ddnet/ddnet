#include "../gamecontext.h"
#include <vector>
#include <string>
#include <base/system.h>
#include "fontconvert.h"

void CGameContext::FoxNetTick()
{
	m_VoteMenu.Tick();
}

void CGameContext::ClearVotes(int ClientId)
{
	if(ClientId == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && !Server()->ClientSlotEmpty(i))
				ClearVotes(i);
		}
		return;
	}

	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientId);
	m_VoteMenu.AddHeader(ClientId);
}

bool CGameContext::ChatDetection(int ClientId, const char *pMsg)
{
	// Thx to Pointer31 for the blueprint
	if(ClientId < 0)
		return false;

	if(m_ChatDetection.empty())
		return false;

	float count = 0; // amount of flagged strings (some strings may count more than others)
	int BanDuration = 0;
	char Reason[64] = "Chat Detection Auto Ban";
	bool IsBan = false;

	const char *ClientName = Server()->ClientName(ClientId);
	const char *pText = FontConvert(pMsg);
	// const char *pActualText = pMsg;

	// make a check for the latest message and the new message coming in right now
	// if the message is the same as the latest one but in a fancy font, then they use some bot client
	// aka ban them

	std::vector<std::string> FoundStrings;
	std::vector<int> Times;
	FoundStrings.clear();
	Times.clear();

	for(const auto &Entry : m_ChatDetection)
	{
		if(Entry.String()[0] == '\0')
			continue;

		if(str_find_nocase(pText, Entry.String()))
		{
			FoundStrings.push_back(Entry.String());
			Times.push_back(Entry.Time());

			count += Entry.Addition();
			BanDuration = Entry.Time();

			if(!IsBan) // if one of the strings is a ban string, then we set IsBan to true
				IsBan = Entry.IsBan();

			if(str_comp(Entry.Reason(), "") != 0)
				str_copy(Reason, Entry.Reason());
		}
	}
	if(!Times.empty())
		BanDuration = *std::max_element(Times.begin(), Times.end());
	else
		BanDuration = 0;

	char InfoMsg[256] = "";
	if(FoundStrings.size() > 0)
	{
		for(const auto &str : FoundStrings)
		{
			str_append(InfoMsg, str.c_str());
			if(&str != &FoundStrings.back())
				str_append(InfoMsg, ", ");
		}

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Name: %s | Strings Found: %s", ClientName, InfoMsg);
		dbg_msg("chat-detection", aBuf);
	}

	if(g_Config.m_SvAntiAdBot)
	{
		// anti whisper ad bot
		if((str_find_nocase(pText, "/whisper") || str_find_nocase(pText, "/w")) && str_find_nocase(pText, "bro, check out this client"))
		{
			str_copy(Reason, "Bot Client Message");
			IsBan = true;
			count += 2;
			BanDuration = 1000;
		}

		// anti mass ping ad bot
		if((str_find_nocase(pText, "stop being a noob") && str_find_nocase(pText, "get good with")) || (str_find_nocase(pText, "Think you could do better") && str_find_nocase(pText, "Not without"))) // mass ping advertising
		{
			if(str_length(pText) > 70) // Usually it pings alot of people
			{
				// try to not remove their message if they are just trying to be funny
				if(!str_find_nocase(pText, "github.com") && !str_find_nocase(pText, "tater") && !str_find_nocase(pText, "tclient") && !str_find_nocase(pText, "t-client") && !str_find_nocase(pText, "tclient.app") // TClient
					&& !str_find_nocase(pText, "aiodob") && !str_find_nocase(pText, "aidob") && !str_find_nocase(pText, "a-client") && !str_find(pText, "A Client") && !str_find(pText, "A client") // AClient
					&& !str_find_nocase(pText, "eclient") && !str_find_nocase(pText, "e client") && !str_find_nocase(pText, "entity client") && !str_find_nocase(pText, "e-client") // Other
					&& !str_find_nocase(pText, "chillerbot") && !str_find_nocase(pText, "cactus")) // Other
				{
					IsBan = true;
					count += 2;
					BanDuration = 1200;
				}
				if(str_find(pText, " ")) // This is the little white space it uses between some letters
				{
					IsBan = true;
					count += 2;
					BanDuration = 1200;
				}
				str_copy(Reason, "Bot Client Message");
			}
		}
	}

	if(count >= 2 && BanDuration > 0)
	{
		if(IsBan)
			Server()->Ban(ClientId, BanDuration * 60, Reason, "");
		else
			MuteWithMessage(Server()->ClientAddr(ClientId), BanDuration * 60, Reason, ClientName);
		
		return true; // Don't send their chat message
	}
	return false;
}

bool CGameContext::NameDetection(int ClientId, const char *pName, bool PreventNameChange)
{
	if(ClientId < 0)
		return false;

	if(m_NameDetection.empty())
		return false;

	const char *ClientName = pName;

	int BanDuration = 0;
	char Reason[64] = "Name Detection Auto Ban";

	std::vector<std::string> FoundStrings;
	std::vector<int> Times;
	FoundStrings.clear();
	Times.clear();

	for(const auto &Entry : m_NameDetection)
	{
		if(Entry.String()[0] == '\0')
			continue;

		bool FoundEntry = false;

		if(Entry.ExactMatch())
		{
			if(!str_comp(ClientName, Entry.String()))
				FoundEntry = true;
		}
		else if(str_find_nocase(ClientName, Entry.String()))
			FoundEntry = true;

		if(FoundEntry)
		{
			FoundStrings.push_back(Entry.String());
			Times.push_back(Entry.Time());

			BanDuration = Entry.Time();

			if(str_comp(Entry.Reason(), "") != 0)
				str_copy(Reason, Entry.Reason());
		}
	}

	if(!Times.empty())
		BanDuration = *std::max_element(Times.begin(), Times.end());
	else
		BanDuration = 0;

	if(FoundStrings.size() > 0)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "Name: %s | Banned String Found: ", ClientName);
		for(const auto &str : FoundStrings)
		{
			str_append(aBuf, str.c_str());
			str_append(aBuf, ", ");
		}
		dbg_msg("name-detection", aBuf);

		if(!PreventNameChange && BanDuration > 0)
			Server()->Ban(ClientId, BanDuration * 60, Reason, "");
		return true;
	}

	return false;
}

