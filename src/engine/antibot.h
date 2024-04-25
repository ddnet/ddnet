#ifndef ENGINE_ANTIBOT_H
#define ENGINE_ANTIBOT_H

#include "kernel.h"

class IAntibot : public IInterface
{
	MACRO_INTERFACE("antibot")
public:
	virtual void RoundStart(class IGameServer *pGameServer) = 0;
	virtual void RoundEnd() = 0;

	// Hooks
	virtual void OnPlayerInit(int ClientId) = 0;
	virtual void OnPlayerDestroy(int ClientId) = 0;
	virtual void OnSpawn(int ClientId) = 0;
	virtual void OnHammerFireReloading(int ClientId) = 0;
	virtual void OnHammerFire(int ClientId) = 0;
	virtual void OnHammerHit(int ClientId, int TargetId) = 0;
	virtual void OnDirectInput(int ClientId) = 0;
	virtual void OnCharacterTick(int ClientId) = 0;
	virtual void OnHookAttach(int ClientId, bool Player) = 0;

	// Commands
	virtual void ConsoleCommand(const char *pCommand) = 0;

	virtual ~IAntibot(){};
};

class IEngineAntibot : public IAntibot
{
	MACRO_INTERFACE("engineantibot")
public:
	virtual void Init() = 0;

	// Hooks
	virtual void OnEngineTick() = 0;
	virtual void OnEngineClientJoin(int ClientId, bool Sixup) = 0;
	virtual void OnEngineClientDrop(int ClientId, const char *pReason) = 0;
	virtual bool OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags) = 0;
	virtual bool OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags) = 0;
	virtual bool OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags) = 0;

	virtual ~IEngineAntibot(){};
};

#endif //ENGINE_ANTIBOT_H
