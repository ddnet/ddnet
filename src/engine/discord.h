#ifndef ENGINE_DISCORD_H
#define ENGINE_DISCORD_H

#include "kernel.h"
#include <base/types.h>

class IDiscord : public IInterface
{
	MACRO_INTERFACE("discord")
public:
	virtual void Update() = 0;

	virtual void ClearGameInfo() = 0;
	virtual void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) = 0;
};

IDiscord *CreateDiscord();

#endif // ENGINE_DISCORD_H
