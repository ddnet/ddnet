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
ANTIBOTAPI void AntibotDump(void);
ANTIBOTAPI void AntibotOnPlayerInit(int ClientID);
ANTIBOTAPI void AntibotOnPlayerDestroy(int ClientID);
ANTIBOTAPI void AntibotOnSpawn(int ClientID);
ANTIBOTAPI void AntibotOnHammerFireReloading(int ClientID);
ANTIBOTAPI void AntibotOnHammerFire(int ClientID);
ANTIBOTAPI void AntibotOnHammerHit(int ClientID, int TargetID);
ANTIBOTAPI void AntibotOnDirectInput(int ClientID);
ANTIBOTAPI void AntibotOnCharacterTick(int ClientID);
ANTIBOTAPI void AntibotOnHookAttach(int ClientID, bool Player);
ANTIBOTAPI void AntibotOnEngineTick(void);
ANTIBOTAPI void AntibotOnEngineClientJoin(int ClientID, bool Sixup);
ANTIBOTAPI void AntibotOnEngineClientDrop(int ClientID, const char *pReason);
// Returns true if the message shouldn't be processed by the server.
ANTIBOTAPI bool AntibotOnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags);
ANTIBOTAPI bool AntibotOnEngineServerMessage(int ClientID, const void *pData, int Size, int Flags);
// Returns true if the server should simulate receiving a client message.
ANTIBOTAPI bool AntibotOnEngineSimulateClientMessage(int *pClientID, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags);
}

#endif // ANTIBOT_ANTIBOT_INTERFACE_H
