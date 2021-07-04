/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "serverbrowser.h"

#include "serverbrowser_http.h"
#include "serverbrowser_ping_cache.h"

#include <algorithm>
#include <limits.h>

#include <base/hash_ctxt.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/memheap.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/serverinfo.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/storage.h>

#include <mastersrv/mastersrv.h>

#include <engine/external/json-parser/json.h>

class SortWrap
{
	typedef bool (CServerBrowser::*SortFunc)(int, int) const;
	SortFunc m_pfnSort;
	CServerBrowser *m_pThis;

public:
	SortWrap(CServerBrowser *t, SortFunc f) :
		m_pfnSort(f), m_pThis(t) {}
	bool operator()(int a, int b) { return (g_Config.m_BrSortOrder ? (m_pThis->*m_pfnSort)(b, a) : (m_pThis->*m_pfnSort)(a, b)); }
};

CServerBrowser::CServerBrowser()
{
	m_ppServerlist = 0;
	m_pSortedServerlist = 0;

	m_NumFavoriteServers = 0;

	mem_zero(m_aServerlistIp, sizeof(m_aServerlistIp));

	m_pFirstReqServer = 0; // request list
	m_pLastReqServer = 0;
	m_NumRequests = 0;

	m_NumSortedServers = 0;
	m_NumSortedServersCapacity = 0;
	m_NumServers = 0;
	m_NumServerCapacity = 0;

	m_Sorthash = 0;
	m_aFilterString[0] = 0;
	m_aFilterGametypeString[0] = 0;

	m_ServerlistType = 0;
	m_BroadcastTime = 0;
	secure_random_fill(m_aTokenSeed, sizeof(m_aTokenSeed));
	m_RequestNumber = 0;

	m_pDDNetInfo = 0;

	m_SortOnNextUpdate = false;
}

CServerBrowser::~CServerBrowser()
{
	if(m_ppServerlist)
		free(m_ppServerlist);

	if(m_pSortedServerlist)
		free(m_pSortedServerlist);

	if(m_pDDNetInfo)
		json_value_free(m_pDDNetInfo);

	delete m_pHttp;
	m_pHttp = nullptr;
	delete m_pPingCache;
	m_pPingCache = nullptr;
}

void CServerBrowser::SetBaseInfo(class CNetClient *pClient, const char *pNetVersion)
{
	m_pNetClient = pClient;
	str_copy(m_aNetVersion, pNetVersion, sizeof(m_aNetVersion));
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pFriends = Kernel()->RequestInterface<IFriends>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this);
	m_pPingCache = CreateServerBrowserPingCache(m_pConsole, m_pStorage);

	RegisterCommands();
}

void CServerBrowser::OnInit()
{
	m_pHttp = CreateServerBrowserHttp(m_pEngine, m_pConsole, m_pStorage, g_Config.m_BrCachedBestServerinfoUrl);
}

void CServerBrowser::RegisterCommands()
{
	m_pConsole->Register("leak_ip_address_to_all_servers", "", CFGFLAG_CLIENT, Con_LeakIpAddress, this, "Leaks your IP address to all servers by pinging each of them, also acquiring the latency in the process");
}

void CServerBrowser::Con_LeakIpAddress(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = (CServerBrowser *)pUserData;

	std::vector<int> aSortedServers;
	// Sort servers by IP address, ignoring port.
	class CAddrComparer
	{
	public:
		CServerBrowser *m_pThis;
		bool operator()(int i, int j)
		{
			NETADDR Addr1 = m_pThis->m_ppServerlist[i]->m_Addr;
			NETADDR Addr2 = m_pThis->m_ppServerlist[j]->m_Addr;
			Addr1.port = 0;
			Addr2.port = 0;
			return net_addr_comp(&Addr1, &Addr2) < 0;
		}
	};
	aSortedServers.reserve(pThis->m_NumServers);
	for(int i = 0; i < pThis->m_NumServers; i++)
	{
		aSortedServers.push_back(i);
	}
	std::sort(aSortedServers.begin(), aSortedServers.end(), CAddrComparer{pThis});

	// Group the servers into those with same IP address (but differing
	// port).
	NETADDR Addr;
	int Start = -1;
	for(int i = 0; i <= (int)aSortedServers.size(); i++)
	{
		NETADDR NextAddr;
		if(i < (int)aSortedServers.size())
		{
			NextAddr = pThis->m_ppServerlist[aSortedServers[i]]->m_Addr;
			NextAddr.port = 0;
		}
		bool New = Start == -1 || i == (int)aSortedServers.size() || net_addr_comp(&Addr, &NextAddr) != 0;
		if(Start != -1 && New)
		{
			int Chosen = Start + secure_rand_below(i - Start);
			CServerEntry *pChosen = pThis->m_ppServerlist[aSortedServers[Chosen]];
			pChosen->m_RequestIgnoreInfo = true;
			pThis->QueueRequest(pChosen);
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&pChosen->m_Addr, aAddr, sizeof(aAddr), true);
			dbg_msg("serverbrowse/dbg", "queuing ping request for %s", aAddr);
		}
		if(i < (int)aSortedServers.size() && New)
		{
			Start = i;
			Addr = NextAddr;
		}
	}
}

const CServerInfo *CServerBrowser::SortedGet(int Index) const
{
	if(Index < 0 || Index >= m_NumSortedServers)
		return 0;
	return &m_ppServerlist[m_pSortedServerlist[Index]]->m_Info;
}

int CServerBrowser::GenerateToken(const NETADDR &Addr) const
{
	SHA256_CTX Sha256;
	sha256_init(&Sha256);
	sha256_update(&Sha256, m_aTokenSeed, sizeof(m_aTokenSeed));
	sha256_update(&Sha256, (unsigned char *)&Addr, sizeof(Addr));
	SHA256_DIGEST Digest = sha256_finish(&Sha256);
	return (Digest.data[0] << 16) | (Digest.data[1] << 8) | Digest.data[2];
}

int CServerBrowser::GetBasicToken(int Token)
{
	return Token & 0xff;
}

int CServerBrowser::GetExtraToken(int Token)
{
	return Token >> 8;
}

bool CServerBrowser::SortCompareName(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	//	make sure empty entries are listed last
	return (a->m_GotInfo && b->m_GotInfo) || (!a->m_GotInfo && !b->m_GotInfo) ? str_comp(a->m_Info.m_aName, b->m_Info.m_aName) < 0 :
										    a->m_GotInfo ? true : false;
}

bool CServerBrowser::SortCompareMap(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	return str_comp(a->m_Info.m_aMap, b->m_Info.m_aMap) < 0;
}

bool CServerBrowser::SortComparePing(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	return a->m_Info.m_Latency < b->m_Info.m_Latency;
}

bool CServerBrowser::SortCompareGametype(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	return str_comp(a->m_Info.m_aGameType, b->m_Info.m_aGameType) < 0;
}

bool CServerBrowser::SortCompareNumPlayers(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	return a->m_Info.m_NumFilteredPlayers > b->m_Info.m_NumFilteredPlayers;
}

bool CServerBrowser::SortCompareNumClients(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];
	return a->m_Info.m_NumClients > b->m_Info.m_NumClients;
}

bool CServerBrowser::SortCompareNumPlayersAndPing(int Index1, int Index2) const
{
	CServerEntry *a = m_ppServerlist[Index1];
	CServerEntry *b = m_ppServerlist[Index2];

	if(a->m_Info.m_NumFilteredPlayers == b->m_Info.m_NumFilteredPlayers)
		return a->m_Info.m_Latency > b->m_Info.m_Latency;
	else if(a->m_Info.m_NumFilteredPlayers == 0 || b->m_Info.m_NumFilteredPlayers == 0 || a->m_Info.m_Latency / 100 == b->m_Info.m_Latency / 100)
		return a->m_Info.m_NumFilteredPlayers < b->m_Info.m_NumFilteredPlayers;
	else
		return a->m_Info.m_Latency > b->m_Info.m_Latency;
}

void CServerBrowser::Filter()
{
	int i = 0, p = 0;
	m_NumSortedServers = 0;

	// allocate the sorted list
	if(m_NumSortedServersCapacity < m_NumServers)
	{
		if(m_pSortedServerlist)
			free(m_pSortedServerlist);
		m_NumSortedServersCapacity = m_NumServers;
		m_pSortedServerlist = (int *)calloc(m_NumSortedServersCapacity, sizeof(int));
	}

	// filter the servers
	for(i = 0; i < m_NumServers; i++)
	{
		int Filtered = 0;

		if(g_Config.m_BrFilterEmpty && m_ppServerlist[i]->m_Info.m_NumFilteredPlayers == 0)
			Filtered = 1;
		else if(g_Config.m_BrFilterFull && Players(m_ppServerlist[i]->m_Info) == Max(m_ppServerlist[i]->m_Info))
			Filtered = 1;
		else if(g_Config.m_BrFilterPw && m_ppServerlist[i]->m_Info.m_Flags & SERVER_FLAG_PASSWORD)
			Filtered = 1;
		else if(g_Config.m_BrFilterPing && g_Config.m_BrFilterPing < m_ppServerlist[i]->m_Info.m_Latency)
			Filtered = 1;
		else if(g_Config.m_BrFilterCompatversion && str_comp_num(m_ppServerlist[i]->m_Info.m_aVersion, m_aNetVersion, 3) != 0)
			Filtered = 1;
		else if(g_Config.m_BrFilterServerAddress[0] && !str_find_nocase(m_ppServerlist[i]->m_Info.m_aAddress, g_Config.m_BrFilterServerAddress))
			Filtered = 1;
		else if(g_Config.m_BrFilterGametypeStrict && g_Config.m_BrFilterGametype[0] && str_comp_nocase(m_ppServerlist[i]->m_Info.m_aGameType, g_Config.m_BrFilterGametype))
			Filtered = 1;
		else if(!g_Config.m_BrFilterGametypeStrict && g_Config.m_BrFilterGametype[0] && !str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aGameType, g_Config.m_BrFilterGametype))
			Filtered = 1;
		else if(g_Config.m_BrFilterUnfinishedMap && m_ppServerlist[i]->m_Info.m_HasRank == 1)
			Filtered = 1;
		else
		{
			if(g_Config.m_BrFilterCountry)
			{
				Filtered = 1;
				// match against player country
				for(p = 0; p < minimum(m_ppServerlist[i]->m_Info.m_NumClients, (int)MAX_CLIENTS); p++)
				{
					if(m_ppServerlist[i]->m_Info.m_aClients[p].m_Country == g_Config.m_BrFilterCountryIndex)
					{
						Filtered = 0;
						break;
					}
				}
			}

			if(!Filtered && g_Config.m_BrFilterString[0] != 0)
			{
				int MatchFound = 0;

				m_ppServerlist[i]->m_Info.m_QuickSearchHit = 0;

				// match against server name
				if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aName, g_Config.m_BrFilterString))
				{
					MatchFound = 1;
					m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_SERVERNAME;
				}

				// match against players
				for(p = 0; p < minimum(m_ppServerlist[i]->m_Info.m_NumClients, (int)MAX_CLIENTS); p++)
				{
					if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aClients[p].m_aName, g_Config.m_BrFilterString) ||
						str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aClients[p].m_aClan, g_Config.m_BrFilterString))
					{
						MatchFound = 1;
						m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_PLAYER;
						break;
					}
				}

				// match against map
				if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aMap, g_Config.m_BrFilterString))
				{
					MatchFound = 1;
					m_ppServerlist[i]->m_Info.m_QuickSearchHit |= IServerBrowser::QUICK_MAPNAME;
				}

				if(!MatchFound)
					Filtered = 1;
			}

			if(!Filtered && g_Config.m_BrExcludeString[0] != 0)
			{
				int MatchFound = 0;

				// match against server name
				if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aName, g_Config.m_BrExcludeString))
				{
					MatchFound = 1;
				}

				// match against map
				if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aMap, g_Config.m_BrExcludeString))
				{
					MatchFound = 1;
				}

				// match against gametype
				if(str_utf8_find_nocase(m_ppServerlist[i]->m_Info.m_aGameType, g_Config.m_BrExcludeString))
				{
					MatchFound = 1;
				}

				if(MatchFound)
					Filtered = 1;
			}
		}

		if(Filtered == 0)
		{
			// check for friend
			m_ppServerlist[i]->m_Info.m_FriendState = IFriends::FRIEND_NO;
			for(p = 0; p < minimum(m_ppServerlist[i]->m_Info.m_NumClients, (int)MAX_CLIENTS); p++)
			{
				m_ppServerlist[i]->m_Info.m_aClients[p].m_FriendState = m_pFriends->GetFriendState(m_ppServerlist[i]->m_Info.m_aClients[p].m_aName,
					m_ppServerlist[i]->m_Info.m_aClients[p].m_aClan);
				m_ppServerlist[i]->m_Info.m_FriendState = maximum(m_ppServerlist[i]->m_Info.m_FriendState, m_ppServerlist[i]->m_Info.m_aClients[p].m_FriendState);
			}

			if(!g_Config.m_BrFilterFriends || m_ppServerlist[i]->m_Info.m_FriendState != IFriends::FRIEND_NO)
				m_pSortedServerlist[m_NumSortedServers++] = i;
		}
	}
}

int CServerBrowser::SortHash() const
{
	int i = g_Config.m_BrSort & 0xff;
	i |= g_Config.m_BrFilterEmpty << 4;
	i |= g_Config.m_BrFilterFull << 5;
	i |= g_Config.m_BrFilterSpectators << 6;
	i |= g_Config.m_BrFilterFriends << 7;
	i |= g_Config.m_BrFilterPw << 8;
	i |= g_Config.m_BrSortOrder << 9;
	i |= g_Config.m_BrFilterCompatversion << 11;
	i |= g_Config.m_BrFilterGametypeStrict << 12;
	i |= g_Config.m_BrFilterUnfinishedMap << 13;
	i |= g_Config.m_BrFilterCountry << 14;
	i |= g_Config.m_BrFilterConnectingPlayers << 15;
	return i;
}

void SetFilteredPlayers(const CServerInfo &Item)
{
	Item.m_NumFilteredPlayers = g_Config.m_BrFilterSpectators ? Item.m_NumPlayers : Item.m_NumClients;
	if(g_Config.m_BrFilterConnectingPlayers)
	{
		for(const auto &Client : Item.m_aClients)
		{
			if((!g_Config.m_BrFilterSpectators || Client.m_Player) && str_comp(Client.m_aName, "(connecting)") == 0 && Client.m_aClan[0] == '\0')
				Item.m_NumFilteredPlayers--;
		}
	}
}

void CServerBrowser::Sort()
{
	int i;

	// fill m_NumFilteredPlayers
	for(i = 0; i < m_NumServers; i++)
	{
		SetFilteredPlayers(m_ppServerlist[i]->m_Info);
	}

	// create filtered list
	Filter();

	// sort
	if(g_Config.m_BrSortOrder == 2 && (g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS || g_Config.m_BrSort == IServerBrowser::SORT_PING))
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortCompareNumPlayersAndPing));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_NAME)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortCompareName));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_PING)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortComparePing));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_MAP)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortCompareMap));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortCompareNumPlayers));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_GAMETYPE)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, SortWrap(this, &CServerBrowser::SortCompareGametype));

	str_copy(m_aFilterGametypeString, g_Config.m_BrFilterGametype, sizeof(m_aFilterGametypeString));
	str_copy(m_aFilterString, g_Config.m_BrFilterString, sizeof(m_aFilterString));
	m_Sorthash = SortHash();
}

void CServerBrowser::RemoveRequest(CServerEntry *pEntry)
{
	if(pEntry->m_pPrevReq || pEntry->m_pNextReq || m_pFirstReqServer == pEntry)
	{
		if(pEntry->m_pPrevReq)
			pEntry->m_pPrevReq->m_pNextReq = pEntry->m_pNextReq;
		else
			m_pFirstReqServer = pEntry->m_pNextReq;

		if(pEntry->m_pNextReq)
			pEntry->m_pNextReq->m_pPrevReq = pEntry->m_pPrevReq;
		else
			m_pLastReqServer = pEntry->m_pPrevReq;

		pEntry->m_pPrevReq = 0;
		pEntry->m_pNextReq = 0;
		m_NumRequests--;
	}
}

CServerBrowser::CServerEntry *CServerBrowser::Find(const NETADDR &Addr)
{
	CServerEntry *pEntry = m_aServerlistIp[Addr.ip[0]];

	for(; pEntry; pEntry = pEntry->m_pNextIp)
	{
		if(net_addr_comp(&pEntry->m_Addr, &Addr) == 0)
			return pEntry;
	}
	return (CServerEntry *)0;
}

void CServerBrowser::QueueRequest(CServerEntry *pEntry)
{
	// add it to the list of servers that we should request info from
	pEntry->m_pPrevReq = m_pLastReqServer;
	if(m_pLastReqServer)
		m_pLastReqServer->m_pNextReq = pEntry;
	else
		m_pFirstReqServer = pEntry;
	m_pLastReqServer = pEntry;
	pEntry->m_pNextReq = 0;
	m_NumRequests++;
}

void CServerBrowser::SetInfo(CServerEntry *pEntry, const CServerInfo &Info)
{
	bool Fav = pEntry->m_Info.m_Favorite;
	bool Off = pEntry->m_Info.m_Official;
	pEntry->m_Info = Info;
	pEntry->m_Info.m_Favorite = Fav;
	pEntry->m_Info.m_Official = Off;
	pEntry->m_Info.m_NetAddr = pEntry->m_Addr;
	net_addr_str(&pEntry->m_Info.m_NetAddr, pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aAddress), 1);

	class CPlayerScoreNameLess
	{
	public:
		bool operator()(const CServerInfo::CClient &p0, const CServerInfo::CClient &p1)
		{
			if(p0.m_Player && !p1.m_Player)
				return true;
			if(!p0.m_Player && p1.m_Player)
				return false;

			int Score0 = p0.m_Score;
			int Score1 = p1.m_Score;
			if(Score0 == -9999)
				Score0 = INT_MIN;
			if(Score1 == -9999)
				Score1 = INT_MIN;

			if(Score0 > Score1)
				return true;
			if(Score0 < Score1)
				return false;
			return str_comp_nocase(p0.m_aName, p1.m_aName) < 0;
		}
	};

	std::sort(pEntry->m_Info.m_aClients, pEntry->m_Info.m_aClients + Info.m_NumReceivedClients, CPlayerScoreNameLess());

	pEntry->m_GotInfo = 1;
}

void CServerBrowser::SetLatency(NETADDR Addr, int Latency)
{
	Addr.port = 0;
	for(CServerEntry *pEntry = m_aServerlistIp[Addr.ip[0]]; pEntry; pEntry = pEntry->m_pNextIp)
	{
		NETADDR Other = pEntry->m_Addr;
		Other.port = 0;
		if(net_addr_comp(&Addr, &Other) == 0 && pEntry->m_GotInfo)
		{
			pEntry->m_Info.m_Latency = Latency;
			pEntry->m_Info.m_LatencyIsEstimated = false;
		}
	}
	m_pPingCache->CachePing(Addr, Latency);
}

CServerBrowser::CServerEntry *CServerBrowser::Add(const NETADDR &Addr)
{
	int Hash = Addr.ip[0];
	CServerEntry *pEntry = 0;

	// create new pEntry
	pEntry = (CServerEntry *)m_ServerlistHeap.Allocate(sizeof(CServerEntry));
	mem_zero(pEntry, sizeof(CServerEntry));

	// set the info
	pEntry->m_Addr = Addr;
	pEntry->m_Info.m_NetAddr = Addr;

	pEntry->m_Info.m_Latency = 999;
	pEntry->m_Info.m_HasRank = -1;
	net_addr_str(&Addr, pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aAddress), true);
	str_copy(pEntry->m_Info.m_aName, pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aName));

	// check if it's a favorite
	pEntry->m_Info.m_Favorite = IsFavorite(Addr);

	// check if it's an official server
	for(auto &Network : m_aNetworks)
	{
		for(int i = 0; i < Network.m_NumCountries; i++)
		{
			CNetworkCountry *pCntr = &Network.m_aCountries[i];
			for(int j = 0; j < pCntr->m_NumServers; j++)
			{
				if(net_addr_comp(&Addr, &pCntr->m_aServers[j]) == 0)
				{
					pEntry->m_Info.m_Official = true;
					break;
				}
			}
		}
	}

	// add to the hash list
	pEntry->m_pNextIp = m_aServerlistIp[Hash];
	m_aServerlistIp[Hash] = pEntry;

	if(m_NumServers == m_NumServerCapacity)
	{
		CServerEntry **ppNewlist;
		m_NumServerCapacity += 100;
		ppNewlist = (CServerEntry **)calloc(m_NumServerCapacity, sizeof(CServerEntry *)); // NOLINT(bugprone-sizeof-expression)
		if(m_NumServers > 0)
			mem_copy(ppNewlist, m_ppServerlist, m_NumServers * sizeof(CServerEntry *)); // NOLINT(bugprone-sizeof-expression)
		free(m_ppServerlist);
		m_ppServerlist = ppNewlist;
	}

	// add to list
	m_ppServerlist[m_NumServers] = pEntry;
	pEntry->m_Info.m_ServerIndex = m_NumServers;
	m_NumServers++;

	return pEntry;
}

void CServerBrowser::Set(const NETADDR &Addr, int Type, int Token, const CServerInfo *pInfo)
{
	CServerEntry *pEntry = 0;
	if(Type == IServerBrowser::SET_MASTER_ADD)
	{
		if(m_ServerlistType != IServerBrowser::TYPE_INTERNET)
			return;
		if(!Find(Addr))
		{
			pEntry = Add(Addr);
			QueueRequest(pEntry);
		}
	}
	else if(Type == IServerBrowser::SET_FAV_ADD)
	{
		if(m_ServerlistType != IServerBrowser::TYPE_FAVORITES)
			return;

		if(!Find(Addr))
		{
			pEntry = Add(Addr);
			QueueRequest(pEntry);
		}
	}
	else if(Type == IServerBrowser::SET_DDNET_ADD)
	{
		if(m_ServerlistType != IServerBrowser::TYPE_DDNET)
			return;

		if(!Find(Addr))
		{
			pEntry = Add(Addr);
			QueueRequest(pEntry);
		}
	}
	else if(Type == IServerBrowser::SET_KOG_ADD)
	{
		if(m_ServerlistType != IServerBrowser::TYPE_KOG)
			return;

		if(!Find(Addr))
		{
			pEntry = Add(Addr);
			QueueRequest(pEntry);
		}
	}
	else if(Type == IServerBrowser::SET_HTTPINFO)
	{
		if(!pEntry)
		{
			pEntry = Add(Addr);
		}
		if(pEntry)
		{
			SetInfo(pEntry, *pInfo);
			pEntry->m_RequestIgnoreInfo = true;
		}
	}
	else if(Type == IServerBrowser::SET_TOKEN)
	{
		int BasicToken = Token;
		int ExtraToken = 0;
		if(pInfo->m_Type == SERVERINFO_EXTENDED)
		{
			BasicToken = Token & 0xff;
			ExtraToken = Token >> 8;
		}

		pEntry = Find(Addr);

		if(m_ServerlistType == IServerBrowser::TYPE_LAN)
		{
			NETADDR Broadcast;
			mem_zero(&Broadcast, sizeof(Broadcast));
			Broadcast.type = m_pNetClient->NetType() | NETTYPE_LINK_BROADCAST;
			int Token = GenerateToken(Broadcast);
			bool Drop = false;
			Drop = Drop || BasicToken != GetBasicToken(Token);
			Drop = Drop || (pInfo->m_Type == SERVERINFO_EXTENDED && ExtraToken != GetExtraToken(Token));
			if(Drop)
			{
				return;
			}

			if(!pEntry)
				pEntry = Add(Addr);
		}
		else
		{
			if(!pEntry)
			{
				return;
			}
			int Token = GenerateToken(Addr);
			bool Drop = false;
			Drop = Drop || BasicToken != GetBasicToken(Token);
			Drop = Drop || (pInfo->m_Type == SERVERINFO_EXTENDED && ExtraToken != GetExtraToken(Token));
			if(Drop)
			{
				return;
			}
		}

		if(m_ServerlistType == IServerBrowser::TYPE_LAN)
		{
			SetInfo(pEntry, *pInfo);
			pEntry->m_Info.m_Latency = minimum(static_cast<int>((time_get() - m_BroadcastTime) * 1000 / time_freq()), 999);
			if(pInfo->m_Type == SERVERINFO_VANILLA && Is64Player(pInfo))
			{
				pEntry->m_Request64Legacy = true;
				// Force a quick update.
				RequestImpl64(pEntry->m_Addr, pEntry);
			}
		}
		else if(pEntry->m_RequestTime > 0)
		{
			if(!pEntry->m_RequestIgnoreInfo)
			{
				SetInfo(pEntry, *pInfo);
			}

			int Latency = minimum(static_cast<int>((time_get() - pEntry->m_RequestTime) * 1000 / time_freq()), 999);
			if(!pEntry->m_RequestIgnoreInfo)
			{
				pEntry->m_Info.m_Latency = Latency;
			}
			else
			{
				char aAddr[NETADDR_MAXSTRSIZE];
				net_addr_str(&Addr, aAddr, sizeof(aAddr), true);
				dbg_msg("serverbrowse/dbg", "received ping response from %s", aAddr);
				SetLatency(Addr, Latency);
			}
			pEntry->m_RequestTime = -1; // Request has been answered

			if(!pEntry->m_RequestIgnoreInfo)
			{
				if(pInfo->m_Type == SERVERINFO_VANILLA && Is64Player(pInfo))
				{
					pEntry->m_Request64Legacy = true;
					// Force a quick update.
					RequestImpl64(pEntry->m_Addr, pEntry);
				}
			}
		}
		RemoveRequest(pEntry);
	}

	m_SortOnNextUpdate = true;
}

void CServerBrowser::Refresh(int Type)
{
	// clear out everything
	m_ServerlistHeap.Reset();
	m_NumServers = 0;
	m_NumSortedServers = 0;
	mem_zero(m_aServerlistIp, sizeof(m_aServerlistIp));
	m_pFirstReqServer = 0;
	m_pLastReqServer = 0;
	m_NumRequests = 0;
	m_CurrentMaxRequests = g_Config.m_BrMaxRequests;
	m_RequestNumber++;

	m_ServerlistType = Type;
	secure_random_fill(m_aTokenSeed, sizeof(m_aTokenSeed));

	if(Type == IServerBrowser::TYPE_LAN)
	{
		unsigned char Buffer[sizeof(SERVERBROWSE_GETINFO) + 1];
		CNetChunk Packet;
		int i;

		/* do the broadcast version */
		Packet.m_ClientID = -1;
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_Address.type = m_pNetClient->NetType() | NETTYPE_LINK_BROADCAST;
		Packet.m_Flags = NETSENDFLAG_CONNLESS | NETSENDFLAG_EXTENDED;
		Packet.m_DataSize = sizeof(Buffer);
		Packet.m_pData = Buffer;
		mem_zero(&Packet.m_aExtraData, sizeof(Packet.m_aExtraData));

		int Token = GenerateToken(Packet.m_Address);
		mem_copy(Buffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));
		Buffer[sizeof(SERVERBROWSE_GETINFO)] = GetBasicToken(Token);

		Packet.m_aExtraData[0] = GetExtraToken(Token) >> 8;
		Packet.m_aExtraData[1] = GetExtraToken(Token) & 0xff;

		m_BroadcastTime = time_get();

		for(i = 8303; i <= 8310; i++)
		{
			Packet.m_Address.port = i;
			m_pNetClient->Send(&Packet);
		}

		if(g_Config.m_Debug)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client_srvbrowse", "broadcasting for servers");
	}
	else if(Type == IServerBrowser::TYPE_FAVORITES || Type == IServerBrowser::TYPE_INTERNET || Type == IServerBrowser::TYPE_DDNET || Type == IServerBrowser::TYPE_KOG)
	{
		m_pHttp->Refresh();
		m_pPingCache->Load();
		m_RefreshingHttp = true;
	}
}

void CServerBrowser::RequestImpl(const NETADDR &Addr, CServerEntry *pEntry, int *pBasicToken, int *pToken, bool RandomToken) const
{
	unsigned char Buffer[sizeof(SERVERBROWSE_GETINFO) + 1];
	CNetChunk Packet;

	if(g_Config.m_Debug)
	{
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aAddrStr, sizeof(aAddrStr), true);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "requesting server info from %s", aAddrStr);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client_srvbrowse", aBuf);
	}

	int Token = GenerateToken(Addr);
	if(RandomToken)
	{
		int AvoidBasicToken = GetBasicToken(Token);
		do
		{
			secure_random_fill(&Token, sizeof(Token));
			Token &= 0xffffff;
		} while(GetBasicToken(Token) == AvoidBasicToken);
	}
	if(pToken)
	{
		*pToken = Token;
	}
	if(pBasicToken)
	{
		*pBasicToken = GetBasicToken(Token);
	}

	mem_copy(Buffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));
	Buffer[sizeof(SERVERBROWSE_GETINFO)] = GetBasicToken(Token);

	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS | NETSENDFLAG_EXTENDED;
	Packet.m_DataSize = sizeof(Buffer);
	Packet.m_pData = Buffer;
	mem_zero(&Packet.m_aExtraData, sizeof(Packet.m_aExtraData));
	Packet.m_aExtraData[0] = GetExtraToken(Token) >> 8;
	Packet.m_aExtraData[1] = GetExtraToken(Token) & 0xff;

	m_pNetClient->Send(&Packet);

	if(pEntry)
		pEntry->m_RequestTime = time_get();
}

void CServerBrowser::RequestImpl64(const NETADDR &Addr, CServerEntry *pEntry) const
{
	unsigned char Buffer[sizeof(SERVERBROWSE_GETINFO_64_LEGACY) + 1];
	CNetChunk Packet;

	if(g_Config.m_Debug)
	{
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aAddrStr, sizeof(aAddrStr), true);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "requesting server info 64 from %s", aAddrStr);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client_srvbrowse", aBuf);
	}

	mem_copy(Buffer, SERVERBROWSE_GETINFO_64_LEGACY, sizeof(SERVERBROWSE_GETINFO_64_LEGACY));
	Buffer[sizeof(SERVERBROWSE_GETINFO_64_LEGACY)] = GetBasicToken(GenerateToken(Addr));

	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(Buffer);
	Packet.m_pData = Buffer;

	m_pNetClient->Send(&Packet);

	if(pEntry)
		pEntry->m_RequestTime = time_get();
}

void CServerBrowser::RequestCurrentServer(const NETADDR &Addr) const
{
	RequestImpl(Addr, nullptr, nullptr, nullptr, false);
}

void CServerBrowser::RequestCurrentServerWithRandomToken(const NETADDR &Addr, int *pBasicToken, int *pToken) const
{
	RequestImpl(Addr, nullptr, pBasicToken, pToken, true);
}

void CServerBrowser::SetCurrentServerPing(const NETADDR &Addr, int Ping)
{
	SetLatency(Addr, std::min(Ping, 999));
}

void ServerBrowserFillEstimatedLatency(int OwnLocation, const IServerBrowserPingCache::CEntry *pEntries, int NumEntries, int *pIndex, NETADDR Addr, CServerInfo *pInfo)
{
	Addr.port = 0;
	while(*pIndex < NumEntries && net_addr_comp(&pEntries[*pIndex].m_Addr, &Addr) < 0)
	{
		*pIndex += 1;
	}
	if(*pIndex >= NumEntries || net_addr_comp(&pEntries[*pIndex].m_Addr, &Addr) != 0)
	{
		pInfo->m_LatencyIsEstimated = true;
		pInfo->m_Latency = CServerInfo::EstimateLatency(OwnLocation, pInfo->m_Location);
		return;
	}
	pInfo->m_LatencyIsEstimated = false;
	pInfo->m_Latency = pEntries[*pIndex].m_Ping;
}

void CServerBrowser::UpdateFromHttp()
{
	const IServerBrowserPingCache::CEntry *pPingEntries;
	int NumPingEntries;
	m_pPingCache->GetPingCache(&pPingEntries, &NumPingEntries);
	int OwnLocation;
	if(str_comp(g_Config.m_BrLocation, "auto") == 0)
	{
		OwnLocation = m_OwnLocation;
	}
	else
	{
		if(CServerInfo::ParseLocation(&OwnLocation, g_Config.m_BrLocation))
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "cannot parse br_location: '%s'", g_Config.m_BrLocation);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowse", aBuf);
		}
	}

	int NumServers = m_pHttp->NumServers();
	int NumLegacyServers = m_pHttp->NumLegacyServers();
	if(m_ServerlistType != IServerBrowser::TYPE_INTERNET)
	{
		class CWantedAddr
		{
		public:
			NETADDR m_Addr;
			bool m_FallbackToPing;
			bool m_Got;
		};
		std::vector<CWantedAddr> aWantedAddresses;
		int LegacySetType;
		if(m_ServerlistType == IServerBrowser::TYPE_FAVORITES)
		{
			for(int i = 0; i < m_NumFavoriteServers; i++)
			{
				aWantedAddresses.push_back(CWantedAddr{m_aFavoriteServers[i], m_aFavoriteServersAllowPing[i], false});
			}
			LegacySetType = IServerBrowser::SET_FAV_ADD;
		}
		else
		{
			int Network;
			char *pExcludeCountries;
			char *pExcludeTypes;
			switch(m_ServerlistType)
			{
			case IServerBrowser::TYPE_DDNET:
				Network = NETWORK_DDNET;
				LegacySetType = IServerBrowser::SET_DDNET_ADD;
				pExcludeCountries = g_Config.m_BrFilterExcludeCountries;
				pExcludeTypes = g_Config.m_BrFilterExcludeTypes;
				break;
			case IServerBrowser::TYPE_KOG:
				Network = NETWORK_KOG;
				LegacySetType = IServerBrowser::SET_KOG_ADD;
				pExcludeCountries = g_Config.m_BrFilterExcludeCountriesKoG;
				pExcludeTypes = g_Config.m_BrFilterExcludeTypesKoG;
				break;
			default:
				dbg_assert(0, "invalid network");
				return;
			}
			// remove unknown elements of exclude list
			CountryFilterClean(Network);
			TypeFilterClean(Network);

			int MaxServers = 0;
			for(int i = 0; i < m_aNetworks[Network].m_NumCountries; i++)
			{
				CNetworkCountry *pCntr = &m_aNetworks[Network].m_aCountries[i];
				MaxServers = maximum(MaxServers, pCntr->m_NumServers);
			}

			for(int g = 0; g < MaxServers; g++)
			{
				for(int i = 0; i < m_aNetworks[Network].m_NumCountries; i++)
				{
					CNetworkCountry *pCntr = &m_aNetworks[Network].m_aCountries[i];

					// check for filter
					if(DDNetFiltered(pExcludeCountries, pCntr->m_aName))
						continue;

					if(g >= pCntr->m_NumServers)
						continue;

					if(DDNetFiltered(pExcludeTypes, pCntr->m_aTypes[g]))
						continue;
					aWantedAddresses.push_back(CWantedAddr{pCntr->m_aServers[g], false, false});
				}
			}
		}
		std::vector<int> aSortedServers;
		std::vector<int> aSortedLegacyServers;
		aSortedServers.reserve(NumServers);
		for(int i = 0; i < NumServers; i++)
		{
			aSortedServers.push_back(i);
		}
		aSortedLegacyServers.reserve(NumLegacyServers);
		for(int i = 0; i < NumLegacyServers; i++)
		{
			aSortedLegacyServers.push_back(i);
		}

		class CWantedAddrComparer
		{
		public:
			bool operator()(const CWantedAddr &a, const CWantedAddr &b)
			{
				return net_addr_comp(&a.m_Addr, &b.m_Addr) < 0;
			}
		};
		class CAddrComparer
		{
		public:
			IServerBrowserHttp *m_pHttp;
			bool operator()(int i, int j)
			{
				return net_addr_comp(&m_pHttp->ServerAddress(i), &m_pHttp->ServerAddress(j)) < 0;
			}
		};
		class CLegacyAddrComparer
		{
		public:
			IServerBrowserHttp *m_pHttp;
			bool operator()(int i, int j)
			{
				return net_addr_comp(&m_pHttp->LegacyServer(i), &m_pHttp->LegacyServer(j)) < 0;
			}
		};

		std::sort(aWantedAddresses.begin(), aWantedAddresses.end(), CWantedAddrComparer());
		std::sort(aSortedServers.begin(), aSortedServers.end(), CAddrComparer{m_pHttp});
		std::sort(aSortedLegacyServers.begin(), aSortedLegacyServers.end(), CLegacyAddrComparer{m_pHttp});

		unsigned i = 0;
		unsigned j = 0;
		int p = 0;
		while(i < aWantedAddresses.size() && j < aSortedServers.size())
		{
			int Cmp = net_addr_comp(&aWantedAddresses[i].m_Addr, &m_pHttp->ServerAddress(aSortedServers[j]));
			if(Cmp != 0)
			{
				if(Cmp < 0)
				{
					i++;
				}
				else
				{
					j++;
				}
				continue;
			}
			aWantedAddresses[i].m_Got = true;
			NETADDR Addr;
			CServerInfo Info;
			m_pHttp->Server(aSortedServers[j], &Addr, &Info);
			ServerBrowserFillEstimatedLatency(OwnLocation, pPingEntries, NumPingEntries, &p, Addr, &Info);
			Info.m_HasRank = HasRank(Info.m_aMap);
			Set(Addr, IServerBrowser::SET_HTTPINFO, -1, &Info);
			i++;
			j++;
		}
		i = 0;
		j = 0;
		while(i < aWantedAddresses.size() && j < aSortedLegacyServers.size())
		{
			int Cmp = net_addr_comp(&aWantedAddresses[i].m_Addr, &m_pHttp->LegacyServer(aSortedLegacyServers[j]));
			if(Cmp != 0)
			{
				if(Cmp < 0)
				{
					i++;
				}
				else
				{
					j++;
				}
				continue;
			}
			aWantedAddresses[i].m_Got = true;
			Set(m_pHttp->LegacyServer(aSortedLegacyServers[j]), LegacySetType, -1, nullptr);
			i++;
			j++;
		}
		for(const CWantedAddr &Wanted : aWantedAddresses)
		{
			if(!Wanted.m_Got)
			{
				if(Wanted.m_FallbackToPing)
				{
					Set(Wanted.m_Addr, LegacySetType, -1, nullptr);
				}
				else
				{
					// Also add favorites we're not allowed to ping.
					if(LegacySetType == IServerBrowser::SET_FAV_ADD && !Find(Wanted.m_Addr))
					{
						Add(Wanted.m_Addr);
					}
				}
			}
		}
		return;
	}
	int p = 0;
	for(int i = 0; i < NumServers; i++)
	{
		NETADDR Addr;
		CServerInfo Info;
		m_pHttp->Server(i, &Addr, &Info);
		ServerBrowserFillEstimatedLatency(OwnLocation, pPingEntries, NumPingEntries, &p, Addr, &Info);
		Info.m_HasRank = HasRank(Info.m_aMap);
		Set(Addr, IServerBrowser::SET_HTTPINFO, -1, &Info);
	}
	for(int i = 0; i < NumLegacyServers; i++)
	{
		NETADDR Addr = m_pHttp->LegacyServer(i);
		Set(Addr, IServerBrowser::SET_MASTER_ADD, -1, nullptr);
	}
}

void CServerBrowser::Update(bool ForceResort)
{
	int64_t Timeout = time_freq();
	int64_t Now = time_get();

	const char *pHttpBestUrl;
	if(!m_pHttp->GetBestUrl(&pHttpBestUrl) && pHttpBestUrl != m_pHttpPrevBestUrl)
	{
		str_copy(g_Config.m_BrCachedBestServerinfoUrl, pHttpBestUrl, sizeof(g_Config.m_BrCachedBestServerinfoUrl));
		m_pHttpPrevBestUrl = pHttpBestUrl;
	}

	m_pHttp->Update();

	if(m_ServerlistType != TYPE_LAN && m_RefreshingHttp && !m_pHttp->IsRefreshing())
	{
		m_RefreshingHttp = false;
		UpdateFromHttp();
		// TODO: move this somewhere else
		if(m_Sorthash != SortHash() || ForceResort)
			Sort();
		return;
	}

	CServerEntry *pEntry = m_pFirstReqServer;
	int Count = 0;
	while(1)
	{
		if(!pEntry) // no more entries
			break;
		if(pEntry->m_RequestTime && pEntry->m_RequestTime + Timeout < Now)
		{
			pEntry = pEntry->m_pNextReq;
			continue;
		}
		// no more than 10 concurrent requests
		if(Count == m_CurrentMaxRequests)
			break;

		if(pEntry->m_RequestTime == 0)
		{
			if(pEntry->m_Request64Legacy)
				RequestImpl64(pEntry->m_Addr, pEntry);
			else
				RequestImpl(pEntry->m_Addr, pEntry, nullptr, nullptr, false);
		}

		Count++;
		pEntry = pEntry->m_pNextReq;
	}

	if(m_pFirstReqServer && Count == 0 && m_CurrentMaxRequests > 1) //NO More current Server Requests
	{
		//reset old ones
		pEntry = m_pFirstReqServer;
		while(1)
		{
			if(!pEntry) // no more entries
				break;
			pEntry->m_RequestTime = 0;
			pEntry = pEntry->m_pNextReq;
		}

		//update max-requests
		m_CurrentMaxRequests = m_CurrentMaxRequests / 2;
		if(m_CurrentMaxRequests < 1)
			m_CurrentMaxRequests = 1;
	}
	else if(Count == 0 && m_CurrentMaxRequests == 1) //we reached the limit, just release all left requests. IF a server sends us a packet, a new request will be added automatically, so we can delete all
	{
		pEntry = m_pFirstReqServer;
		while(1)
		{
			if(!pEntry) // no more entries
				break;
			CServerEntry *pNext = pEntry->m_pNextReq;
			RemoveRequest(pEntry); //release request
			pEntry = pNext;
		}
	}

	// check if we need to resort
	if(m_Sorthash != SortHash() || ForceResort || m_SortOnNextUpdate)
	{
		Sort();
		m_SortOnNextUpdate = false;
	}
}

int CServerBrowser::FindFavorite(const NETADDR &Addr) const
{
	// search for the address
	for(int i = 0; i < m_NumFavoriteServers; i++)
	{
		if(net_addr_comp(&Addr, &m_aFavoriteServers[i]) == 0)
			return i;
	}
	return -1;
}

bool CServerBrowser::GotInfo(const NETADDR &Addr) const
{
	CServerEntry *pEntry = ((CServerBrowser *)this)->Find(Addr);
	return pEntry && pEntry->m_GotInfo;
}

bool CServerBrowser::IsFavorite(const NETADDR &Addr) const
{
	return FindFavorite(Addr) >= 0;
}

bool CServerBrowser::IsFavoritePingAllowed(const NETADDR &Addr) const
{
	int i = FindFavorite(Addr);
	dbg_assert(i >= 0, "invalid favorite");
	return i >= 0 && m_aFavoriteServersAllowPing[i];
}

void CServerBrowser::AddFavorite(const NETADDR &Addr)
{
	CServerEntry *pEntry;

	if(m_NumFavoriteServers == MAX_FAVORITES)
		return;

	// make sure that we don't already have the server in our list
	if(IsFavorite(Addr))
	{
		return;
	}

	// add the server to the list
	m_aFavoriteServers[m_NumFavoriteServers] = Addr;
	m_aFavoriteServersAllowPing[m_NumFavoriteServers] = false;
	m_NumFavoriteServers++;
	pEntry = Find(Addr);
	if(pEntry)
		pEntry->m_Info.m_Favorite = true;

	if(g_Config.m_Debug)
	{
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aAddrStr, sizeof(aAddrStr), true);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "added fav, %s", aAddrStr);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client_srvbrowse", aBuf);
	}
}

void CServerBrowser::FavoriteAllowPing(const NETADDR &Addr, bool AllowPing)
{
	int i = FindFavorite(Addr);
	dbg_assert(i >= 0, "invalid favorite");
	m_aFavoriteServersAllowPing[i] = AllowPing;
}

void CServerBrowser::RemoveFavorite(const NETADDR &Addr)
{
	int i = FindFavorite(Addr);
	if(i < 0)
	{
		return;
	}
	mem_move(&m_aFavoriteServers[i], &m_aFavoriteServers[i + 1], sizeof(NETADDR) * (m_NumFavoriteServers - (i + 1)));
	mem_move(&m_aFavoriteServersAllowPing[i], &m_aFavoriteServersAllowPing[i + 1], sizeof(bool) * (m_NumFavoriteServers - (i + 1)));
	m_NumFavoriteServers--;

	CServerEntry *pEntry = Find(Addr);
	if(pEntry)
		pEntry->m_Info.m_Favorite = false;
}

void CServerBrowser::LoadDDNetServers()
{
	if(!m_pDDNetInfo)
		return;

	// reset servers / countries
	for(int Network = 0; Network < NUM_NETWORKS; Network++)
	{
		CNetwork *pNet = &m_aNetworks[Network];

		// parse JSON
		const json_value *pServers = json_object_get(m_pDDNetInfo, Network == NETWORK_DDNET ? "servers" : "servers-kog");

		if(!pServers || pServers->type != json_array)
			return;

		pNet->m_NumCountries = 0;
		pNet->m_NumTypes = 0;

		for(int i = 0; i < json_array_length(pServers) && pNet->m_NumCountries < MAX_COUNTRIES; i++)
		{
			// pSrv - { name, flagId, servers }
			const json_value *pSrv = json_array_get(pServers, i);
			const json_value *pTypes = json_object_get(pSrv, "servers");
			const json_value *pName = json_object_get(pSrv, "name");
			const json_value *pFlagID = json_object_get(pSrv, "flagId");

			if(pSrv->type != json_object || pTypes->type != json_object || pName->type != json_string || pFlagID->type != json_integer)
			{
				dbg_msg("client_srvbrowse", "invalid attributes");
				continue;
			}

			// build structure
			CNetworkCountry *pCntr = &pNet->m_aCountries[pNet->m_NumCountries];

			pCntr->Reset();

			str_copy(pCntr->m_aName, json_string_get(pName), sizeof(pCntr->m_aName));
			pCntr->m_FlagID = json_int_get(pFlagID);

			// add country
			for(unsigned int t = 0; t < pTypes->u.object.length; t++)
			{
				const char *pType = pTypes->u.object.values[t].name;
				const json_value *pAddrs = pTypes->u.object.values[t].value;

				if(pAddrs->type != json_array)
				{
					dbg_msg("client_srvbrowse", "invalid attributes");
					continue;
				}

				// add type
				if(json_array_length(pAddrs) > 0 && pNet->m_NumTypes < MAX_TYPES)
				{
					int Pos;
					for(Pos = 0; Pos < pNet->m_NumTypes; Pos++)
					{
						if(!str_comp(pNet->m_aTypes[Pos], pType))
							break;
					}
					if(Pos == pNet->m_NumTypes)
					{
						str_copy(pNet->m_aTypes[pNet->m_NumTypes], pType, sizeof(pNet->m_aTypes[pNet->m_NumTypes]));
						pNet->m_NumTypes++;
					}
				}

				// add addresses
				for(int g = 0; g < json_array_length(pAddrs); g++, pCntr->m_NumServers++)
				{
					const json_value *pAddr = json_array_get(pAddrs, g);
					if(pAddr->type != json_string)
					{
						dbg_msg("client_srvbrowse", "invalid attributes");
						continue;
					}
					const char *pStr = json_string_get(pAddr);
					net_addr_from_str(&pCntr->m_aServers[pCntr->m_NumServers], pStr);
					str_copy(pCntr->m_aTypes[pCntr->m_NumServers], pType, sizeof(pCntr->m_aTypes[pCntr->m_NumServers]));
				}
			}

			pNet->m_NumCountries++;
		}
	}
}

void CServerBrowser::RecheckOfficial()
{
	for(auto &Network : m_aNetworks)
	{
		for(int i = 0; i < Network.m_NumCountries; i++)
		{
			CNetworkCountry *pCntr = &Network.m_aCountries[i];
			for(int j = 0; j < pCntr->m_NumServers; j++)
			{
				CServerEntry *pEntry = Find(pCntr->m_aServers[j]);
				if(pEntry)
				{
					pEntry->m_Info.m_Official = true;
				}
			}
		}
	}
}

void CServerBrowser::LoadDDNetRanks()
{
	for(int i = 0; i < m_NumServers; i++)
	{
		if(m_ppServerlist[i]->m_Info.m_aMap[0])
			m_ppServerlist[i]->m_Info.m_HasRank = HasRank(m_ppServerlist[i]->m_Info.m_aMap);
	}
}

int CServerBrowser::HasRank(const char *pMap)
{
	if(m_ServerlistType != IServerBrowser::TYPE_DDNET || !m_pDDNetInfo)
		return -1;

	const json_value *pDDNetRanks = json_object_get(m_pDDNetInfo, "maps");

	if(!pDDNetRanks || pDDNetRanks->type != json_array)
		return -1;

	for(int i = 0; i < json_array_length(pDDNetRanks); i++)
	{
		const json_value *pJson = json_array_get(pDDNetRanks, i);
		if(!pJson || pJson->type != json_string)
			continue;

		const char *pStr = json_string_get(pJson);

		if(str_comp(pMap, pStr) == 0)
			return 1;
	}

	return 0;
}

void CServerBrowser::LoadDDNetInfoJson()
{
	IOHANDLE File = m_pStorage->OpenFile(DDNET_INFO, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const int Length = io_length(File);
	if(Length <= 0)
	{
		io_close(File);
		return;
	}

	char *pBuf = (char *)malloc(Length);
	pBuf[0] = '\0';

	io_read(File, pBuf, Length);
	io_close(File);

	if(m_pDDNetInfo)
		json_value_free(m_pDDNetInfo);

	m_pDDNetInfo = json_parse(pBuf, Length);

	free(pBuf);

	if(m_pDDNetInfo && m_pDDNetInfo->type != json_object)
	{
		json_value_free(m_pDDNetInfo);
		m_pDDNetInfo = 0;
	}

	m_OwnLocation = CServerInfo::LOC_UNKNOWN;
	if(m_pDDNetInfo)
	{
		const json_value &Location = (*m_pDDNetInfo)["location"];
		if(Location.type != json_string || CServerInfo::ParseLocation(&m_OwnLocation, Location))
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "cannot parse location from info.json: '%s'", (const char *)Location);
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowse", aBuf);
		}
	}
}

const json_value *CServerBrowser::LoadDDNetInfo()
{
	LoadDDNetInfoJson();
	LoadDDNetServers();

	RecheckOfficial();
	LoadDDNetRanks();

	return m_pDDNetInfo;
}

bool CServerBrowser::IsRefreshing() const
{
	return m_pFirstReqServer != 0;
}

bool CServerBrowser::IsGettingServerlist() const
{
	return m_pHttp->IsRefreshing();
}

int CServerBrowser::LoadingProgression() const
{
	if(m_NumServers == 0)
		return 0;

	int Servers = m_NumServers;
	int Loaded = m_NumServers - m_NumRequests;
	return 100.0f * Loaded / Servers;
}

void CServerBrowser::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CServerBrowser *pSelf = (CServerBrowser *)pUserData;

	char aAddrStr[128];
	char aBuffer[256];
	for(int i = 0; i < pSelf->m_NumFavoriteServers; i++)
	{
		net_addr_str(&pSelf->m_aFavoriteServers[i], aAddrStr, sizeof(aAddrStr), true);
		if(!pSelf->m_aFavoriteServersAllowPing[i])
		{
			str_format(aBuffer, sizeof(aBuffer), "add_favorite %s", aAddrStr);
		}
		else
		{
			// Add quotes to the first parameter for backward
			// compatibility with versions that took a `r` console
			// parameter.
			str_format(aBuffer, sizeof(aBuffer), "add_favorite \"%s\" allow_ping", aAddrStr);
		}
		pConfigManager->WriteLine(aBuffer);
	}
}

void CServerBrowser::DDNetFilterAdd(char *pFilter, const char *pName)
{
	if(DDNetFiltered(pFilter, pName))
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), ",%s", pName);
	str_append(pFilter, aBuf, 128);
}

void CServerBrowser::DDNetFilterRem(char *pFilter, const char *pName)
{
	if(!DDNetFiltered(pFilter, pName))
		return;

	// rewrite exclude/filter list
	char aBuf[128];

	str_copy(aBuf, pFilter, sizeof(aBuf));
	pFilter[0] = '\0';

	char aToken[128];
	for(const char *tok = aBuf; (tok = str_next_token(tok, ",", aToken, sizeof(aToken)));)
	{
		if(str_comp_nocase(pName, aToken) != 0)
		{
			char aBuf2[128];
			str_format(aBuf2, sizeof(aBuf2), ",%s", aToken);
			str_append(pFilter, aBuf2, 128);
		}
	}
}

bool CServerBrowser::DDNetFiltered(char *pFilter, const char *pName)
{
	return str_in_list(pFilter, ",", pName); // country not excluded
}

void CServerBrowser::CountryFilterClean(int Network)
{
	char *pExcludeCountries = Network == NETWORK_KOG ? g_Config.m_BrFilterExcludeCountriesKoG : g_Config.m_BrFilterExcludeCountries;
	char aNewList[128];
	aNewList[0] = '\0';

	for(auto &Network : m_aNetworks)
	{
		for(int i = 0; i < Network.m_NumCountries; i++)
		{
			const char *pName = Network.m_aCountries[i].m_aName;
			if(DDNetFiltered(pExcludeCountries, pName))
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), ",%s", pName);
				str_append(aNewList, aBuf, sizeof(aNewList));
			}
		}
	}

	str_copy(pExcludeCountries, aNewList, sizeof(g_Config.m_BrFilterExcludeCountries));
}

void CServerBrowser::TypeFilterClean(int Network)
{
	char *pExcludeTypes = Network == NETWORK_KOG ? g_Config.m_BrFilterExcludeTypesKoG : g_Config.m_BrFilterExcludeTypes;
	char aNewList[128];
	aNewList[0] = '\0';

	for(int i = 0; i < m_aNetworks[Network].m_NumTypes; i++)
	{
		const char *pName = m_aNetworks[Network].m_aTypes[i];
		if(DDNetFiltered(pExcludeTypes, pName))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), ",%s", pName);
			str_append(aNewList, aBuf, sizeof(aNewList));
		}
	}

	str_copy(pExcludeTypes, aNewList, sizeof(g_Config.m_BrFilterExcludeTypes));
}

int CServerInfo::EstimateLatency(int Loc1, int Loc2)
{
	if(Loc1 == LOC_UNKNOWN || Loc2 == LOC_UNKNOWN)
	{
		return 999;
	}
	if(Loc1 != Loc2)
	{
		return 199;
	}
	return 99;
}
bool CServerInfo::ParseLocation(int *pResult, const char *pString)
{
	*pResult = LOC_UNKNOWN;
	int Length = str_length(pString);
	if(Length < 2)
	{
		return true;
	}
	// ISO continent code. Allow antarctica, but treat it as unknown.
	static const char LOCATIONS[][6] = {
		"an", // LOC_UNKNOWN
		"af", // LOC_AFRICA
		"as", // LOC_ASIA
		"oc", // LOC_AUSTRALIA
		"eu", // LOC_EUROPE
		"na", // LOC_NORTH_AMERICA
		"sa", // LOC_SOUTH_AMERICA
		"as:cn", // LOC_CHINA
	};
	for(int i = sizeof(LOCATIONS) / sizeof(LOCATIONS[0]) - 1; i >= 0; i--)
	{
		if(str_startswith(pString, LOCATIONS[i]))
		{
			*pResult = i;
			return false;
		}
	}
	return true;
}

bool IsVanilla(const CServerInfo *pInfo)
{
	return !str_comp(pInfo->m_aGameType, "DM") || !str_comp(pInfo->m_aGameType, "TDM") || !str_comp(pInfo->m_aGameType, "CTF");
}

bool IsCatch(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "catch");
}

bool IsInsta(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "idm") || str_find_nocase(pInfo->m_aGameType, "itdm") || str_find_nocase(pInfo->m_aGameType, "ictf");
}

bool IsFNG(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fng");
}

bool IsRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "race") || str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsFastCap(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsBlockInfectionZ(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "blockz") ||
	       str_find_nocase(pInfo->m_aGameType, "infectionz");
}

bool IsBlockWorlds(const CServerInfo *pInfo)
{
	return (str_comp_nocase_num(pInfo->m_aGameType, "bw  ", 4) == 0) || (str_comp_nocase(pInfo->m_aGameType, "bw") == 0);
}

bool IsCity(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "city");
}

bool IsDDRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddrace") || str_find_nocase(pInfo->m_aGameType, "mkrace");
}

bool IsDDNet(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddracenet") || str_find_nocase(pInfo->m_aGameType, "ddnet");
}

// other

bool Is64Player(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "64") || str_find(pInfo->m_aName, "64") || IsDDNet(pInfo);
}

bool IsPlus(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "+");
}
