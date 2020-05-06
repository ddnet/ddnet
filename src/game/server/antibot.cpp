#include "antibot.h"
#include <antibot/antibot_data.h>
#include <antibot/antibot_interface.h>

#include <game/server/gamecontext.h>

#ifdef CONF_ANTIBOT
static CAntibotData g_Data;

static void Log(const char *pMessage, void *pUser)
{
	CGameContext *pGameContext = (CGameContext *)pUser;
	pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", pMessage);
}
static void Report(int ClientID, const char *pMessage, void *pUser)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%d: %s", ClientID, pMessage);
	Log(aBuf, pUser);
}
static void Send(int ClientID, const void *pData, int Size, int Flags, void *pUser)
{
	CGameContext *pGameContext = (CGameContext *)pUser;
	int RealFlags = MSGFLAG_VITAL;
	if(Flags&ANTIBOT_MSGFLAG_NONVITAL)
	{
		RealFlags &= ~MSGFLAG_VITAL;
	}
	if(Flags&ANTIBOT_MSGFLAG_FLUSH)
	{
		RealFlags |= MSGFLAG_FLUSH;
	}
	pGameContext->Server()->SendMsgRaw(ClientID, pData, Size, RealFlags);
}

CAntibot::CAntibot()
	: m_pGameContext(0)
{
}
CAntibot::~CAntibot()
{
	// Check if `Init` was called. There is no easy way to prevent two
	// destructors running without an `Init` call in between.
	if(m_pGameContext)
	{
		AntibotDestroy();
		free(g_Data.m_Map.m_pTiles);
		g_Data.m_Map.m_pTiles = 0;
		m_pGameContext = 0;
	}
}
void CAntibot::Init(CGameContext *pGameContext)
{
	m_pGameContext = pGameContext;
	mem_zero(&g_Data, sizeof(g_Data));
	g_Data.m_Map.m_pTiles = 0;
	g_Data.m_pfnLog = Log;
	g_Data.m_pfnReport = Report;
	g_Data.m_pfnSend = Send;
	g_Data.m_pUser = m_pGameContext;
	AntibotInit(&g_Data);
	Update();
}
void CAntibot::Dump() { AntibotDump(); }
void CAntibot::Update()
{
	m_pGameContext->FillAntibot(&g_Data);
	AntibotUpdateData();
}

void CAntibot::OnPlayerInit(int ClientID) { Update(); AntibotOnPlayerInit(ClientID); }
void CAntibot::OnPlayerDestroy(int ClientID) { Update(); AntibotOnPlayerDestroy(ClientID); }
void CAntibot::OnSpawn(int ClientID) { Update(); AntibotOnSpawn(ClientID); }
void CAntibot::OnHammerFireReloading(int ClientID) { Update(); AntibotOnHammerFireReloading(ClientID); }
void CAntibot::OnHammerFire(int ClientID) { Update(); AntibotOnHammerFire(ClientID); }
void CAntibot::OnHammerHit(int ClientID) { Update(); AntibotOnHammerHit(ClientID); }
void CAntibot::OnDirectInput(int ClientID) { Update(); AntibotOnDirectInput(ClientID); }
void CAntibot::OnTick(int ClientID) { Update(); AntibotOnTick(ClientID); }
void CAntibot::OnHookAttach(int ClientID, bool Player) { Update(); AntibotOnHookAttach(ClientID, Player); }

void CAntibot::OnEngineClientJoin(int ClientID) { AntibotOnEngineClientJoin(ClientID); }
void CAntibot::OnEngineClientDrop(int ClientID, const char *pReason) { AntibotOnEngineClientDrop(ClientID, pReason); }
void CAntibot::OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags)
{
	int AntibotFlags = 0;
	if((Flags&MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	AntibotOnEngineClientMessage(ClientID, pData, Size, Flags);
}
#else
CAntibot::CAntibot() :
	m_pGameContext(0)
{
}
CAntibot::~CAntibot()
{
}
void CAntibot::Init(CGameContext *pGameContext)
{
	m_pGameContext = pGameContext;
}
void CAntibot::Dump()
{
	m_pGameContext->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "antibot support not compiled in");
}
void CAntibot::Update()
{
}

void CAntibot::OnPlayerInit(int ClientID) { }
void CAntibot::OnPlayerDestroy(int ClientID) { }
void CAntibot::OnSpawn(int ClientID) { }
void CAntibot::OnHammerFireReloading(int ClientID) { }
void CAntibot::OnHammerFire(int ClientID) { }
void CAntibot::OnHammerHit(int ClientID) { }
void CAntibot::OnDirectInput(int ClientID) { }
void CAntibot::OnTick(int ClientID) { }
void CAntibot::OnHookAttach(int ClientID, bool Player) { }

void CAntibot::OnEngineClientJoin(int ClientID) { }
void CAntibot::OnEngineClientDrop(int ClientID, const char *pReason) { }
void CAntibot::OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags) { }
#endif
