/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SERVERBROWSER_H
#define ENGINE_CLIENT_SERVERBROWSER_H

#include <base/system.h>

#include <engine/console.h>
#include <engine/serverbrowser.h>
#include <engine/shared/memheap.h>

#include <unordered_map>

typedef struct _json_value json_value;
class CNetClient;
class IConfigManager;
class IConsole;
class IEngine;
class IFavorites;
class IFriends;
class IServerBrowserHttp;
class IServerBrowserPingCache;
class IStorage;
class IHttp;

class CCommunityServer
{
	char m_aCommunityId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	char m_aCountryName[CServerInfo::MAX_COMMUNITY_COUNTRY_LENGTH];
	char m_aTypeName[CServerInfo::MAX_COMMUNITY_TYPE_LENGTH];

public:
	CCommunityServer(const char *pCommunityId, const char *pCountryName, const char *pTypeName)
	{
		str_copy(m_aCommunityId, pCommunityId);
		str_copy(m_aCountryName, pCountryName);
		str_copy(m_aTypeName, pTypeName);
	}

	const char *CommunityId() const { return m_aCommunityId; }
	const char *CountryName() const { return m_aCountryName; }
	const char *TypeName() const { return m_aTypeName; }
};

class CFilterList : public IFilterList
{
	char *m_pFilter;
	size_t m_FilterSize;

public:
	CFilterList(char *pFilter, size_t FilterSize) :
		m_pFilter(pFilter), m_FilterSize(FilterSize)
	{
	}

	void Add(const char *pElement) override;
	void Remove(const char *pElement) override;
	void Clear() override;
	bool Filtered(const char *pElement) const override;
	bool Empty() const override;
	void Clean(const std::vector<const char *> &vpAllowedElements);
};

class CServerBrowser : public IServerBrowser
{
public:
	class CServerEntry
	{
	public:
		int64_t m_RequestTime;
		bool m_RequestIgnoreInfo;
		int m_GotInfo;
		CServerInfo m_Info;

		CServerEntry *m_pPrevReq; // request list
		CServerEntry *m_pNextReq;
	};

	CServerBrowser();
	virtual ~CServerBrowser();

	// interface functions
	void Refresh(int Type) override;
	bool IsRefreshing() const override;
	bool IsGettingServerlist() const override;
	int LoadingProgression() const override;
	void RequestResort() { m_NeedResort = true; }

	int NumServers() const override { return m_NumServers; }
	int Players(const CServerInfo &Item) const override;
	int Max(const CServerInfo &Item) const override;
	int NumSortedServers() const override { return m_NumSortedServers; }
	int NumSortedPlayers() const override { return m_NumSortedPlayers; }
	const CServerInfo *SortedGet(int Index) const override;

	const json_value *LoadDDNetInfo();
	void LoadDDNetInfoJson();
	void LoadDDNetLocation();
	void LoadDDNetServers();
	void UpdateServerFilteredPlayers(CServerInfo *pInfo) const;
	void UpdateServerFriends(CServerInfo *pInfo) const;
	void UpdateServerCommunity(CServerInfo *pInfo) const;
	void UpdateServerRank(CServerInfo *pInfo) const;
	const char *GetTutorialServer() override;

	const std::vector<CCommunity> &Communities() const override;
	const CCommunity *Community(const char *pCommunityId) const override;
	std::vector<const CCommunity *> SelectedCommunities() const override;
	int64_t DDNetInfoUpdateTime() const override { return m_DDNetInfoUpdateTime; }

	CFilterList &CommunitiesFilter() override { return m_CommunitiesFilter; }
	CFilterList &CountriesFilter() override { return m_CountriesFilter; }
	CFilterList &TypesFilter() override { return m_TypesFilter; }
	const CFilterList &CommunitiesFilter() const override { return m_CommunitiesFilter; }
	const CFilterList &CountriesFilter() const override { return m_CountriesFilter; }
	const CFilterList &TypesFilter() const override { return m_TypesFilter; }
	void CleanFilters() override;

	void CommunitiesFilterClean();
	void CountriesFilterClean();
	void TypesFilterClean();

	//
	void Update();
	void OnServerInfoUpdate(const NETADDR &Addr, int Token, const CServerInfo *pInfo);
	void SetHttpInfo(const CServerInfo *pInfo);
	void RequestCurrentServer(const NETADDR &Addr) const;
	void RequestCurrentServerWithRandomToken(const NETADDR &Addr, int *pBasicToken, int *pToken) const;
	void SetCurrentServerPing(const NETADDR &Addr, int Ping);

	void SetBaseInfo(class CNetClient *pClient, const char *pNetVersion);
	void OnInit();

	void QueueRequest(CServerEntry *pEntry);
	CServerEntry *Find(const NETADDR &Addr);
	int GetCurrentType() override { return m_ServerlistType; }
	bool IsRegistered(const NETADDR &Addr);

private:
	CNetClient *m_pNetClient = nullptr;
	IConfigManager *m_pConfigManager = nullptr;
	IConsole *m_pConsole = nullptr;
	IEngine *m_pEngine = nullptr;
	IFriends *m_pFriends = nullptr;
	IFavorites *m_pFavorites = nullptr;
	IStorage *m_pStorage = nullptr;
	IHttp *m_pHttpClient = nullptr;
	char m_aNetVersion[128];

	bool m_RefreshingHttp = false;
	IServerBrowserHttp *m_pHttp = nullptr;
	IServerBrowserPingCache *m_pPingCache = nullptr;
	const char *m_pHttpPrevBestUrl = nullptr;

	CHeap m_ServerlistHeap;
	CServerEntry **m_ppServerlist;
	int *m_pSortedServerlist;
	std::unordered_map<NETADDR, int> m_ByAddr;

	std::vector<CCommunity> m_vCommunities;
	std::unordered_map<NETADDR, CCommunityServer> m_CommunityServersByAddr;

	int m_OwnLocation = CServerInfo::LOC_UNKNOWN;

	CFilterList m_CommunitiesFilter;
	CFilterList m_CountriesFilter;
	CFilterList m_TypesFilter;

	json_value *m_pDDNetInfo;
	int64_t m_DDNetInfoUpdateTime;

	CServerEntry *m_pFirstReqServer; // request list
	CServerEntry *m_pLastReqServer;
	int m_NumRequests;

	bool m_NeedResort;
	int m_Sorthash;

	// used instead of g_Config.br_max_requests to get more servers
	int m_CurrentMaxRequests;

	int m_NumSortedServers;
	int m_NumSortedServersCapacity;
	int m_NumSortedPlayers;
	int m_NumServers;
	int m_NumServerCapacity;

	int m_ServerlistType;
	int64_t m_BroadcastTime;
	unsigned char m_aTokenSeed[16];

	int GenerateToken(const NETADDR &Addr) const;
	static int GetBasicToken(int Token);
	static int GetExtraToken(int Token);

	// sorting criteria
	bool SortCompareName(int Index1, int Index2) const;
	bool SortCompareMap(int Index1, int Index2) const;
	bool SortComparePing(int Index1, int Index2) const;
	bool SortCompareGametype(int Index1, int Index2) const;
	bool SortCompareNumPlayers(int Index1, int Index2) const;
	bool SortCompareNumClients(int Index1, int Index2) const;
	bool SortCompareNumPlayersAndPing(int Index1, int Index2) const;

	//
	void Filter();
	void Sort();
	int SortHash() const;

	void CleanUp();

	void UpdateFromHttp();
	CServerEntry *Add(const NETADDR *pAddrs, int NumAddrs);

	void RemoveRequest(CServerEntry *pEntry);

	void RequestImpl(const NETADDR &Addr, CServerEntry *pEntry, int *pBasicToken, int *pToken, bool RandomToken) const;

	void RegisterCommands();
	static void Con_LeakIpAddress(IConsole::IResult *pResult, void *pUserData);

	void SetInfo(CServerEntry *pEntry, const CServerInfo &Info) const;
	void SetLatency(NETADDR Addr, int Latency);

	static bool ParseCommunityFinishes(CCommunity *pCommunity, const json_value &Finishes);
	static bool ParseCommunityServers(CCommunity *pCommunity, const json_value &Servers);
};

#endif
