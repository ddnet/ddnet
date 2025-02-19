#include "antibot.h"

#include <antibot/antibot_interface.h>

#include <base/system.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/server.h>

class IEngineAntibot;

#ifdef CONF_ANTIBOT
CAntibot::CAntibot() :
	m_pServer(0), m_pConsole(0), m_pGameServer(0), m_Initialized(false)
{
}
CAntibot::~CAntibot()
{
	if(m_pGameServer)
		free(m_RoundData.m_Map.m_pTiles);

	if(m_Initialized)
		AntibotDestroy();
}
void CAntibot::Kick(int ClientId, const char *pMessage, void *pUser)
{
	CAntibot *pAntibot = (CAntibot *)pUser;
	pAntibot->Server()->Kick(ClientId, pMessage);
}
void CAntibot::Log(const char *pMessage, void *pUser)
{
	CAntibot *pAntibot = (CAntibot *)pUser;
	pAntibot->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", pMessage);
}
void CAntibot::Report(int ClientId, const char *pMessage, void *pUser)
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%d: %s", ClientId, pMessage);
	Log(aBuf, pUser);
}
void CAntibot::Send(int ClientId, const void *pData, int Size, int Flags, void *pUser)
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
	pAntibot->Server()->SendMsgRaw(ClientId, pData, Size, RealFlags);
}
void CAntibot::Teehistorian(const void *pData, int Size, void *pUser)
{
	CAntibot *pAntibot = (CAntibot *)pUser;
	pAntibot->m_pGameServer->TeehistorianRecordAntibot(pData, Size);
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
	m_Data.m_pfnKick = Kick;
	m_Data.m_pfnLog = Log;
	m_Data.m_pfnReport = Report;
	m_Data.m_pfnSend = Send;
	m_Data.m_pfnTeehistorian = Teehistorian;
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
	free(m_RoundData.m_Map.m_pTiles);
}
void CAntibot::ConsoleCommand(const char *pCommand)
{
	AntibotConsoleCommand(pCommand);
}
void CAntibot::Update()
{
	m_Data.m_Now = time_get();
	m_Data.m_Freq = time_freq();

	Server()->FillAntibot(&m_RoundData);
	if(GameServer())
	{
		GameServer()->FillAntibot(&m_RoundData);
		AntibotUpdateData();
	}
}

void CAntibot::OnPlayerInit(int ClientId)
{
	Update();
	AntibotOnPlayerInit(ClientId);
}
void CAntibot::OnPlayerDestroy(int ClientId)
{
	Update();
	AntibotOnPlayerDestroy(ClientId);
}
void CAntibot::OnSpawn(int ClientId)
{
	Update();
	AntibotOnSpawn(ClientId);
}
void CAntibot::OnHammerFireReloading(int ClientId)
{
	Update();
	AntibotOnHammerFireReloading(ClientId);
}
void CAntibot::OnHammerFire(int ClientId)
{
	Update();
	AntibotOnHammerFire(ClientId);
}
void CAntibot::OnHammerHit(int ClientId, int TargetId)
{
	Update();
	AntibotOnHammerHit(ClientId, TargetId);
}
void CAntibot::OnDirectInput(int ClientId)
{
	Update();
	AntibotOnDirectInput(ClientId);
}
void CAntibot::OnCharacterTick(int ClientId)
{
	Update();
	AntibotOnCharacterTick(ClientId);
}
void CAntibot::OnHookAttach(int ClientId, bool Player)
{
	Update();
	AntibotOnHookAttach(ClientId, Player);
}

void CAntibot::OnEngineTick()
{
	Update();
	AntibotOnEngineTick();
}
void CAntibot::OnEngineClientJoin(int ClientId, bool Sixup)
{
	Update();
	AntibotOnEngineClientJoin(ClientId, Sixup);
}
void CAntibot::OnEngineClientDrop(int ClientId, const char *pReason)
{
	Update();
	AntibotOnEngineClientDrop(ClientId, pReason);
}
bool CAntibot::OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags)
{
	Update();
	int AntibotFlags = 0;
	if((Flags & MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	return AntibotOnEngineClientMessage(ClientId, pData, Size, AntibotFlags);
}
bool CAntibot::OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags)
{
	Update();
	int AntibotFlags = 0;
	if((Flags & MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	return AntibotOnEngineServerMessage(ClientId, pData, Size, AntibotFlags);
}
bool CAntibot::OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags)
{
	int AntibotFlags = 0;
	bool Result = AntibotOnEngineSimulateClientMessage(pClientId, pBuffer, BufferSize, pOutSize, &AntibotFlags);
	if(Result)
	{
		*pFlags = 0;
		if((AntibotFlags & ANTIBOT_MSGFLAG_NONVITAL) == 0)
		{
			*pFlags |= MSGFLAG_VITAL;
		}
	}
	return Result;
}
#else
CAntibot::CAntibot() :
	m_pServer(nullptr), m_pConsole(nullptr), m_pGameServer(nullptr), m_Initialized(false)
{
}
CAntibot::~CAntibot() = default;
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
	m_pGameServer = nullptr;
}
void CAntibot::ConsoleCommand(const char *pCommand)
{
	if(str_comp(pCommand, "dump") == 0)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "antibot support not compiled in");
	}
	else
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "unknown command");
	}
}
void CAntibot::Update()
{
}

void CAntibot::OnPlayerInit(int ClientId) {}
void CAntibot::OnPlayerDestroy(int ClientId) {}
void CAntibot::OnSpawn(int ClientId) {}
void CAntibot::OnHammerFireReloading(int ClientId) {}
void CAntibot::OnHammerFire(int ClientId) {}
void CAntibot::OnHammerHit(int ClientId, int TargetId) {}
void CAntibot::OnDirectInput(int ClientId) {}
void CAntibot::OnCharacterTick(int ClientId) {}
void CAntibot::OnHookAttach(int ClientId, bool Player) {}

void CAntibot::OnEngineTick() {}
void CAntibot::OnEngineClientJoin(int ClientId, bool Sixup) {}
void CAntibot::OnEngineClientDrop(int ClientId, const char *pReason) {}
bool CAntibot::OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags) { return false; }
bool CAntibot::OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags) { return false; }
bool CAntibot::OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags) { return false; }
#endif

IEngineAntibot *CreateEngineAntibot()
{
	return new CAntibot;
}
