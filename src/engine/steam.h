#ifndef ENGINE_STEAM_H
#define ENGINE_STEAM_H

#include <base/types.h>

#include "kernel.h"

class ISteam : public IInterface
{
	MACRO_INTERFACE("steam")
public:
	// Returns NULL if the name cannot be determined.
	virtual const char *GetPlayerName() = 0;

	// Returns NULL if the no server needs to be joined.
	// Can change while the game is running.
	virtual const NETADDR *GetConnectAddress() = 0;
	virtual void ClearConnectAddress() = 0;

	virtual void Update() = 0;

	virtual void ClearGameInfo() = 0;
	virtual void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) = 0;
};

ISteam *CreateSteam();

#endif // ENGINE_STEAM_H
