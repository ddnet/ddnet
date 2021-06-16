#include "antibot_data.h"

#ifdef _MSC_VER
#define MSVS_DLL __declspec(dllexport)
#else
#define MSVS_DLL
#endif

static CAntibotData *g_pData;

extern "C" {

MSVS_DLL int AntibotAbiVersion()
{
	return ANTIBOT_ABI_VERSION;
}
MSVS_DLL void AntibotInit(CAntibotData *pData)
{
	g_pData = pData;
	g_pData->m_pfnLog("null antibot initialized", g_pData->m_pUser);
}
MSVS_DLL void AntibotRoundStart(CAntibotRoundData *pRoundData){};
MSVS_DLL void AntibotRoundEnd(void){};
MSVS_DLL void AntibotUpdateData(void) {}
MSVS_DLL void AntibotDestroy(void) { g_pData = 0; }
MSVS_DLL void AntibotDump(void)
{
	g_pData->m_pfnLog("null antibot", g_pData->m_pUser);
}
MSVS_DLL void AntibotOnPlayerInit(int /*ClientID*/) {}
MSVS_DLL void AntibotOnPlayerDestroy(int /*ClientID*/) {}
MSVS_DLL void AntibotOnSpawn(int /*ClientID*/) {}
MSVS_DLL void AntibotOnHammerFireReloading(int /*ClientID*/) {}
MSVS_DLL void AntibotOnHammerFire(int /*ClientID*/) {}
MSVS_DLL void AntibotOnHammerHit(int /*ClientID*/) {}
MSVS_DLL void AntibotOnDirectInput(int /*ClientID*/) {}
MSVS_DLL void AntibotOnCharacterTick(int /*ClientID*/) {}
MSVS_DLL void AntibotOnHookAttach(int /*ClientID*/, bool /*Player*/) {}
MSVS_DLL void AntibotOnEngineTick(void) {}
MSVS_DLL void AntibotOnEngineClientJoin(int /*ClientID*/, bool /*Sixup*/) {}
MSVS_DLL void AntibotOnEngineClientDrop(int /*ClientID*/, const char * /*pReason*/) {}
MSVS_DLL void AntibotOnEngineClientMessage(int /*ClientID*/, const void * /*pData*/, int /*Size*/, int /*Flags*/) {}
}
