/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "serverbrowser.h"

#include "serverbrowser_http.h"
#include "serverbrowser_ping_cache.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include <base/hash_ctxt.h>
#include <base/log.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/serverinfo.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/friends.h>
#include <engine/http.h>
#include <engine/storage.h>

class CSortWrap
{
	typedef bool (CServerBrowser::*SortFunc)(int, int) const;
	SortFunc m_pfnSort;
	CServerBrowser *m_pThis;

public:
	CSortWrap(CServerBrowser *pServer, SortFunc Func) :
		m_pfnSort(Func), m_pThis(pServer) {}
	bool operator()(int a, int b) { return (g_Config.m_BrSortOrder ? (m_pThis->*m_pfnSort)(b, a) : (m_pThis->*m_pfnSort)(a, b)); }
};

bool matchesPart(const char *a, const char *b)
{
	return str_utf8_find_nocase(a, b) != nullptr;
}

bool matchesExactly(const char *a, const char *b)
{
	return str_comp(a, &b[1]) == 0;
}

CServerBrowser::CServerBrowser() :
	m_CommunityCache(this),
	m_CountriesFilter(&m_CommunityCache),
	m_TypesFilter(&m_CommunityCache)
{
	m_ppServerlist = nullptr;
	m_pSortedServerlist = nullptr;

	m_NeedResort = false;
	m_Sorthash = 0;

	m_NumSortedServersCapacity = 0;
	m_NumServerCapacity = 0;

	m_ServerlistType = 0;
	m_BroadcastTime = 0;
	secure_random_fill(m_aTokenSeed, sizeof(m_aTokenSeed));

	CleanUp();
}

CServerBrowser::~CServerBrowser()
{
	free(m_ppServerlist);
	free(m_pSortedServerlist);
	json_value_free(m_pDDNetInfo);

	delete m_pHttp;
	m_pHttp = nullptr;
	delete m_pPingCache;
	m_pPingCache = nullptr;
}

void CServerBrowser::SetBaseInfo(class CNetClient *pClient, const char *pNetVersion)
{
	m_pNetClient = pClient;
	str_copy(m_aNetVersion, pNetVersion);
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pFavorites = Kernel()->RequestInterface<IFavorites>();
	m_pFriends = Kernel()->RequestInterface<IFriends>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pHttpClient = Kernel()->RequestInterface<IHttp>();
	m_pPingCache = CreateServerBrowserPingCache(m_pConsole, m_pStorage);

	RegisterCommands();
}

void CServerBrowser::OnInit()
{
	m_pHttp = CreateServerBrowserHttp(m_pEngine, m_pStorage, m_pHttpClient, g_Config.m_BrCachedBestServerinfoUrl);
}

void CServerBrowser::RegisterCommands()
{
	m_pConfigManager->RegisterCallback(CServerBrowser::ConfigSaveCallback, this);
	m_pConsole->Register("add_favorite_community", "s[community_id]", CFGFLAG_CLIENT, Con_AddFavoriteCommunity, this, "Add a community as a favorite");
	m_pConsole->Register("remove_favorite_community", "s[community_id]", CFGFLAG_CLIENT, Con_RemoveFavoriteCommunity, this, "Remove a community from the favorites");
	m_pConsole->Register("add_excluded_community", "s[community_id]", CFGFLAG_CLIENT, Con_AddExcludedCommunity, this, "Add a community to the exclusion filter");
	m_pConsole->Register("remove_excluded_community", "s[community_id]", CFGFLAG_CLIENT, Con_RemoveExcludedCommunity, this, "Remove a community from the exclusion filter");
	m_pConsole->Register("add_excluded_country", "s[community_id] s[country_code]", CFGFLAG_CLIENT, Con_AddExcludedCountry, this, "Add a country to the exclusion filter for a specific community");
	m_pConsole->Register("remove_excluded_country", "s[community_id] s[country_code]", CFGFLAG_CLIENT, Con_RemoveExcludedCountry, this, "Remove a country from the exclusion filter for a specific community");
	m_pConsole->Register("add_excluded_type", "s[community_id] s[type]", CFGFLAG_CLIENT, Con_AddExcludedType, this, "Add a type to the exclusion filter for a specific community");
	m_pConsole->Register("remove_excluded_type", "s[community_id] s[type]", CFGFLAG_CLIENT, Con_RemoveExcludedType, this, "Remove a type from the exclusion filter for a specific community");
	m_pConsole->Register("leak_ip_address_to_all_servers", "", CFGFLAG_CLIENT, Con_LeakIpAddress, this, "Leaks your IP address to all servers by pinging each of them, also acquiring the latency in the process");
}

void CServerBrowser::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	pThis->FavoriteCommunitiesFilter().Save(pConfigManager);
	pThis->CommunitiesFilter().Save(pConfigManager);
	pThis->CountriesFilter().Save(pConfigManager);
	pThis->TypesFilter().Save(pConfigManager);
}

void CServerBrowser::Con_AddFavoriteCommunity(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	if(!pThis->ValidateCommunityId(pCommunityId))
		return;
	pThis->FavoriteCommunitiesFilter().Add(pCommunityId);
}

void CServerBrowser::Con_RemoveFavoriteCommunity(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	if(!pThis->ValidateCommunityId(pCommunityId))
		return;
	pThis->FavoriteCommunitiesFilter().Remove(pCommunityId);
}

void CServerBrowser::Con_AddExcludedCommunity(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	if(!pThis->ValidateCommunityId(pCommunityId))
		return;
	pThis->CommunitiesFilter().Add(pCommunityId);
}

void CServerBrowser::Con_RemoveExcludedCommunity(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	if(!pThis->ValidateCommunityId(pCommunityId))
		return;
	pThis->CommunitiesFilter().Remove(pCommunityId);
}

void CServerBrowser::Con_AddExcludedCountry(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	const char *pCountryName = pResult->GetString(1);
	if(!pThis->ValidateCommunityId(pCommunityId) || !pThis->ValidateCountryName(pCountryName))
		return;
	pThis->CountriesFilter().Add(pCommunityId, pCountryName);
}

void CServerBrowser::Con_RemoveExcludedCountry(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	const char *pCountryName = pResult->GetString(1);
	if(!pThis->ValidateCommunityId(pCommunityId) || !pThis->ValidateCountryName(pCountryName))
		return;
	pThis->CountriesFilter().Remove(pCommunityId, pCountryName);
}

void CServerBrowser::Con_AddExcludedType(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	const char *pTypeName = pResult->GetString(1);
	if(!pThis->ValidateCommunityId(pCommunityId) || !pThis->ValidateTypeName(pTypeName))
		return;
	pThis->TypesFilter().Add(pCommunityId, pTypeName);
}

void CServerBrowser::Con_RemoveExcludedType(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);
	const char *pCommunityId = pResult->GetString(0);
	const char *pTypeName = pResult->GetString(1);
	if(!pThis->ValidateCommunityId(pCommunityId) || !pThis->ValidateTypeName(pTypeName))
		return;
	pThis->TypesFilter().Remove(pCommunityId, pTypeName);
}

void CServerBrowser::Con_LeakIpAddress(IConsole::IResult *pResult, void *pUserData)
{
	CServerBrowser *pThis = static_cast<CServerBrowser *>(pUserData);

	// We only consider the first address of every server.

	std::vector<int> vSortedServers;
	// Sort servers by IP address, ignoring port.
	class CAddrComparer
	{
	public:
		CServerBrowser *m_pThis;
		bool operator()(int i, int j) const
		{
			NETADDR Addr1 = m_pThis->m_ppServerlist[i]->m_Info.m_aAddresses[0];
			NETADDR Addr2 = m_pThis->m_ppServerlist[j]->m_Info.m_aAddresses[0];
			Addr1.port = 0;
			Addr2.port = 0;
			return net_addr_comp(&Addr1, &Addr2) < 0;
		}
	};
	vSortedServers.reserve(pThis->m_NumServers);
	for(int i = 0; i < pThis->m_NumServers; i++)
	{
		vSortedServers.push_back(i);
	}
	std::sort(vSortedServers.begin(), vSortedServers.end(), CAddrComparer{pThis});

	// Group the servers into those with same IP address (but differing
	// port).
	NETADDR Addr;
	int Start = -1;
	for(int i = 0; i <= (int)vSortedServers.size(); i++)
	{
		NETADDR NextAddr;
		if(i < (int)vSortedServers.size())
		{
			NextAddr = pThis->m_ppServerlist[vSortedServers[i]]->m_Info.m_aAddresses[0];
			NextAddr.port = 0;
		}
		bool New = Start == -1 || i == (int)vSortedServers.size() || net_addr_comp(&Addr, &NextAddr) != 0;
		if(Start != -1 && New)
		{
			int Chosen = Start + secure_rand_below(i - Start);
			CServerEntry *pChosen = pThis->m_ppServerlist[vSortedServers[Chosen]];
			pChosen->m_RequestIgnoreInfo = true;
			pThis->QueueRequest(pChosen);
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&pChosen->m_Info.m_aAddresses[0], aAddr, sizeof(aAddr), true);
			dbg_msg("serverbrowser", "queuing ping request for %s", aAddr);
		}
		if(i < (int)vSortedServers.size() && New)
		{
			Start = i;
			Addr = NextAddr;
		}
	}
}

static bool ValidIdentifier(const char *pId, size_t MaxLength)
{
	if(pId[0] == '\0' || (size_t)str_length(pId) >= MaxLength)
	{
		return false;
	}

	for(int i = 0; pId[i] != '\0'; ++i)
	{
		if(pId[i] == '"' || pId[i] == '/' || pId[i] == '\\')
		{
			return false;
		}
	}
	return true;
}

static bool ValidateIdentifier(const char *pId, size_t MaxLength, const char *pContext, IConsole *pConsole)
{
	if(!ValidIdentifier(pId, MaxLength))
	{
		char aError[32 + IConsole::CMDLINE_LENGTH];
		str_format(aError, sizeof(aError), "%s '%s' is not valid", pContext, pId);
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowser", aError);
		return false;
	}
	return true;
}

bool CServerBrowser::ValidateCommunityId(const char *pCommunityId) const
{
	return ValidateIdentifier(pCommunityId, CServerInfo::MAX_COMMUNITY_ID_LENGTH, "Community ID", m_pConsole);
}

bool CServerBrowser::ValidateCountryName(const char *pCountryName) const
{
	return ValidateIdentifier(pCountryName, CServerInfo::MAX_COMMUNITY_COUNTRY_LENGTH, "Country name", m_pConsole);
}

bool CServerBrowser::ValidateTypeName(const char *pTypeName) const
{
	return ValidateIdentifier(pTypeName, CServerInfo::MAX_COMMUNITY_TYPE_LENGTH, "Type name", m_pConsole);
}

int CServerBrowser::Players(const CServerInfo &Item) const
{
	return g_Config.m_BrFilterSpectators ? Item.m_NumPlayers : Item.m_NumClients;
}

int CServerBrowser::Max(const CServerInfo &Item) const
{
	return g_Config.m_BrFilterSpectators ? Item.m_MaxPlayers : Item.m_MaxClients;
}

const CServerInfo *CServerBrowser::SortedGet(int Index) const
{
	if(Index < 0 || Index >= m_NumSortedServers)
		return nullptr;
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
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	//	make sure empty entries are listed last
	return (pIndex1->m_GotInfo && pIndex2->m_GotInfo) || (!pIndex1->m_GotInfo && !pIndex2->m_GotInfo) ? str_comp(pIndex1->m_Info.m_aName, pIndex2->m_Info.m_aName) < 0 :
													    pIndex1->m_GotInfo != 0;
}

bool CServerBrowser::SortCompareMap(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	return str_comp(pIndex1->m_Info.m_aMap, pIndex2->m_Info.m_aMap) < 0;
}

bool CServerBrowser::SortComparePing(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	return pIndex1->m_Info.m_Latency < pIndex2->m_Info.m_Latency;
}

bool CServerBrowser::SortCompareGametype(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	return str_comp(pIndex1->m_Info.m_aGameType, pIndex2->m_Info.m_aGameType) < 0;
}

bool CServerBrowser::SortCompareNumPlayers(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	return pIndex1->m_Info.m_NumFilteredPlayers > pIndex2->m_Info.m_NumFilteredPlayers;
}

bool CServerBrowser::SortCompareNumClients(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];
	return pIndex1->m_Info.m_NumClients > pIndex2->m_Info.m_NumClients;
}

bool CServerBrowser::SortCompareNumFriends(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];

	if(pIndex1->m_Info.m_FriendNum == pIndex2->m_Info.m_FriendNum)
		return pIndex1->m_Info.m_NumFilteredPlayers > pIndex2->m_Info.m_NumFilteredPlayers;
	else
		return pIndex1->m_Info.m_FriendNum > pIndex2->m_Info.m_FriendNum;
}

bool CServerBrowser::SortCompareNumPlayersAndPing(int Index1, int Index2) const
{
	CServerEntry *pIndex1 = m_ppServerlist[Index1];
	CServerEntry *pIndex2 = m_ppServerlist[Index2];

	if(pIndex1->m_Info.m_NumFilteredPlayers == pIndex2->m_Info.m_NumFilteredPlayers)
		return pIndex1->m_Info.m_Latency > pIndex2->m_Info.m_Latency;
	else if(pIndex1->m_Info.m_NumFilteredPlayers == 0 || pIndex2->m_Info.m_NumFilteredPlayers == 0 || pIndex1->m_Info.m_Latency / 100 == pIndex2->m_Info.m_Latency / 100)
		return pIndex1->m_Info.m_NumFilteredPlayers < pIndex2->m_Info.m_NumFilteredPlayers;
	else
		return pIndex1->m_Info.m_Latency > pIndex2->m_Info.m_Latency;
}

void CServerBrowser::Filter()
{
	m_NumSortedServers = 0;
	m_NumSortedPlayers = 0;

	// allocate the sorted list
	if(m_NumSortedServersCapacity < m_NumServers)
	{
		free(m_pSortedServerlist);
		m_NumSortedServersCapacity = m_NumServers;
		m_pSortedServerlist = (int *)calloc(m_NumSortedServersCapacity, sizeof(int));
	}

	// filter the servers
	for(int i = 0; i < m_NumServers; i++)
	{
		CServerInfo &Info = m_ppServerlist[i]->m_Info;
		bool Filtered = false;

		if(g_Config.m_BrFilterEmpty && Info.m_NumFilteredPlayers == 0)
			Filtered = true;
		else if(g_Config.m_BrFilterFull && Players(Info) == Max(Info))
			Filtered = true;
		else if(g_Config.m_BrFilterPw && Info.m_Flags & SERVER_FLAG_PASSWORD)
			Filtered = true;
		else if(g_Config.m_BrFilterServerAddress[0] && !str_find_nocase(Info.m_aAddress, g_Config.m_BrFilterServerAddress))
			Filtered = true;
		else if(g_Config.m_BrFilterGametypeStrict && g_Config.m_BrFilterGametype[0] && str_comp_nocase(Info.m_aGameType, g_Config.m_BrFilterGametype))
			Filtered = true;
		else if(!g_Config.m_BrFilterGametypeStrict && g_Config.m_BrFilterGametype[0] && !str_utf8_find_nocase(Info.m_aGameType, g_Config.m_BrFilterGametype))
			Filtered = true;
		else if(g_Config.m_BrFilterUnfinishedMap && Info.m_HasRank == CServerInfo::RANK_RANKED)
			Filtered = true;
		else if(g_Config.m_BrFilterLogin && Info.m_RequiresLogin)
			Filtered = true;
		else
		{
			if(!Communities().empty())
			{
				if(m_ServerlistType == IServerBrowser::TYPE_INTERNET || m_ServerlistType == IServerBrowser::TYPE_FAVORITES)
				{
					Filtered = CommunitiesFilter().Filtered(Info.m_aCommunityId);
				}
				if(m_ServerlistType == IServerBrowser::TYPE_INTERNET || m_ServerlistType == IServerBrowser::TYPE_FAVORITES ||
					(m_ServerlistType >= IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 && m_ServerlistType <= IServerBrowser::TYPE_FAVORITE_COMMUNITY_5))
				{
					Filtered = Filtered || CountriesFilter().Filtered(Info.m_aCommunityCountry);
					Filtered = Filtered || TypesFilter().Filtered(Info.m_aCommunityType);
				}
			}

			if(!Filtered && g_Config.m_BrFilterCountry)
			{
				Filtered = true;
				// match against player country
				for(int p = 0; p < minimum(Info.m_NumClients, (int)MAX_CLIENTS); p++)
				{
					if(Info.m_aClients[p].m_Country == g_Config.m_BrFilterCountryIndex)
					{
						Filtered = false;
						break;
					}
				}
			}

			if(!Filtered && g_Config.m_BrFilterString[0] != '\0')
			{
				Info.m_QuickSearchHit = 0;

				const char *pStr = g_Config.m_BrFilterString;
				char aFilterStr[sizeof(g_Config.m_BrFilterString)];
				char aFilterStrTrimmed[sizeof(g_Config.m_BrFilterString)];
				while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aFilterStr, sizeof(aFilterStr))))
				{
					str_copy(aFilterStrTrimmed, str_utf8_skip_whitespaces(aFilterStr));
					str_utf8_trim_right(aFilterStrTrimmed);

					if(aFilterStrTrimmed[0] == '\0')
					{
						continue;
					}
					auto MatchesFn = matchesPart;
					const int FilterLen = str_length(aFilterStrTrimmed);
					if(aFilterStrTrimmed[0] == '"' && aFilterStrTrimmed[FilterLen - 1] == '"')
					{
						aFilterStrTrimmed[FilterLen - 1] = '\0';
						MatchesFn = matchesExactly;
					}

					// match against server name
					if(MatchesFn(Info.m_aName, aFilterStrTrimmed))
					{
						Info.m_QuickSearchHit |= IServerBrowser::QUICK_SERVERNAME;
					}

					// match against players
					for(int p = 0; p < minimum(Info.m_NumClients, (int)MAX_CLIENTS); p++)
					{
						if(MatchesFn(Info.m_aClients[p].m_aName, aFilterStrTrimmed) ||
							MatchesFn(Info.m_aClients[p].m_aClan, aFilterStrTrimmed))
						{
							if(g_Config.m_BrFilterConnectingPlayers &&
								str_comp(Info.m_aClients[p].m_aName, "(connecting)") == 0 &&
								Info.m_aClients[p].m_aClan[0] == '\0')
							{
								continue;
							}
							Info.m_QuickSearchHit |= IServerBrowser::QUICK_PLAYER;
							break;
						}
					}

					// match against map
					if(MatchesFn(Info.m_aMap, aFilterStrTrimmed))
					{
						Info.m_QuickSearchHit |= IServerBrowser::QUICK_MAPNAME;
					}
				}

				if(!Info.m_QuickSearchHit)
					Filtered = true;
			}

			if(!Filtered && g_Config.m_BrExcludeString[0] != '\0')
			{
				const char *pStr = g_Config.m_BrExcludeString;
				char aExcludeStr[sizeof(g_Config.m_BrExcludeString)];
				char aExcludeStrTrimmed[sizeof(g_Config.m_BrExcludeString)];
				while((pStr = str_next_token(pStr, IServerBrowser::SEARCH_EXCLUDE_TOKEN, aExcludeStr, sizeof(aExcludeStr))))
				{
					str_copy(aExcludeStrTrimmed, str_utf8_skip_whitespaces(aExcludeStr));
					str_utf8_trim_right(aExcludeStrTrimmed);

					if(aExcludeStrTrimmed[0] == '\0')
					{
						continue;
					}
					auto MatchesFn = matchesPart;
					const int FilterLen = str_length(aExcludeStrTrimmed);
					if(aExcludeStrTrimmed[0] == '"' && aExcludeStrTrimmed[FilterLen - 1] == '"')
					{
						aExcludeStrTrimmed[FilterLen - 1] = '\0';
						MatchesFn = matchesExactly;
					}

					// match against server name
					if(MatchesFn(Info.m_aName, aExcludeStrTrimmed))
					{
						Filtered = true;
						break;
					}

					// match against map
					if(MatchesFn(Info.m_aMap, aExcludeStrTrimmed))
					{
						Filtered = true;
						break;
					}

					// match against gametype
					if(MatchesFn(Info.m_aGameType, aExcludeStrTrimmed))
					{
						Filtered = true;
						break;
					}
				}
			}
		}

		if(!Filtered)
		{
			UpdateServerFriends(&Info);

			if(!g_Config.m_BrFilterFriends || Info.m_FriendState != IFriends::FRIEND_NO)
			{
				m_NumSortedPlayers += Info.m_NumFilteredPlayers;
				m_pSortedServerlist[m_NumSortedServers++] = i;
			}
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
	i |= g_Config.m_BrFilterGametypeStrict << 12;
	i |= g_Config.m_BrFilterUnfinishedMap << 13;
	i |= g_Config.m_BrFilterCountry << 14;
	i |= g_Config.m_BrFilterConnectingPlayers << 15;
	i |= g_Config.m_BrFilterLogin << 16;
	return i;
}

void CServerBrowser::Sort()
{
	// update number of filtered players
	for(int i = 0; i < m_NumServers; i++)
	{
		UpdateServerFilteredPlayers(&m_ppServerlist[i]->m_Info);
	}

	// create filtered list
	Filter();

	// sort
	if(g_Config.m_BrSortOrder == 2 && (g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS || g_Config.m_BrSort == IServerBrowser::SORT_PING))
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareNumPlayersAndPing));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_NAME)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareName));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_PING)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortComparePing));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_MAP)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareMap));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_NUMFRIENDS)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareNumFriends));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareNumPlayers));
	else if(g_Config.m_BrSort == IServerBrowser::SORT_GAMETYPE)
		std::stable_sort(m_pSortedServerlist, m_pSortedServerlist + m_NumSortedServers, CSortWrap(this, &CServerBrowser::SortCompareGametype));

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

		pEntry->m_pPrevReq = nullptr;
		pEntry->m_pNextReq = nullptr;
		m_NumRequests--;
	}
}

CServerBrowser::CServerEntry *CServerBrowser::Find(const NETADDR &Addr)
{
	auto Entry = m_ByAddr.find(Addr);
	if(Entry == m_ByAddr.end())
	{
		return nullptr;
	}
	return m_ppServerlist[Entry->second];
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
	pEntry->m_pNextReq = nullptr;
	m_NumRequests++;
}

void ServerBrowserFormatAddresses(char *pBuffer, int BufferSize, NETADDR *pAddrs, int NumAddrs)
{
	for(int i = 0; i < NumAddrs; i++)
	{
		if(i != 0)
		{
			if(BufferSize <= 1)
			{
				return;
			}
			pBuffer[0] = ',';
			pBuffer[1] = '\0';
			pBuffer += 1;
			BufferSize -= 1;
		}
		if(BufferSize <= 1)
		{
			return;
		}
		char aIpAddr[512];
		if(!net_addr_str(&pAddrs[i], aIpAddr, sizeof(aIpAddr), true))
		{
			str_copy(pBuffer, aIpAddr, BufferSize);
			return;
		}
		if(pAddrs[i].type & NETTYPE_TW7)
		{
			str_format(
				pBuffer,
				BufferSize,
				"tw-0.7+udp://%s",
				aIpAddr);
			return;
		}
		str_copy(pBuffer, aIpAddr, BufferSize);
		int Length = str_length(pBuffer);
		pBuffer += Length;
		BufferSize -= Length;
	}
}

void CServerBrowser::SetInfo(CServerEntry *pEntry, const CServerInfo &Info) const
{
	const CServerInfo TmpInfo = pEntry->m_Info;
	pEntry->m_Info = Info;
	pEntry->m_Info.m_Favorite = TmpInfo.m_Favorite;
	pEntry->m_Info.m_FavoriteAllowPing = TmpInfo.m_FavoriteAllowPing;
	mem_copy(pEntry->m_Info.m_aAddresses, TmpInfo.m_aAddresses, sizeof(pEntry->m_Info.m_aAddresses));
	pEntry->m_Info.m_NumAddresses = TmpInfo.m_NumAddresses;
	ServerBrowserFormatAddresses(pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aAddress), pEntry->m_Info.m_aAddresses, pEntry->m_Info.m_NumAddresses);
	str_copy(pEntry->m_Info.m_aCommunityId, TmpInfo.m_aCommunityId);
	str_copy(pEntry->m_Info.m_aCommunityCountry, TmpInfo.m_aCommunityCountry);
	str_copy(pEntry->m_Info.m_aCommunityType, TmpInfo.m_aCommunityType);
	UpdateServerRank(&pEntry->m_Info);

	if(pEntry->m_Info.m_ClientScoreKind == CServerInfo::CLIENT_SCORE_KIND_UNSPECIFIED)
	{
		if(str_find_nocase(pEntry->m_Info.m_aGameType, "race") || str_find_nocase(pEntry->m_Info.m_aGameType, "fastcap"))
		{
			pEntry->m_Info.m_ClientScoreKind = CServerInfo::CLIENT_SCORE_KIND_TIME_BACKCOMPAT;
		}
		else
		{
			pEntry->m_Info.m_ClientScoreKind = CServerInfo::CLIENT_SCORE_KIND_POINTS;
		}
	}

	class CPlayerScoreNameLess
	{
		const int m_ScoreKind;

	public:
		CPlayerScoreNameLess(int ClientScoreKind) :
			m_ScoreKind(ClientScoreKind)
		{
		}

		bool operator()(const CServerInfo::CClient &p0, const CServerInfo::CClient &p1) const
		{
			// Sort players before non players
			if(p0.m_Player && !p1.m_Player)
				return true;
			if(!p0.m_Player && p1.m_Player)
				return false;

			int Score0 = p0.m_Score;
			int Score1 = p1.m_Score;

			if(m_ScoreKind == CServerInfo::CLIENT_SCORE_KIND_TIME || m_ScoreKind == CServerInfo::CLIENT_SCORE_KIND_TIME_BACKCOMPAT)
			{
				// Sort unfinished (-9999) and still connecting players (-1) after others
				if(Score0 < 0 && Score1 >= 0)
					return false;
				if(Score0 >= 0 && Score1 < 0)
					return true;
			}

			if(Score0 != Score1)
			{
				// Handle the sign change introduced with CLIENT_SCORE_KIND_TIME
				if(m_ScoreKind == CServerInfo::CLIENT_SCORE_KIND_TIME)
					return Score0 < Score1;
				else
					return Score0 > Score1;
			}

			return str_comp_nocase(p0.m_aName, p1.m_aName) < 0;
		}
	};

	std::sort(pEntry->m_Info.m_aClients, pEntry->m_Info.m_aClients + Info.m_NumReceivedClients, CPlayerScoreNameLess(pEntry->m_Info.m_ClientScoreKind));

	pEntry->m_GotInfo = 1;
}

void CServerBrowser::SetLatency(NETADDR Addr, int Latency)
{
	m_pPingCache->CachePing(Addr, Latency);

	Addr.port = 0;
	for(int i = 0; i < m_NumServers; i++)
	{
		if(!m_ppServerlist[i]->m_GotInfo)
		{
			continue;
		}
		bool Found = false;
		for(int j = 0; j < m_ppServerlist[i]->m_Info.m_NumAddresses; j++)
		{
			NETADDR Other = m_ppServerlist[i]->m_Info.m_aAddresses[j];
			Other.port = 0;
			if(Addr == Other)
			{
				Found = true;
				break;
			}
		}
		if(!Found)
		{
			continue;
		}
		int Ping = m_pPingCache->GetPing(m_ppServerlist[i]->m_Info.m_aAddresses, m_ppServerlist[i]->m_Info.m_NumAddresses);
		if(Ping == -1)
		{
			continue;
		}
		m_ppServerlist[i]->m_Info.m_Latency = Ping;
		m_ppServerlist[i]->m_Info.m_LatencyIsEstimated = false;
	}
}

CServerBrowser::CServerEntry *CServerBrowser::Add(const NETADDR *pAddrs, int NumAddrs)
{
	// create new pEntry
	CServerEntry *pEntry = m_ServerlistHeap.Allocate<CServerEntry>();
	mem_zero(pEntry, sizeof(CServerEntry));

	// set the info
	mem_copy(pEntry->m_Info.m_aAddresses, pAddrs, NumAddrs * sizeof(pAddrs[0]));
	pEntry->m_Info.m_NumAddresses = NumAddrs;

	pEntry->m_Info.m_Latency = 999;
	pEntry->m_Info.m_HasRank = CServerInfo::RANK_UNAVAILABLE;
	ServerBrowserFormatAddresses(pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aAddress), pEntry->m_Info.m_aAddresses, pEntry->m_Info.m_NumAddresses);
	UpdateServerCommunity(&pEntry->m_Info);
	str_copy(pEntry->m_Info.m_aName, pEntry->m_Info.m_aAddress, sizeof(pEntry->m_Info.m_aName));

	// check if it's a favorite
	pEntry->m_Info.m_Favorite = m_pFavorites->IsFavorite(pEntry->m_Info.m_aAddresses, pEntry->m_Info.m_NumAddresses);
	pEntry->m_Info.m_FavoriteAllowPing = m_pFavorites->IsPingAllowed(pEntry->m_Info.m_aAddresses, pEntry->m_Info.m_NumAddresses);

	for(int i = 0; i < NumAddrs; i++)
	{
		m_ByAddr[pAddrs[i]] = m_NumServers;
	}

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

void CServerBrowser::OnServerInfoUpdate(const NETADDR &Addr, int Token, const CServerInfo *pInfo)
{
	int BasicToken = Token;
	int ExtraToken = 0;
	if(pInfo->m_Type == SERVERINFO_EXTENDED)
	{
		BasicToken = Token & 0xff;
		ExtraToken = Token >> 8;
	}

	CServerEntry *pEntry = Find(Addr);

	if(m_ServerlistType == IServerBrowser::TYPE_LAN)
	{
		NETADDR Broadcast;
		mem_zero(&Broadcast, sizeof(Broadcast));
		Broadcast.type = m_pNetClient->NetType() | NETTYPE_LINK_BROADCAST;
		int TokenBC = GenerateToken(Broadcast);
		bool Drop = false;
		Drop = Drop || BasicToken != GetBasicToken(TokenBC);
		Drop = Drop || (pInfo->m_Type == SERVERINFO_EXTENDED && ExtraToken != GetExtraToken(TokenBC));
		if(Drop)
		{
			return;
		}

		if(!pEntry)
			pEntry = Add(&Addr, 1);
	}
	else
	{
		if(!pEntry)
		{
			return;
		}
		int TokenAddr = GenerateToken(Addr);
		bool Drop = false;
		Drop = Drop || BasicToken != GetBasicToken(TokenAddr);
		Drop = Drop || (pInfo->m_Type == SERVERINFO_EXTENDED && ExtraToken != GetExtraToken(TokenAddr));
		if(Drop)
		{
			return;
		}
	}

	if(m_ServerlistType == IServerBrowser::TYPE_LAN)
	{
		SetInfo(pEntry, *pInfo);
		pEntry->m_Info.m_Latency = minimum(static_cast<int>((time_get() - m_BroadcastTime) * 1000 / time_freq()), 999);
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
			dbg_msg("serverbrowser", "received ping response from %s", aAddr);
			SetLatency(Addr, Latency);
		}
		pEntry->m_RequestTime = -1; // Request has been answered
	}
	RemoveRequest(pEntry);
	RequestResort();
}

void CServerBrowser::Refresh(int Type, bool Force)
{
	bool ServerListTypeChanged = Force || m_ServerlistType != Type;
	int OldServerListType = m_ServerlistType;
	m_ServerlistType = Type;
	secure_random_fill(m_aTokenSeed, sizeof(m_aTokenSeed));

	if(Type == IServerBrowser::TYPE_LAN || (ServerListTypeChanged && OldServerListType == IServerBrowser::TYPE_LAN))
		CleanUp();

	if(Type == IServerBrowser::TYPE_LAN)
	{
		unsigned char aBuffer[sizeof(SERVERBROWSE_GETINFO) + 1];
		CNetChunk Packet;

		/* do the broadcast version */
		Packet.m_ClientId = -1;
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_Address.type = m_pNetClient->NetType() | NETTYPE_LINK_BROADCAST;
		Packet.m_Flags = NETSENDFLAG_CONNLESS | NETSENDFLAG_EXTENDED;
		Packet.m_DataSize = sizeof(aBuffer);
		Packet.m_pData = aBuffer;
		mem_zero(&Packet.m_aExtraData, sizeof(Packet.m_aExtraData));

		int Token = GenerateToken(Packet.m_Address);
		mem_copy(aBuffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));
		aBuffer[sizeof(SERVERBROWSE_GETINFO)] = GetBasicToken(Token);

		Packet.m_aExtraData[0] = GetExtraToken(Token) >> 8;
		Packet.m_aExtraData[1] = GetExtraToken(Token) & 0xff;

		m_BroadcastTime = time_get();

		for(int Port = LAN_PORT_BEGIN; Port <= LAN_PORT_END; Port++)
		{
			Packet.m_Address.port = Port;
			m_pNetClient->Send(&Packet);
		}

		if(g_Config.m_Debug)
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "serverbrowser", "broadcasting for servers");
	}
	else
	{
		m_pHttp->Refresh();
		m_pPingCache->Load();
		m_RefreshingHttp = true;

		if(ServerListTypeChanged && m_pHttp->NumServers() > 0)
		{
			CleanUp();
			UpdateFromHttp();
			Sort();
		}
	}
}

void CServerBrowser::RequestImpl(const NETADDR &Addr, CServerEntry *pEntry, int *pBasicToken, int *pToken, bool RandomToken) const
{
	if(g_Config.m_Debug)
	{
		char aAddrStr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Addr, aAddrStr, sizeof(aAddrStr), true);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "requesting server info from %s", aAddrStr);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "serverbrowser", aBuf);
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

	unsigned char aBuffer[sizeof(SERVERBROWSE_GETINFO) + 1];
	mem_copy(aBuffer, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO));
	aBuffer[sizeof(SERVERBROWSE_GETINFO)] = GetBasicToken(Token);

	CNetChunk Packet;
	Packet.m_ClientId = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS | NETSENDFLAG_EXTENDED;
	Packet.m_DataSize = sizeof(aBuffer);
	Packet.m_pData = aBuffer;
	mem_zero(&Packet.m_aExtraData, sizeof(Packet.m_aExtraData));
	Packet.m_aExtraData[0] = GetExtraToken(Token) >> 8;
	Packet.m_aExtraData[1] = GetExtraToken(Token) & 0xff;

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
	SetLatency(Addr, minimum(Ping, 999));
}

void CServerBrowser::UpdateFromHttp()
{
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
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "serverbrowser", aBuf);
		}
	}

	int NumServers = m_pHttp->NumServers();
	std::function<bool(const NETADDR *, int)> Want = [](const NETADDR *pAddrs, int NumAddrs) { return true; };
	if(m_ServerlistType == IServerBrowser::TYPE_FAVORITES)
	{
		Want = [this](const NETADDR *pAddrs, int NumAddrs) -> bool {
			return m_pFavorites->IsFavorite(pAddrs, NumAddrs) != TRISTATE::NONE;
		};
	}
	else if(m_ServerlistType >= IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 && m_ServerlistType <= IServerBrowser::TYPE_FAVORITE_COMMUNITY_5)
	{
		const size_t CommunityIndex = m_ServerlistType - IServerBrowser::TYPE_FAVORITE_COMMUNITY_1;
		std::vector<const CCommunity *> vpFavoriteCommunities = FavoriteCommunities();
		dbg_assert(CommunityIndex < vpFavoriteCommunities.size(), "Invalid community index");
		const CCommunity *pWantedCommunity = vpFavoriteCommunities[CommunityIndex];
		const bool IsNoneCommunity = str_comp(pWantedCommunity->Id(), COMMUNITY_NONE) == 0;
		Want = [this, pWantedCommunity, IsNoneCommunity](const NETADDR *pAddrs, int NumAddrs) -> bool {
			for(int AddressIndex = 0; AddressIndex < NumAddrs; AddressIndex++)
			{
				const auto CommunityServer = m_CommunityServersByAddr.find(pAddrs[AddressIndex]);
				if(CommunityServer != m_CommunityServersByAddr.end())
				{
					if(IsNoneCommunity)
					{
						// Servers with community "none" are not present in m_CommunityServersByAddr, so we ignore
						// any server that is found in this map to determine only the servers without community.
						return false;
					}
					else if(str_comp(CommunityServer->second.CommunityId(), pWantedCommunity->Id()) == 0)
					{
						return true;
					}
				}
			}
			return IsNoneCommunity;
		};
	}

	for(int i = 0; i < NumServers; i++)
	{
		CServerInfo Info = m_pHttp->Server(i);
		if(!Want(Info.m_aAddresses, Info.m_NumAddresses))
		{
			continue;
		}
		int Ping = m_pPingCache->GetPing(Info.m_aAddresses, Info.m_NumAddresses);
		Info.m_LatencyIsEstimated = Ping == -1;
		if(Info.m_LatencyIsEstimated)
		{
			Info.m_Latency = CServerInfo::EstimateLatency(OwnLocation, Info.m_Location);
		}
		else
		{
			Info.m_Latency = Ping;
		}
		CServerEntry *pEntry = Add(Info.m_aAddresses, Info.m_NumAddresses);
		SetInfo(pEntry, Info);
		pEntry->m_RequestIgnoreInfo = true;
	}

	if(m_ServerlistType == IServerBrowser::TYPE_FAVORITES)
	{
		const IFavorites::CEntry *pFavorites;
		int NumFavorites;
		m_pFavorites->AllEntries(&pFavorites, &NumFavorites);
		for(int i = 0; i < NumFavorites; i++)
		{
			bool Found = false;
			for(int j = 0; j < pFavorites[i].m_NumAddrs; j++)
			{
				if(Find(pFavorites[i].m_aAddrs[j]))
				{
					Found = true;
					break;
				}
			}
			if(Found)
			{
				continue;
			}
			// (Also add favorites we're not allowed to ping.)
			CServerEntry *pEntry = Add(pFavorites[i].m_aAddrs, pFavorites[i].m_NumAddrs);
			if(pFavorites[i].m_AllowPing)
			{
				QueueRequest(pEntry);
			}
		}
	}

	RequestResort();
}

void CServerBrowser::CleanUp()
{
	// clear out everything
	m_ServerlistHeap.Reset();
	m_NumServers = 0;
	m_NumSortedServers = 0;
	m_NumSortedPlayers = 0;
	m_ByAddr.clear();
	m_pFirstReqServer = nullptr;
	m_pLastReqServer = nullptr;
	m_NumRequests = 0;
	m_CurrentMaxRequests = g_Config.m_BrMaxRequests;
}

void CServerBrowser::Update()
{
	int64_t Timeout = time_freq();
	int64_t Now = time_get();

	const char *pHttpBestUrl;
	if(!m_pHttp->GetBestUrl(&pHttpBestUrl) && pHttpBestUrl != m_pHttpPrevBestUrl)
	{
		str_copy(g_Config.m_BrCachedBestServerinfoUrl, pHttpBestUrl);
		m_pHttpPrevBestUrl = pHttpBestUrl;
	}

	m_pHttp->Update();

	if(m_ServerlistType != TYPE_LAN && m_RefreshingHttp && !m_pHttp->IsRefreshing())
	{
		m_RefreshingHttp = false;
		CleanUp();
		UpdateFromHttp();
		// TODO: move this somewhere else
		Sort();
		return;
	}

	CServerEntry *pEntry = m_pFirstReqServer;
	int Count = 0;
	while(true)
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
			RequestImpl(pEntry->m_Info.m_aAddresses[0], pEntry, nullptr, nullptr, false);
		}

		Count++;
		pEntry = pEntry->m_pNextReq;
	}

	if(m_pFirstReqServer && Count == 0 && m_CurrentMaxRequests > 1) //NO More current Server Requests
	{
		//reset old ones
		pEntry = m_pFirstReqServer;
		while(true)
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
		while(true)
		{
			if(!pEntry) // no more entries
				break;
			CServerEntry *pNext = pEntry->m_pNextReq;
			RemoveRequest(pEntry); //release request
			pEntry = pNext;
		}
	}

	// check if we need to resort
	if(m_Sorthash != SortHash() || m_NeedResort)
	{
		for(int i = 0; i < m_NumServers; i++)
		{
			CServerInfo *pInfo = &m_ppServerlist[i]->m_Info;
			pInfo->m_Favorite = m_pFavorites->IsFavorite(pInfo->m_aAddresses, pInfo->m_NumAddresses);
			pInfo->m_FavoriteAllowPing = m_pFavorites->IsPingAllowed(pInfo->m_aAddresses, pInfo->m_NumAddresses);
		}
		Sort();
		m_NeedResort = false;
	}
}

const json_value *CServerBrowser::LoadDDNetInfo()
{
	LoadDDNetInfoJson();
	LoadDDNetLocation();
	LoadDDNetServers();
	for(int i = 0; i < m_NumServers; i++)
	{
		UpdateServerCommunity(&m_ppServerlist[i]->m_Info);
		UpdateServerRank(&m_ppServerlist[i]->m_Info);
	}
	return m_pDDNetInfo;
}

void CServerBrowser::LoadDDNetInfoJson()
{
	void *pBuf;
	unsigned Length;
	if(!m_pStorage->ReadFile(DDNET_INFO_FILE, IStorage::TYPE_SAVE, &pBuf, &Length))
	{
		m_DDNetInfoSha256 = SHA256_ZEROED;
		return;
	}

	m_DDNetInfoSha256 = sha256(pBuf, Length);

	json_value_free(m_pDDNetInfo);
	json_settings JsonSettings{};
	char aError[256];
	m_pDDNetInfo = json_parse_ex(&JsonSettings, static_cast<json_char *>(pBuf), Length, aError);
	free(pBuf);

	if(m_pDDNetInfo == nullptr)
	{
		log_error("serverbrowser", "invalid info json: '%s'", aError);
	}
	else if(m_pDDNetInfo->type != json_object)
	{
		log_error("serverbrowser", "invalid info root");
		json_value_free(m_pDDNetInfo);
		m_pDDNetInfo = nullptr;
	}
}

void CServerBrowser::LoadDDNetLocation()
{
	m_OwnLocation = CServerInfo::LOC_UNKNOWN;
	if(m_pDDNetInfo)
	{
		const json_value &Location = (*m_pDDNetInfo)["location"];
		if(Location.type != json_string || CServerInfo::ParseLocation(&m_OwnLocation, Location))
		{
			log_error("serverbrowser", "invalid location");
		}
	}
}

bool CServerBrowser::ParseCommunityServers(CCommunity *pCommunity, const json_value &Servers)
{
	for(unsigned ServerIndex = 0; ServerIndex < Servers.u.array.length; ++ServerIndex)
	{
		// pServer - { name, flagId, servers }
		const json_value &Server = Servers[ServerIndex];
		if(Server.type != json_object)
		{
			log_error("serverbrowser", "invalid server (ServerIndex=%u)", ServerIndex);
			return false;
		}

		const json_value &Name = Server["name"];
		const json_value &FlagId = Server["flagId"];
		const json_value &Types = Server["servers"];
		if(Name.type != json_string || FlagId.type != json_integer || Types.type != json_object)
		{
			log_error("serverbrowser", "invalid server attribute (ServerIndex=%u)", ServerIndex);
			return false;
		}
		if(Types.u.object.length == 0)
			continue;

		pCommunity->m_vCountries.emplace_back(Name.u.string.ptr, FlagId.u.integer);
		CCommunityCountry *pCountry = &pCommunity->m_vCountries.back();

		for(unsigned TypeIndex = 0; TypeIndex < Types.u.object.length; ++TypeIndex)
		{
			const json_value &Addresses = *Types.u.object.values[TypeIndex].value;
			if(Addresses.type != json_array)
			{
				log_error("serverbrowser", "invalid addresses (ServerIndex=%u, TypeIndex=%u)", ServerIndex, TypeIndex);
				return false;
			}
			if(Addresses.u.array.length == 0)
				continue;

			const char *pTypeName = Types.u.object.values[TypeIndex].name;

			// add type if it doesn't exist already
			const auto CommunityType = std::find_if(pCommunity->m_vTypes.begin(), pCommunity->m_vTypes.end(), [pTypeName](const auto &Elem) {
				return str_comp(Elem.Name(), pTypeName) == 0;
			});
			if(CommunityType == pCommunity->m_vTypes.end())
			{
				pCommunity->m_vTypes.emplace_back(pTypeName);
			}

			// add addresses
			for(unsigned AddressIndex = 0; AddressIndex < Addresses.u.array.length; ++AddressIndex)
			{
				const json_value &Address = Addresses[AddressIndex];
				if(Address.type != json_string)
				{
					log_error("serverbrowser", "invalid address (ServerIndex=%u, TypeIndex=%u, AddressIndex=%u)", ServerIndex, TypeIndex, AddressIndex);
					return false;
				}
				NETADDR NetAddr;
				if(net_addr_from_str(&NetAddr, Address.u.string.ptr))
				{
					log_error("serverbrowser", "invalid address (ServerIndex=%u, TypeIndex=%u, AddressIndex=%u)", ServerIndex, TypeIndex, AddressIndex);
					continue;
				}
				pCountry->m_vServers.emplace_back(NetAddr, pTypeName);
			}
		}
	}
	return true;
}

bool CServerBrowser::ParseCommunityFinishes(CCommunity *pCommunity, const json_value &Finishes)
{
	for(unsigned FinishIndex = 0; FinishIndex < Finishes.u.array.length; ++FinishIndex)
	{
		const json_value &Finish = Finishes[FinishIndex];
		if(Finish.type != json_string)
		{
			log_error("serverbrowser", "invalid rank (FinishIndex=%u)", FinishIndex);
			return false;
		}
		pCommunity->m_FinishedMaps.emplace((const char *)Finish);
	}
	return true;
}

void CServerBrowser::LoadDDNetServers()
{
	// Parse communities
	m_vCommunities.clear();
	m_CommunityServersByAddr.clear();

	if(!m_pDDNetInfo)
	{
		return;
	}

	const json_value &Communities = (*m_pDDNetInfo)["communities"];
	if(Communities.type != json_array)
	{
		return;
	}

	for(unsigned CommunityIndex = 0; CommunityIndex < Communities.u.array.length; ++CommunityIndex)
	{
		const json_value &Community = Communities[CommunityIndex];
		if(Community.type != json_object)
		{
			log_error("serverbrowser", "invalid community (CommunityIndex=%d)", (int)CommunityIndex);
			continue;
		}
		const json_value &Id = Community["id"];
		if(Id.type != json_string)
		{
			log_error("serverbrowser", "invalid community id (CommunityIndex=%d)", (int)CommunityIndex);
			continue;
		}
		const json_value &Icon = Community["icon"];
		const json_value &IconSha256 = Icon["sha256"];
		const json_value &IconUrl = Icon["url"];
		const json_value &Name = Community["name"];
		const json_value HasFinishes = Community["has_finishes"];
		const json_value *pFinishes = &Community["finishes"];
		const json_value *pServers = &Community["servers"];
		// We accidentally set finishes/servers to be part of icon in
		// the past, so support that, too. Can be removed once we make
		// a breaking change to the whole thing, necessitating a new
		// endpoint.
		if(pFinishes->type == json_none)
		{
			pServers = &Icon["finishes"];
		}
		if(pServers->type == json_none)
		{
			pServers = &Icon["servers"];
		}
		// Backward compatibility.
		if(pFinishes->type == json_none)
		{
			if(str_comp(Id, COMMUNITY_DDNET) == 0)
			{
				pFinishes = &(*m_pDDNetInfo)["maps"];
			}
		}
		if(pServers->type == json_none)
		{
			if(str_comp(Id, COMMUNITY_DDNET) == 0)
			{
				pServers = &(*m_pDDNetInfo)["servers"];
			}
			else if(str_comp(Id, "kog") == 0)
			{
				pServers = &(*m_pDDNetInfo)["servers-kog"];
			}
		}
		if(false ||
			Icon.type != json_object ||
			IconSha256.type != json_string ||
			IconUrl.type != json_string ||
			Name.type != json_string ||
			HasFinishes.type != json_boolean ||
			(pFinishes->type != json_array && pFinishes->type != json_none) ||
			pServers->type != json_array)
		{
			log_error("serverbrowser", "invalid community attribute (CommunityId=%s)", (const char *)Id);
			continue;
		}
		SHA256_DIGEST ParsedIconSha256;
		if(sha256_from_str(&ParsedIconSha256, IconSha256) != 0)
		{
			log_error("serverbrowser", "invalid community icon sha256 (CommunityId=%s)", (const char *)Id);
			continue;
		}
		CCommunity NewCommunity(Id, Name, ParsedIconSha256, IconUrl);
		if(!ParseCommunityServers(&NewCommunity, *pServers))
		{
			log_error("serverbrowser", "invalid community servers (CommunityId=%s)", NewCommunity.Id());
			continue;
		}
		NewCommunity.m_HasFinishes = HasFinishes;
		if(NewCommunity.m_HasFinishes && pFinishes->type == json_array && !ParseCommunityFinishes(&NewCommunity, *pFinishes))
		{
			log_error("serverbrowser", "invalid community finishes (CommunityId=%s)", NewCommunity.Id());
			continue;
		}

		for(const auto &Country : NewCommunity.Countries())
		{
			for(const auto &Server : Country.Servers())
			{
				m_CommunityServersByAddr.emplace(Server.Address(), CCommunityServer(NewCommunity.Id(), Country.Name(), Server.TypeName()));
			}
		}
		m_vCommunities.push_back(std::move(NewCommunity));
	}

	// Add default none community
	{
		CCommunity NoneCommunity(COMMUNITY_NONE, "None", SHA256_ZEROED, "");
		NoneCommunity.m_vCountries.emplace_back(COMMUNITY_COUNTRY_NONE, -1);
		NoneCommunity.m_vTypes.emplace_back(COMMUNITY_TYPE_NONE);
		m_vCommunities.push_back(std::move(NoneCommunity));
	}

	// Remove unknown elements from exclude lists
	CleanFilters();
}

void CServerBrowser::UpdateServerFilteredPlayers(CServerInfo *pInfo) const
{
	pInfo->m_NumFilteredPlayers = g_Config.m_BrFilterSpectators ? pInfo->m_NumPlayers : pInfo->m_NumClients;
	if(g_Config.m_BrFilterConnectingPlayers)
	{
		for(const auto &Client : pInfo->m_aClients)
		{
			if((!g_Config.m_BrFilterSpectators || Client.m_Player) && str_comp(Client.m_aName, "(connecting)") == 0 && Client.m_aClan[0] == '\0')
				pInfo->m_NumFilteredPlayers--;
		}
	}
}

void CServerBrowser::UpdateServerFriends(CServerInfo *pInfo) const
{
	pInfo->m_FriendState = IFriends::FRIEND_NO;
	pInfo->m_FriendNum = 0;
	for(int ClientIndex = 0; ClientIndex < minimum(pInfo->m_NumReceivedClients, (int)MAX_CLIENTS); ClientIndex++)
	{
		pInfo->m_aClients[ClientIndex].m_FriendState = m_pFriends->GetFriendState(pInfo->m_aClients[ClientIndex].m_aName, pInfo->m_aClients[ClientIndex].m_aClan);
		pInfo->m_FriendState = maximum(pInfo->m_FriendState, pInfo->m_aClients[ClientIndex].m_FriendState);
		if(pInfo->m_aClients[ClientIndex].m_FriendState != IFriends::FRIEND_NO)
			pInfo->m_FriendNum++;
	}
}

void CServerBrowser::UpdateServerCommunity(CServerInfo *pInfo) const
{
	for(int AddressIndex = 0; AddressIndex < pInfo->m_NumAddresses; AddressIndex++)
	{
		const auto Community = m_CommunityServersByAddr.find(pInfo->m_aAddresses[AddressIndex]);
		if(Community != m_CommunityServersByAddr.end())
		{
			str_copy(pInfo->m_aCommunityId, Community->second.CommunityId());
			str_copy(pInfo->m_aCommunityCountry, Community->second.CountryName());
			str_copy(pInfo->m_aCommunityType, Community->second.TypeName());
			return;
		}
	}
	str_copy(pInfo->m_aCommunityId, COMMUNITY_NONE);
	str_copy(pInfo->m_aCommunityCountry, COMMUNITY_COUNTRY_NONE);
	str_copy(pInfo->m_aCommunityType, COMMUNITY_TYPE_NONE);
}

void CServerBrowser::UpdateServerRank(CServerInfo *pInfo) const
{
	const CCommunity *pCommunity = Community(pInfo->m_aCommunityId);
	pInfo->m_HasRank = pCommunity == nullptr ? CServerInfo::RANK_UNAVAILABLE : pCommunity->HasRank(pInfo->m_aMap);
}

const char *CServerBrowser::GetTutorialServer()
{
	const CCommunity *pCommunity = Community(COMMUNITY_DDNET);
	if(pCommunity == nullptr)
		return nullptr;

	const char *pBestAddr = nullptr;
	int BestLatency = std::numeric_limits<int>::max();
	for(const auto &Country : pCommunity->Countries())
	{
		for(const auto &Server : Country.Servers())
		{
			if(str_comp(Server.TypeName(), "Tutorial") != 0)
				continue;
			const CServerEntry *pEntry = Find(Server.Address());
			if(!pEntry)
				continue;
			if(pEntry->m_Info.m_NumPlayers > pEntry->m_Info.m_MaxPlayers - 10)
				continue;
			if(pEntry->m_Info.m_Latency >= BestLatency)
				continue;
			BestLatency = pEntry->m_Info.m_Latency;
			pBestAddr = pEntry->m_Info.m_aAddress;
		}
	}
	return pBestAddr;
}

bool CServerBrowser::IsRefreshing() const
{
	return m_pFirstReqServer != nullptr;
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

bool CCommunity::HasCountry(const char *pCountryName) const
{
	return std::find_if(Countries().begin(), Countries().end(), [pCountryName](const auto &Elem) {
		return str_comp(Elem.Name(), pCountryName) == 0;
	}) != Countries().end();
}

bool CCommunity::HasType(const char *pTypeName) const
{
	return std::find_if(Types().begin(), Types().end(), [pTypeName](const auto &Elem) {
		return str_comp(Elem.Name(), pTypeName) == 0;
	}) != Types().end();
}

CServerInfo::ERankState CCommunity::HasRank(const char *pMap) const
{
	if(!HasRanks())
		return CServerInfo::RANK_UNAVAILABLE;
	const CCommunityMap Needle(pMap);
	return m_FinishedMaps.count(Needle) == 0 ? CServerInfo::RANK_UNRANKED : CServerInfo::RANK_RANKED;
}

const std::vector<CCommunity> &CServerBrowser::Communities() const
{
	return m_vCommunities;
}

const CCommunity *CServerBrowser::Community(const char *pCommunityId) const
{
	const auto Community = std::find_if(Communities().begin(), Communities().end(), [pCommunityId](const auto &Elem) {
		return str_comp(Elem.Id(), pCommunityId) == 0;
	});
	return Community == Communities().end() ? nullptr : &(*Community);
}

std::vector<const CCommunity *> CServerBrowser::SelectedCommunities() const
{
	std::vector<const CCommunity *> vpSelected;
	for(const auto &Community : Communities())
	{
		if(!CommunitiesFilter().Filtered(Community.Id()))
		{
			vpSelected.push_back(&Community);
		}
	}
	return vpSelected;
}

std::vector<const CCommunity *> CServerBrowser::FavoriteCommunities() const
{
	// This is done differently than SelectedCommunities because the favorite
	// communities should be returned in the order specified by the user.
	std::vector<const CCommunity *> vpFavorites;
	for(const auto &CommunityId : FavoriteCommunitiesFilter().Entries())
	{
		const CCommunity *pCommunity = Community(CommunityId.Id());
		if(pCommunity)
		{
			vpFavorites.push_back(pCommunity);
		}
	}
	return vpFavorites;
}

std::vector<const CCommunity *> CServerBrowser::CurrentCommunities() const
{
	if(m_ServerlistType == IServerBrowser::TYPE_INTERNET || m_ServerlistType == IServerBrowser::TYPE_FAVORITES)
	{
		return SelectedCommunities();
	}
	else if(m_ServerlistType >= IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 && m_ServerlistType <= IServerBrowser::TYPE_FAVORITE_COMMUNITY_5)
	{
		const size_t CommunityIndex = m_ServerlistType - IServerBrowser::TYPE_FAVORITE_COMMUNITY_1;
		std::vector<const CCommunity *> vpFavoriteCommunities = FavoriteCommunities();
		dbg_assert(CommunityIndex < vpFavoriteCommunities.size(), "Invalid favorite community serverbrowser type");
		return {vpFavoriteCommunities[CommunityIndex]};
	}
	else
	{
		return {};
	}
}

unsigned CServerBrowser::CurrentCommunitiesHash() const
{
	std::vector<const CCommunity *> vpCommunities = CurrentCommunities();
	unsigned Hash = 5381;
	for(const CCommunity *pCommunity : CurrentCommunities())
	{
		Hash = (Hash << 5) + Hash + str_quickhash(pCommunity->Id());
	}
	return Hash;
}

void CCommunityCache::Update(bool Force)
{
	const unsigned CommunitiesHash = m_pServerBrowser->CurrentCommunitiesHash();
	const bool TypeChanged = m_LastType != m_pServerBrowser->GetCurrentType();
	const bool CurrentCommunitiesChanged = m_LastType == m_pServerBrowser->GetCurrentType() && m_SelectedCommunitiesHash != CommunitiesHash;
	if(CurrentCommunitiesChanged && m_pServerBrowser->GetCurrentType() >= IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 && m_pServerBrowser->GetCurrentType() <= IServerBrowser::TYPE_FAVORITE_COMMUNITY_5)
	{
		// Favorite community was changed while its type is active,
		// refresh to get correct serverlist for updated community.
		m_pServerBrowser->Refresh(m_pServerBrowser->GetCurrentType(), true);
	}

	if(!Force && m_InfoSha256 == m_pServerBrowser->DDNetInfoSha256() &&
		!CurrentCommunitiesChanged && !TypeChanged)
	{
		return;
	}

	m_InfoSha256 = m_pServerBrowser->DDNetInfoSha256();
	m_LastType = m_pServerBrowser->GetCurrentType();
	m_SelectedCommunitiesHash = CommunitiesHash;
	m_vpSelectedCommunities = m_pServerBrowser->CurrentCommunities();

	m_vpSelectableCountries.clear();
	m_vpSelectableTypes.clear();
	for(const CCommunity *pCommunity : m_vpSelectedCommunities)
	{
		for(const auto &Country : pCommunity->Countries())
		{
			const auto ExistingCountry = std::find_if(m_vpSelectableCountries.begin(), m_vpSelectableCountries.end(), [&](const CCommunityCountry *pOther) {
				return str_comp(Country.Name(), pOther->Name()) == 0 && Country.FlagId() == pOther->FlagId();
			});
			if(ExistingCountry == m_vpSelectableCountries.end())
			{
				m_vpSelectableCountries.push_back(&Country);
			}
		}
		for(const auto &Type : pCommunity->Types())
		{
			const auto ExistingType = std::find_if(m_vpSelectableTypes.begin(), m_vpSelectableTypes.end(), [&](const CCommunityType *pOther) {
				return str_comp(Type.Name(), pOther->Name()) == 0;
			});
			if(ExistingType == m_vpSelectableTypes.end())
			{
				m_vpSelectableTypes.push_back(&Type);
			}
		}
	}

	m_AnyRanksAvailable = std::any_of(m_vpSelectedCommunities.begin(), m_vpSelectedCommunities.end(), [](const CCommunity *pCommunity) {
		return pCommunity->HasRanks();
	});

	// Country/type filters not shown if there are no countries and types, or if only the none-community is selected
	m_CountryTypesFilterAvailable = (!m_vpSelectableCountries.empty() || !m_vpSelectableTypes.empty()) &&
					(m_vpSelectedCommunities.size() != 1 || str_comp(m_vpSelectedCommunities[0]->Id(), IServerBrowser::COMMUNITY_NONE) != 0);

	if(m_pServerBrowser->GetCurrentType() >= IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 && m_pServerBrowser->GetCurrentType() <= IServerBrowser::TYPE_FAVORITE_COMMUNITY_5)
	{
		const size_t CommunityIndex = m_pServerBrowser->GetCurrentType() - IServerBrowser::TYPE_FAVORITE_COMMUNITY_1;
		std::vector<const CCommunity *> vpFavoriteCommunities = m_pServerBrowser->FavoriteCommunities();
		dbg_assert(CommunityIndex < vpFavoriteCommunities.size(), "Invalid favorite community serverbrowser type");
		m_pCountryTypeFilterKey = vpFavoriteCommunities[CommunityIndex]->Id();
	}
	else
	{
		m_pCountryTypeFilterKey = IServerBrowser::COMMUNITY_ALL;
	}

	m_pServerBrowser->CleanFilters();
}

void CFavoriteCommunityFilterList::Add(const char *pCommunityId)
{
	// Remove community if it's already a favorite, so it will be added again at
	// the end of the list, to allow setting the entire list easier with binds.
	Remove(pCommunityId);

	// Ensure maximum number of favorite communities, by removing the least-recently
	// added community from the beginning, when the maximum number of favorite
	// communities has been reached.
	constexpr size_t MaxFavoriteCommunities = IServerBrowser::TYPE_FAVORITE_COMMUNITY_5 - IServerBrowser::TYPE_FAVORITE_COMMUNITY_1 + 1;
	if(m_vEntries.size() >= MaxFavoriteCommunities)
	{
		dbg_assert(m_vEntries.size() == MaxFavoriteCommunities, "Maximum number of communities can never be exceeded");
		m_vEntries.erase(m_vEntries.begin());
	}
	m_vEntries.emplace_back(pCommunityId);
}

void CFavoriteCommunityFilterList::Remove(const char *pCommunityId)
{
	auto FoundCommunity = std::find(m_vEntries.begin(), m_vEntries.end(), CCommunityId(pCommunityId));
	if(FoundCommunity != m_vEntries.end())
	{
		m_vEntries.erase(FoundCommunity);
	}
}

void CFavoriteCommunityFilterList::Clear()
{
	m_vEntries.clear();
}

bool CFavoriteCommunityFilterList::Filtered(const char *pCommunityId) const
{
	return std::find(m_vEntries.begin(), m_vEntries.end(), CCommunityId(pCommunityId)) != m_vEntries.end();
}

bool CFavoriteCommunityFilterList::Empty() const
{
	return m_vEntries.empty();
}

void CFavoriteCommunityFilterList::Clean(const std::vector<CCommunity> &vAllowedCommunities)
{
	auto It = std::remove_if(m_vEntries.begin(), m_vEntries.end(), [&](const auto &Community) {
		return std::find_if(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const CCommunity &AllowedCommunity) {
			return str_comp(Community.Id(), AllowedCommunity.Id()) == 0;
		}) == vAllowedCommunities.end();
	});
	m_vEntries.erase(It, m_vEntries.end());
}

void CFavoriteCommunityFilterList::Save(IConfigManager *pConfigManager) const
{
	char aBuf[32 + CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	for(const auto &FavoriteCommunity : m_vEntries)
	{
		str_copy(aBuf, "add_favorite_community \"");
		str_append(aBuf, FavoriteCommunity.Id());
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf);
	}
}

const std::vector<CCommunityId> &CFavoriteCommunityFilterList::Entries() const
{
	return m_vEntries;
}

template<typename TNamedElement, typename TElementName>
static bool IsSubsetEquals(const std::vector<const TNamedElement *> &vpLeft, const std::unordered_set<TElementName> &Right)
{
	return vpLeft.size() <= Right.size() && std::all_of(vpLeft.begin(), vpLeft.end(), [&](const TNamedElement *pElem) {
		return Right.count(TElementName(pElem->Name())) > 0;
	});
}

void CExcludedCommunityFilterList::Add(const char *pCommunityId)
{
	m_Entries.emplace(pCommunityId);
}

void CExcludedCommunityFilterList::Remove(const char *pCommunityId)
{
	m_Entries.erase(CCommunityId(pCommunityId));
}

void CExcludedCommunityFilterList::Clear()
{
	m_Entries.clear();
}

bool CExcludedCommunityFilterList::Filtered(const char *pCommunityId) const
{
	return std::find(m_Entries.begin(), m_Entries.end(), CCommunityId(pCommunityId)) != m_Entries.end();
}

bool CExcludedCommunityFilterList::Empty() const
{
	return m_Entries.empty();
}

void CExcludedCommunityFilterList::Clean(const std::vector<CCommunity> &vAllowedCommunities)
{
	for(auto It = m_Entries.begin(); It != m_Entries.end();)
	{
		const bool Found = std::find_if(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const CCommunity &AllowedCommunity) {
			return str_comp(It->Id(), AllowedCommunity.Id()) == 0;
		}) != vAllowedCommunities.end();
		if(Found)
		{
			++It;
		}
		else
		{
			It = m_Entries.erase(It);
		}
	}
	// Prevent filter that would exclude all allowed communities
	if(m_Entries.size() == vAllowedCommunities.size())
	{
		m_Entries.clear();
	}
}

void CExcludedCommunityFilterList::Save(IConfigManager *pConfigManager) const
{
	char aBuf[32 + CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	for(const auto &ExcludedCommunity : m_Entries)
	{
		str_copy(aBuf, "add_excluded_community \"");
		str_append(aBuf, ExcludedCommunity.Id());
		str_append(aBuf, "\"");
		pConfigManager->WriteLine(aBuf);
	}
}

void CExcludedCommunityCountryFilterList::Add(const char *pCountryName)
{
	// Handle special case that all selectable entries are currently filtered,
	// where adding more entries to the exclusion list would have no effect.
	auto CommunityEntry = m_Entries.find(m_pCommunityCache->CountryTypeFilterKey());
	if(CommunityEntry != m_Entries.end() && IsSubsetEquals(m_pCommunityCache->SelectableCountries(), CommunityEntry->second))
	{
		for(const CCommunityCountry *pSelectableCountry : m_pCommunityCache->SelectableCountries())
		{
			CommunityEntry->second.erase(pSelectableCountry->Name());
		}
	}

	Add(m_pCommunityCache->CountryTypeFilterKey(), pCountryName);
}

void CExcludedCommunityCountryFilterList::Add(const char *pCommunityId, const char *pCountryName)
{
	CCommunityId CommunityId(pCommunityId);
	if(m_Entries.find(CommunityId) == m_Entries.end())
	{
		m_Entries[CommunityId] = {};
	}
	m_Entries[CommunityId].emplace(pCountryName);
}

void CExcludedCommunityCountryFilterList::Remove(const char *pCountryName)
{
	Remove(m_pCommunityCache->CountryTypeFilterKey(), pCountryName);
}

void CExcludedCommunityCountryFilterList::Remove(const char *pCommunityId, const char *pCountryName)
{
	auto CommunityEntry = m_Entries.find(CCommunityId(pCommunityId));
	if(CommunityEntry != m_Entries.end())
	{
		CommunityEntry->second.erase(pCountryName);
	}
}

void CExcludedCommunityCountryFilterList::Clear()
{
	auto CommunityEntry = m_Entries.find(m_pCommunityCache->CountryTypeFilterKey());
	if(CommunityEntry != m_Entries.end())
	{
		CommunityEntry->second.clear();
	}
}

bool CExcludedCommunityCountryFilterList::Filtered(const char *pCountryName) const
{
	auto CommunityEntry = m_Entries.find(CCommunityId(m_pCommunityCache->CountryTypeFilterKey()));
	if(CommunityEntry == m_Entries.end())
		return false;

	const auto &CountryEntries = CommunityEntry->second;
	return !IsSubsetEquals(m_pCommunityCache->SelectableCountries(), CountryEntries) &&
	       CountryEntries.find(CCommunityCountryName(pCountryName)) != CountryEntries.end();
}

bool CExcludedCommunityCountryFilterList::Empty() const
{
	auto CommunityEntry = m_Entries.find(CCommunityId(m_pCommunityCache->CountryTypeFilterKey()));
	return CommunityEntry == m_Entries.end() ||
	       CommunityEntry->second.empty() ||
	       IsSubsetEquals(m_pCommunityCache->SelectableCountries(), CommunityEntry->second);
}

void CExcludedCommunityCountryFilterList::Clean(const std::vector<CCommunity> &vAllowedCommunities)
{
	for(auto It = m_Entries.begin(); It != m_Entries.end();)
	{
		const bool AllEntry = str_comp(It->first.Id(), IServerBrowser::COMMUNITY_ALL) == 0;
		const bool Found = AllEntry || std::find_if(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const CCommunity &AllowedCommunity) {
			return str_comp(It->first.Id(), AllowedCommunity.Id()) == 0;
		}) != vAllowedCommunities.end();
		if(Found)
		{
			++It;
		}
		else
		{
			It = m_Entries.erase(It);
		}
	}

	for(const CCommunity &AllowedCommunity : vAllowedCommunities)
	{
		auto CommunityEntry = m_Entries.find(CCommunityId(AllowedCommunity.Id()));
		if(CommunityEntry != m_Entries.end())
		{
			auto &CountryEntries = CommunityEntry->second;
			for(auto It = CountryEntries.begin(); It != CountryEntries.end();)
			{
				if(AllowedCommunity.HasCountry(It->Name()))
				{
					++It;
				}
				else
				{
					It = CountryEntries.erase(It);
				}
			}
			// Prevent filter that would exclude all allowed countries
			if(CountryEntries.size() == AllowedCommunity.Countries().size())
			{
				CountryEntries.clear();
			}
		}
	}

	auto AllCommunityEntry = m_Entries.find(CCommunityId(IServerBrowser::COMMUNITY_ALL));
	if(AllCommunityEntry != m_Entries.end())
	{
		auto &CountryEntries = AllCommunityEntry->second;
		for(auto It = CountryEntries.begin(); It != CountryEntries.end();)
		{
			if(std::any_of(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const auto &Community) { return Community.HasCountry(It->Name()); }))
			{
				++It;
			}
			else
			{
				It = CountryEntries.erase(It);
			}
		}
		// Prevent filter that would exclude all allowed countries
		std::unordered_set<CCommunityCountryName> UniqueCountries;
		for(const CCommunity &AllowedCommunity : vAllowedCommunities)
		{
			for(const CCommunityCountry &Country : AllowedCommunity.Countries())
			{
				UniqueCountries.emplace(Country.Name());
			}
		}
		if(CountryEntries.size() == UniqueCountries.size())
		{
			CountryEntries.clear();
		}
	}
}

void CExcludedCommunityCountryFilterList::Save(IConfigManager *pConfigManager) const
{
	char aBuf[32 + CServerInfo::MAX_COMMUNITY_ID_LENGTH + CServerInfo::MAX_COMMUNITY_COUNTRY_LENGTH];
	for(const auto &[Community, Countries] : m_Entries)
	{
		for(const auto &Country : Countries)
		{
			str_copy(aBuf, "add_excluded_country \"");
			str_append(aBuf, Community.Id());
			str_append(aBuf, "\" \"");
			str_append(aBuf, Country.Name());
			str_append(aBuf, "\"");
			pConfigManager->WriteLine(aBuf);
		}
	}
}

void CExcludedCommunityTypeFilterList::Add(const char *pTypeName)
{
	// Handle special case that all selectable entries are currently filtered,
	// where adding more entries to the exclusion list would have no effect.
	auto CommunityEntry = m_Entries.find(m_pCommunityCache->CountryTypeFilterKey());
	if(CommunityEntry != m_Entries.end() && IsSubsetEquals(m_pCommunityCache->SelectableTypes(), CommunityEntry->second))
	{
		for(const CCommunityType *pSelectableType : m_pCommunityCache->SelectableTypes())
		{
			CommunityEntry->second.erase(pSelectableType->Name());
		}
	}

	Add(m_pCommunityCache->CountryTypeFilterKey(), pTypeName);
}

void CExcludedCommunityTypeFilterList::Add(const char *pCommunityId, const char *pTypeName)
{
	CCommunityId CommunityId(pCommunityId);
	if(m_Entries.find(CommunityId) == m_Entries.end())
	{
		m_Entries[CommunityId] = {};
	}
	m_Entries[CommunityId].emplace(pTypeName);
}

void CExcludedCommunityTypeFilterList::Remove(const char *pTypeName)
{
	Remove(m_pCommunityCache->CountryTypeFilterKey(), pTypeName);
}

void CExcludedCommunityTypeFilterList::Remove(const char *pCommunityId, const char *pTypeName)
{
	auto CommunityEntry = m_Entries.find(CCommunityId(pCommunityId));
	if(CommunityEntry != m_Entries.end())
	{
		CommunityEntry->second.erase(pTypeName);
	}
}

void CExcludedCommunityTypeFilterList::Clear()
{
	auto CommunityEntry = m_Entries.find(m_pCommunityCache->CountryTypeFilterKey());
	if(CommunityEntry != m_Entries.end())
	{
		CommunityEntry->second.clear();
	}
}

bool CExcludedCommunityTypeFilterList::Filtered(const char *pTypeName) const
{
	auto CommunityEntry = m_Entries.find(CCommunityId(m_pCommunityCache->CountryTypeFilterKey()));
	if(CommunityEntry == m_Entries.end())
		return false;

	const auto &TypeEntries = CommunityEntry->second;
	return !IsSubsetEquals(m_pCommunityCache->SelectableTypes(), TypeEntries) &&
	       TypeEntries.find(CCommunityTypeName(pTypeName)) != TypeEntries.end();
}

bool CExcludedCommunityTypeFilterList::Empty() const
{
	auto CommunityEntry = m_Entries.find(CCommunityId(m_pCommunityCache->CountryTypeFilterKey()));
	return CommunityEntry == m_Entries.end() ||
	       CommunityEntry->second.empty() ||
	       IsSubsetEquals(m_pCommunityCache->SelectableTypes(), CommunityEntry->second);
}

void CExcludedCommunityTypeFilterList::Clean(const std::vector<CCommunity> &vAllowedCommunities)
{
	for(auto It = m_Entries.begin(); It != m_Entries.end();)
	{
		const bool AllEntry = str_comp(It->first.Id(), IServerBrowser::COMMUNITY_ALL) == 0;
		const bool Found = AllEntry || std::find_if(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const CCommunity &AllowedCommunity) {
			return str_comp(It->first.Id(), AllowedCommunity.Id()) == 0;
		}) != vAllowedCommunities.end();
		if(Found)
		{
			++It;
		}
		else
		{
			It = m_Entries.erase(It);
		}
	}

	for(const CCommunity &AllowedCommunity : vAllowedCommunities)
	{
		auto CommunityEntry = m_Entries.find(CCommunityId(AllowedCommunity.Id()));
		if(CommunityEntry != m_Entries.end())
		{
			auto &TypeEntries = CommunityEntry->second;
			for(auto It = TypeEntries.begin(); It != TypeEntries.end();)
			{
				if(AllowedCommunity.HasType(It->Name()))
				{
					++It;
				}
				else
				{
					It = TypeEntries.erase(It);
				}
			}
			// Prevent filter that would exclude all allowed types
			if(TypeEntries.size() == AllowedCommunity.Types().size())
			{
				TypeEntries.clear();
			}
		}
	}

	auto AllCommunityEntry = m_Entries.find(CCommunityId(IServerBrowser::COMMUNITY_ALL));
	if(AllCommunityEntry != m_Entries.end())
	{
		auto &TypeEntries = AllCommunityEntry->second;
		for(auto It = TypeEntries.begin(); It != TypeEntries.end();)
		{
			if(std::any_of(vAllowedCommunities.begin(), vAllowedCommunities.end(), [&](const auto &Community) { return Community.HasType(It->Name()); }))
			{
				++It;
			}
			else
			{
				It = TypeEntries.erase(It);
			}
		}
		// Prevent filter that would exclude all allowed types
		std::unordered_set<CCommunityCountryName> UniqueTypes;
		for(const CCommunity &AllowedCommunity : vAllowedCommunities)
		{
			for(const CCommunityType &Type : AllowedCommunity.Types())
			{
				UniqueTypes.emplace(Type.Name());
			}
		}
		if(TypeEntries.size() == UniqueTypes.size())
		{
			TypeEntries.clear();
		}
	}
}

void CExcludedCommunityTypeFilterList::Save(IConfigManager *pConfigManager) const
{
	char aBuf[32 + CServerInfo::MAX_COMMUNITY_ID_LENGTH + CServerInfo::MAX_COMMUNITY_TYPE_LENGTH];
	for(const auto &[Community, Types] : m_Entries)
	{
		for(const auto &Type : Types)
		{
			str_copy(aBuf, "add_excluded_type \"");
			str_append(aBuf, Community.Id());
			str_append(aBuf, "\" \"");
			str_append(aBuf, Type.Name());
			str_append(aBuf, "\"");
			pConfigManager->WriteLine(aBuf);
		}
	}
}

void CServerBrowser::CleanFilters()
{
	// Keep filters if we failed to load any communities
	if(Communities().empty())
		return;
	FavoriteCommunitiesFilter().Clean(Communities());
	CommunitiesFilter().Clean(Communities());
	CountriesFilter().Clean(Communities());
	TypesFilter().Clean(Communities());
}

bool CServerBrowser::IsRegistered(const NETADDR &Addr)
{
	const int NumServers = m_pHttp->NumServers();
	for(int i = 0; i < NumServers; i++)
	{
		const CServerInfo &Info = m_pHttp->Server(i);
		for(int j = 0; j < Info.m_NumAddresses; j++)
		{
			if(net_addr_comp(&Info.m_aAddresses[j], &Addr) == 0)
			{
				return true;
			}
		}
	}
	return false;
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
	static const char s_apLocations[NUM_LOCS][6] = {
		"an", // LOC_UNKNOWN
		"af", // LOC_AFRICA
		"as", // LOC_ASIA
		"oc", // LOC_AUSTRALIA
		"eu", // LOC_EUROPE
		"na", // LOC_NORTH_AMERICA
		"sa", // LOC_SOUTH_AMERICA
		"as:cn", // LOC_CHINA
	};
	for(int i = std::size(s_apLocations) - 1; i >= 0; i--)
	{
		if(str_startswith(pString, s_apLocations[i]))
		{
			*pResult = i;
			return false;
		}
	}
	return true;
}
