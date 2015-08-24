/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVERBROWSER_H
#define ENGINE_SERVERBROWSER_H

#include <engine/shared/protocol.h>

#include "kernel.h"

/*
	Structure: CServerInfo
*/
class CServerInfo
{
public:
	/*
		Structure: CInfoClient
	*/
	class CClient
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		bool m_Player;

		int m_FriendState;
	};

	int m_SortedIndex;
	int m_ServerIndex;

	NETADDR m_NetAddr;

	int m_QuickSearchHit;
	int m_FriendState;

	int m_MaxClients;
	int m_NumClients;
	int m_MaxPlayers;
	int m_NumPlayers;
	int m_Flags;
	int m_Favorite;
	int m_Latency; // in ms
	char m_aGameType[16];
	char m_aName[64];
	char m_aMap[32];
	char m_aVersion[32];
	char m_aAddress[NETADDR_MAXSTRSIZE];
	CClient m_aClients[MAX_CLIENTS];
};

bool IsRace(const CServerInfo *pInfo);
bool IsDDRace(const CServerInfo *pInfo);
bool IsDDNet(const CServerInfo *pInfo);
bool Is64Player(const CServerInfo *pInfo);

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

		SET_MASTER_ADD=1,
		SET_FAV_ADD,
		SET_DDNET_ADD,
		SET_TOKEN
	};

	virtual void Refresh(int Type) = 0;
	virtual bool IsRefreshing() const = 0;
	virtual bool IsRefreshingMasters() const = 0;
	virtual int LoadingProgression() const = 0;

	virtual int NumServers() const = 0;

	virtual int NumSortedServers() const = 0;
	virtual const CServerInfo *SortedGet(int Index) const = 0;

	virtual bool IsFavorite(const NETADDR &Addr) const = 0;
	virtual void AddFavorite(const NETADDR &Addr) = 0;
	virtual void RemoveFavorite(const NETADDR &Addr) = 0;

	virtual int NumDDNetCountries() = 0;
	virtual int GetDDNetCountryFlag(int Index) = 0;
	virtual const char *GetDDNetCountryName(int Index) = 0;

	virtual int NumDDNetTypes() = 0;
	virtual const char *GetDDNetType(int Index) = 0;

	virtual void DDNetFilterAdd(char *pFilter, const char *pName) = 0;
	virtual void DDNetFilterRem(char *pFilter, const char *pName) = 0;
	virtual bool DDNetFiltered(char *pFilter, const char *pName) = 0;
	virtual int GetCurrentType() = 0;
};

#endif
