/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVERBROWSER_H
#define ENGINE_SERVERBROWSER_H

#include <base/hash.h>
#include <base/system.h>

#include <engine/map.h>
#include <engine/shared/protocol.h>

#include "kernel.h"

#include <unordered_set>
#include <vector>

static constexpr const char *DDNET_INFO_FILE = "ddnet-info.json";
static constexpr const char *DDNET_INFO_URL = "https://info.ddnet.org/info";

class CUIElement;

class CServerInfo
{
public:
	enum
	{
		LOC_UNKNOWN = 0,
		LOC_AFRICA,
		LOC_ASIA,
		LOC_AUSTRALIA,
		LOC_EUROPE,
		LOC_NORTH_AMERICA,
		LOC_SOUTH_AMERICA,
		// Special case China because it has an exceptionally bad
		// connection to the outside due to the Great Firewall of
		// China:
		// https://en.wikipedia.org/w/index.php?title=Great_Firewall&oldid=1019589632
		LOC_CHINA,
		NUM_LOCS,
	};

	enum EClientScoreKind
	{
		CLIENT_SCORE_KIND_UNSPECIFIED,
		CLIENT_SCORE_KIND_POINTS,
		CLIENT_SCORE_KIND_TIME,
		CLIENT_SCORE_KIND_TIME_BACKCOMPAT,
	};

	enum ERankState
	{
		RANK_UNAVAILABLE,
		RANK_RANKED,
		RANK_UNRANKED,
	};

	enum
	{
		MAX_COMMUNITY_ID_LENGTH = 32,
		MAX_COMMUNITY_COUNTRY_LENGTH = 32,
		MAX_COMMUNITY_TYPE_LENGTH = 32,
	};

	class CClient
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		bool m_Player;
		bool m_Afk;

		// skin info
		char m_aSkin[MAX_SKIN_LENGTH];
		bool m_CustomSkinColors;
		int m_CustomSkinColorBody;
		int m_CustomSkinColorFeet;

		int m_FriendState;
	};

	int m_ServerIndex;

	int m_Type;
	uint64_t m_ReceivedPackets;
	int m_NumReceivedClients;

	int m_NumAddresses;
	NETADDR m_aAddresses[MAX_SERVER_ADDRESSES];

	int m_QuickSearchHit;
	int m_FriendState;
	int m_FriendNum;

	int m_MaxClients;
	int m_NumClients;
	int m_MaxPlayers;
	int m_NumPlayers;
	int m_Flags;
	EClientScoreKind m_ClientScoreKind;
	TRISTATE m_Favorite;
	TRISTATE m_FavoriteAllowPing;
	char m_aCommunityId[MAX_COMMUNITY_ID_LENGTH];
	char m_aCommunityCountry[MAX_COMMUNITY_COUNTRY_LENGTH];
	char m_aCommunityType[MAX_COMMUNITY_TYPE_LENGTH];
	int m_Location;
	bool m_LatencyIsEstimated;
	int m_Latency; // in ms
	ERankState m_HasRank;
	char m_aGameType[16];
	char m_aName[64];
	char m_aMap[MAX_MAP_LENGTH];
	int m_MapCrc;
	int m_MapSize;
	char m_aVersion[32];
	char m_aAddress[MAX_SERVER_ADDRESSES * NETADDR_MAXSTRSIZE];
	CClient m_aClients[SERVERINFO_MAX_CLIENTS];
	int m_NumFilteredPlayers;
	bool m_RequiresLogin;

	static int EstimateLatency(int Loc1, int Loc2);
	static bool ParseLocation(int *pResult, const char *pString);
};

class CCommunityCountryServer
{
	NETADDR m_Address;
	char m_aTypeName[CServerInfo::MAX_COMMUNITY_TYPE_LENGTH];

public:
	CCommunityCountryServer(NETADDR Address, const char *pTypeName) :
		m_Address(Address)
	{
		str_copy(m_aTypeName, pTypeName);
	}

	NETADDR Address() const { return m_Address; }
	const char *TypeName() const { return m_aTypeName; }
};

class CCommunityCountry
{
	friend class CServerBrowser;

	char m_aName[CServerInfo::MAX_COMMUNITY_COUNTRY_LENGTH];
	int m_FlagId;
	std::vector<CCommunityCountryServer> m_vServers;

public:
	CCommunityCountry(const char *pName, int FlagId) :
		m_FlagId(FlagId)
	{
		str_copy(m_aName, pName);
	}

	const char *Name() const { return m_aName; }
	int FlagId() const { return m_FlagId; }
	const std::vector<CCommunityCountryServer> &Servers() const { return m_vServers; }
};

class CCommunityType
{
	char m_aName[CServerInfo::MAX_COMMUNITY_TYPE_LENGTH];

public:
	CCommunityType(const char *pName)
	{
		str_copy(m_aName, pName);
	}

	const char *Name() const { return m_aName; }
};

class CCommunityMap
{
	char m_aName[MAX_MAP_LENGTH];

public:
	CCommunityMap(const char *pName)
	{
		str_copy(m_aName, pName);
	}

	const char *Name() const { return m_aName; }

	bool operator==(const CCommunityMap &Other) const
	{
		return str_comp(Name(), Other.Name()) == 0;
	}

	bool operator!=(const CCommunityMap &Other) const
	{
		return !(*this == Other);
	}

	struct SHash
	{
		size_t operator()(const CCommunityMap &Map) const
		{
			return str_quickhash(Map.Name());
		}
	};
};

class CCommunity
{
	friend class CServerBrowser;

	char m_aId[CServerInfo::MAX_COMMUNITY_ID_LENGTH];
	char m_aName[64];
	SHA256_DIGEST m_IconSha256;
	char m_aIconUrl[128];
	std::vector<CCommunityCountry> m_vCountries;
	std::vector<CCommunityType> m_vTypes;
	bool m_HasFinishes = false;
	std::unordered_set<CCommunityMap, CCommunityMap::SHash> m_FinishedMaps;

public:
	CCommunity(const char *pId, const char *pName, SHA256_DIGEST IconSha256, const char *pIconUrl) :
		m_IconSha256(IconSha256)
	{
		str_copy(m_aId, pId);
		str_copy(m_aName, pName);
		str_copy(m_aIconUrl, pIconUrl);
	}

	const char *Id() const { return m_aId; }
	const char *Name() const { return m_aName; }
	const char *IconUrl() const { return m_aIconUrl; }
	const SHA256_DIGEST &IconSha256() const { return m_IconSha256; }
	const std::vector<CCommunityCountry> &Countries() const { return m_vCountries; }
	const std::vector<CCommunityType> &Types() const { return m_vTypes; }
	bool HasCountry(const char *pCountryName) const;
	bool HasType(const char *pTypeName) const;
	bool HasRanks() const { return m_HasFinishes; }
	CServerInfo::ERankState HasRank(const char *pMap) const;
};

class IFilterList
{
public:
	virtual void Add(const char *pElement) = 0;
	virtual void Remove(const char *pElement) = 0;
	virtual void Clear() = 0;
	virtual bool Empty() const = 0;
	virtual bool Filtered(const char *pElement) const = 0;
};

class ICommunityCache
{
public:
	virtual void Update(bool Force) = 0;
	virtual const std::vector<const CCommunity *> &SelectedCommunities() const = 0;
	virtual const std::vector<const CCommunityCountry *> &SelectableCountries() const = 0;
	virtual const std::vector<const CCommunityType *> &SelectableTypes() const = 0;
	virtual bool AnyRanksAvailable() const = 0;
	virtual bool CountriesTypesFilterAvailable() const = 0;
	virtual const char *CountryTypeFilterKey() const = 0;
};

class IServerBrowser : public IInterface
{
	MACRO_INTERFACE("serverbrowser")
public:
	/* Constants: Server Browser Sorting
		SORT_NAME - Sort by name.
		SORT_PING - Sort by ping.
		SORT_MAP - Sort by map
		SORT_GAMETYPE - Sort by game type. DM, TDM etc.
		SORT_NUMPLAYERS - Sort after how many players there are on the server.
		SORT_NUMFRIENDS - Sort after how many friends there are on the server.
	*/
	enum
	{
		SORT_NAME = 0,
		SORT_PING,
		SORT_MAP,
		SORT_GAMETYPE,
		SORT_NUMPLAYERS,
		SORT_NUMFRIENDS,

		QUICK_SERVERNAME = 1,
		QUICK_PLAYER = 2,
		QUICK_MAPNAME = 4,

		TYPE_INTERNET = 0,
		TYPE_LAN,
		TYPE_FAVORITES,
		TYPE_FAVORITE_COMMUNITY_1,
		TYPE_FAVORITE_COMMUNITY_2,
		TYPE_FAVORITE_COMMUNITY_3,
		TYPE_FAVORITE_COMMUNITY_4,
		TYPE_FAVORITE_COMMUNITY_5,
		NUM_TYPES,

		LAN_PORT_BEGIN = 8303,
		LAN_PORT_END = 8310,
	};

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

	static constexpr const char *COMMUNITY_DDNET = "ddnet";
	static constexpr const char *COMMUNITY_NONE = "none";

	static constexpr const char *COMMUNITY_COUNTRY_NONE = "none";
	static constexpr const char *COMMUNITY_TYPE_NONE = "None";
	/**
	 * Special community value for country/type filters that
	 * affect all communities.
	 */
	static constexpr const char *COMMUNITY_ALL = "all";

	static constexpr const char *SEARCH_EXCLUDE_TOKEN = ";";

	virtual void Refresh(int Type, bool Force = false) = 0;
	virtual bool IsGettingServerlist() const = 0;
	virtual bool IsRefreshing() const = 0;
	virtual int LoadingProgression() const = 0;

	virtual int NumServers() const = 0;

	virtual int Players(const CServerInfo &Item) const = 0;
	virtual int Max(const CServerInfo &Item) const = 0;

	virtual int NumSortedServers() const = 0;
	virtual int NumSortedPlayers() const = 0;
	virtual const CServerInfo *SortedGet(int Index) const = 0;

	virtual const std::vector<CCommunity> &Communities() const = 0;
	virtual const CCommunity *Community(const char *pCommunityId) const = 0;
	virtual std::vector<const CCommunity *> SelectedCommunities() const = 0;
	virtual std::vector<const CCommunity *> FavoriteCommunities() const = 0;
	virtual std::vector<const CCommunity *> CurrentCommunities() const = 0;
	virtual unsigned CurrentCommunitiesHash() const = 0;

	virtual bool DDNetInfoAvailable() const = 0;
	virtual SHA256_DIGEST DDNetInfoSha256() const = 0;

	virtual ICommunityCache &CommunityCache() = 0;
	virtual const ICommunityCache &CommunityCache() const = 0;
	virtual IFilterList &FavoriteCommunitiesFilter() = 0;
	virtual IFilterList &CommunitiesFilter() = 0;
	virtual IFilterList &CountriesFilter() = 0;
	virtual IFilterList &TypesFilter() = 0;
	virtual const IFilterList &FavoriteCommunitiesFilter() const = 0;
	virtual const IFilterList &CommunitiesFilter() const = 0;
	virtual const IFilterList &CountriesFilter() const = 0;
	virtual const IFilterList &TypesFilter() const = 0;
	virtual void CleanFilters() = 0;

	virtual CServerEntry *Find(const NETADDR &Addr) = 0;
	virtual int GetCurrentType() = 0;
	virtual const char *GetTutorialServer() = 0;
};

#endif
