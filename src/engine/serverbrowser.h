/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVERBROWSER_H
#define ENGINE_SERVERBROWSER_H

#include <base/types.h>
#include <engine/map.h>
#include <engine/shared/protocol.h>

#include "kernel.h"

#include <vector>

#define DDNET_INFO "ddnet-info.json"

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

	enum
	{
		CLIENT_SCORE_KIND_UNSPECIFIED,
		CLIENT_SCORE_KIND_POINTS,
		CLIENT_SCORE_KIND_TIME,
		CLIENT_SCORE_KIND_TIME_BACKCOMPAT,
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
		char m_aSkin[24 + 1];
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

	int m_MaxClients;
	int m_NumClients;
	int m_MaxPlayers;
	int m_NumPlayers;
	int m_Flags;
	int m_ClientScoreKind;
	TRISTATE m_Favorite;
	TRISTATE m_FavoriteAllowPing;
	bool m_Official;
	int m_Location;
	bool m_LatencyIsEstimated;
	int m_Latency; // in ms
	int m_HasRank;
	char m_aGameType[16];
	char m_aName[64];
	char m_aMap[MAX_MAP_LENGTH];
	int m_MapCrc;
	int m_MapSize;
	char m_aVersion[32];
	char m_aAddress[MAX_SERVER_ADDRESSES * NETADDR_MAXSTRSIZE];
	CClient m_aClients[SERVERINFO_MAX_CLIENTS];
	int m_NumFilteredPlayers;

	static int EstimateLatency(int Loc1, int Loc2);
	static bool ParseLocation(int *pResult, const char *pString);
	void InfoToString(char *pBuffer, int BufferSize) const;
};

class CCommunityCountryServer
{
	NETADDR m_Address;
	char m_aTypeName[32];

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

	char m_aName[256];
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
	char m_aName[32];

public:
	CCommunityType(const char *pName)
	{
		str_copy(m_aName, pName);
	}

	const char *Name() const { return m_aName; }
};

class CCommunity
{
	friend class CServerBrowser;

	char m_aId[32];
	char m_aName[64];
	char m_aJsonServersKey[32];
	std::vector<CCommunityCountry> m_vCountries;
	std::vector<CCommunityType> m_vTypes;

public:
	CCommunity(const char *pId, const char *pName, const char *pJsonServersKey)
	{
		str_copy(m_aId, pId);
		str_copy(m_aName, pName);
		str_copy(m_aJsonServersKey, pJsonServersKey);
	}

	const char *Id() const { return m_aId; }
	const char *Name() const { return m_aName; }
	const char *JsonServersKey() const { return m_aJsonServersKey; }
	const std::vector<CCommunityCountry> &Countries() const { return m_vCountries; }
	const std::vector<CCommunityType> &Types() const { return m_vTypes; }
};

class IServerBrowser : public IInterface
{
	MACRO_INTERFACE("serverbrowser", 0)
public:
	/* Constants: Server Browser Sorting
		SORT_NAME - Sort by name.
		SORT_PING - Sort by ping.
		SORT_MAP - Sort by map
		SORT_GAMETYPE - Sort by game type. DM, TDM etc.
		SORT_NUMPLAYERS - Sort after how many players there are on the server.
	*/
	enum
	{
		SORT_NAME = 0,
		SORT_PING,
		SORT_MAP,
		SORT_GAMETYPE,
		SORT_NUMPLAYERS,

		QUICK_SERVERNAME = 1,
		QUICK_PLAYER = 2,
		QUICK_MAPNAME = 4,

		TYPE_INTERNET = 0,
		TYPE_LAN,
		TYPE_FAVORITES,
		TYPE_DDNET,
		TYPE_KOG,
		NUM_TYPES,

		// TODO: remove integer community index and used string IDs instead
		NETWORK_DDNET = 0,
		NETWORK_KOG = 1,
		NUM_NETWORKS,
	};

	static constexpr const char *COMMUNITY_DDNET = "ddnet";

	static constexpr const char *SEARCH_EXCLUDE_TOKEN = ";";

	virtual void Refresh(int Type) = 0;
	virtual bool IsGettingServerlist() const = 0;
	virtual bool IsRefreshing() const = 0;
	virtual int LoadingProgression() const = 0;

	virtual int NumServers() const = 0;

	virtual int Players(const CServerInfo &Item) const = 0;
	virtual int Max(const CServerInfo &Item) const = 0;

	virtual int NumSortedServers() const = 0;
	virtual const CServerInfo *SortedGet(int Index) const = 0;

	virtual const std::vector<CCommunity> &Communities() const = 0;
	virtual const CCommunity *Community(const char *pCommunityId) const = 0;

	virtual void DDNetFilterAdd(char *pFilter, int FilterSize, const char *pName) const = 0;
	virtual void DDNetFilterRem(char *pFilter, int FilterSize, const char *pName) const = 0;
	virtual bool DDNetFiltered(const char *pFilter, const char *pName) const = 0;
	virtual void CountryFilterClean(int CommunityIndex) = 0;
	virtual void TypeFilterClean(int CommunityIndex) = 0;
	virtual int GetCurrentType() = 0;
	virtual const char *GetTutorialServer() = 0;
};

#endif
