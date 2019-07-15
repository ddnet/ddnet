/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVERBROWSER_H
#define ENGINE_SERVERBROWSER_H

#include <engine/shared/protocol.h>
#include <engine/shared/serverinfo.h>

#include "kernel.h"

#define DDNET_INFO "ddnet-info.json"
#define DDNET_INFO_TMP DDNET_INFO ".tmp"

/*
	Structure: CBrowserEntry
*/
class CBrowserEntry : public CServerInfo
{
public:
	int m_SortedIndex = {};
	int m_ServerIndex = {};

	int m_Type = {};
	uint64 m_ReceivedPackets = {};
	int m_NumReceivedClients = {};

	int m_QuickSearchHit = {};
	int m_FriendState = {};

	bool m_Favorite = {};
	bool m_Official = {};
	int m_Latency = {}; // in ms
	int m_HasRank = {};
	char m_aAddress[NETADDR_MAXSTRSIZE] = {};
	mutable int m_NumFilteredPlayers = {};

	CBrowserEntry() = default;
	CBrowserEntry(const NETADDR *pAddr, const CServerInfo &i) : CServerInfo(i)
	{
		net_addr_str(pAddr, m_aAddress, sizeof(m_aAddress), true);
	};
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
	enum{
		SORT_NAME = 0,
		SORT_PING,
		SORT_MAP,
		SORT_GAMETYPE,
		SORT_NUMPLAYERS,

		QUICK_SERVERNAME=1,
		QUICK_PLAYER=2,
		QUICK_MAPNAME=4,

		TYPE_NONE = 0,
		TYPE_INTERNET = 1,
		TYPE_LAN = 2,
		TYPE_FAVORITES = 3,
		TYPE_DDNET = 4,
		TYPE_KOG = 5,

		SET_MASTER_ADD=1,
		SET_FAV_ADD,
		SET_DDNET_ADD,
		SET_KOG_ADD,
		SET_TOKEN,

		NETWORK_DDNET=0,
		NETWORK_KOG=1,
		NUM_NETWORKS,
	};

	virtual void Refresh(int Type) = 0;
	virtual bool IsRefreshing() const = 0;
	virtual bool IsRefreshingMasters() const = 0;
	virtual int LoadingProgression() const = 0;

	virtual int NumServers() const = 0;

	virtual int Players(const CBrowserEntry &Item) const = 0;
	virtual int Max(const CBrowserEntry &Item) const = 0;

	virtual int NumSortedServers() const = 0;
	virtual const CBrowserEntry *SortedGet(int Index) const = 0;

	virtual bool IsFavorite(const NETADDR &Addr) const = 0;
	virtual void AddFavorite(const NETADDR &Addr) = 0;
	virtual void RemoveFavorite(const NETADDR &Addr) = 0;

	virtual int NumCountries(int Network) = 0;
	virtual int GetCountryFlag(int Network, int Index) = 0;
	virtual const char *GetCountryName(int Network, int Index) = 0;

	virtual int NumTypes(int Network) = 0;
	virtual const char *GetType(int Network, int Index) = 0;

	virtual void DDNetFilterAdd(char *pFilter, const char *pName) = 0;
	virtual void DDNetFilterRem(char *pFilter, const char *pName) = 0;
	virtual bool DDNetFiltered(char *pFilter, const char *pName) = 0;
	virtual void CountryFilterClean(int Network) = 0;
	virtual void TypeFilterClean(int Network) = 0;
	virtual int GetCurrentType() = 0;
};

#endif
