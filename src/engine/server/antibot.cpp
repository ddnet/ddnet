#include "antibot.h"

#include <base/dbg.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/server.h>

class IEngineAntibot;

#ifdef CONF_ANTIBOT
#if defined(CONF_FAMILY_WINDOWS)
#include <base/windows.h>

#include <windows.h>
static void *ModuleOpen(const char *pName)
{
	void *pModule = LoadLibraryA(pName);
	dbg_assert(pModule != nullptr, "failed to load antibot module '%s': %s", pName, windows_format_system_message(GetLastError()).c_str());
	return pModule;
}
static void *ModuleSymbol(void *pModule, const char *pName)
{
	return (void *)GetProcAddress((HMODULE)pModule, pName);
}
static void ModuleClose(void *pModule)
{
	FreeLibrary((HMODULE)pModule);
}
#else
#include <dlfcn.h>
static void *ModuleOpen(const char *pName)
{
	void *pModule = dlopen(pName, RTLD_NOW);
	dbg_assert(pModule != nullptr, "failed to load antibot module: %s", dlerror());
	return pModule;
}
static void *ModuleSymbol(void *pModule, const char *pName)
{
	return dlsym(pModule, pName);
}
static void ModuleClose(void *pModule)
{
	dlclose(pModule);
}
#endif

template<typename F>
static void LoadSymbol(void *pModule, F &pfnSymbol, const char *pName)
{
	pfnSymbol = (F)ModuleSymbol(pModule, pName);
	dbg_assert(pfnSymbol != nullptr, "antibot module is missing symbol '%s'", pName);
}

CAntibot::CAntibot() :
	m_pServer(nullptr), m_pConsole(nullptr), m_pGameServer(nullptr)
{
}
CAntibot::~CAntibot()
{
	if(m_pGameServer)
		free(m_RoundData.m_Map.m_pTiles);

	if(m_pModule)
	{
		m_pfnDestroy();
		ModuleClose(m_pModule);
	}
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
	LoadModule();
}
void CAntibot::LoadModule()
{
	m_pModule = ModuleOpen(ANTIBOT_LIBRARY);
#define MACRO_ANTIBOT_FUNCTION(Name) LoadSymbol(m_pModule, m_pfn##Name, "Antibot" #Name);
	ANTIBOT_FUNCTIONS(MACRO_ANTIBOT_FUNCTION)
#undef MACRO_ANTIBOT_FUNCTION
	dbg_assert(m_pfnAbiVersion() == ANTIBOT_ABI_VERSION, "antibot abi version mismatch (antibot=%d server=%d)", m_pfnAbiVersion(), ANTIBOT_ABI_VERSION);

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
	m_pfnInit(&m_Data);
}
void CAntibot::Reload()
{
	if(!m_pModule)
	{
		return;
	}
	if(!GameServer())
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "cannot reload antibot outside of a round");
		return;
	}
	m_pfnRoundEnd();
	m_pfnDestroy();
	ModuleClose(m_pModule);
	LoadModule();

	// Replay the events the new module missed, in the order of a server start.
	m_pfnRoundStart(&m_RoundData);
	Update();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(Server()->ClientSlotEmpty(ClientId))
		{
			continue;
		}
		m_pfnOnEngineClientJoin(ClientId);
		if(GameServer()->PlayerExists(ClientId))
		{
			m_pfnOnPlayerInit(ClientId);
			if(m_RoundData.m_aCharacters[ClientId].m_Alive)
			{
				m_pfnOnSpawn(ClientId);
			}
		}
	}
}
void CAntibot::RoundStart(IGameServer *pGameServer)
{
	m_pGameServer = pGameServer;
	mem_zero(&m_RoundData, sizeof(m_RoundData));
	m_RoundData.m_Map.m_pTiles = nullptr;
	m_pfnRoundStart(&m_RoundData);
	Update();
}
void CAntibot::RoundEnd()
{
	// Let the external module clean up first
	m_pfnRoundEnd();

	m_pGameServer = nullptr;
	free(m_RoundData.m_Map.m_pTiles);
}
void CAntibot::ConsoleCommand(const char *pCommand)
{
	m_pfnConsoleCommand(pCommand);
}
void CAntibot::Update()
{
	m_Data.m_Now = time_get();
	m_Data.m_Freq = time_freq();

	Server()->FillAntibot(&m_RoundData);
	if(GameServer())
	{
		GameServer()->FillAntibot(&m_RoundData);
		m_pfnUpdateData();
	}
}

void CAntibot::OnPlayerInit(int ClientId)
{
	Update();
	m_pfnOnPlayerInit(ClientId);
}
void CAntibot::OnPlayerDestroy(int ClientId)
{
	Update();
	m_pfnOnPlayerDestroy(ClientId);
}
void CAntibot::OnSpawn(int ClientId)
{
	Update();
	m_pfnOnSpawn(ClientId);
}
void CAntibot::OnHammerFireReloading(int ClientId)
{
	Update();
	m_pfnOnHammerFireReloading(ClientId);
}
void CAntibot::OnHammerFire(int ClientId)
{
	Update();
	m_pfnOnHammerFire(ClientId);
}
void CAntibot::OnHammerHit(int ClientId, int TargetId)
{
	Update();
	m_pfnOnHammerHit(ClientId, TargetId);
}
void CAntibot::OnDirectInput(int ClientId)
{
	Update();
	m_pfnOnDirectInput(ClientId);
}
void CAntibot::OnCharacterTick(int ClientId)
{
	Update();
	m_pfnOnCharacterTick(ClientId);
}
void CAntibot::OnHookAttach(int ClientId, bool Player)
{
	Update();
	m_pfnOnHookAttach(ClientId, Player);
}

void CAntibot::OnEngineTick()
{
	Update();
	m_pfnOnEngineTick();
}
void CAntibot::OnEngineClientJoin(int ClientId)
{
	Update();
	m_pfnOnEngineClientJoin(ClientId);
}
void CAntibot::OnEngineClientDrop(int ClientId, const char *pReason)
{
	Update();
	m_pfnOnEngineClientDrop(ClientId, pReason);
}
bool CAntibot::OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags)
{
	Update();
	int AntibotFlags = 0;
	if((Flags & MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	return m_pfnOnEngineClientMessage(ClientId, pData, Size, AntibotFlags);
}
bool CAntibot::OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags)
{
	Update();
	int AntibotFlags = 0;
	if((Flags & MSGFLAG_VITAL) == 0)
	{
		AntibotFlags |= ANTIBOT_MSGFLAG_NONVITAL;
	}
	return m_pfnOnEngineServerMessage(ClientId, pData, Size, AntibotFlags);
}
bool CAntibot::OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags)
{
	int AntibotFlags = 0;
	bool Result = m_pfnOnEngineSimulateClientMessage(pClientId, pBuffer, BufferSize, pOutSize, &AntibotFlags);
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
	m_pServer(nullptr), m_pConsole(nullptr), m_pGameServer(nullptr)
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
void CAntibot::Reload()
{
	if(!m_pConsole)
	{
		return;
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "antibot", "antibot support not compiled in");
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
void CAntibot::OnEngineClientJoin(int ClientId) {}
void CAntibot::OnEngineClientDrop(int ClientId, const char *pReason) {}
bool CAntibot::OnEngineClientMessage(int ClientId, const void *pData, int Size, int Flags) { return false; }
bool CAntibot::OnEngineServerMessage(int ClientId, const void *pData, int Size, int Flags) { return false; }
bool CAntibot::OnEngineSimulateClientMessage(int *pClientId, void *pBuffer, int BufferSize, int *pOutSize, int *pFlags) { return false; }
#endif

IEngineAntibot *CreateEngineAntibot()
{
	return new CAntibot;
}
