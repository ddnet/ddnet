#define ANTIBOTAPI DYNAMIC_EXPORT

#include "antibot_interface.h"

static CAntibotData *g_pData;

extern "C" {

int AntibotAbiVersion()
{
	return ANTIBOT_ABI_VERSION;
}
void AntibotInit(CAntibotData *pData)
{
	g_pData = pData;
	g_pData->m_pfnLog("null antibot initialized", g_pData->m_pUser);
}
void AntibotRoundStart(CAntibotRoundData *pRoundData){};
void AntibotRoundEnd(void){};
void AntibotUpdateData(void) {}
void AntibotDestroy(void) { g_pData = 0; }
void AntibotDump(void)
{
	g_pData->m_pfnLog("null antibot", g_pData->m_pUser);
}
void AntibotOnPlayerInit(int /*ClientID*/) {}
void AntibotOnPlayerDestroy(int /*ClientID*/) {}
void AntibotOnSpawn(int /*ClientID*/) {}
void AntibotOnHammerFireReloading(int /*ClientID*/) {}
void AntibotOnHammerFire(int /*ClientID*/) {}
void AntibotOnHammerHit(int /*ClientID*/, int /*TargetID*/) {}
void AntibotOnDirectInput(int /*ClientID*/) {}
void AntibotOnCharacterTick(int /*ClientID*/) {}
void AntibotOnHookAttach(int /*ClientID*/, bool /*Player*/) {}
void AntibotOnEngineTick(void) {}
void AntibotOnEngineClientJoin(int /*ClientID*/, bool /*Sixup*/) {}
void AntibotOnEngineClientDrop(int /*ClientID*/, const char * /*pReason*/) {}
bool AntibotOnEngineClientMessage(int /*ClientID*/, const void * /*pData*/, int /*Size*/, int /*Flags*/) { return false; }
bool AntibotOnEngineServerMessage(int /*ClientID*/, const void * /*pData*/, int /*Size*/, int /*Flags*/) { return false; }
bool AntibotOnEngineSimulateClientMessage(int * /*pClientID*/, void * /*pBuffer*/, int /*BufferSize*/, int * /*pOutSize*/, int * /*pFlags*/) { return false; }
}
