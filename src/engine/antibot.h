#ifndef ENGINE_ANTIBOT_H
#define ENGINE_ANTIBOT_H

#include "kernel.h"

class IAntibot : public IInterface
{
	MACRO_INTERFACE("antibot", 0)
public:
	virtual void RoundStart(class IGameServer *pGameServer) = 0;
	virtual void RoundEnd() = 0;

	// Hooks
	virtual void OnPlayerInit(int ClientID) = 0;
	virtual void OnPlayerDestroy(int ClientID) = 0;
	virtual void OnSpawn(int ClientID) = 0;
	virtual void OnHammerFireReloading(int ClientID) = 0;
	virtual void OnHammerFire(int ClientID) = 0;
	virtual void OnHammerHit(int ClientID) = 0;
	virtual void OnDirectInput(int ClientID) = 0;
	virtual void OnCharacterTick(int ClientID) = 0;
	virtual void OnHookAttach(int ClientID, bool Player) = 0;

	// Commands
	virtual void Dump() = 0;

	virtual ~IAntibot(){};
};

class IEngineAntibot : public IAntibot
{
	MACRO_INTERFACE("engineantibot", 0)
public:
	virtual void Init() = 0;

	// Hooks
	virtual void OnEngineTick() = 0;
	virtual void OnEngineClientJoin(int ClientID, bool Sixup) = 0;
	virtual void OnEngineClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags) = 0;

	virtual ~IEngineAntibot(){};
};

#endif //ENGINE_ANTIBOT_H
