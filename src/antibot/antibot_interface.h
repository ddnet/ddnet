#ifndef ANTIBOT_ANTIBOT_INTERFACE_H
#define ANTIBOT_ANTIBOT_INTERFACE_H

#include "antibot_data.h"
extern "C" {

int AntibotAbiVersion();
void AntibotInit(CAntibotData *pCallbackData);
void AntibotRoundStart(CAntibotRoundData *pRoundData);
void AntibotRoundEnd(void);
void AntibotUpdateData(void);
void AntibotDestroy(void);
void AntibotDump(void);
void AntibotOnPlayerInit(int ClientID);
void AntibotOnPlayerDestroy(int ClientID);
void AntibotOnSpawn(int ClientID);
void AntibotOnHammerFireReloading(int ClientID);
void AntibotOnHammerFire(int ClientID);
void AntibotOnHammerHit(int ClientID);
void AntibotOnDirectInput(int ClientID);
void AntibotOnCharacterTick(int ClientID);
void AntibotOnHookAttach(int ClientID, bool Player);
void AntibotOnEngineTick(void);
void AntibotOnEngineClientJoin(int ClientID, bool Sixup);
void AntibotOnEngineClientDrop(int ClientID, const char *pReason);
void AntibotOnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags);
}

#endif // ANTIBOT_ANTIBOT_INTERFACE_H
