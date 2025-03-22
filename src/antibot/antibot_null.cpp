#define ANTIBOTAPI DYNAMIC_EXPORT

#include <antibot/antibot_interface.h>
#include <base/log.h>

#include <cstring>

static CAntibotData *g_pData;

extern "C" {

int AntibotAbiVersion()
{
	return ANTIBOT_ABI_VERSION;
}
void AntibotInit(CAntibotData *pCallbackData)
{
	g_pData = pCallbackData;
	g_pData->m_pfnLog(LEVEL_INFO, "null antibot initialized");
}
void AntibotRoundStart(CAntibotRoundData *pRoundData){};
void AntibotRoundEnd(void){};
void AntibotUpdateData(void) {}
void AntibotDestroy(void) { g_pData = 0; }
void AntibotConsoleCommand(const char *pCommand)
{
	if(strcmp(pCommand, "dump") == 0)
	{
		g_pData->m_pfnLog(LEVEL_INFO, "null antibot");
	}
	else
	{
		g_pData->m_pfnLog(LEVEL_INFO, "unknown command");
	}
}
void AntibotOnPlayerInit(int /*ClientId*/) {}
void AntibotOnPlayerDestroy(int /*ClientId*/) {}
void AntibotOnSpawn(int /*ClientId*/) {}
void AntibotOnHammerFireReloading(int /*ClientId*/) {}
void AntibotOnHammerFire(int /*ClientId*/) {}
void AntibotOnHammerHit(int /*ClientId*/, int /*TargetId*/) {}
void AntibotOnDirectInput(int /*ClientId*/) {}
void AntibotOnCharacterTick(int /*ClientId*/) {}
void AntibotOnHookAttach(int /*ClientId*/, bool /*Player*/) {}
void AntibotOnEngineTick(void) {}
void AntibotOnEngineClientJoin(int /*ClientId*/, bool /*Sixup*/) {}
void AntibotOnEngineClientDrop(int /*ClientId*/, const char * /*pReason*/) {}
bool AntibotOnEngineClientMessage(int /*ClientId*/, const void * /*pData*/, int /*Size*/, int /*Flags*/) { return false; }
bool AntibotOnEngineServerMessage(int /*ClientId*/, const void * /*pData*/, int /*Size*/, int /*Flags*/) { return false; }
bool AntibotOnEngineSimulateClientMessage(int * /*pClientId*/, void * /*pBuffer*/, int /*BufferSize*/, int * /*pOutSize*/, int * /*pFlags*/) { return false; }
}
