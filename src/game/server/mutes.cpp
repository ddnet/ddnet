/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "gamecontext.h"

#include <base/log.h>

#include <engine/shared/config.h>

static NETADDR KeyAddress(NETADDR Addr)
{
	if(Addr.type == NETTYPE_WEBSOCKET_IPV4)
	{
		Addr.type = NETTYPE_IPV4;
	}
	else if(Addr.type == NETTYPE_WEBSOCKET_IPV6)
	{
		Addr.type = NETTYPE_IPV6;
	}
	Addr.port = 0;
	return Addr;
}

int CMute::SecondsLeft() const
{
	return m_Expire - time_timestamp();
}

CMutes::CMutes(const char *pSystemName) :
	m_pSystemName(pSystemName)
{
}

bool CMutes::Mute(const NETADDR *pAddr, int Seconds, const char *pReason, bool InitialDelay)
{
	if(!in_range(Seconds, 1, 365 * 24 * 60 * 60))
	{
		log_info(m_pSystemName, "Invalid mute duration: %d", Seconds);
		return false;
	}

	// Find a matching mute for this IP, update expiration time if found
	const int64_t Expire = time_timestamp() + Seconds;
	CMute &Mute = m_Mutes[KeyAddress(*pAddr)];
	if(!Mute.m_Initialized)
	{
		Mute.m_Initialized = true;
		Mute.m_Expire = Expire;
		str_copy(Mute.m_aReason, pReason);
		Mute.m_InitialDelay = InitialDelay;
		return true;
	}
	if(Expire > Mute.m_Expire)
	{
		Mute.m_Expire = Expire;
		str_copy(Mute.m_aReason, pReason);
		Mute.m_InitialDelay = InitialDelay;
	}
	return true;
}

void CMutes::UnmuteIndex(int Index)
{
	if(Index < 0 || Index >= (int)m_Mutes.size())
	{
		log_info(m_pSystemName, "Invalid index to unmute: %d", Index);
		return;
	}
	auto It = std::next(m_Mutes.begin(), Index);
	char aAddrString[NETADDR_MAXSTRSIZE];
	net_addr_str(&It->first, aAddrString, sizeof(aAddrString), false);
	log_info(m_pSystemName, "Unmuted: %s", aAddrString);
	m_Mutes.erase(It);
}

void CMutes::UnmuteAddr(const NETADDR *pAddr)
{
	NETADDR KeyAddr = KeyAddress(*pAddr);
	char aAddrString[NETADDR_MAXSTRSIZE];
	net_addr_str(&KeyAddr, aAddrString, sizeof(aAddrString), false);
	auto It = m_Mutes.find(KeyAddr);
	if(It == m_Mutes.end())
	{
		log_info(m_pSystemName, "No mutes for this IP address found: %s", aAddrString);
		return;
	}
	log_info(m_pSystemName, "Unmuted: %s", aAddrString);
	m_Mutes.erase(It);
}

std::optional<CMute> CMutes::IsMuted(const NETADDR *pAddr, bool RespectInitialDelay) const
{
	const auto It = m_Mutes.find(KeyAddress(*pAddr));
	if(It == m_Mutes.end())
	{
		return std::nullopt;
	}
	if(!RespectInitialDelay && !It->second.m_InitialDelay)
	{
		return std::nullopt;
	}
	return It->second;
}

void CMutes::UnmuteExpired()
{
	const int64_t Now = time_timestamp();
	for(auto It = m_Mutes.begin(); It != m_Mutes.end();)
	{
		if(It->second.m_Expire <= Now)
		{
			It = m_Mutes.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void CMutes::Print(int Page) const
{
	if(m_Mutes.empty())
	{
		log_info(m_pSystemName, "The mute list is empty.");
		return;
	}

	static constexpr int ENTRIES_PER_PAGE = 20;
	const int NumPages = std::ceil(m_Mutes.size() / (float)ENTRIES_PER_PAGE);
	if(!in_range(Page, 1, NumPages))
	{
		log_info(m_pSystemName, "Invalid page number. There %s %d %s available.",
			NumPages == 1 ? "is" : "are", NumPages, NumPages == 1 ? "page" : "pages");
		return;
	}

	const int Start = (Page - 1) * ENTRIES_PER_PAGE;
	const int End = Page * ENTRIES_PER_PAGE;

	int Count = 0;
	for(const auto &[Addr, MuteEntry] : m_Mutes)
	{
		if(Count < Start)
		{
			Count++;
			continue;
		}
		else if(Count >= End)
		{
			break;
		}
		char aAddrString[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aAddrString, sizeof(aAddrString), false);
		log_info(m_pSystemName, "#%d '%s' muted for %d seconds (%s)",
			Count, aAddrString, MuteEntry.SecondsLeft(), MuteEntry.m_aReason[0] == '\0' ? "No reason given" : MuteEntry.m_aReason);
		Count++;
	}
	log_info(m_pSystemName, "%d %s, showing entries %d - %d (page %d/%d)",
		(int)m_Mutes.size(), m_Mutes.size() == 1 ? "mute" : "mutes", Start, Count - 1, Page, NumPages);
}

void CGameContext::MuteWithMessage(const NETADDR *pAddr, int Seconds, const char *pReason, const char *pDisplayName)
{
	if(!m_Mutes.Mute(pAddr, Seconds, pReason, false))
	{
		return;
	}

	char aChatMessage[256];
	if(pReason[0] != '\0')
	{
		str_format(aChatMessage, sizeof(aChatMessage), "'%s' has been muted for %d seconds (%s)", pDisplayName, Seconds, pReason);
	}
	else
	{
		str_format(aChatMessage, sizeof(aChatMessage), "'%s' has been muted for %d seconds", pDisplayName, Seconds);
	}
	SendChat(-1, TEAM_ALL, aChatMessage);
}

void CGameContext::VoteMuteWithMessage(const NETADDR *pAddr, int Seconds, const char *pReason, const char *pDisplayName)
{
	if(!m_VoteMutes.Mute(pAddr, Seconds, pReason, false))
	{
		return;
	}

	char aChatMessage[256];
	if(pReason[0] != '\0')
	{
		str_format(aChatMessage, sizeof(aChatMessage), "'%s' has been banned from voting for %d seconds (%s)", pDisplayName, Seconds, pReason);
	}
	else
	{
		str_format(aChatMessage, sizeof(aChatMessage), "'%s' has been banned from voting for %d seconds", pDisplayName, Seconds);
	}
	SendChat(-1, TEAM_ALL, aChatMessage);
}

void CGameContext::ConMute(IConsole::IResult *pResult, void *pUserData)
{
	log_info("mutes", "This command is deprecated. Use either 'muteid <client_id> <seconds> <reason>' or 'muteip <ip> <seconds> <reason>'");
}

void CGameContext::ConMuteId(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int Victim = pResult->GetVictim();
	if(!CheckClientId(Victim) || !pSelf->m_apPlayers[Victim])
	{
		log_info("mutes", "Client ID not found: %d", Victim);
		return;
	}

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->MuteWithMessage(pSelf->Server()->ClientAddr(Victim), pResult->GetInteger(1), pReason, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConMuteIp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
	{
		log_info("mutes", "Invalid IP address to mute: %s", pResult->GetString(0));
		return;
	}

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->m_Mutes.Mute(&Addr, pResult->GetInteger(1), pReason, false);
}

void CGameContext::ConUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_Mutes.UnmuteIndex(pResult->GetInteger(0));
}

void CGameContext::ConUnmuteId(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int Victim = pResult->GetVictim();
	if(!CheckClientId(Victim) || !pSelf->m_apPlayers[Victim])
	{
		log_info("mutes", "Client ID not found: %d", Victim);
		return;
	}

	pSelf->m_Mutes.UnmuteAddr(pSelf->Server()->ClientAddr(Victim));
}

void CGameContext::ConUnmuteIp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
	{
		log_info("mutes", "Invalid IP address to unmute: %s", pResult->GetString(0));
		return;
	}

	pSelf->m_Mutes.UnmuteAddr(&Addr);
}

void CGameContext::ConMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_Mutes.Print(pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 1);
}

void CGameContext::ConVoteMute(IConsole::IResult *pResult, void *pUserData)
{
	log_info("votemutes", "Use either 'vote_muteid <client_id> <seconds> <reason>' or 'vote_muteip <ip> <seconds> <reason>'");
}

void CGameContext::ConVoteMuteId(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int Victim = pResult->GetVictim();
	if(!CheckClientId(Victim) || !pSelf->m_apPlayers[Victim])
	{
		log_info("votemutes", "Client ID not found: %d", Victim);
		return;
	}

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->VoteMuteWithMessage(pSelf->Server()->ClientAddr(Victim), pResult->GetInteger(1), pReason, pSelf->Server()->ClientName(Victim));
}

void CGameContext::ConVoteMuteIp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
	{
		log_info("votemutes", "Invalid IP address to mute: %s", pResult->GetString(0));
		return;
	}

	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "";
	pSelf->m_VoteMutes.Mute(&Addr, pResult->GetInteger(1), pReason, false);
}

void CGameContext::ConVoteUnmute(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_VoteMutes.UnmuteIndex(pResult->GetInteger(0));
}

void CGameContext::ConVoteUnmuteId(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int Victim = pResult->GetVictim();
	if(!CheckClientId(Victim) || !pSelf->m_apPlayers[Victim])
	{
		log_info("votemutes", "Client ID not found: %d", Victim);
		return;
	}

	pSelf->m_VoteMutes.UnmuteAddr(pSelf->Server()->ClientAddr(Victim));
}

void CGameContext::ConVoteUnmuteIp(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
	{
		log_info("votemutes", "Invalid IP address to unmute: %s", pResult->GetString(0));
		return;
	}

	pSelf->m_VoteMutes.UnmuteAddr(&Addr);
}

void CGameContext::ConVoteMutes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_VoteMutes.Print(pResult->NumArguments() > 0 ? pResult->GetInteger(0) : 1);
}
