#ifndef ENGINE_SHARED_NET_IO_THREAD_H
#define ENGINE_SHARED_NET_IO_THREAD_H

#include <engine/shared/ReaderWriterQueue.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>

class CNetRange;

struct CEvent
{
	virtual ~CEvent() = default;
	enum EType
	{
		EV_RECV,
		EV_NEWCLIENT,
		EV_NEWCLIENT_NOAUTH,
		EV_CLIENTREJOIN,
		EV_DELCLIENT,
		EV_CLIENT_STATE,
	} m_Type;
};

struct CEventRecv final : CEvent
{
	CEventRecv() { m_Type = EV_RECV; }
	CNetChunk m_Chunk{};
	SECURITY_TOKEN m_Token{};
	unsigned char m_aData[NET_MAX_PACKETSIZE]{};
};

struct CEventClientBase : CEvent
{
	int m_ClientId{};
	NETADDR m_Addr{};
	std::array<char, NETADDR_MAXSTRSIZE> m_aAddrStr{};
	std::array<char, NETADDR_MAXSTRSIZE> m_aAddrStrNoPort{};
};

struct CEventNewClient final : CEventClientBase
{
	CEventNewClient()
	{
		m_Type = EV_NEWCLIENT;
	}
	bool m_Sixup{};
};

struct CEventNewClientNoAuth final : CEventClientBase
{
	CEventNewClientNoAuth()
	{
		m_Type = EV_NEWCLIENT_NOAUTH;
	}
};

struct CEventClientRejoin final : CEventClientBase
{
	CEventClientRejoin()
	{
		m_Type = EV_CLIENTREJOIN;
	}
};

struct CEventDelClient final : CEventClientBase
{
	CEventDelClient()
	{
		m_Type = EV_DELCLIENT;
	}
	char m_aReason[256]{};
};

struct CClientRuntimeState
{
	CNetConnection::EState m_State = CNetConnection::EState::OFFLINE;
	int m_SecurityToken = NET_SECURITY_TOKEN_UNKNOWN;
	char m_aErrorString[256]{};
	bool m_TimeoutProtected = false;
	bool m_TimeoutSituation = false;
};

struct CEventClientState final : CEvent
{
	CEventClientState()
	{
		m_Type = EV_CLIENT_STATE;
	}
	int m_ClientId{};
	CClientRuntimeState m_State{};
};

struct CCommand
{
	virtual ~CCommand() = default;
	enum EType
	{
		CMD_CLOSE,
		CMD_DROP,
		CMD_SEND_PACKET_CONNLESS_WITH_TOKEN7,
		CMD_SEND_TOKEN_SIXUP,
		CMD_SET_MAX_CLIENTS_PER_IP,
		CMD_SET_TIMED_OUT,
		CMD_SET_TIMEOUT_PROTECTED,
		CMD_IGNORE_TIMEOUTS,
		CMD_SET_ACCEPTING_NEW_CONNECTIONS,
		CMD_RESET_ERROR_STRING,
		CMD_GET_GLOBAL_TOKEN,
		CMD_GET_TOKEN,
		CMD_GET_VANILLA_TOKEN,
		CMD_SEND,
		CMD_BAN_ADDRESS,
		CMD_BAN_RANGE,
	} m_Type;
};

struct CCmdClose final : CCommand
{
	CCmdClose() { m_Type = CMD_CLOSE; }
};

struct CCmdDrop final : CCommand
{
	CCmdDrop() { m_Type = CMD_DROP; }
	int m_ClientId{};
	char m_aReason[256]{};
};

struct CCmdSendPacketConnlessWithToken7 final : CCommand
{
	CCmdSendPacketConnlessWithToken7() { m_Type = CMD_SEND_PACKET_CONNLESS_WITH_TOKEN7; }
	NETADDR m_Addr{};
	int m_DataSize{};
	unsigned char m_aData[NET_MAX_PACKETSIZE]{};
	SECURITY_TOKEN m_Token{};
	SECURITY_TOKEN m_ResponseToken{};
};

struct CCmdSendTokenSixup final : CCommand
{
	CCmdSendTokenSixup() { m_Type = CMD_SEND_TOKEN_SIXUP; }
	NETADDR m_Addr{};
	SECURITY_TOKEN m_Token{};
};

struct CCmdSetMaxClientsPerIp final : CCommand
{
	CCmdSetMaxClientsPerIp() { m_Type = CMD_SET_MAX_CLIENTS_PER_IP; }
	int m_Max{};
};

struct CCmdSetTimeoutProtected final : CCommand
{
	CCmdSetTimeoutProtected() { m_Type = CMD_SET_TIMEOUT_PROTECTED; }
	int m_ClientId{};
};

struct CCmdIgnoreTimeouts final : CCommand
{
	CCmdIgnoreTimeouts() { m_Type = CMD_IGNORE_TIMEOUTS; }
	int m_ClientId{};
};

struct CCmdSetAcceptingNewConnections final : CCommand
{
	CCmdSetAcceptingNewConnections() { m_Type = CMD_SET_ACCEPTING_NEW_CONNECTIONS; }
	bool m_Value{};
};

struct CCmdResetErrorString final : CCommand
{
	CCmdResetErrorString() { m_Type = CMD_RESET_ERROR_STRING; }
	int m_ClientId{};
};

struct CCmdSetTimedOut final : CCommand
{
	CCmdSetTimedOut() { m_Type = CMD_SET_TIMED_OUT; }
	int m_ClientId{};
	int m_OrigId{};
	std::promise<bool> m_Promise;
};

struct CCmdGetGlobalToken final : CCommand
{
	CCmdGetGlobalToken() { m_Type = CMD_GET_GLOBAL_TOKEN; }
	std::promise<SECURITY_TOKEN> m_Promise;
};

struct CCmdGetToken final : CCommand
{
	CCmdGetToken() { m_Type = CMD_GET_TOKEN; }
	NETADDR m_Addr{};
	std::promise<SECURITY_TOKEN> m_Promise;
};

struct CCmdGetVanillaToken final : CCommand
{
	CCmdGetVanillaToken() { m_Type = CMD_GET_VANILLA_TOKEN; }
	NETADDR m_Addr{};
	std::promise<SECURITY_TOKEN> m_Promise;
};

struct CCmdSend final : CCommand
{
	CCmdSend() { m_Type = CMD_SEND; }
	CNetChunk m_Chunk{};
	unsigned char m_aData[NET_MAX_PACKETSIZE]{};
};

struct CCmdBanAddress final : CCommand
{
	CCmdBanAddress() { m_Type = CMD_BAN_ADDRESS; }
	NETADDR m_Addr{};
	int m_Seconds{};
	char m_aReason[128]{};
	bool m_VerbatimReason{};
};

struct CCmdBanRange final : CCommand
{
	CCmdBanRange() { m_Type = CMD_BAN_RANGE; }
	CNetRange m_Range{};
	int m_Seconds{};
	char m_aReason[128]{};
};

class CNetIOThread : public INetServer
{
	std::atomic_bool m_Running{false};
	void *m_pThread{nullptr};

	CNetServer m_Server;

	NETADDR m_AddressCache{};
	std::atomic<int> m_NetTypeCache{NETTYPE_INVALID};
	std::atomic<int> m_MaxClientsCache{0};

	struct CClientPublicState
	{
		NETADDR m_Addr{};
		std::array<char, NETADDR_MAXSTRSIZE> m_aAddrStr{};
		std::array<char, NETADDR_MAXSTRSIZE> m_aAddrStrNoPort{};
	};

	std::array<CClientPublicState, NET_MAX_CLIENTS> m_aClientPublicState{};
	std::array<CClientRuntimeState, NET_MAX_CLIENTS> m_aClientRuntimeState{};
	std::array<CClientRuntimeState, NET_MAX_CLIENTS> m_aClientRuntimeThreadCache{};

	moodycamel::ReaderWriterQueue<std::unique_ptr<CCommand>> m_CommandQueue;
	moodycamel::ReaderWriterQueue<std::unique_ptr<CEvent>> m_EventQueue;
	moodycamel::ReaderWriterQueue<std::unique_ptr<CEventRecv>> m_RecvQueue;

	unsigned char m_aRecvStorage[NET_MAX_PACKETSIZE]{};

	SEMAPHORE m_WorkSem{};
	bool m_WorkSemInitialized = false;
	std::atomic<uint64_t> m_WorkGen{0};

	static void Run(void *pUser);

	NETFUNC_NEWCLIENT m_pfnNewClient{};
	NETFUNC_NEWCLIENT_NOAUTH m_pfnNewClientNoAuth{};
	NETFUNC_CLIENTREJOIN m_pfnClientRejoin{};
	NETFUNC_DELCLIENT m_pfnDelClient{};
	CNetConnection::NETFUNC_CONN_DIRTY m_pfnConnDirty = nullptr;
	void *m_pCallbackUser{};

	void StartWorker();
	void StopWorker();
	void ProcessCommands();
	void PumpNetwork();
	void ProcessPendingEvents();
	void UpdateClientPublicState(int ClientId, const NETADDR &Addr, const std::array<char, NETADDR_MAXSTRSIZE> &AddrStrWithPort, const std::array<char, NETADDR_MAXSTRSIZE> &AddrStrWithoutPort);
	bool HasPendingEvents() const;
	bool HasPendingCommands() const;
	void NotifyWorkAvailable();
	void EnqueueEvent(std::unique_ptr<CEvent> pEvent);
	void EnqueueCommand(std::unique_ptr<CCommand> pCmd);
	int SendMsg(CMsgPacker *pMsg, int Flags, int ClientId, bool SixUp);
	void ProcessSnap();

	CSnapThreaded *m_pSnapThreaded = nullptr;
	void *m_pConnDirtyUser = nullptr;

	static int NewClientTrampoline(int ClientId, void *pUser, bool Sixup);
	static int NewClientNoAuthTrampoline(int ClientId, void *pUser);
	static int ClientRejoinTrampoline(int ClientId, void *pUser);
	static int DelClientTrampoline(int ClientId, const char *pReason, void *pUser);

public:
	CNetIOThread() = default;
	~CNetIOThread() override { CNetIOThread::Close(); }

	int SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_NEWCLIENT_NOAUTH pfnNewClientNoAuth, NETFUNC_CLIENTREJOIN pfnClientRejoin, NETFUNC_DELCLIENT pfnDelClient, void *pUser) override;

	bool Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIp, CSnapThreaded *pSnapThreaded) override;
	void Close() override;

	int Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken) override;
	int Send(CNetChunk *pChunk) override;

	void Update() override;

	void Drop(int ClientId, const char *pReason) override;

	const NETADDR *ClientAddr(int ClientId) const override;
	const std::array<char, NETADDR_MAXSTRSIZE> &ClientAddrString(int ClientId, bool IncludePort) const override;
	bool HasSecurityToken(int ClientId) const override;
	int NumOnlineClients() override;
	NETADDR Address() const override { return m_AddressCache; }
	int NetType() const override { return m_NetTypeCache.load(std::memory_order_relaxed); }
	int MaxClients() const override { return m_MaxClientsCache.load(std::memory_order_relaxed); }

	void SendTokenSixup(NETADDR &Addr, SECURITY_TOKEN Token) override;
	void SetMaxClientsPerIp(int Max) override;
	bool HasErrored(int ClientId) override;
	void ResumeOldConnection(int ClientId, int OrigId) override;
	void ResumeOldConnectionProtected(int ClientId) override;
	void IgnoreTimeouts(int ClientId) override;
	void SetAcceptingNewConnections(bool Value) override;

	void ResetErrorString(int ClientId) override;
	const char *ErrorString(int ClientId) override;

	SECURITY_TOKEN GetGlobalToken() override;
	SECURITY_TOKEN GetToken(const NETADDR &Addr) override;
	SECURITY_TOKEN GetVanillaToken(const NETADDR &Addr) override;

	CNetConnection::EState State(int ClientId) override;

	void BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason, bool VerbatimReason = false) override;
	void BanRange(const CNetRange *pRange, int Seconds, const char *pReason) override;

	bool NetWait(int64_t TimeToTick) override;
	bool NetWaitActive() override;
	bool NetWaitNonActive() override;

	void SendPacketConnlessWithToken7(NETADDR *pAddr, const void *pData, int DataSize, SECURITY_TOKEN Token, SECURITY_TOKEN ResponseToken) override;

	NETSOCKET Socket() const override { return m_Server.Socket(); }
	CNetBan *NetBan() const override { return m_Server.NetBan(); }
};

#endif
