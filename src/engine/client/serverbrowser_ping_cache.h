#ifndef ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
#define ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
#include <base/system.h>

class IConsole;
class IStorage;

class IServerBrowserPingCache
{
public:
	class CEntry
	{
	public:
		NETADDR m_Addr;
		int m_Ping;
	};

	virtual ~IServerBrowserPingCache() {}

	virtual void Load() = 0;

	virtual void CachePing(NETADDR Addr, int Ping) = 0;
	// The returned list is sorted by address, the addresses don't have a
	// port.
	virtual void GetPingCache(const CEntry **ppEntries, int *pNumEntries) = 0;
};

IServerBrowserPingCache *CreateServerBrowserPingCache(IConsole *pConsole, IStorage *pStorage);
#endif // ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
