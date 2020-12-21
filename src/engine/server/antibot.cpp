#include "antibot.h"
#include <antibot/antibot_interface.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/server.h>

#ifdef CONF_ANTIBOT
CAntibot::CAntibot() :
	m_pServer(0), m_pConsole(0), m_pGameServer(0), m_Initialized(false)
{
}
CAntibot::~CAntibot()
{
	if(m_pGameServer && m_RoundData.m_Map.m_pTiles)
		free(m_RoundData.m_Map.m_pTiles);

	if(m_Initialized)
		AntibotDestroy();
}
void CAntibot::Send(int ClientID, const void *pData, int Size, int Flags, void *pUser)
{
	CAntibot *pAntibot = (CAntibot *)pUser;

	int RealFlags = MSGFLAG_VITAL;
	if(Flags & ANTIBOT_MSGFLAG_NONVITAL)
	{
		RealFlags &= ~MSGFLAG_VITAL;
	}
	if(Flags & ANTIBOT_MSGFLAG_FLUSH)
	{
		RealFlags |= MSGFLAG_FLUSH;
	}
	pAntibot->Server()->SendMsgRaw(ClientID, pData, Size, RealFlags);
}
void CAntibot::Log(const char *pMessage, void *pUser)
{
	CAntibot *pAntibot = (CAntibot *)pUser;
	pAntibot->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", pMessage);
}
void CAntibot::Report(int ClientID, const char *pMessage, void *pUser)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%d: %s", ClientID, pMessage);
	Log(aBuf, pUser);
}
void CAntibot::Init()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	dbg_assert(m_pServer && m_pConsole, "antibot requires server and console");
	dbg_assert(AntibotAbiVersion() == ANTIBOT_ABI_VERSION, "antibot abi version mismatch");

	mem_zero(&m_Data, sizeof(m_Data));
	CAntibotVersion Version = ANTIBOT_VERSION;
	m_Data.m_Version = Version;

	m_Data.m_Now = time_get();
	m_Data.m_Freq = time_freq();
	m_Data.m_pfnLog = Log;
	m_Data.m_pfnReport = Report;
	m_Data.m_pfnSend = Send;
	m_Data.m_pUser = this;
	AntibotInit(&m_Data);

	m_Initialized = true;
}
void CAntibot::RoundStart(IGameServer *pGameServer)
{
	m_pGameServer = pGameServer;
	mem_zero(&m_RoundData, sizeof(m_RoundData));
	m_RoundData.m_Map.m_pTiles = 0;
	AntibotRoundStart(&m_RoundData);
	Update();
}
void CAntibot::RoundEnd()
{
	// Let the external module clean up first
	AntibotRoundEnd();

	m_pGameServer = 0;
	if(m_RoundData.m_Map.m_pTiles)
		free(m_RoundData.m_Map.m_pTiles);
}
void CAntibot::Dump() { AntibotDump(); }
void CAntibot::Update()
{
	m_Data.m_Now = time_get();
	m_Data.m_Freq = time_freq();

	if(GameServer())
	{
		GameServer()->FillAntibot(&m_RoundData);
		AntibotUpdateData();
	}
}

void CAntibot::OnPlayerInit(int ClientID)
{
	Update();
	AntibotOnPlayerInit(ClientID);
}
void CAntibot::OnPlayerDestroy(int ClientID)
{
	Update();
	AntibotOnPlayerDestroy(ClientID);
}
void CAntibot::OnSpawn(int ClientID)
{
	Update();
	AntibotOnSpawn(ClientID);
}
void CAntibot::OnHammerFireReloading(int ClientID)
{
	Update();
	AntibotOnHammerFireReloading(ClientID);
}
void CAntibot::OnHammerFire(int ClientID)
{
	Update();
	AntibotOnHammerFire(ClientID);
}
void CAntibot::OnHammerHit(int ClientID)
{
	Update();
	AntibotOnHammerHit(ClientID);
}
void CAntibot::OnDirectInput(int ClientID)
{
	Update();
	AntibotOnDirectInput(ClientID);
}
void CAntibot::OnCharacterTick(int ClientID)
{
	Update();
	AntibotOnCharacterTick(ClientID);
}
void CAntibot::OnHookAttach(int ClientID, bool Player)
{
	Update();
	AntibotOnHookAttach(ClientID, Player);
}

void CAntibot::OnEngineTick()
{
	Update();
	AntibotOnEngineTick();
}
void CAntibot::OnEngineClientJoin(int ClientID, bool Sixup)
{
	Update();
	AntibotOnEngineClientJoin(ClientID, Sixup);
}
void CAntibot::OnEngineClientDrop(int ClientID, const char *pReason)
{
	Update();
	AntibotOnEngineClientDrop(ClientID, pReason);
}
void CAntibot::OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags)
{
	Update();
	int AntibotFlags = 0;
	if((Flags & MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	AntibotOnEngineClientMessage(ClientID, pData, Size, Flags);
}
#else
CAntibot::CAntibot() :
	m_pServer(0), m_pConsole(0), m_pGameServer(0), m_Initialized(false)
{
}
CAntibot::~CAntibot()
{
}
void CAntibot::Init()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	dbg_assert(m_pServer && m_pConsole, "antibot requires server and console");
}
void CAntibot::RoundStart(IGameServer *pGameServer)
{
	m_pGameServer = pGameServer;
}
void CAntibot::RoundEnd()
{
	m_pGameServer = 0;
}
void CAntibot::Dump()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "antibot support not compiled in");
}
void CAntibot::Update()
{
}

void CAntibot::OnPlayerInit(int ClientID) {}
void CAntibot::OnPlayerDestroy(int ClientID) {}
void CAntibot::OnSpawn(int ClientID) {}
void CAntibot::OnHammerFireReloading(int ClientID) {}
void CAntibot::OnHammerFire(int ClientID) {}
void CAntibot::OnHammerHit(int ClientID) {}
void CAntibot::OnDirectInput(int ClientID) {}
void CAntibot::OnCharacterTick(int ClientID) {}
void CAntibot::OnHookAttach(int ClientID, bool Player) {}

void CAntibot::OnEngineTick() {}
void CAntibot::OnEngineClientJoin(int ClientID, bool Sixup) {}
void CAntibot::OnEngineClientDrop(int ClientID, const char *pReason) {}
void CAntibot::OnEngineClientMessage(int ClientID, const void *pData, int Size, int Flags) {}
#endif

IEngineAntibot *CreateEngineAntibot()
{
	return new CAntibot;
}
