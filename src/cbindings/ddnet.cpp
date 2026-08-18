#include "ddnet.h"

#include <base/log.h>
#include <base/math.h>
#include <engine/engine.h>
#include <engine/server/server.h>
#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>
#include <game/version.h>

CServerLib::CServerLib() :
	m_GameContext(new CGameContext())
{
	mem_zero(m_aHasInput, sizeof(m_aHasInput));
	mem_zero(m_aInputs, sizeof(m_aInputs));
}

CServerLib::~CServerLib()
{
	if(m_pKernel)
	{
		delete m_pKernel;
		m_pKernel = nullptr;
		m_pServer = nullptr;
	}
}

CPlayer *CServerLib::GetPlayer(int32_t Id) {
    dbg_assert(Id >= 0, "invalid PlayerId");
    dbg_assert(Id < MAX_CLIENTS, "invalid PlayerId");
	return GameContext()->m_apPlayers[Id];
}

const CPlayer *CServerLib::GetPlayer(int32_t Id) const {
    dbg_assert(Id >= 0, "invalid PlayerId");
	dbg_assert(Id < MAX_CLIENTS, "invalid PlayerId");
	return GameContext()->m_apPlayers[Id];
}

int CServerLib::Init(const char *pDirectory) {
	const int64_t MainStart = time_get();

	m_pServer = CreateServer();

	m_pKernel = IKernel::Create();
	m_pKernel->RegisterInterface(m_pServer);

	// create the components
	IEngine *pEngine = CreateEngine(GAME_NAME, nullptr);
	m_pKernel->RegisterInterface(pEngine);

	std::unique_ptr<IStorage> pStorageOwner = CreateTempStorage(pDirectory, 1, &pDirectory);
	IStorage *pStorage = pStorageOwner.release();
	if(!pStorage)
	{
		log_error("server", "failed to initialize storage");
		if(m_pKernel)
		{
			delete m_pKernel;
			m_pKernel = nullptr;
			m_pServer = nullptr;
		}
		return -1;
	}
	m_pKernel->RegisterInterface(pStorage);

	{
		char aBuf[IO_MAX_PATH_LENGTH];
		char aBufName[IO_MAX_PATH_LENGTH];
		char aDate[64];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aBufName, sizeof(aBufName), "dumps/" GAME_NAME "-Server_%s_crash_log_%s_%d_%s.RTP", CONF_PLATFORM_STRING, aDate, pid(), GIT_SHORTREV_HASH != nullptr ? GIT_SHORTREV_HASH : "");
		pStorage->GetCompletePath(IStorage::TYPE_SAVE, aBufName, aBuf, sizeof(aBuf));
		crashdump_init_if_available(aBuf);
	}

	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
	m_pKernel->RegisterInterface(pConsole);

	IConfigManager *pConfigManager = CreateConfigManager();
	m_pKernel->RegisterInterface(pConfigManager);

	IEngineMap *pEngineMap = CreateEngineMap();
	m_pKernel->RegisterInterface(pEngineMap); // IEngineMap
	m_pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

	IEngineAntibot *pEngineAntibot = CreateEngineAntibot();
	m_pKernel->RegisterInterface(pEngineAntibot); // IEngineAntibot
	m_pKernel->RegisterInterface(static_cast<IAntibot *>(pEngineAntibot), false);

	// m_GameContext got initialized in constructor
	m_pKernel->RegisterInterface(static_cast<IGameServer *>(m_GameContext));

	pEngine->Init();
	pConsole->Init();
	pConfigManager->Init();

	// register all console commands
	m_pServer->RegisterCommands();

	// execute autoexec file set by teehistorian
	if(pStorage->FileExists(AUTOEXEC_SERVER_FILE, IStorage::TYPE_ALL))
	{
		pConsole->ExecuteFile(AUTOEXEC_SERVER_FILE, IConsole::CLIENT_ID_UNSPECIFIED);
	}

	pConfigManager->SetReadOnly("sv_max_clients", true);
	pConfigManager->SetReadOnly("sv_test_cmds", true);
	pConfigManager->SetReadOnly("sv_rescue", true);
	pConfigManager->SetReadOnly("sv_port", true);
	pConfigManager->SetReadOnly("bindaddr", true);

	// run the server
	log_trace("server", "initialization finished after %.2fms, starting...", (time_get() - MainStart) * 1000.0f / (float)time_freq());
	// int Ret = pServer->Run();
	if(m_pServer->m_RunServer == CServer::UNINITIALIZED)
		m_pServer->m_RunServer = CServer::RUNNING;

	m_pServer->m_AuthManager.Init();
	// TODO: database
	m_pEngine = m_pKernel->RequestInterface<IEngine>();
	m_pServer->m_NetServer.SetCallbacks(CServer::NewClientCallback, CServer::NewClientNoAuthCallback, CServer::ClientRejoinCallback, CServer::DelClientCallback, m_pServer);

	// TODO don't initialize?
	m_pServer->m_Econ.Init(m_pServer->Config(), m_pServer->Console(), &m_pServer->m_ServerBan);

	// TODO don't initialize?
	m_pServer->m_Fifo.Init(m_pServer->Console(), m_pServer->Config()->m_SvInputFifo, CFGFLAG_SERVER);

	// load map
	if(!Server()->LoadMap("twmap"))
	{
		delete m_pKernel;
		m_pKernel = nullptr;
		m_pServer = nullptr;
		return -1;
	}

	m_pServer->GameServer()->OnInit(nullptr);
	if(m_pServer->ErrorShutdown())
	{
		m_pServer->m_RunServer = CServer::STOPPING;
	}

	return 0;
}

// ---------------------------------------------------------------------------
// Game trait
// ---------------------------------------------------------------------------

void CServerLib::PlayerJoin(int32_t Id) {
    dbg_assert(Id >= 0, "invalid PlayerId");
	dbg_assert(Id < MAX_CLIENTS, "invalid ClientId");
	auto& Client = m_pServer->m_aClients[Id];
	Client.Reset();
	Client.m_State = CServer::CClient::STATE_CONNECTING;
	Client.m_AuthKey = -1;
	m_pServer->GameServer()->OnClientConnected(Id, nullptr);
}

void CServerLib::PlayerReady(int32_t Id) {
    dbg_assert(Id >= 0, "invalid PlayerId");
    dbg_assert(Id < MAX_CLIENTS, "invalid ClientId");

	auto &Client = m_pServer->m_aClients[Id];
	if(Client.m_State == CServer::CClient::STATE_INGAME)
	{
		return;
	}

	Client.m_State = CServer::CClient::STATE_INGAME;
	m_pServer->GameServer()->OnClientEnter(Id);
}

void CServerLib::PlayerInput(int32_t Id, const CNetObj_PlayerInput *pInput)
{
    dbg_assert(Id >= 0, "invalid PlayerId");
    dbg_assert(Id < MAX_CLIENTS, "invalid ClientId");
	if(pInput == nullptr)
	{
		return;
	}

	auto &Client = m_pServer->m_aClients[Id];
	if(Client.m_State != CServer::CClient::STATE_INGAME)
	{
		return;
	}

	m_aInputs[Id] = *pInput;
	m_aHasInput[Id] = true;

	CServer::CClient::CInput *pStoredInput = &Client.m_aInputs[Client.m_CurrentInput];
	mem_zero(pStoredInput, sizeof(*pStoredInput));
	mem_copy(pStoredInput->m_aData, pInput, minimum<int>(sizeof(*pInput), sizeof(pStoredInput->m_aData)));
	pStoredInput->m_GameTick = m_pServer->Tick() + 1;

	mem_copy(Client.m_LatestInput.m_aData, pStoredInput->m_aData, MAX_INPUT_SIZE * sizeof(int));
	Client.m_CurrentInput = (Client.m_CurrentInput + 1) % 200;
}

void CServerLib::PlayerLeave(int32_t Id)
{
    dbg_assert(Id >= 0, "invalid PlayerId");
    dbg_assert(Id < MAX_CLIENTS, "invalid ClientId");

	auto &Client = m_pServer->m_aClients[Id];
	if(Client.m_State != CServer::CClient::STATE_EMPTY)
	{
		m_pServer->GameServer()->OnClientDrop(Id, "disconnect");
		Client.m_State = CServer::CClient::STATE_EMPTY;
	}
	Client.Reset();
	m_aHasInput[Id] = false;
}

void CServerLib::PlayerNet(int32_t Id, const unsigned char *pNetMsg, uint32_t Len)
{
    dbg_assert(Id >= 0, "invalid PlayerId");
    dbg_assert(Id < MAX_CLIENTS, "invalid ClientId");
	if(pNetMsg == nullptr || Len == 0)
	{
		return;
	}

	auto &Client = m_pServer->m_aClients[Id];
	if(Client.m_State < CServer::CClient::STATE_CONNECTING)
	{
		return;
	}

	CUnpacker Unpacker;
	Unpacker.Reset(pNetMsg, Len);

	CMsgPacker Packer(NETMSG_EX, true);
	int Msg = 0;
	bool Sys = false;
	CUuid Uuid;
	int Result = UnpackMessageId(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR || Sys)
	{
		return;
	}

	GameContext()->OnMessage(Msg, &Unpacker, Id);
}

void CServerLib::OnCommand(int32_t Id, const char *pCommand)
{
	CPlayer *pPlayer = GetPlayer(Id);
	if(!pPlayer || m_pServer->m_aClients[Id].m_State != CServer::CClient::STATE_INGAME)
	{
		return;
	}

	IConsole *pConsole = m_pServer->Console();
	dbg_assert(pConsole, "console not initialized");

	pConsole->SetFlagMask(CFGFLAG_CHAT);
	pConsole->ExecuteLine(pCommand, Id, false);
}

void CServerLib::SwapTees(int32_t Id1, int32_t Id2)
{
	GameContext()->m_World.SwapClients(Id1, Id2);
}

void CServerLib::Tick(int32_t CurTime)
{
	if(!m_pServer || m_pServer->m_RunServer >= CServer::STOPPING)
	{
		return;
	}

	(void)CurTime;
	set_new_tick();

	// advance one game tick and snapshot the state
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pServer->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
		{
			m_aHasInput[i] = false;
			continue;
		}
		const CNetObj_PlayerInput *pInput = m_aHasInput[i] ? &m_aInputs[i] : nullptr;
		Server()->GameServer()->OnClientPredictedEarlyInput(i, pInput);
	}
	// advance tick
	m_pServer->m_CurrentGameTick++;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_pServer->m_aClients[i].m_State != CServer::CClient::STATE_INGAME)
		{
			m_aHasInput[i] = false;
			continue;
		}
		const CNetObj_PlayerInput *pInput = m_aHasInput[i] ? &m_aInputs[i] : nullptr;
		Server()->GameServer()->OnClientPredictedInput(i, pInput);
		m_aHasInput[i] = false;
	}
	m_pServer->GameServer()->OnTick();

	m_pServer->DoSnapshot();

	if(m_pServer->ErrorShutdown())
	{
		m_pServer->m_RunServer = CServer::STOPPING;
	}
}

// ---------------------------------------------------------------------------
// GameValidator trait
// ---------------------------------------------------------------------------

int32_t CServerLib::MaxTeeId() const {
	// TODO: keep track of current max PlayerId
	return MAX_CLIENTS;
}

int32_t CServerLib::PlayerTeam(int32_t Id) const {
	const CPlayer *pPlayer = GetPlayer(Id);
	if(!pPlayer)
		return 0;
	return GameContext()->GetDDRaceTeam(static_cast<int>(Id));
}

void CServerLib::SetPlayerTeam(int32_t Id, int32_t Team) {
	GameContext()->m_pController->DoTeamChange(GetPlayer(Id), Team);
}

void CServerLib::TeePos(int32_t Id, bool *pHasPos, int32_t *pX, int32_t *pY) const {
	*pHasPos = false;
	*pX = 0;
	*pY = 0;
	const CPlayer *pPlayer = GetPlayer(Id);
	if(!pPlayer)
		return;
	const CCharacter *pChar = pPlayer->GetCharacter();
	if(!pChar)
		return;

	*pHasPos = true;
	*pX = round_to_int(pChar->m_Pos.x);
	*pY = round_to_int(pChar->m_Pos.y);
}

void CServerLib::SetTeePos(int32_t Id, bool HasPos, int32_t X, int32_t Y) {
	CPlayer *pPlayer = GetPlayer(Id);
	if(!pPlayer)
		return;

	CCharacter *pChar = pPlayer->GetCharacter();
	if(!HasPos)
	{
		if(pChar)
		{
			pPlayer->KillCharacter(WEAPON_GAME, false);
		}
		return;
	}

	vec2 Pos(static_cast<float>(X), static_cast<float>(Y));
	if(!pChar)
	{
		pPlayer->ForceSpawn(Pos);
		pChar = pPlayer->GetCharacter();
	}

	if(pChar)
	{
		pChar->SetPosition(Pos);
		pChar->ResetVelocity();
		pChar->m_Pos = Pos;
	}
}

// ---------------------------------------------------------------------------
// Snapper trait
// ---------------------------------------------------------------------------

void CServerLib::Snap(const uint8_t **ppData, uint32_t *pLen)
{
	dbg_assert(ppData, "Snap output data pointer required");
	dbg_assert(pLen, "Snap output length required");

	m_pServer->m_SnapshotBuilder.Init();
	m_pServer->GameServer()->OnSnap(SERVER_DEMO_CLIENT, true);

	char aCompData[CSnapshot::MAX_SIZE];
	int SnapshotSize = m_pServer->m_SnapshotBuilder.Finish(aCompData);
	*pLen = CVariableInt::Compress(aCompData, SnapshotSize, m_aSnapBuffer, sizeof(aCompData));
	*ppData = m_aSnapBuffer;
}
