#ifndef ENGINE_DISCORD_H
#define ENGINE_DISCORD_H

#include "kernel.h"
#include <base/types.h>
#include <engine/serverbrowser.h>

class IDiscord : public IInterface
{
	MACRO_INTERFACE("discord")
public:
	virtual void Update() = 0;

	virtual void ClearGameInfo() = 0;
	virtual void SetGameInfo(const CServerInfo &ServerInfo, const char *pMapName, bool Registered) = 0;
	virtual void UpdateServerInfo(const CServerInfo &ServerInfo, const char *pMapName) = 0;
	virtual void UpdatePlayerCount(int Count) = 0;
};

IDiscord *CreateDiscord();

#endif // ENGINE_DISCORD_H
