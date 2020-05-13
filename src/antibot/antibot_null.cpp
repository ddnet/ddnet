#include "antibot_data.h"

static CAntibotCallbackData *g_pCallbacks;

extern "C"
{

void AntibotInit(CAntibotCallbackData *pData)
{
	g_pCallbacks = pData;
	if(g_pCallbacks->m_pfnLog)
	{
		g_pCallbacks->m_pfnLog("null antibot initialized", g_pCallbacks->m_pUser);
	}
}
void AntibotRoundStart(CAntibotRoundData *pRoundData) { };
void AntibotRoundEnd(void) { };
void AntibotUpdateData(void) { }
void AntibotDestroy(void) { g_pCallbacks = 0; }
void AntibotDump(void)
{
	if(g_pCallbacks->m_pfnLog)
	{
		g_pCallbacks->m_pfnLog("null antibot", g_pCallbacks->m_pUser);
	}
}
void AntibotOnPlayerInit(int ClientID) { (void)ClientID; }
void AntibotOnPlayerDestroy(int ClientID) { (void)ClientID; }
void AntibotOnSpawn(int ClientID) { (void)ClientID; }
void AntibotOnHammerFireReloading(int ClientID) { (void)ClientID; }
void AntibotOnHammerFire(int ClientID) { (void)ClientID; }
void AntibotOnHammerHit(int ClientID) { (void)ClientID; }
void AntibotOnDirectInput(int ClientID) { (void)ClientID; }
void AntibotOnTick(int ClientID) { (void)ClientID; }
void AntibotOnHookAttach(int ClientID, bool Player) { (void)ClientID; (void)Player; }
void AntibotOnEngineClientJoin(int ClientID) { (void)ClientID; }
void AntibotOnEngineClientDrop(int ClientID, const char *pReason) { (void)ClientID; (void)pReason; }
void AntibotOnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags) { (void)ClientID; (void)pData; (void)Size; (void)Flags; }

}
