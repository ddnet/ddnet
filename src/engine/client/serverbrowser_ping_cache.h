#ifndef ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
#define ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
#include <base/types.h>

class IConsole;
class IStorage;

class IServerBrowserPingCache
{
public:
	virtual ~IServerBrowserPingCache() {}

	virtual void Load() = 0;

	virtual int NumEntries() const = 0;
	virtual void CachePing(const NETADDR &Addr, int Ping) = 0;
	// Returns -1 if the ping isn't cached.
	virtual int GetPing(const NETADDR *pAddrs, int NumAddrs) const = 0;
};

IServerBrowserPingCache *CreateServerBrowserPingCache(IConsole *pConsole, IStorage *pStorage);
#endif // ENGINE_CLIENT_SERVERBROWSER_PING_CACHE_H
