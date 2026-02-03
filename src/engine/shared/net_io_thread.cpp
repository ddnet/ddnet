#include "net_io_thread.h"

#include <base/system.h>

#include <engine/shared/config.h>

int CNetIOThread::NewClientTrampoline(int ClientId, void *pUser, bool Sixup)
{
	auto *pSelf = static_cast<CNetIOThread *>(pUser);
	if(!pSelf->m_pfnNewClient)
		return 0;

	if(!g_Config.m_SvNetThreaded)
		return pSelf->m_pfnNewClient(ClientId, pSelf->m_pCallbackUser, Sixup);

	auto pEvent = std::make_unique<CEventNewClient>();
	pEvent->m_ClientId = ClientId;
	pEvent->m_Sixup = Sixup;
	const NETADDR *pAddr = pSelf->m_Server.ClientAddr(ClientId);
	if(pAddr)
		pEvent->m_Addr = *pAddr;
	pEvent->m_aAddrStr = pSelf->m_Server.ClientAddrString(ClientId, true);
	pEvent->m_aAddrStrNoPort = pSelf->m_Server.ClientAddrString(ClientId, false);
	pSelf->EnqueueEvent(std::move(pEvent));
	return 0;
}

int CNetIOThread::NewClientNoAuthTrampoline(int ClientId, void *pUser)
{
	auto *pSelf = static_cast<CNetIOThread *>(pUser);
	if(!pSelf->m_pfnNewClientNoAuth)
		return 0;

	if(!g_Config.m_SvNetThreaded)
		return pSelf->m_pfnNewClientNoAuth(ClientId, pSelf->m_pCallbackUser);

	auto pEvent = std::make_unique<CEventNewClientNoAuth>();
	pEvent->m_ClientId = ClientId;
	const NETADDR *pAddr = pSelf->m_Server.ClientAddr(ClientId);
	if(pAddr)
		pEvent->m_Addr = *pAddr;
	pEvent->m_aAddrStr = pSelf->m_Server.ClientAddrString(ClientId, true);
	pEvent->m_aAddrStrNoPort = pSelf->m_Server.ClientAddrString(ClientId, false);
	pSelf->EnqueueEvent(std::move(pEvent));
	return 0;
}

int CNetIOThread::ClientRejoinTrampoline(int ClientId, void *pUser)
{
	auto *pSelf = static_cast<CNetIOThread *>(pUser);
	if(!pSelf->m_pfnClientRejoin)
		return 0;

	if(!g_Config.m_SvNetThreaded)
		return pSelf->m_pfnClientRejoin(ClientId, pSelf->m_pCallbackUser);

	auto pEvent = std::make_unique<CEventClientRejoin>();
	pEvent->m_ClientId = ClientId;
	const NETADDR *pAddr = pSelf->m_Server.ClientAddr(ClientId);
	if(pAddr)
		pEvent->m_Addr = *pAddr;
	pEvent->m_aAddrStr = pSelf->m_Server.ClientAddrString(ClientId, true);
	pEvent->m_aAddrStrNoPort = pSelf->m_Server.ClientAddrString(ClientId, false);
	pSelf->EnqueueEvent(std::move(pEvent));
	return 0;
}

int CNetIOThread::DelClientTrampoline(int ClientId, const char *pReason, void *pUser)
{
	auto *pSelf = static_cast<CNetIOThread *>(pUser);
	if(!pSelf->m_pfnDelClient)
		return 0;

	if(!g_Config.m_SvNetThreaded)
		return pSelf->m_pfnDelClient(ClientId, pReason, pSelf->m_pCallbackUser);

	auto pEvent = std::make_unique<CEventDelClient>();
	pEvent->m_ClientId = ClientId;
	const NETADDR *pAddr = pSelf->m_Server.ClientAddr(ClientId);
	if(pAddr)
		pEvent->m_Addr = *pAddr;
	pEvent->m_aAddrStr = pSelf->m_Server.ClientAddrString(ClientId, true);
	pEvent->m_aAddrStrNoPort = pSelf->m_Server.ClientAddrString(ClientId, false);
	str_copy(pEvent->m_aReason, pReason ? pReason : "", sizeof(pEvent->m_aReason));
	pSelf->EnqueueEvent(std::move(pEvent));
	return 0;
}

void CNetIOThread::Run(void *pUser)
{
	CNetIOThread *pSelf = static_cast<CNetIOThread *>(pUser);

	while(pSelf->m_Running.load(std::memory_order_acquire))
	{
		pSelf->ProcessCommands();
		pSelf->ProcessSnap();
		pSelf->PumpNetwork();
		if(!pSelf->HasPendingCommands())
		{
			if(pSelf->m_Server.NumOnlineClients() > 0)
				pSelf->m_Server.NetWaitActive();
			else
				pSelf->m_Server.NetWaitNonActive();
		}
	}
}

void CNetIOThread::StartWorker()
{
	if(m_Running.exchange(true))
		return;

	dbg_assert(m_WorkSemInitialized, "work semaphore not initialized");

	m_Server.SetConnDirtyObserver(
		[](int ClientId, const CNetConnection::CConnStateDelta &Delta, void *pUser) {
			auto *pSelf = static_cast<CNetIOThread *>(pUser);
			auto State = std::make_unique<CEventClientState>();
			State->m_ClientId = ClientId;
			CClientRuntimeState StateCached = pSelf->m_aClientRuntimeThreadCache[ClientId];
			if(Delta.m_StateChanged)
				StateCached.m_State = Delta.m_State;
			if(Delta.m_SecurityTokenChanged)
				StateCached.m_SecurityToken = Delta.m_SecurityToken;
			if(Delta.m_ErrorChanged)
				str_copy(StateCached.m_aErrorString, Delta.m_aErrorString);
			if(Delta.m_TimeoutProtectedChanged)
				StateCached.m_TimeoutProtected = Delta.m_TimeoutProtected;
			if(Delta.m_TimeoutSituationChanged)
				StateCached.m_TimeoutSituation = Delta.m_TimeoutSituation;

			pSelf->m_aClientRuntimeThreadCache[ClientId] = StateCached;
			State->m_State = StateCached;
			pSelf->EnqueueEvent(std::move(State));
		},
		this);

	m_pThread = thread_init(Run, this, "net io thread");
}

void CNetIOThread::StopWorker()
{
	if(!m_Running.exchange(false))
		return;
	if(m_pThread)
		thread_wait(m_pThread);
	m_pThread = nullptr;
}

void CNetIOThread::ProcessCommands()
{
	std::unique_ptr<CCommand> pCmd;
	while(m_CommandQueue.try_dequeue(pCmd))
	{
		switch(pCmd->m_Type)
		{
		case CCommand::CMD_CLOSE:
			m_Server.Close();
			break;
		case CCommand::CMD_DROP:
		{
			auto *p = static_cast<CCmdDrop *>(pCmd.get());
			m_Server.Drop(p->m_ClientId, p->m_aReason);
			break;
		}
		case CCommand::CMD_SEND_PACKET_CONNLESS_WITH_TOKEN7:
		{
			auto *p = static_cast<CCmdSendPacketConnlessWithToken7 *>(pCmd.get());
			NETADDR Addr = p->m_Addr;
			const void *pData = p->m_DataSize > 0 ? static_cast<const void *>(p->m_aData) : nullptr;
			m_Server.SendPacketConnlessWithToken7(&Addr, pData, p->m_DataSize, p->m_Token, p->m_ResponseToken);
			break;
		}
		case CCommand::CMD_SEND_TOKEN_SIXUP:
		{
			auto *p = static_cast<CCmdSendTokenSixup *>(pCmd.get());
			m_Server.SendTokenSixup(p->m_Addr, p->m_Token);
			break;
		}
		case CCommand::CMD_SET_MAX_CLIENTS_PER_IP:
		{
			auto *p = static_cast<CCmdSetMaxClientsPerIp *>(pCmd.get());
			m_Server.SetMaxClientsPerIp(p->m_Max);
			break;
		}
		case CCommand::CMD_SET_TIMEOUT_PROTECTED:
		{
			auto *p = static_cast<CCmdSetTimeoutProtected *>(pCmd.get());
			m_Server.ResumeOldConnectionProtected(p->m_ClientId);
			break;
		}
		case CCommand::CMD_IGNORE_TIMEOUTS:
		{
			auto *p = static_cast<CCmdIgnoreTimeouts *>(pCmd.get());
			m_Server.IgnoreTimeouts(p->m_ClientId);
			break;
		}
		case CCommand::CMD_SET_ACCEPTING_NEW_CONNECTIONS:
		{
			auto *p = static_cast<CCmdSetAcceptingNewConnections *>(pCmd.get());
			m_Server.SetAcceptingNewConnections(p->m_Value);
			break;
		}
		case CCommand::CMD_RESET_ERROR_STRING:
		{
			auto *p = static_cast<CCmdResetErrorString *>(pCmd.get());
			m_Server.ResetErrorString(p->m_ClientId);
			break;
		}
		case CCommand::CMD_SET_TIMED_OUT:
		{
			auto *p = static_cast<CCmdSetTimedOut *>(pCmd.get());
			m_Server.ResumeOldConnection(p->m_ClientId, p->m_OrigId);
			break;
		}
		case CCommand::CMD_GET_GLOBAL_TOKEN:
		{
			auto *p = static_cast<CCmdGetGlobalToken *>(pCmd.get());
			p->m_Promise.set_value(m_Server.GetGlobalToken());
			break;
		}
		case CCommand::CMD_GET_TOKEN:
		{
			auto *p = static_cast<CCmdGetToken *>(pCmd.get());
			p->m_Promise.set_value(m_Server.GetToken(p->m_Addr));
			break;
		}
		case CCommand::CMD_GET_VANILLA_TOKEN:
		{
			auto *p = static_cast<CCmdGetVanillaToken *>(pCmd.get());
			p->m_Promise.set_value(m_Server.GetVanillaToken(p->m_Addr));
			break;
		}
		case CCommand::CMD_SEND:
		{
			auto *p = static_cast<CCmdSend *>(pCmd.get());
			(void)m_Server.Send(&p->m_Chunk);
			break;
		}
		case CCommand::CMD_BAN_ADDRESS:
		{
			auto *p = static_cast<CCmdBanAddress *>(pCmd.get());
			m_Server.BanAddr(&p->m_Addr, p->m_Seconds, p->m_aReason, p->m_VerbatimReason);
			break;
		}
		case CCommand::CMD_BAN_RANGE:
		{
			auto *p = static_cast<CCmdBanRange *>(pCmd.get());
			m_Server.BanRange(&p->m_Range, p->m_Seconds, p->m_aReason);
			break;
		}
		default: break;
		}
	}
}

void CNetIOThread::PumpNetwork()
{
	m_Server.Update();

	SECURITY_TOKEN Token = NET_SECURITY_TOKEN_UNKNOWN;

	for(;;)
	{
		CEventRecv Ev;
		if(!m_Server.Recv(&Ev.m_Chunk, &Token))
			break;

		if(Ev.m_Chunk.m_DataSize > NET_MAX_PACKETSIZE)
			Ev.m_Chunk.m_DataSize = NET_MAX_PACKETSIZE;

		if(Ev.m_Chunk.m_DataSize > 0 && Ev.m_Chunk.m_pData)
		{
			mem_copy(Ev.m_aData, Ev.m_Chunk.m_pData, Ev.m_Chunk.m_DataSize);
			Ev.m_Chunk.m_pData = Ev.m_aData;
		}
		else
		{
			Ev.m_Chunk.m_pData = nullptr;
		}

		Ev.m_Token = Token;
		Token = NET_SECURITY_TOKEN_UNKNOWN;

		m_RecvQueue.enqueue(std::make_unique<CEventRecv>(Ev));
		NotifyWorkAvailable();
	}
}

void CNetIOThread::ProcessPendingEvents()
{
	if(!m_EventQueue.peek())
		return;
	std::unique_ptr<CEvent> pEvBase;
	while(m_EventQueue.try_dequeue(pEvBase))
	{
		switch(pEvBase->m_Type)
		{
		case CEvent::EV_NEWCLIENT:
		{
			auto *pEvent = static_cast<CEventNewClient *>(pEvBase.get());
			UpdateClientPublicState(pEvent->m_ClientId, pEvent->m_Addr, pEvent->m_aAddrStr, pEvent->m_aAddrStrNoPort);
			if(m_pfnNewClient)
				m_pfnNewClient(pEvent->m_ClientId, m_pCallbackUser, pEvent->m_Sixup);
			break;
		}
		case CEvent::EV_NEWCLIENT_NOAUTH:
		{
			auto *pEvent = static_cast<CEventNewClientNoAuth *>(pEvBase.get());
			UpdateClientPublicState(pEvent->m_ClientId, pEvent->m_Addr, pEvent->m_aAddrStr, pEvent->m_aAddrStrNoPort);
			if(m_pfnNewClientNoAuth)
				m_pfnNewClientNoAuth(pEvent->m_ClientId, m_pCallbackUser);
			break;
		}
		case CEvent::EV_CLIENTREJOIN:
		{
			auto *pEvent = static_cast<CEventClientRejoin *>(pEvBase.get());
			UpdateClientPublicState(pEvent->m_ClientId, pEvent->m_Addr, pEvent->m_aAddrStr, pEvent->m_aAddrStrNoPort);
			if(m_pfnClientRejoin)
				m_pfnClientRejoin(pEvent->m_ClientId, m_pCallbackUser);
			break;
		}
		case CEvent::EV_DELCLIENT:
		{
			auto *pEvent = static_cast<CEventDelClient *>(pEvBase.get());
			UpdateClientPublicState(pEvent->m_ClientId, pEvent->m_Addr, pEvent->m_aAddrStr, pEvent->m_aAddrStrNoPort);
			if(m_pfnDelClient)
				m_pfnDelClient(pEvent->m_ClientId, pEvent->m_aReason, m_pCallbackUser);
			break;
		}
		case CEvent::EV_CLIENT_STATE:
		{
			auto *pEvent = static_cast<CEventClientState *>(pEvBase.get());
			if(pEvent->m_ClientId >= 0 && pEvent->m_ClientId < (int)m_aClientRuntimeState.size())
				m_aClientRuntimeState[pEvent->m_ClientId] = pEvent->m_State;
			break;
		}
		default: break;
		}
	}
}

void CNetIOThread::UpdateClientPublicState(int ClientId, const NETADDR &Addr, const std::array<char, NETADDR_MAXSTRSIZE> &AddrStrWithPort, const std::array<char, NETADDR_MAXSTRSIZE> &AddrStrWithoutPort)
{
	m_aClientPublicState[ClientId].m_Addr = Addr;
	m_aClientPublicState[ClientId].m_aAddrStr = AddrStrWithPort;
	m_aClientPublicState[ClientId].m_aAddrStrNoPort = AddrStrWithoutPort;
}

bool CNetIOThread::HasPendingEvents() const
{
	const auto *pEvent = m_RecvQueue.peek();
	if(pEvent && pEvent->get())
		return true;

	const auto *pRecv = m_EventQueue.peek();
	return pRecv && pRecv->get();
}

bool CNetIOThread::HasPendingCommands() const
{
	const auto *pCmd = m_CommandQueue.peek();
	return pCmd && pCmd->get();
}

void CNetIOThread::NotifyWorkAvailable()
{
	m_WorkGen.fetch_add(1, std::memory_order_release);
	if(m_WorkSemInitialized)
		sphore_signal(&m_WorkSem);
}

void CNetIOThread::EnqueueEvent(std::unique_ptr<CEvent> pEvent)
{
	if(!pEvent)
		return;

	m_EventQueue.enqueue(std::move(pEvent));
	NotifyWorkAvailable();
}

void CNetIOThread::EnqueueCommand(std::unique_ptr<CCommand> pCmd)
{
	if(!pCmd)
		return;

	m_CommandQueue.enqueue(std::move(pCmd));
}

int CNetIOThread::SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient,
	NETFUNC_NEWCLIENT_NOAUTH pfnNewClientNoAuth,
	NETFUNC_CLIENTREJOIN pfnClientRejoin,
	NETFUNC_DELCLIENT pfnDelClient,
	void *pUser)
{
	m_pfnNewClient = pfnNewClient;
	m_pfnNewClientNoAuth = pfnNewClientNoAuth;
	m_pfnClientRejoin = pfnClientRejoin;
	m_pfnDelClient = pfnDelClient;
	m_pCallbackUser = pUser;

	int Result = m_Server.SetCallbacks(NewClientTrampoline, NewClientNoAuthTrampoline, ClientRejoinTrampoline, DelClientTrampoline, this);

	if(g_Config.m_SvNetThreaded)
		StartWorker();

	return Result;
}

bool CNetIOThread::Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIp, CSnapThreaded *pSnapThreaded)
{
	if(g_Config.m_SvNetThreaded && !m_WorkSemInitialized)
	{
		sphore_init(&m_WorkSem);
		m_WorkSemInitialized = true;
	}

	bool Ok = m_Server.Open(BindAddr, pNetBan, MaxClients, MaxClientsPerIp, pSnapThreaded);
	if(Ok)
	{
		m_AddressCache = m_Server.Address();
		m_NetTypeCache.store(m_Server.NetType(), std::memory_order_relaxed);
		m_MaxClientsCache.store(m_Server.MaxClients(), std::memory_order_relaxed);
	}
	else
	{
		m_AddressCache = NETADDR{};
		m_NetTypeCache.store(NETTYPE_INVALID, std::memory_order_relaxed);
		m_MaxClientsCache.store(0, std::memory_order_relaxed);
		if(m_WorkSemInitialized)
		{
			sphore_destroy(&m_WorkSem);
			m_WorkSemInitialized = false;
		}
	}
	return Ok;
}

void CNetIOThread::Close()
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.Close();
		return;
	}

	if(m_Running.load(std::memory_order_acquire))
	{
		EnqueueCommand(std::make_unique<CCmdClose>());
		StopWorker();
	}
	else
	{
		m_Server.Close();
	}

	m_AddressCache = NETADDR{};
	m_NetTypeCache.store(NETTYPE_INVALID, std::memory_order_relaxed);
	m_MaxClientsCache.store(0, std::memory_order_relaxed);

	if(m_WorkSemInitialized)
	{
		sphore_destroy(&m_WorkSem);
		m_WorkSemInitialized = false;
	}
}

int CNetIOThread::Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken)
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.Recv(pChunk, pResponseToken);
	}

	ProcessPendingEvents();

	std::unique_ptr<CEventRecv> pEv;
	if(!m_RecvQueue.try_dequeue(pEv))
		return 0;

	*pChunk = pEv->m_Chunk;
	if(pEv->m_Chunk.m_DataSize > 0)
	{
		mem_copy(m_aRecvStorage, pEv->m_aData, pEv->m_Chunk.m_DataSize);
		pChunk->m_pData = m_aRecvStorage;
	}
	else
	{
		pChunk->m_pData = nullptr;
	}
	if(pResponseToken)
		*pResponseToken = pEv->m_Token;

	return 1;
}

int CNetIOThread::Send(CNetChunk *pChunk)
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.Send(pChunk);
	}

	auto Cmd = std::make_unique<CCmdSend>();
	Cmd->m_Chunk = *pChunk;

	if(Cmd->m_Chunk.m_DataSize > NET_MAX_PACKETSIZE)
		Cmd->m_Chunk.m_DataSize = NET_MAX_PACKETSIZE;

	if(Cmd->m_Chunk.m_DataSize > 0 && Cmd->m_Chunk.m_pData)
	{
		mem_copy(Cmd->m_aData, Cmd->m_Chunk.m_pData, Cmd->m_Chunk.m_DataSize);
		Cmd->m_Chunk.m_pData = Cmd->m_aData;
	}
	else
	{
		Cmd->m_Chunk.m_pData = nullptr;
	}

	EnqueueCommand(std::move(Cmd));
	return 0;
}

void CNetIOThread::Update()
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.Update();
		return;
	}

	ProcessPendingEvents();
}

void CNetIOThread::Drop(int ClientId, const char *pReason)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.Drop(ClientId, pReason);
		return;
	}

	auto Cmd = std::make_unique<CCmdDrop>();
	Cmd->m_ClientId = ClientId;
	str_copy(Cmd->m_aReason, pReason ? pReason : "", sizeof(Cmd->m_aReason));
	EnqueueCommand(std::move(Cmd));
}

const NETADDR *CNetIOThread::ClientAddr(int ClientId) const
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.ClientAddr(ClientId);
	}
	return &m_aClientPublicState[ClientId].m_Addr;
}

const std::array<char, NETADDR_MAXSTRSIZE> &CNetIOThread::ClientAddrString(int ClientId, bool IncludePort) const
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.ClientAddrString(ClientId, IncludePort);
	}
	return IncludePort ? m_aClientPublicState[ClientId].m_aAddrStr : m_aClientPublicState[ClientId].m_aAddrStrNoPort;
}

bool CNetIOThread::HasSecurityToken(int ClientId) const
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.HasSecurityToken(ClientId);
	}
	int SecurityToken = m_aClientRuntimeState[ClientId].m_SecurityToken;
	return SecurityToken != NET_SECURITY_TOKEN_UNKNOWN && SecurityToken != NET_SECURITY_TOKEN_UNSUPPORTED;
}

int CNetIOThread::NumOnlineClients()
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.NumOnlineClients();
	}

	int Found = 0;
	for(int i = 0; i < MaxClients(); ++i)
	{
		if(m_aClientRuntimeState[i].m_State != CNetConnection::EState::ONLINE)
			continue;
		Found++;
	}
	return Found;
}

void CNetIOThread::SendTokenSixup(NETADDR &Addr, SECURITY_TOKEN Token)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.SendTokenSixup(Addr, Token);
		return;
	}

	auto Cmd = std::make_unique<CCmdSendTokenSixup>();
	Cmd->m_Addr = Addr;
	Cmd->m_Token = Token;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::SetMaxClientsPerIp(int Max)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.SetMaxClientsPerIp(Max);
		return;
	}

	auto Cmd = std::make_unique<CCmdSetMaxClientsPerIp>();
	Cmd->m_Max = Max;
	m_CommandQueue.enqueue(std::move(Cmd)); // without EnqueueCommand wrapper as executed before open
}

bool CNetIOThread::HasErrored(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.HasErrored(ClientId);
	}

	return m_aClientRuntimeState[ClientId].m_State == CNetConnection::EState::ERROR;
}

void CNetIOThread::ResumeOldConnectionProtected(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.ResumeOldConnectionProtected(ClientId);
		return;
	}

	auto Cmd = std::make_unique<CCmdSetTimeoutProtected>();
	Cmd->m_ClientId = ClientId;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::IgnoreTimeouts(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.IgnoreTimeouts(ClientId);
		return;
	}

	auto Cmd = std::make_unique<CCmdIgnoreTimeouts>();
	Cmd->m_ClientId = ClientId;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::SetAcceptingNewConnections(bool Value)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.SetAcceptingNewConnections(Value);
		return;
	}

	auto Cmd = std::make_unique<CCmdSetAcceptingNewConnections>();
	Cmd->m_Value = Value;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::ResetErrorString(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.ResetErrorString(ClientId);
		return;
	}

	auto Cmd = std::make_unique<CCmdResetErrorString>();
	Cmd->m_ClientId = ClientId;
	EnqueueCommand(std::move(Cmd));
}

const char *CNetIOThread::ErrorString(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.ErrorString(ClientId);
	}
	return m_aClientRuntimeState[ClientId].m_aErrorString;
}

void CNetIOThread::ResumeOldConnection(int ClientId, int OrigId)
{
	if(!g_Config.m_SvNetThreaded || !m_Running.load(std::memory_order_acquire))
	{
		m_Server.ResumeOldConnection(ClientId, OrigId);
		return;
	}

	auto Cmd = std::make_unique<CCmdSetTimedOut>();
	Cmd->m_ClientId = ClientId;
	Cmd->m_OrigId = OrigId;
	EnqueueCommand(std::move(Cmd));
}

SECURITY_TOKEN CNetIOThread::GetGlobalToken()
{
	if(!g_Config.m_SvNetThreaded || !m_Running.load(std::memory_order_acquire))
	{
		return m_Server.GetGlobalToken();
	}

	auto Cmd = std::make_unique<CCmdGetGlobalToken>();
	auto Future = Cmd->m_Promise.get_future();
	EnqueueCommand(std::move(Cmd));
	return Future.get();
}

SECURITY_TOKEN CNetIOThread::GetToken(const NETADDR &Addr)
{
	if(!g_Config.m_SvNetThreaded || !m_Running.load(std::memory_order_acquire))
	{
		return m_Server.GetToken(Addr);
	}

	auto Cmd = std::make_unique<CCmdGetToken>();
	Cmd->m_Addr = Addr;
	auto Future = Cmd->m_Promise.get_future();
	EnqueueCommand(std::move(Cmd));
	return Future.get();
}

SECURITY_TOKEN CNetIOThread::GetVanillaToken(const NETADDR &Addr)
{
	if(!g_Config.m_SvNetThreaded || !m_Running.load(std::memory_order_acquire))
	{
		return m_Server.GetVanillaToken(Addr);
	}

	auto Cmd = std::make_unique<CCmdGetVanillaToken>();
	Cmd->m_Addr = Addr;
	auto Future = Cmd->m_Promise.get_future();
	EnqueueCommand(std::move(Cmd));
	return Future.get();
}

CNetConnection::EState CNetIOThread::State(int ClientId)
{
	if(!g_Config.m_SvNetThreaded)
	{
		return m_Server.State(ClientId);
	}
	return m_aClientRuntimeState[ClientId].m_State;
}

void CNetIOThread::BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason, bool VerbatimReason)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.BanAddr(pAddr, Seconds, pReason, VerbatimReason);
		return;
	}

	auto Cmd = std::make_unique<CCmdBanAddress>();
	if(pAddr)
		Cmd->m_Addr = *pAddr;
	Cmd->m_Seconds = Seconds;
	str_copy(Cmd->m_aReason, pReason ? pReason : "", sizeof(Cmd->m_aReason));
	Cmd->m_VerbatimReason = VerbatimReason;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::BanRange(const CNetRange *pRange, int Seconds, const char *pReason)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.BanRange(pRange, Seconds, pReason);
		return;
	}

	auto Cmd = std::make_unique<CCmdBanRange>();
	if(pRange)
		Cmd->m_Range = *pRange;
	Cmd->m_Seconds = Seconds;
	str_copy(Cmd->m_aReason, pReason ? pReason : "", sizeof(Cmd->m_aReason));
	EnqueueCommand(std::move(Cmd));
}

bool CNetIOThread::NetWait(int64_t TimeToTick)
{
	if(!g_Config.m_SvNetThreaded)
		return m_Server.NetWait(TimeToTick);

	if(HasPendingEvents())
		return true;

	if(!m_WorkSemInitialized)
		return HasPendingEvents();

	if(TimeToTick > 0)
	{
		const uint64_t GenBefore = m_WorkGen.load(std::memory_order_acquire);
		sphore_wait_deadline_ns(&m_WorkSem, TimeToTick);
		if(m_WorkGen.load(std::memory_order_acquire) != GenBefore)
			return true;
		return HasPendingEvents();
	}

	return false;
}

bool CNetIOThread::NetWaitActive()
{
	if(!g_Config.m_SvNetThreaded)
		return m_Server.NetWaitActive();
	return NetWait(500000);
}

bool CNetIOThread::NetWaitNonActive()
{
	if(!g_Config.m_SvNetThreaded)
		return m_Server.NetWaitNonActive();
	return NetWait(1000000000);
}

void CNetIOThread::SendPacketConnlessWithToken7(NETADDR *pAddr, const void *pData, int DataSize, SECURITY_TOKEN Token, SECURITY_TOKEN ResponseToken)
{
	if(!g_Config.m_SvNetThreaded)
	{
		m_Server.SendPacketConnlessWithToken7(pAddr, pData, DataSize, Token, ResponseToken);
		return;
	}

	auto Cmd = std::make_unique<CCmdSendPacketConnlessWithToken7>();
	if(pAddr)
		Cmd->m_Addr = *pAddr;

	if(DataSize > NET_MAX_PACKETSIZE)
		DataSize = NET_MAX_PACKETSIZE;
	Cmd->m_DataSize = DataSize > 0 ? DataSize : 0;
	if(Cmd->m_DataSize > 0 && pData)
		mem_copy(Cmd->m_aData, pData, Cmd->m_DataSize);

	Cmd->m_Token = Token;
	Cmd->m_ResponseToken = ResponseToken;
	EnqueueCommand(std::move(Cmd));
}

void CNetIOThread::ProcessSnap()
{
	m_Server.ProcessSnap();
}
