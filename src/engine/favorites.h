#ifndef ENGINE_FAVORITES_H
#define ENGINE_FAVORITES_H

#include <memory>

#include <base/types.h>
#include <engine/shared/protocol.h>

#include "kernel.h"

class IConfigManager;

class IFavorites : public IInterface
{
	MACRO_INTERFACE("favorites")

protected:
	virtual void OnConfigSave(IConfigManager *pConfigManager) = 0;

public:
	class CEntry
	{
	public:
		int m_NumAddrs;
		NETADDR m_aAddrs[MAX_SERVER_ADDRESSES];
		bool m_AllowPing;
	};

	virtual ~IFavorites() {}

	virtual TRISTATE IsFavorite(const NETADDR *pAddrs, int NumAddrs) const = 0;
	// Only considers the addresses that are actually favorites.
	virtual TRISTATE IsPingAllowed(const NETADDR *pAddrs, int NumAddrs) const = 0;
	virtual void Add(const NETADDR *pAddrs, int NumAddrs) = 0;
	// Only considers the addresses that are actually favorites.
	virtual void AllowPing(const NETADDR *pAddrs, int NumAddrs, bool AllowPing) = 0;
	virtual void Remove(const NETADDR *pAddrs, int NumAddrs) = 0;
	virtual void AllEntries(const CEntry **ppEntries, int *pNumEntries) = 0;

	// Pass the `IFavorites` instance as callback.
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
};

std::unique_ptr<IFavorites> CreateFavorites();
#endif // ENGINE_FAVORITES_H
