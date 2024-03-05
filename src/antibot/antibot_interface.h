#ifndef ANTIBOT_ANTIBOT_INTERFACE_H
#define ANTIBOT_ANTIBOT_INTERFACE_H

#include <base/dynamic.h>

#ifndef ANTIBOTAPI
#define ANTIBOTAPI DYNAMIC_IMPORT
#endif

#include "antibot_data.h"
extern "C" {

ANTIBOTAPI int AntibotAbiVersion();
ANTIBOTAPI void AntibotInit(CAntibotData *pCallbackData);
ANTIBOTAPI void AntibotRoundStart(CAntibotRoundData *pRoundData);
ANTIBOTAPI void AntibotRoundEnd(void);
ANTIBOTAPI void AntibotUpdateData(void);
ANTIBOTAPI void AntibotDestroy(void);
ANTIBOTAPI void AntibotConsoleCommand(const char *pCommand);
ANTIBOTAPI void AntibotOnPlayerInit(int ClientId);
ANTIBOTAPI void AntibotOnPlayerDestroy(int ClientId);
ANTIBOTAPI void AntibotOnSpawn(int ClientId);
ANTIBOTAPI void AntibotOnHammerFireReloading(int ClientId);
ANTIBOTAPI void AntibotOnHammerFire(int ClientId);
ANTIBOTAPI void AntibotOnHammerHit(int ClientId, int TargetId);
ANTIBOTAPI void AntibotOnDirectInput(int ClientId);
ANTIBOTAPI void AntibotOnCharacterTick(int ClientId);
ANTIBOTAPI void AntibotOnHookAttach(int ClientId, bool Player);
ANTIBOTAPI void AntibotOnEngineTick(void);
ANTIBOTAPI void AntibotOnEngineClientJoin(int ClientId, bool Sixup);
ANTIBOTAPI void AntibotOnEngineClientDrop(int ClientId, const char *pReason);
// Returns true if the message shouldn't be processed by the server.
ANTIBOTAPI bool AntibotOnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags);
ANTIBOTAPI bool AntibotOnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags);
// Returns true if the server should simulate receiving a client message.
ANTIBOTAPI bool AntibotOnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags);
}

#endif // ANTIBOT_ANTIBOT_INTERFACE_H
