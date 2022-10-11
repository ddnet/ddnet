/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_SERVERBROWSER_H
#define ENGINE_CLIENT_SERVERBROWSER_H

#include <base/system.h>

#include <engine/console.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/memheap.h>

#include <unordered_map>

class CNetClient;
class IConfigManager;
class IConsole;
class IEngine;
class IFavorites;
class IFriends;
class IServerBrowserHttp;
class IServerBrowserPingCache;
class IStorage;

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

	struct CNetworkCountry
	{
		enum
		{
			MAX_SERVERS = 1024
		};

		char m_aName[256];
		int m_FlagID;
		NETADDR m_aServers[MAX_SERVERS];
		char m_aTypes[MAX_SERVERS][32];
		int m_NumServers;

		void Reset()
		{
			m_NumServers = 0;
			m_FlagID = -1;
			m_aName[0] = '\0';
		};
	};

	enum
	{
		MAX_FAVORITES = 2048,
		MAX_COUNTRIES = 32,
		MAX_TYPES = 32,
	};

	struct CNetwork
	{
		CNetworkCountry m_aCountries[MAX_COUNTRIES];
		int m_NumCountries;

		char m_aTypes[MAX_TYPES][32];
		int m_NumTypes;
	};

	CServerBrowser();
	virtual ~CServerBrowser();

	// interface functions
	void Refresh(int Type) override;
	bool IsRefreshing() const override;
	bool IsGettingServerlist() const override;
	int LoadingProgression() const override;

	int NumServers() const override { return m_NumServers; }

	int Players(const CServerInfo &Item) const override
	{
		return g_Config.m_BrFilterSpectators ? Item.m_NumPlayers : Item.m_NumClients;
	}

	int Max(const CServerInfo &Item) const override
	{
		return g_Config.m_BrFilterSpectators ? Item.m_MaxPlayers : Item.m_MaxClients;
	}

	int NumSortedServers() const override { return m_NumSortedServers; }
	const CServerInfo *SortedGet(int Index) const override;

	const char *GetTutorialServer() override;
	void LoadDDNetRanks();
	void RecheckOfficial();
	void LoadDDNetServers();
	void LoadDDNetInfoJson();
	const json_value *LoadDDNetInfo();
	int HasRank(const char *pMap);
	int NumCountries(int Network) override { return m_aNetworks[Network].m_NumCountries; }
	int GetCountryFlag(int Network, int Index) override { return m_aNetworks[Network].m_aCountries[Index].m_FlagID; }
	const char *GetCountryName(int Network, int Index) override { return m_aNetworks[Network].m_aCountries[Index].m_aName; }

	int NumTypes(int Network) override { return m_aNetworks[Network].m_NumTypes; }
	const char *GetType(int Network, int Index) override { return m_aNetworks[Network].m_aTypes[Index]; }

	void DDNetFilterAdd(char *pFilter, const char *pName) override;
	void DDNetFilterRem(char *pFilter, const char *pName) override;
	bool DDNetFiltered(char *pFilter, const char *pName) override;
	void CountryFilterClean(int Network) override;
	void TypeFilterClean(int Network) override;

	//
	void Update(bool ForceResort);
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
	IConsole *m_pConsole = nullptr;
	IEngine *m_pEngine = nullptr;
	IFriends *m_pFriends = nullptr;
	IFavorites *m_pFavorites = nullptr;
	IStorage *m_pStorage = nullptr;
	char m_aNetVersion[128];

	bool m_RefreshingHttp = false;
	IServerBrowserHttp *m_pHttp = nullptr;
	IServerBrowserPingCache *m_pPingCache = nullptr;
	const char *m_pHttpPrevBestUrl = nullptr;

	CHeap m_ServerlistHeap;
	CServerEntry **m_ppServerlist;
	int *m_pSortedServerlist;
	std::unordered_map<NETADDR, int> m_ByAddr;

	CNetwork m_aNetworks[NUM_NETWORKS];
	int m_OwnLocation = CServerInfo::LOC_UNKNOWN;

	json_value *m_pDDNetInfo;

	CServerEntry *m_pFirstReqServer; // request list
	CServerEntry *m_pLastReqServer;
	int m_NumRequests;

	// used instead of g_Config.br_max_requests to get more servers
	int m_CurrentMaxRequests;

	int m_NeedRefresh;

	int m_NumSortedServers;
	int m_NumSortedServersCapacity;
	int m_NumServers;
	int m_NumServerCapacity;

	int m_Sorthash;
	char m_aFilterString[64];
	char m_aFilterGametypeString[128];

	int m_ServerlistType;
	int64_t m_BroadcastTime;
	unsigned char m_aTokenSeed[16];

	bool m_SortOnNextUpdate;

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

	void SetInfo(CServerEntry *pEntry, const CServerInfo &Info);
	void SetLatency(NETADDR Addr, int Latency);
};

#endif
