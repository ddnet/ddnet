#include "antibot_data.h"

#include <base/system.h>

static CAntibotData *g_pAntibot;

extern "C"
{

void AntibotInit(CAntibotData *pData)
{
	g_pAntibot = pData;
	if(g_pAntibot->m_pfnLog)
	{
		g_pAntibot->m_pfnLog("null antibot initialized", g_pAntibot->m_pUser);
	}
}
void AntibotUpdateData() { }
void AntibotDestroy() { g_pAntibot = 0; }
void AntibotDump()
{
	if(g_pAntibot->m_pfnLog)
	{
		g_pAntibot->m_pfnLog("null antibot", g_pAntibot->m_pUser);
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
