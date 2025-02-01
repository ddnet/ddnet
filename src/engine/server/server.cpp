/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "server.h"

#include <base/logger.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/console.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/fifo.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/host_lookup.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/masterserver.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol7.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/rust_version.h>
#include <engine/shared/snapshot.h>

#include <game/version.h>

// DDRace
#include <engine/shared/linereader.h>
#include <vector>
#include <zlib.h>

#include "databases/connection.h"
#include "databases/connection_pool.h"
#include "register.h"

extern bool IsInterrupted();

#if defined(CONF_PLATFORM_ANDROID)
extern std::vector<std::string> FetchAndroidServerCommandQueue();
#endif

void CServerBan::InitServerBan(IConsole *pConsole, IStorage *pStorage, CServer *pServer)
{
	CNetBan::Init(pConsole, pStorage);

	m_pServer = pServer;

	// overwrites base command, todo: improve this
	Console()->Register("ban", "s[ip|id] ?i[minutes] r[reason]", CFGFLAG_SERVER | CFGFLAG_STORE, ConBanExt, this, "Ban player with ip/client id for x minutes for any reason");
	Console()->Register("ban_region", "s[region] s[ip|id] ?i[minutes] r[reason]", CFGFLAG_SERVER | CFGFLAG_STORE, ConBanRegion, this, "Ban player in a region");
	Console()->Register("ban_region_range", "s[region] s[first ip] s[last ip] ?i[minutes] r[reason]", CFGFLAG_SERVER | CFGFLAG_STORE, ConBanRegionRange, this, "Ban range in a region");
}

template<class T>
int CServerBan::BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason, bool VerbatimReason)
{
	// validate address
	if(Server()->m_RconClientId >= 0 && Server()->m_RconClientId < MAX_CLIENTS &&
		Server()->m_aClients[Server()->m_RconClientId].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(NetMatch(pData, Server()->ClientAddr(Server()->m_RconClientId)))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (you can't ban yourself)");
			return -1;
		}

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(i == Server()->m_RconClientId || Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed >= Server()->m_RconAuthLevel && NetMatch(pData, Server()->ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}
	else if(Server()->m_RconClientId == IServer::RCON_CID_VOTE)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed != AUTHED_NO && NetMatch(pData, Server()->ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}

	int Result = Ban(pBanPool, pData, Seconds, pReason, VerbatimReason);
	if(Result != 0)
		return Result;

	// drop banned clients
	typename T::CDataType Data = *pData;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;

		if(NetMatch(&Data, Server()->ClientAddr(i)))
		{
			CNetHash NetHash(&Data);
			char aBuf[256];
			MakeBanInfo(pBanPool->Find(&Data, &NetHash), aBuf, sizeof(aBuf), MSGTYPE_PLAYER);
			Server()->m_NetServer.Drop(i, aBuf);
		}
	}

	return Result;
}

int CServerBan::BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason, bool VerbatimReason)
{
	return BanExt(&m_BanAddrPool, pAddr, Seconds, pReason, VerbatimReason);
}

int CServerBan::BanRange(const CNetRange *pRange, int Seconds, const char *pReason)
{
	if(pRange->IsValid())
		return BanExt(&m_BanRangePool, pRange, Seconds, pReason, true);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban failed (invalid range)");
	return -1;
}

void CServerBan::ConBanExt(IConsole::IResult *pResult, void *pUser)
{
	CServerBan *pThis = static_cast<CServerBan *>(pUser);

	const char *pStr = pResult->GetString(0);
	int Minutes = pResult->NumArguments() > 1 ? clamp(pResult->GetInteger(1), 0, 525600) : 10;
	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "Follow the server rules. Type /rules into the chat.";

	if(str_isallnum(pStr))
	{
		int ClientId = str_toint(pStr);
		if(ClientId < 0 || ClientId >= MAX_CLIENTS || pThis->Server()->m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY)
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid client id)");
		else
			pThis->BanAddr(pThis->Server()->ClientAddr(ClientId), Minutes * 60, pReason, false);
	}
	else
		ConBan(pResult, pUser);
}

void CServerBan::ConBanRegion(IConsole::IResult *pResult, void *pUser)
{
	const char *pRegion = pResult->GetString(0);
	if(str_comp_nocase(pRegion, g_Config.m_SvRegionName))
		return;

	pResult->RemoveArgument(0);
	ConBanExt(pResult, pUser);
}

void CServerBan::ConBanRegionRange(IConsole::IResult *pResult, void *pUser)
{
	CServerBan *pServerBan = static_cast<CServerBan *>(pUser);

	const char *pRegion = pResult->GetString(0);
	if(str_comp_nocase(pRegion, g_Config.m_SvRegionName))
		return;

	pResult->RemoveArgument(0);
	ConBanRange(pResult, static_cast<CNetBan *>(pServerBan));
}

// Not thread-safe!
class CRconClientLogger : public ILogger
{
	CServer *m_pServer;
	int m_ClientId;

public:
	CRconClientLogger(CServer *pServer, int ClientId) :
		m_pServer(pServer),
		m_ClientId(ClientId)
	{
	}
	void Log(const CLogMessage *pMessage) override;
};

void CRconClientLogger::Log(const CLogMessage *pMessage)
{
	if(m_Filter.Filters(pMessage))
	{
		return;
	}
	m_pServer->SendRconLogLine(m_ClientId, pMessage);
}

void CServer::CClient::Reset()
{
	// reset input
	for(auto &Input : m_aInputs)
		Input.m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = -1;
	m_NextMapChunk = 0;
	m_Flags = 0;
	m_RedirectDropTime = 0;
}

CServer::CServer()
{
	m_pConfig = &g_Config;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aDemoRecorder[i] = CDemoRecorder(&m_SnapshotDelta, true);
	m_aDemoRecorder[RECORDER_MANUAL] = CDemoRecorder(&m_SnapshotDelta, false);
	m_aDemoRecorder[RECORDER_AUTO] = CDemoRecorder(&m_SnapshotDelta, false);

	m_pGameServer = nullptr;

	m_CurrentGameTick = MIN_TICK;
	m_RunServer = UNINITIALIZED;

	m_aShutdownReason[0] = 0;

	for(int i = 0; i < NUM_MAP_TYPES; i++)
	{
		m_apCurrentMapData[i] = nullptr;
		m_aCurrentMapSize[i] = 0;
	}

	m_MapReload = false;
	m_SameMapReload = false;
	m_ReloadedWhenEmpty = false;
	m_aCurrentMap[0] = '\0';
	m_pCurrentMapName = m_aCurrentMap;
	m_aMapDownloadUrl[0] = '\0';

	m_RconClientId = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	m_ServerInfoFirstRequest = 0;
	m_ServerInfoNumRequests = 0;
	m_ServerInfoNeedsUpdate = false;

#ifdef CONF_FAMILY_UNIX
	m_ConnLoggingSocketCreated = false;
#endif

	m_pConnectionPool = new CDbConnectionPool();
	m_pRegister = nullptr;

	m_aErrorShutdownReason[0] = 0;

	Init();
}

CServer::~CServer()
{
	for(auto &pCurrentMapData : m_apCurrentMapData)
	{
		free(pCurrentMapData);
	}

	if(m_RunServer != UNINITIALIZED)
	{
		for(auto &Client : m_aClients)
		{
			free(Client.m_pPersistentData);
		}
	}
	free(m_pPersistentData);

	delete m_pRegister;
	delete m_pConnectionPool;
}

bool CServer::IsClientNameAvailable(int ClientId, const char *pNameRequest)
{
	// check for empty names
	if(!pNameRequest[0])
		return false;

	// check for names starting with /, as they can be abused to make people
	// write chat commands
	if(pNameRequest[0] == '/')
		return false;

	// make sure that two clients don't have the same name
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i != ClientId && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if(str_utf8_comp_confusable(pNameRequest, m_aClients[i].m_aName) == 0)
				return false;
		}
	}

	return true;
}

bool CServer::SetClientNameImpl(int ClientId, const char *pNameRequest, bool Set)
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid client id");
	if(m_aClients[ClientId].m_State < CClient::STATE_READY)
		return false;

	const CNameBan *pBanned = m_NameBans.IsBanned(pNameRequest);
	if(pBanned)
	{
		if(m_aClients[ClientId].m_State == CClient::STATE_READY && Set)
		{
			char aBuf[256];
			if(pBanned->m_aReason[0])
			{
				str_format(aBuf, sizeof(aBuf), "Kicked (your name is banned: %s)", pBanned->m_aReason);
			}
			else
			{
				str_copy(aBuf, "Kicked (your name is banned)");
			}
			Kick(ClientId, aBuf);
		}
		return false;
	}

	// trim the name
	char aTrimmedName[MAX_NAME_LENGTH];
	str_copy(aTrimmedName, str_utf8_skip_whitespaces(pNameRequest));
	str_utf8_trim_right(aTrimmedName);

	char aNameTry[MAX_NAME_LENGTH];
	str_copy(aNameTry, aTrimmedName);

	if(!IsClientNameAvailable(ClientId, aNameTry))
	{
		// auto rename
		for(int i = 1;; i++)
		{
			str_format(aNameTry, sizeof(aNameTry), "(%d)%s", i, aTrimmedName);
			if(IsClientNameAvailable(ClientId, aNameTry))
				break;
		}
	}

	bool Changed = str_comp(m_aClients[ClientId].m_aName, aNameTry) != 0;

	if(Set && Changed)
	{
		// set the client name
		str_copy(m_aClients[ClientId].m_aName, aNameTry);
		GameServer()->TeehistorianRecordPlayerName(ClientId, m_aClients[ClientId].m_aName);
	}

	return Changed;
}

bool CServer::SetClientClanImpl(int ClientId, const char *pClanRequest, bool Set)
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid client id");
	if(m_aClients[ClientId].m_State < CClient::STATE_READY)
		return false;

	const CNameBan *pBanned = m_NameBans.IsBanned(pClanRequest);
	if(pBanned)
	{
		if(m_aClients[ClientId].m_State == CClient::STATE_READY && Set)
		{
			char aBuf[256];
			if(pBanned->m_aReason[0])
			{
				str_format(aBuf, sizeof(aBuf), "Kicked (your clan is banned: %s)", pBanned->m_aReason);
			}
			else
			{
				str_copy(aBuf, "Kicked (your clan is banned)");
			}
			Kick(ClientId, aBuf);
		}
		return false;
	}

	// trim the clan
	char aTrimmedClan[MAX_CLAN_LENGTH];
	str_copy(aTrimmedClan, str_utf8_skip_whitespaces(pClanRequest));
	str_utf8_trim_right(aTrimmedClan);

	bool Changed = str_comp(m_aClients[ClientId].m_aClan, aTrimmedClan) != 0;

	if(Set)
	{
		// set the client clan
		str_copy(m_aClients[ClientId].m_aClan, aTrimmedClan);
	}

	return Changed;
}

bool CServer::WouldClientNameChange(int ClientId, const char *pNameRequest)
{
	return SetClientNameImpl(ClientId, pNameRequest, false);
}

bool CServer::WouldClientClanChange(int ClientId, const char *pClanRequest)
{
	return SetClientClanImpl(ClientId, pClanRequest, false);
}

void CServer::SetClientName(int ClientId, const char *pName)
{
	SetClientNameImpl(ClientId, pName, true);
}

void CServer::SetClientClan(int ClientId, const char *pClan)
{
	SetClientClanImpl(ClientId, pClan, true);
}

void CServer::SetClientCountry(int ClientId, int Country)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientId].m_Country = Country;
}

void CServer::SetClientScore(int ClientId, std::optional<int> Score)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State < CClient::STATE_READY)
		return;

	if(m_aClients[ClientId].m_Score != Score)
		ExpireServerInfo();

	m_aClients[ClientId].m_Score = Score;
}

void CServer::SetClientFlags(int ClientId, int Flags)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientId].m_Flags = Flags;
}

void CServer::Kick(int ClientId, const char *pReason)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return;
	}
	else if(m_RconClientId == ClientId)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
		return;
	}
	else if(m_aClients[ClientId].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
		return;
	}

	m_NetServer.Drop(ClientId, pReason);
}

void CServer::Ban(int ClientId, int Seconds, const char *pReason, bool VerbatimReason)
{
	m_NetServer.NetBan()->BanAddr(ClientAddr(ClientId), Seconds, pReason, VerbatimReason);
}

void CServer::ReconnectClient(int ClientId)
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid client id");

	if(GetClientVersion(ClientId) < VERSION_DDNET_RECONNECT)
	{
		RedirectClient(ClientId, m_NetServer.Address().port);
		return;
	}
	log_info("server", "telling client to reconnect, cid=%d", ClientId);

	CMsgPacker Msg(NETMSG_RECONNECT, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);

	if(m_aClients[ClientId].m_State >= CClient::STATE_READY)
	{
		GameServer()->OnClientDrop(ClientId, "reconnect");
	}

	m_aClients[ClientId].m_RedirectDropTime = time_get() + time_freq() * 10;
	m_aClients[ClientId].m_State = CClient::STATE_REDIRECTED;
}

void CServer::RedirectClient(int ClientId, int Port)
{
	dbg_assert(0 <= ClientId && ClientId < MAX_CLIENTS, "invalid client id");

	bool SupportsRedirect = GetClientVersion(ClientId) >= VERSION_DDNET_REDIRECT;

	log_info("server", "redirecting client, cid=%d port=%d supported=%d", ClientId, Port, SupportsRedirect);

	if(!SupportsRedirect)
	{
		char aBuf[128];
		bool SamePort = Port == this->Port();
		str_format(aBuf, sizeof(aBuf), "Redirect unsupported: please connect to port %d", Port);
		Kick(ClientId, SamePort ? "Redirect unsupported: please reconnect" : aBuf);
		return;
	}

	CMsgPacker Msg(NETMSG_REDIRECT, true);
	Msg.AddInt(Port);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);

	if(m_aClients[ClientId].m_State >= CClient::STATE_READY)
	{
		GameServer()->OnClientDrop(ClientId, "redirect");
	}

	m_aClients[ClientId].m_RedirectDropTime = time_get() + time_freq() * 10;
	m_aClients[ClientId].m_State = CClient::STATE_REDIRECTED;
}

int64_t CServer::TickStartTime(int Tick)
{
	return m_GameStartTime + (time_freq() * Tick) / TickSpeed();
}

int CServer::Init()
{
	for(auto &Client : m_aClients)
	{
		Client.m_State = CClient::STATE_EMPTY;
		Client.m_aName[0] = 0;
		Client.m_aClan[0] = 0;
		Client.m_Country = -1;
		Client.m_Snapshots.Init();
		Client.m_Traffic = 0;
		Client.m_TrafficSince = 0;
		Client.m_ShowIps = false;
		Client.m_DebugDummy = false;
		Client.m_AuthKey = -1;
		Client.m_Latency = 0;
		Client.m_Sixup = false;
		Client.m_RedirectDropTime = 0;
	}

	m_CurrentGameTick = MIN_TICK;

	m_AnnouncementLastLine = -1;
	mem_zero(m_aPrevStates, sizeof(m_aPrevStates));

	return 0;
}

void CServer::SendLogLine(const CLogMessage *pMessage)
{
	if(pMessage->m_Level <= IConsole::ToLogLevelFilter(g_Config.m_ConsoleOutputLevel))
	{
		SendRconLogLine(-1, pMessage);
	}
	if(pMessage->m_Level <= IConsole::ToLogLevelFilter(g_Config.m_EcOutputLevel))
	{
		m_Econ.Send(-1, pMessage->m_aLine);
	}
}

void CServer::SetRconCid(int ClientId)
{
	m_RconClientId = ClientId;
}

int CServer::GetAuthedState(int ClientId) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");
	dbg_assert(m_aClients[ClientId].m_State != CServer::CClient::STATE_EMPTY, "Client slot is empty");
	return m_aClients[ClientId].m_Authed;
}

const char *CServer::GetAuthName(int ClientId) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");
	dbg_assert(m_aClients[ClientId].m_State != CServer::CClient::STATE_EMPTY, "Client slot is empty");
	int Key = m_aClients[ClientId].m_AuthKey;
	dbg_assert(Key != -1, "Client not authed");
	return m_AuthManager.KeyIdent(Key);
}

bool CServer::GetClientInfo(int ClientId, CClientInfo *pInfo) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");
	dbg_assert(pInfo != nullptr, "pInfo cannot be null");

	if(m_aClients[ClientId].m_State == CClient::STATE_INGAME)
	{
		pInfo->m_pName = m_aClients[ClientId].m_aName;
		pInfo->m_Latency = m_aClients[ClientId].m_Latency;
		pInfo->m_GotDDNetVersion = m_aClients[ClientId].m_DDNetVersionSettled;
		pInfo->m_DDNetVersion = m_aClients[ClientId].m_DDNetVersion >= 0 ? m_aClients[ClientId].m_DDNetVersion : VERSION_VANILLA;
		if(m_aClients[ClientId].m_GotDDNetVersionPacket)
		{
			pInfo->m_pConnectionId = &m_aClients[ClientId].m_ConnectionId;
			pInfo->m_pDDNetVersionStr = m_aClients[ClientId].m_aDDNetVersionStr;
		}
		else
		{
			pInfo->m_pConnectionId = nullptr;
			pInfo->m_pDDNetVersionStr = nullptr;
		}
		return true;
	}
	return false;
}

void CServer::SetClientDDNetVersion(int ClientId, int DDNetVersion)
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");

	if(m_aClients[ClientId].m_State == CClient::STATE_INGAME)
	{
		m_aClients[ClientId].m_DDNetVersion = DDNetVersion;
		m_aClients[ClientId].m_DDNetVersionSettled = true;
	}
}

const NETADDR *CServer::ClientAddr(int ClientId) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");
	dbg_assert(m_aClients[ClientId].m_State != CServer::CClient::STATE_EMPTY, "Client slot is empty");
#ifdef CONF_DEBUG
	if(m_aClients[ClientId].m_DebugDummy)
	{
		return &m_aClients[ClientId].m_DebugDummyAddr;
	}
#endif
	return m_NetServer.ClientAddr(ClientId);
}

const std::array<char, NETADDR_MAXSTRSIZE> &CServer::ClientAddrStringImpl(int ClientId, bool IncludePort) const
{
	dbg_assert(ClientId >= 0 && ClientId < MAX_CLIENTS, "ClientId is not valid");
	dbg_assert(m_aClients[ClientId].m_State != CServer::CClient::STATE_EMPTY, "Client slot is empty");
#ifdef CONF_DEBUG
	if(m_aClients[ClientId].m_DebugDummy)
	{
		return IncludePort ? m_aClients[ClientId].m_aDebugDummyAddrString : m_aClients[ClientId].m_aDebugDummyAddrStringNoPort;
	}
#endif
	return m_NetServer.ClientAddrString(ClientId, IncludePort);
}

const char *CServer::ClientName(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY)
		return "(invalid)";
	if(m_aClients[ClientId].m_State == CServer::CClient::STATE_INGAME || m_aClients[ClientId].m_State == CServer::CClient::STATE_REDIRECTED)
		return m_aClients[ClientId].m_aName;
	else
		return "(connecting)";
}

const char *CServer::ClientClan(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientId].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientId].m_aClan;
	else
		return "";
}

int CServer::ClientCountry(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientId].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientId].m_Country;
	else
		return -1;
}

bool CServer::ClientSlotEmpty(int ClientId) const
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS && m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY;
}

bool CServer::ClientIngame(int ClientId) const
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS && m_aClients[ClientId].m_State == CServer::CClient::STATE_INGAME;
}

int CServer::Port() const
{
	return m_NetServer.Address().port;
}

int CServer::MaxClients() const
{
	return m_RunServer == UNINITIALIZED ? 0 : m_NetServer.MaxClients();
}

int CServer::ClientCount() const
{
	int ClientCount = 0;
	for(const auto &Client : m_aClients)
	{
		if(Client.m_State != CClient::STATE_EMPTY)
		{
			ClientCount++;
		}
	}

	return ClientCount;
}

int CServer::DistinctClientCount() const
{
	const NETADDR *apAddresses[MAX_CLIENTS];
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// connecting clients with spoofed ips can clog slots without being ingame
		apAddresses[i] = ClientIngame(i) ? ClientAddr(i) : nullptr;
	}

	int ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(apAddresses[i] == nullptr)
		{
			continue;
		}
		ClientCount++;
		for(int j = 0; j < i; j++)
		{
			if(apAddresses[j] != nullptr && !net_addr_comp_noport(apAddresses[i], apAddresses[j]))
			{
				ClientCount--;
				break;
			}
		}
	}
	return ClientCount;
}

int CServer::GetClientVersion(int ClientId) const
{
	// Assume latest client version for server demos
	if(ClientId == SERVER_DEMO_CLIENT)
		return DDNET_VERSION_NUMBER;

	CClientInfo Info;
	if(GetClientInfo(ClientId, &Info))
		return Info.m_DDNetVersion;
	return VERSION_NONE;
}

static inline bool RepackMsg(const CMsgPacker *pMsg, CPacker &Packer, bool Sixup)
{
	int MsgId = pMsg->m_MsgId;
	Packer.Reset();

	if(Sixup && !pMsg->m_NoTranslate)
	{
		if(pMsg->m_System)
		{
			if(MsgId >= OFFSET_UUID)
				;
			else if(MsgId >= NETMSG_MAP_CHANGE && MsgId <= NETMSG_MAP_DATA)
				;
			else if(MsgId >= NETMSG_CON_READY && MsgId <= NETMSG_INPUTTIMING)
				MsgId += 1;
			else if(MsgId == NETMSG_RCON_LINE)
				MsgId = protocol7::NETMSG_RCON_LINE;
			else if(MsgId >= NETMSG_PING && MsgId <= NETMSG_PING_REPLY)
				MsgId += 4;
			else if(MsgId >= NETMSG_RCON_CMD_ADD && MsgId <= NETMSG_RCON_CMD_REM)
				MsgId -= 11;
			else
			{
				dbg_msg("net", "DROP send sys %d", MsgId);
				return true;
			}
		}
		else
		{
			if(MsgId >= 0 && MsgId < OFFSET_UUID)
				MsgId = Msg_SixToSeven(MsgId);

			if(MsgId < 0)
				return true;
		}
	}

	if(MsgId < OFFSET_UUID)
	{
		Packer.AddInt((MsgId << 1) | (pMsg->m_System ? 1 : 0));
	}
	else
	{
		Packer.AddInt(pMsg->m_System ? 1 : 0); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(MsgId, &Packer);
	}
	Packer.AddRaw(pMsg->Data(), pMsg->Size());

	return false;
}

int CServer::SendMsg(CMsgPacker *pMsg, int Flags, int ClientId)
{
	CNetChunk Packet;
	mem_zero(&Packet, sizeof(CNetChunk));
	if(Flags & MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags & MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(ClientId < 0)
	{
		CPacker Pack6, Pack7;
		if(RepackMsg(pMsg, Pack6, false))
			return -1;
		if(RepackMsg(pMsg, Pack7, true))
			return -1;

		// write message to demo recorders
		if(!(Flags & MSGFLAG_NORECORD))
		{
			for(auto &Recorder : m_aDemoRecorder)
				if(Recorder.IsRecording())
					Recorder.RecordMessage(Pack6.Data(), Pack6.Size());
		}

		if(!(Flags & MSGFLAG_NOSEND))
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_aClients[i].m_State == CClient::STATE_INGAME)
				{
					CPacker *pPack = m_aClients[i].m_Sixup ? &Pack7 : &Pack6;
					Packet.m_pData = pPack->Data();
					Packet.m_DataSize = pPack->Size();
					Packet.m_ClientId = i;
					if(Antibot()->OnEngineServerMessage(i, Packet.m_pData, Packet.m_DataSize, Flags))
					{
						continue;
					}
					m_NetServer.Send(&Packet);
				}
			}
		}
	}
	else
	{
		CPacker Pack;
		if(RepackMsg(pMsg, Pack, m_aClients[ClientId].m_Sixup))
			return -1;

		Packet.m_ClientId = ClientId;
		Packet.m_pData = Pack.Data();
		Packet.m_DataSize = Pack.Size();

		if(Antibot()->OnEngineServerMessage(ClientId, Packet.m_pData, Packet.m_DataSize, Flags))
		{
			return 0;
		}

		// write message to demo recorders
		if(!(Flags & MSGFLAG_NORECORD))
		{
			if(m_aDemoRecorder[ClientId].IsRecording())
				m_aDemoRecorder[ClientId].RecordMessage(Pack.Data(), Pack.Size());
			if(m_aDemoRecorder[RECORDER_MANUAL].IsRecording())
				m_aDemoRecorder[RECORDER_MANUAL].RecordMessage(Pack.Data(), Pack.Size());
			if(m_aDemoRecorder[RECORDER_AUTO].IsRecording())
				m_aDemoRecorder[RECORDER_AUTO].RecordMessage(Pack.Data(), Pack.Size());
		}

		if(!(Flags & MSGFLAG_NOSEND))
			m_NetServer.Send(&Packet);
	}

	return 0;
}

void CServer::SendMsgRaw(int ClientId, const void *pData, int Size, int Flags)
{
	CNetChunk Packet;
	mem_zero(&Packet, sizeof(CNetChunk));
	Packet.m_ClientId = ClientId;
	Packet.m_pData = pData;
	Packet.m_DataSize = Size;
	Packet.m_Flags = 0;
	if(Flags & MSGFLAG_VITAL)
	{
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	}
	if(Flags & MSGFLAG_FLUSH)
	{
		Packet.m_Flags |= NETSENDFLAG_FLUSH;
	}
	m_NetServer.Send(&Packet);
}

void CServer::DoSnapshot()
{
	GameServer()->OnPreSnap();

	if(m_aDemoRecorder[RECORDER_MANUAL].IsRecording() || m_aDemoRecorder[RECORDER_AUTO].IsRecording())
	{
		// create snapshot for demo recording
		char aData[CSnapshot::MAX_SIZE];

		// build snap and possibly add some messages
		m_SnapshotBuilder.Init();
		GameServer()->OnSnap(-1);
		int SnapshotSize = m_SnapshotBuilder.Finish(aData);

		// write snapshot
		if(m_aDemoRecorder[RECORDER_MANUAL].IsRecording())
			m_aDemoRecorder[RECORDER_MANUAL].RecordSnapshot(Tick(), aData, SnapshotSize);
		if(m_aDemoRecorder[RECORDER_AUTO].IsRecording())
			m_aDemoRecorder[RECORDER_AUTO].RecordSnapshot(Tick(), aData, SnapshotSize);
	}

	// create snapshots for all clients
	for(int i = 0; i < MaxClients(); i++)
	{
		// client must be ingame to receive snapshots
		if(m_aClients[i].m_State != CClient::STATE_INGAME)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick() % TickSpeed()) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick() % 10) != 0)
			continue;

		{
			m_SnapshotBuilder.Init(m_aClients[i].m_Sixup);

			GameServer()->OnSnap(i);

			// finish snapshot
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot *pData = (CSnapshot *)aData; // Fix compiler warning for strict-aliasing
			int SnapshotSize = m_SnapshotBuilder.Finish(pData);

			if(m_aDemoRecorder[i].IsRecording())
			{
				// write snapshot
				m_aDemoRecorder[i].RecordSnapshot(Tick(), aData, SnapshotSize);
			}

			int Crc = pData->Crc();

			// remove old snapshots
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick - TickSpeed() * 3);

			// save the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0, nullptr);

			// find snapshot that we can perform delta against
			int DeltaTick = -1;
			const CSnapshot *pDeltashot = CSnapshot::EmptySnapshot();
			{
				int DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, nullptr, &pDeltashot, nullptr);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			m_SnapshotDelta.SetStaticsize(protocol7::NETEVENTTYPE_SOUNDWORLD, m_aClients[i].m_Sixup);
			m_SnapshotDelta.SetStaticsize(protocol7::NETEVENTTYPE_DAMAGE, m_aClients[i].m_Sixup);
			char aDeltaData[CSnapshot::MAX_SIZE];
			int DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData);

			if(DeltaSize)
			{
				// compress it
				const int MaxSize = MAX_SNAPSHOT_PACKSIZE;

				char aCompData[CSnapshot::MAX_SIZE];
				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData, sizeof(aCompData));
				int NumPackets = (SnapshotSize + MaxSize - 1) / MaxSize;

				for(int n = 0, Left = SnapshotSize; Left > 0; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY, true);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick - DeltaTick);
				SendMsg(&Msg, MSGFLAG_FLUSH, i);
			}
		}
	}

	GameServer()->OnPostSnap();
}

int CServer::ClientRejoinCallback(int ClientId, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	pThis->m_aClients[ClientId].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientId].m_AuthKey = -1;
	pThis->m_aClients[ClientId].m_pRconCmdToSend = nullptr;
	pThis->m_aClients[ClientId].m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;
	pThis->m_aClients[ClientId].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientId].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientId].m_DDNetVersionSettled = false;

	pThis->m_aClients[ClientId].Reset();

	pThis->GameServer()->TeehistorianRecordPlayerRejoin(ClientId);
	pThis->Antibot()->OnEngineClientDrop(ClientId, "rejoin");
	pThis->Antibot()->OnEngineClientJoin(ClientId, false);

	pThis->SendMap(ClientId);

	return 0;
}

int CServer::NewClientNoAuthCallback(int ClientId, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	pThis->m_aClients[ClientId].m_DnsblState = CClient::DNSBL_STATE_NONE;

	pThis->m_aClients[ClientId].m_State = CClient::STATE_CONNECTING;
	pThis->m_aClients[ClientId].m_aName[0] = 0;
	pThis->m_aClients[ClientId].m_aClan[0] = 0;
	pThis->m_aClients[ClientId].m_Country = -1;
	pThis->m_aClients[ClientId].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientId].m_AuthKey = -1;
	pThis->m_aClients[ClientId].m_AuthTries = 0;
	pThis->m_aClients[ClientId].m_pRconCmdToSend = nullptr;
	pThis->m_aClients[ClientId].m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;
	pThis->m_aClients[ClientId].m_ShowIps = false;
	pThis->m_aClients[ClientId].m_DebugDummy = false;
	pThis->m_aClients[ClientId].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientId].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientId].m_DDNetVersionSettled = false;
	pThis->m_aClients[ClientId].Reset();

	pThis->GameServer()->TeehistorianRecordPlayerJoin(ClientId, false);
	pThis->Antibot()->OnEngineClientJoin(ClientId, false);

	pThis->SendCapabilities(ClientId);
	pThis->SendMap(ClientId);
#if defined(CONF_FAMILY_UNIX)
	pThis->SendConnLoggingCommand(OPEN_SESSION, pThis->ClientAddr(ClientId));
#endif
	return 0;
}

int CServer::NewClientCallback(int ClientId, void *pUser, bool Sixup)
{
	CServer *pThis = (CServer *)pUser;
	pThis->m_aClients[ClientId].m_State = CClient::STATE_PREAUTH;
	pThis->m_aClients[ClientId].m_DnsblState = CClient::DNSBL_STATE_NONE;
	pThis->m_aClients[ClientId].m_aName[0] = 0;
	pThis->m_aClients[ClientId].m_aClan[0] = 0;
	pThis->m_aClients[ClientId].m_Country = -1;
	pThis->m_aClients[ClientId].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientId].m_AuthKey = -1;
	pThis->m_aClients[ClientId].m_AuthTries = 0;
	pThis->m_aClients[ClientId].m_pRconCmdToSend = nullptr;
	pThis->m_aClients[ClientId].m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;
	pThis->m_aClients[ClientId].m_Traffic = 0;
	pThis->m_aClients[ClientId].m_TrafficSince = 0;
	pThis->m_aClients[ClientId].m_ShowIps = false;
	pThis->m_aClients[ClientId].m_DebugDummy = false;
	pThis->m_aClients[ClientId].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientId].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientId].m_DDNetVersionSettled = false;
	mem_zero(&pThis->m_aClients[ClientId].m_Addr, sizeof(NETADDR));
	pThis->m_aClients[ClientId].Reset();

	pThis->GameServer()->TeehistorianRecordPlayerJoin(ClientId, Sixup);
	pThis->Antibot()->OnEngineClientJoin(ClientId, Sixup);

	pThis->m_aClients[ClientId].m_Sixup = Sixup;

#if defined(CONF_FAMILY_UNIX)
	pThis->SendConnLoggingCommand(OPEN_SESSION, pThis->ClientAddr(ClientId));
#endif
	return 0;
}

void CServer::InitDnsbl(int ClientId)
{
	NETADDR Addr = *ClientAddr(ClientId);

	//TODO: support ipv6
	if(Addr.type != NETTYPE_IPV4)
		return;

	// build dnsbl host lookup
	char aBuf[256];
	if(Config()->m_SvDnsblKey[0] == '\0')
	{
		// without key
		str_format(aBuf, sizeof(aBuf), "%d.%d.%d.%d.%s", Addr.ip[3], Addr.ip[2], Addr.ip[1], Addr.ip[0], Config()->m_SvDnsblHost);
	}
	else
	{
		// with key
		str_format(aBuf, sizeof(aBuf), "%s.%d.%d.%d.%d.%s", Config()->m_SvDnsblKey, Addr.ip[3], Addr.ip[2], Addr.ip[1], Addr.ip[0], Config()->m_SvDnsblHost);
	}

	m_aClients[ClientId].m_pDnsblLookup = std::make_shared<CHostLookup>(aBuf, NETTYPE_IPV4);
	Engine()->AddJob(m_aClients[ClientId].m_pDnsblLookup);
	m_aClients[ClientId].m_DnsblState = CClient::DNSBL_STATE_PENDING;
}

#ifdef CONF_FAMILY_UNIX
void CServer::SendConnLoggingCommand(CONN_LOGGING_CMD Cmd, const NETADDR *pAddr)
{
	if(!Config()->m_SvConnLoggingServer[0] || !m_ConnLoggingSocketCreated)
		return;

	// pack the data and send it
	unsigned char aData[23] = {0};
	aData[0] = Cmd;
	mem_copy(&aData[1], &pAddr->type, 4);
	mem_copy(&aData[5], pAddr->ip, 16);
	mem_copy(&aData[21], &pAddr->port, 2);

	net_unix_send(m_ConnLoggingSocket, &m_ConnLoggingDestAddr, aData, sizeof(aData));
}
#endif

int CServer::DelClientCallback(int ClientId, const char *pReason, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=<{%s}> reason='%s'", ClientId, pThis->ClientAddrString(ClientId, true), pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

#if defined(CONF_FAMILY_UNIX)
	// Make copy of address because the client slot will be empty at the end of the function
	const NETADDR Addr = *pThis->ClientAddr(ClientId);
#endif

	// notify the mod about the drop
	if(pThis->m_aClients[ClientId].m_State >= CClient::STATE_READY)
		pThis->GameServer()->OnClientDrop(ClientId, pReason);

	pThis->m_aClients[ClientId].m_State = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientId].m_aName[0] = 0;
	pThis->m_aClients[ClientId].m_aClan[0] = 0;
	pThis->m_aClients[ClientId].m_Country = -1;
	pThis->m_aClients[ClientId].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientId].m_AuthKey = -1;
	pThis->m_aClients[ClientId].m_AuthTries = 0;
	pThis->m_aClients[ClientId].m_pRconCmdToSend = nullptr;
	pThis->m_aClients[ClientId].m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;
	pThis->m_aClients[ClientId].m_Traffic = 0;
	pThis->m_aClients[ClientId].m_TrafficSince = 0;
	pThis->m_aClients[ClientId].m_ShowIps = false;
	pThis->m_aClients[ClientId].m_DebugDummy = false;
	pThis->m_aPrevStates[ClientId] = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientId].m_Snapshots.PurgeAll();
	pThis->m_aClients[ClientId].m_Sixup = false;
	pThis->m_aClients[ClientId].m_RedirectDropTime = 0;
	pThis->m_aClients[ClientId].m_HasPersistentData = false;

	pThis->GameServer()->TeehistorianRecordPlayerDrop(ClientId, pReason);
	pThis->Antibot()->OnEngineClientDrop(ClientId, pReason);
#if defined(CONF_FAMILY_UNIX)
	pThis->SendConnLoggingCommand(CLOSE_SESSION, &Addr);
#endif
	return 0;
}

void CServer::SendRconType(int ClientId, bool UsernameReq)
{
	CMsgPacker Msg(NETMSG_RCONTYPE, true);
	Msg.AddInt(UsernameReq);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc)
{
	str_copy(pMapName, GetMapName(), MapNameSize);
	*pMapSize = m_aCurrentMapSize[MAP_TYPE_SIX];
	*pMapSha256 = m_aCurrentMapSha256[MAP_TYPE_SIX];
	*pMapCrc = m_aCurrentMapCrc[MAP_TYPE_SIX];
}

void CServer::SendCapabilities(int ClientId)
{
	CMsgPacker Msg(NETMSG_CAPABILITIES, true);
	Msg.AddInt(SERVERCAP_CURVERSION); // version
	Msg.AddInt(SERVERCAPFLAG_DDNET | SERVERCAPFLAG_CHATTIMEOUTCODE | SERVERCAPFLAG_ANYPLAYERFLAG | SERVERCAPFLAG_PINGEX | SERVERCAPFLAG_ALLOWDUMMY | SERVERCAPFLAG_SYNCWEAPONINPUT); // flags
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendMap(int ClientId)
{
	int MapType = IsSixup(ClientId) ? MAP_TYPE_SIXUP : MAP_TYPE_SIX;
	{
		CMsgPacker Msg(NETMSG_MAP_DETAILS, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddRaw(&m_aCurrentMapSha256[MapType].data, sizeof(m_aCurrentMapSha256[MapType].data));
		Msg.AddInt(m_aCurrentMapCrc[MapType]);
		Msg.AddInt(m_aCurrentMapSize[MapType]);
		if(m_aMapDownloadUrl[0])
		{
			Msg.AddString(m_aMapDownloadUrl, 0);
		}
		else
		{
			Msg.AddString("", 0);
		}
		SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
	}
	{
		CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddInt(m_aCurrentMapCrc[MapType]);
		Msg.AddInt(m_aCurrentMapSize[MapType]);
		if(MapType == MAP_TYPE_SIXUP)
		{
			Msg.AddInt(Config()->m_SvMapWindow);
			Msg.AddInt(1024 - 128);
			Msg.AddRaw(m_aCurrentMapSha256[MapType].data, sizeof(m_aCurrentMapSha256[MapType].data));
		}
		SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);
	}

	m_aClients[ClientId].m_NextMapChunk = 0;
}

void CServer::SendMapData(int ClientId, int Chunk)
{
	int MapType = IsSixup(ClientId) ? MAP_TYPE_SIXUP : MAP_TYPE_SIX;
	unsigned int ChunkSize = 1024 - 128;
	unsigned int Offset = Chunk * ChunkSize;
	int Last = 0;

	// drop faulty map data requests
	if(Chunk < 0 || Offset > m_aCurrentMapSize[MapType])
		return;

	if(Offset + ChunkSize >= m_aCurrentMapSize[MapType])
	{
		ChunkSize = m_aCurrentMapSize[MapType] - Offset;
		Last = 1;
	}

	CMsgPacker Msg(NETMSG_MAP_DATA, true);
	if(MapType == MAP_TYPE_SIX)
	{
		Msg.AddInt(Last);
		Msg.AddInt(m_aCurrentMapCrc[MAP_TYPE_SIX]);
		Msg.AddInt(Chunk);
		Msg.AddInt(ChunkSize);
	}
	Msg.AddRaw(&m_apCurrentMapData[MapType][Offset], ChunkSize);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);

	if(Config()->m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "sending chunk %d with size %d", Chunk, ChunkSize);
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
	}
}

void CServer::SendMapReload(int ClientId)
{
	CMsgPacker Msg(NETMSG_MAP_RELOAD, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);
}

void CServer::SendConnectionReady(int ClientId)
{
	CMsgPacker Msg(NETMSG_CON_READY, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);
}

void CServer::SendRconLine(int ClientId, const char *pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE, true);
	Msg.AddString(pLine, 512);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendRconLogLine(int ClientId, const CLogMessage *pMessage)
{
	const char *pLine = pMessage->m_aLine;
	const char *pStart = str_find(pLine, "<{");
	const char *pEnd = pStart == nullptr ? nullptr : str_find(pStart + 2, "}>");
	const char *pLineWithoutIps;
	char aLine[512];
	char aLineWithoutIps[512];
	aLine[0] = '\0';
	aLineWithoutIps[0] = '\0';

	if(pStart == nullptr || pEnd == nullptr)
	{
		pLineWithoutIps = pLine;
	}
	else
	{
		str_append(aLine, pLine, pStart - pLine + 1);
		str_append(aLine, pStart + 2, pStart - pLine + pEnd - pStart - 1);
		str_append(aLine, pEnd + 2);

		str_append(aLineWithoutIps, pLine, pStart - pLine + 1);
		str_append(aLineWithoutIps, "XXX");
		str_append(aLineWithoutIps, pEnd + 2);

		pLine = aLine;
		pLineWithoutIps = aLineWithoutIps;
	}

	if(ClientId == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY && m_aClients[i].m_Authed >= AUTHED_ADMIN)
				SendRconLine(i, m_aClients[i].m_ShowIps ? pLine : pLineWithoutIps);
		}
	}
	else
	{
		if(m_aClients[ClientId].m_State != CClient::STATE_EMPTY)
			SendRconLine(ClientId, m_aClients[ClientId].m_ShowIps ? pLine : pLineWithoutIps);
	}
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientId)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_ADD, true);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientId)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_REM, true);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendRconCmdGroupStart(int ClientId)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_GROUP_START, true);
	Msg.AddInt(NumRconCommands(ClientId));
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendRconCmdGroupEnd(int ClientId)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_GROUP_END, true);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

int CServer::NumRconCommands(int ClientId)
{
	int Num = 0;
	const int ConsoleAccessLevel = m_aClients[ClientId].ConsoleAccessLevel();
	for(const IConsole::CCommandInfo *pCmd = Console()->FirstCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
		pCmd; pCmd = pCmd->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER))
	{
		Num++;
	}
	return Num;
}

void CServer::UpdateClientRconCommands(int ClientId)
{
	CClient &Client = m_aClients[ClientId];
	if(Client.m_State != CClient::STATE_INGAME ||
		!Client.m_Authed ||
		Client.m_pRconCmdToSend == nullptr)
	{
		return;
	}

	const int ConsoleAccessLevel = Client.ConsoleAccessLevel();
	for(int i = 0; i < MAX_RCONCMD_SEND && Client.m_pRconCmdToSend; ++i)
	{
		SendRconCmdAdd(Client.m_pRconCmdToSend, ClientId);
		Client.m_pRconCmdToSend = Client.m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
		if(Client.m_pRconCmdToSend == nullptr)
		{
			SendRconCmdGroupEnd(ClientId);
		}
	}
}

CServer::CMaplistEntry::CMaplistEntry(const char *pName)
{
	str_copy(m_aName, pName);
}

bool CServer::CMaplistEntry::operator<(const CMaplistEntry &Other) const
{
	return str_comp_filenames(m_aName, Other.m_aName) < 0;
}

void CServer::SendMaplistGroupStart(int ClientId)
{
	CMsgPacker Msg(NETMSG_MAPLIST_GROUP_START, true);
	Msg.AddInt(m_vMaplistEntries.size());
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::SendMaplistGroupEnd(int ClientId)
{
	CMsgPacker Msg(NETMSG_MAPLIST_GROUP_END, true);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
}

void CServer::UpdateClientMaplistEntries(int ClientId)
{
	CClient &Client = m_aClients[ClientId];
	if(Client.m_State != CClient::STATE_INGAME ||
		!Client.m_Authed ||
		Client.m_Sixup ||
		Client.m_pRconCmdToSend != nullptr || // wait for command sending
		Client.m_MaplistEntryToSend == CClient::MAPLIST_DISABLED ||
		Client.m_MaplistEntryToSend == CClient::MAPLIST_DONE)
	{
		return;
	}

	if(Client.m_MaplistEntryToSend == CClient::MAPLIST_UNINITIALIZED)
	{
		static const char *const MAP_COMMANDS[] = {"sv_map", "change_map"};
		const int ConsoleAccessLevel = Client.ConsoleAccessLevel();
		const bool MapCommandAllowed = std::any_of(std::begin(MAP_COMMANDS), std::end(MAP_COMMANDS), [&](const char *pMapCommand) {
			const IConsole::CCommandInfo *pInfo = Console()->GetCommandInfo(pMapCommand, CFGFLAG_SERVER, false);
			dbg_assert(pInfo != nullptr, "Map command not found");
			return ConsoleAccessLevel <= pInfo->GetAccessLevel();
		});
		if(MapCommandAllowed)
		{
			Client.m_MaplistEntryToSend = 0;
			SendMaplistGroupStart(ClientId);
		}
		else
		{
			Client.m_MaplistEntryToSend = CClient::MAPLIST_DISABLED;
			return;
		}
	}

	if((size_t)Client.m_MaplistEntryToSend < m_vMaplistEntries.size())
	{
		CMsgPacker Msg(NETMSG_MAPLIST_ADD, true);
		int Limit = NET_MAX_PAYLOAD - 128;
		while((size_t)Client.m_MaplistEntryToSend < m_vMaplistEntries.size())
		{
			// Space for null termination not included in Limit
			const int SizeBefore = Msg.Size();
			Msg.AddString(m_vMaplistEntries[Client.m_MaplistEntryToSend].m_aName, Limit - 1, false);
			if(Msg.Error())
			{
				break;
			}
			Limit -= Msg.Size() - SizeBefore;
			if(Limit <= 1)
			{
				break;
			}
			++Client.m_MaplistEntryToSend;
		}
		SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
	}

	if((size_t)Client.m_MaplistEntryToSend >= m_vMaplistEntries.size())
	{
		SendMaplistGroupEnd(ClientId);
		Client.m_MaplistEntryToSend = CClient::MAPLIST_DONE;
	}
}

static inline int MsgFromSixup(int Msg, bool System)
{
	if(System)
	{
		if(Msg == NETMSG_INFO)
			;
		else if(Msg >= 14 && Msg <= 15)
			Msg += 11;
		else if(Msg >= 18 && Msg <= 28)
			Msg = NETMSG_READY + Msg - 18;
		else if(Msg < OFFSET_UUID)
			return -1;
	}

	return Msg;
}

bool CServer::CheckReservedSlotAuth(int ClientId, const char *pPassword)
{
	char aBuf[256];

	if(Config()->m_SvReservedSlotsPass[0] && !str_comp(Config()->m_SvReservedSlotsPass, pPassword))
	{
		str_format(aBuf, sizeof(aBuf), "cid=%d joining reserved slot with reserved pass", ClientId);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return true;
	}

	// "^([^:]*):(.*)$"
	if(Config()->m_SvReservedSlotsAuthLevel != 4)
	{
		char aName[sizeof(Config()->m_Password)];
		const char *pInnerPassword = str_next_token(pPassword, ":", aName, sizeof(aName));
		if(!pInnerPassword)
		{
			return false;
		}
		int Slot = m_AuthManager.FindKey(aName);
		if(m_AuthManager.CheckKey(Slot, pInnerPassword + 1) && m_AuthManager.KeyLevel(Slot) >= Config()->m_SvReservedSlotsAuthLevel)
		{
			str_format(aBuf, sizeof(aBuf), "cid=%d joining reserved slot with key=%s", ClientId, m_AuthManager.KeyIdent(Slot));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return true;
		}
	}

	return false;
}

void CServer::ProcessClientPacket(CNetChunk *pPacket)
{
	int ClientId = pPacket->m_ClientId;
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX, true);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageId(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(m_aClients[ClientId].m_Sixup && (Msg = MsgFromSixup(Msg, Sys)) < 0)
	{
		return;
	}

	if(Config()->m_SvNetlimit && Msg != NETMSG_REQUEST_MAP_DATA)
	{
		int64_t Now = time_get();
		int64_t Diff = Now - m_aClients[ClientId].m_TrafficSince;
		double Alpha = Config()->m_SvNetlimitAlpha / 100.0;
		double Limit = (double)(Config()->m_SvNetlimit * 1024) / time_freq();

		if(m_aClients[ClientId].m_Traffic > Limit)
		{
			m_NetServer.NetBan()->BanAddr(&pPacket->m_Address, 600, "Stressing network", false);
			return;
		}
		if(Diff > 100)
		{
			m_aClients[ClientId].m_Traffic = (Alpha * ((double)pPacket->m_DataSize / Diff)) + (1.0 - Alpha) * m_aClients[ClientId].m_Traffic;
			m_aClients[ClientId].m_TrafficSince = Now;
		}
	}

	if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(&Packer, MSGFLAG_VITAL, ClientId);
	}

	if(Sys)
	{
		// system message
		if(Msg == NETMSG_CLIENTVER)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientId].m_State == CClient::STATE_PREAUTH)
			{
				CUuid *pConnectionId = (CUuid *)Unpacker.GetRaw(sizeof(*pConnectionId));
				int DDNetVersion = Unpacker.GetInt();
				const char *pDDNetVersionStr = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error() || DDNetVersion < 0)
				{
					return;
				}
				m_aClients[ClientId].m_ConnectionId = *pConnectionId;
				m_aClients[ClientId].m_DDNetVersion = DDNetVersion;
				str_copy(m_aClients[ClientId].m_aDDNetVersionStr, pDDNetVersionStr);
				m_aClients[ClientId].m_DDNetVersionSettled = true;
				m_aClients[ClientId].m_GotDDNetVersionPacket = true;
				m_aClients[ClientId].m_State = CClient::STATE_AUTH;
			}
		}
		else if(Msg == NETMSG_INFO)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientId].m_State == CClient::STATE_PREAUTH || m_aClients[ClientId].m_State == CClient::STATE_AUTH))
			{
				const char *pVersion = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error())
				{
					return;
				}
				if(str_comp(pVersion, GameServer()->NetVersion()) != 0 && str_comp(pVersion, "0.7 802f1be60a05665f") != 0)
				{
					// wrong version
					char aReason[256];
					str_format(aReason, sizeof(aReason), "Wrong version. Server is running '%s' and client '%s'", GameServer()->NetVersion(), pVersion);
					m_NetServer.Drop(ClientId, aReason);
					return;
				}

				const char *pPassword = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error())
				{
					return;
				}
				if(Config()->m_Password[0] != 0 && str_comp(Config()->m_Password, pPassword) != 0)
				{
					// wrong password
					m_NetServer.Drop(ClientId, "Wrong password");
					return;
				}

				// reserved slot
				if(ClientId >= MaxClients() - Config()->m_SvReservedSlots && !CheckReservedSlotAuth(ClientId, pPassword))
				{
					m_NetServer.Drop(ClientId, "This server is full");
					return;
				}

				m_aClients[ClientId].m_State = CClient::STATE_CONNECTING;
				SendRconType(ClientId, m_AuthManager.NumNonDefaultKeys() > 0);
				SendCapabilities(ClientId);
				SendMap(ClientId);
			}
		}
		else if(Msg == NETMSG_REQUEST_MAP_DATA)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) == 0 || m_aClients[ClientId].m_State < CClient::STATE_CONNECTING)
				return;

			if(m_aClients[ClientId].m_Sixup)
			{
				for(int i = 0; i < Config()->m_SvMapWindow; i++)
				{
					SendMapData(ClientId, m_aClients[ClientId].m_NextMapChunk++);
				}
				return;
			}

			int Chunk = Unpacker.GetInt();
			if(Unpacker.Error())
			{
				return;
			}
			if(Chunk != m_aClients[ClientId].m_NextMapChunk || !Config()->m_SvFastDownload)
			{
				SendMapData(ClientId, Chunk);
				return;
			}

			if(Chunk == 0)
			{
				for(int i = 0; i < Config()->m_SvMapWindow; i++)
				{
					SendMapData(ClientId, i);
				}
			}
			SendMapData(ClientId, Config()->m_SvMapWindow + m_aClients[ClientId].m_NextMapChunk);
			m_aClients[ClientId].m_NextMapChunk++;
		}
		else if(Msg == NETMSG_READY)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientId].m_State == CClient::STATE_CONNECTING))
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player is ready. ClientId=%d addr=<{%s}> secure=%s", ClientId, ClientAddrString(ClientId, true), m_NetServer.HasSecurityToken(ClientId) ? "yes" : "no");
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

				void *pPersistentData = nullptr;
				if(m_aClients[ClientId].m_HasPersistentData)
				{
					pPersistentData = m_aClients[ClientId].m_pPersistentData;
					m_aClients[ClientId].m_HasPersistentData = false;
				}
				m_aClients[ClientId].m_State = CClient::STATE_READY;
				GameServer()->OnClientConnected(ClientId, pPersistentData);
			}

			SendConnectionReady(ClientId);
		}
		else if(Msg == NETMSG_ENTERGAME)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientId].m_State == CClient::STATE_READY && GameServer()->IsClientReady(ClientId))
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player has entered the game. ClientId=%d addr=<{%s}> sixup=%d", ClientId, ClientAddrString(ClientId, true), IsSixup(ClientId));
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				m_aClients[ClientId].m_State = CClient::STATE_INGAME;
				if(!IsSixup(ClientId))
				{
					SendServerInfo(ClientAddr(ClientId), -1, SERVERINFO_EXTENDED, false);
				}
				else
				{
					CMsgPacker Msgp(protocol7::NETMSG_SERVERINFO, true, true);
					GetServerInfoSixup(&Msgp, -1, false);
					SendMsg(&Msgp, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientId);
				}
				GameServer()->OnClientEnter(ClientId);
			}
		}
		else if(Msg == NETMSG_INPUT)
		{
			const int LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();
			if(Unpacker.Error() || Size / 4 > MAX_INPUT_SIZE || IntendedTick < MIN_TICK || IntendedTick >= MAX_TICK)
			{
				return;
			}

			m_aClients[ClientId].m_LastAckedSnapshot = LastAckedSnapshot;
			if(m_aClients[ClientId].m_LastAckedSnapshot > 0)
				m_aClients[ClientId].m_SnapRate = CClient::SNAPRATE_FULL;

			int64_t TagTime;
			if(m_aClients[ClientId].m_Snapshots.Get(m_aClients[ClientId].m_LastAckedSnapshot, &TagTime, nullptr, nullptr) >= 0)
				m_aClients[ClientId].m_Latency = (int)(((time_get() - TagTime) * 1000) / time_freq());

			// add message to report the input timing
			// skip packets that are old
			if(IntendedTick > m_aClients[ClientId].m_LastInputTick)
			{
				const int TimeLeft = (TickStartTime(IntendedTick) - time_get()) / (time_freq() / 1000);

				CMsgPacker Msgp(NETMSG_INPUTTIMING, true);
				Msgp.AddInt(IntendedTick);
				Msgp.AddInt(TimeLeft);
				SendMsg(&Msgp, 0, ClientId);
			}

			m_aClients[ClientId].m_LastInputTick = IntendedTick;

			CClient::CInput *pInput = &m_aClients[ClientId].m_aInputs[m_aClients[ClientId].m_CurrentInput];

			if(IntendedTick <= Tick())
				IntendedTick = Tick() + 1;

			pInput->m_GameTick = IntendedTick;

			for(int i = 0; i < Size / 4; i++)
			{
				pInput->m_aData[i] = Unpacker.GetInt();
			}
			if(Unpacker.Error())
			{
				return;
			}

			GameServer()->OnClientPrepareInput(ClientId, pInput->m_aData);
			mem_copy(m_aClients[ClientId].m_LatestInput.m_aData, pInput->m_aData, MAX_INPUT_SIZE * sizeof(int));

			m_aClients[ClientId].m_CurrentInput++;
			m_aClients[ClientId].m_CurrentInput %= 200;

			// call the mod with the fresh input data
			if(m_aClients[ClientId].m_State == CClient::STATE_INGAME)
				GameServer()->OnClientDirectInput(ClientId, m_aClients[ClientId].m_LatestInput.m_aData);
		}
		else if(Msg == NETMSG_RCON_CMD)
		{
			const char *pCmd = Unpacker.GetString();
			if(Unpacker.Error())
			{
				return;
			}
			if(!str_comp(pCmd, "crashmeplx"))
			{
				int Version = m_aClients[ClientId].m_DDNetVersion;
				if(GameServer()->PlayerExists(ClientId) && Version < VERSION_DDNET_OLD)
				{
					m_aClients[ClientId].m_DDNetVersion = VERSION_DDNET_OLD;
				}
			}
			else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientId].m_Authed)
			{
				if(GameServer()->PlayerExists(ClientId))
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientId=%d rcon='%s'", ClientId, pCmd);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					m_RconClientId = ClientId;
					m_RconAuthLevel = m_aClients[ClientId].m_Authed;
					Console()->SetAccessLevel(m_aClients[ClientId].ConsoleAccessLevel());
					{
						CRconClientLogger Logger(this, ClientId);
						CLogScope Scope(&Logger);
						Console()->ExecuteLineFlag(pCmd, CFGFLAG_SERVER, ClientId);
					}
					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
					m_RconClientId = IServer::RCON_CID_SERV;
					m_RconAuthLevel = AUTHED_ADMIN;
				}
			}
		}
		else if(Msg == NETMSG_RCON_AUTH)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) == 0)
			{
				return;
			}
			const char *pName = "";
			if(!IsSixup(ClientId))
			{
				pName = Unpacker.GetString(CUnpacker::SANITIZE_CC); // login name, now used
			}
			const char *pPw = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error())
			{
				return;
			}

			int AuthLevel = -1;
			int KeySlot = -1;

			if(!pName[0])
			{
				if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_ADMIN)), pPw))
					AuthLevel = AUTHED_ADMIN;
				else if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_MOD)), pPw))
					AuthLevel = AUTHED_MOD;
				else if(m_AuthManager.CheckKey((KeySlot = m_AuthManager.DefaultKey(AUTHED_HELPER)), pPw))
					AuthLevel = AUTHED_HELPER;
			}
			else
			{
				KeySlot = m_AuthManager.FindKey(pName);
				if(m_AuthManager.CheckKey(KeySlot, pPw))
					AuthLevel = m_AuthManager.KeyLevel(KeySlot);
			}

			if(AuthLevel != -1)
			{
				if(m_aClients[ClientId].m_Authed != AuthLevel)
				{
					if(!IsSixup(ClientId))
					{
						CMsgPacker Msgp(NETMSG_RCON_AUTH_STATUS, true);
						Msgp.AddInt(1); //authed
						Msgp.AddInt(1); //cmdlist
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientId);
					}
					else
					{
						CMsgPacker Msgp(protocol7::NETMSG_RCON_AUTH_ON, true, true);
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientId);
					}

					m_aClients[ClientId].m_Authed = AuthLevel; // Keeping m_Authed around is unwise...
					m_aClients[ClientId].m_AuthKey = KeySlot;
					int SendRconCmds = IsSixup(ClientId) ? true : Unpacker.GetInt();
					if(!Unpacker.Error() && SendRconCmds)
					{
						m_aClients[ClientId].m_pRconCmdToSend = Console()->FirstCommandInfo(m_aClients[ClientId].ConsoleAccessLevel(), CFGFLAG_SERVER);
						SendRconCmdGroupStart(ClientId);
						if(m_aClients[ClientId].m_pRconCmdToSend == nullptr)
						{
							SendRconCmdGroupEnd(ClientId);
						}
					}

					char aBuf[256];
					const char *pIdent = m_AuthManager.KeyIdent(KeySlot);
					switch(AuthLevel)
					{
					case AUTHED_ADMIN:
					{
						SendRconLine(ClientId, "Admin authentication successful. Full remote console access granted.");
						str_format(aBuf, sizeof(aBuf), "ClientId=%d authed with key=%s (admin)", ClientId, pIdent);
						break;
					}
					case AUTHED_MOD:
					{
						SendRconLine(ClientId, "Moderator authentication successful. Limited remote console access granted.");
						str_format(aBuf, sizeof(aBuf), "ClientId=%d authed with key=%s (moderator)", ClientId, pIdent);
						break;
					}
					case AUTHED_HELPER:
					{
						SendRconLine(ClientId, "Helper authentication successful. Limited remote console access granted.");
						str_format(aBuf, sizeof(aBuf), "ClientId=%d authed with key=%s (helper)", ClientId, pIdent);
						break;
					}
					}
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

					// DDRace
					GameServer()->OnSetAuthed(ClientId, AuthLevel);
				}
			}
			else if(Config()->m_SvRconMaxTries)
			{
				m_aClients[ClientId].m_AuthTries++;
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Wrong password %d/%d.", m_aClients[ClientId].m_AuthTries, Config()->m_SvRconMaxTries);
				SendRconLine(ClientId, aBuf);
				if(m_aClients[ClientId].m_AuthTries >= Config()->m_SvRconMaxTries)
				{
					if(!Config()->m_SvRconBantime)
						m_NetServer.Drop(ClientId, "Too many remote console authentication tries");
					else
						m_ServerBan.BanAddr(ClientAddr(ClientId), Config()->m_SvRconBantime * 60, "Too many remote console authentication tries", false);
				}
			}
			else
			{
				SendRconLine(ClientId, "Wrong password.");
			}
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker Msgp(NETMSG_PING_REPLY, true);
			int Vital = (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 ? MSGFLAG_VITAL : 0;
			SendMsg(&Msgp, MSGFLAG_FLUSH | Vital, ClientId);
		}
		else if(Msg == NETMSG_PINGEX)
		{
			CUuid *pId = (CUuid *)Unpacker.GetRaw(sizeof(*pId));
			if(Unpacker.Error())
			{
				return;
			}
			CMsgPacker Msgp(NETMSG_PONGEX, true);
			Msgp.AddRaw(pId, sizeof(*pId));
			int Vital = (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 ? MSGFLAG_VITAL : 0;
			SendMsg(&Msgp, MSGFLAG_FLUSH | Vital, ClientId);
		}
		else
		{
			if(Config()->m_Debug)
			{
				constexpr int MaxDumpedDataSize = 32;
				char aBuf[MaxDumpedDataSize * 3 + 1];
				str_hex(aBuf, sizeof(aBuf), pPacket->m_pData, minimum(pPacket->m_DataSize, MaxDumpedDataSize));

				char aBufMsg[256];
				str_format(aBufMsg, sizeof(aBufMsg), "strange message ClientId=%d msg=%d data_size=%d", ClientId, Msg, pPacket->m_DataSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBufMsg);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
	}
	else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientId].m_State >= CClient::STATE_READY)
	{
		// game message
		GameServer()->OnMessage(Msg, &Unpacker, ClientId);
	}
}

bool CServer::RateLimitServerInfoConnless()
{
	bool SendClients = true;
	if(Config()->m_SvServerInfoPerSecond)
	{
		SendClients = m_ServerInfoNumRequests <= Config()->m_SvServerInfoPerSecond;
		const int64_t Now = Tick();

		if(Now <= m_ServerInfoFirstRequest + TickSpeed())
		{
			m_ServerInfoNumRequests++;
		}
		else
		{
			m_ServerInfoNumRequests = 1;
			m_ServerInfoFirstRequest = Now;
		}
	}

	return SendClients;
}

void CServer::SendServerInfoConnless(const NETADDR *pAddr, int Token, int Type)
{
	SendServerInfo(pAddr, Token, Type, RateLimitServerInfoConnless());
}

static inline int GetCacheIndex(int Type, bool SendClient)
{
	if(Type == SERVERINFO_INGAME)
		Type = SERVERINFO_VANILLA;
	else if(Type == SERVERINFO_EXTENDED_MORE)
		Type = SERVERINFO_EXTENDED;

	return Type * 2 + SendClient;
}

CServer::CCache::CCache()
{
	m_vCache.clear();
}

CServer::CCache::~CCache()
{
	Clear();
}

CServer::CCache::CCacheChunk::CCacheChunk(const void *pData, int Size)
{
	m_vData.assign((const uint8_t *)pData, (const uint8_t *)pData + Size);
}

void CServer::CCache::AddChunk(const void *pData, int Size)
{
	m_vCache.emplace_back(pData, Size);
}

void CServer::CCache::Clear()
{
	m_vCache.clear();
}

void CServer::CacheServerInfo(CCache *pCache, int Type, bool SendClients)
{
	pCache->Clear();

	// One chance to improve the protocol!
	CPacker p;
	char aBuf[128];

	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].IncludedInServerInfo())
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	p.Reset();

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_format(aBuf, sizeof(aBuf), "%d", x); \
		(p).AddString(aBuf, 0); \
	} while(0)

	p.AddString(GameServer()->Version(), 32);
	if(Type != SERVERINFO_VANILLA)
	{
		p.AddString(Config()->m_SvName, 256);
	}
	else
	{
		if(m_NetServer.MaxClients() <= VANILLA_MAX_CLIENTS)
		{
			p.AddString(Config()->m_SvName, 64);
		}
		else
		{
			const int MaxClients = maximum(ClientCount, m_NetServer.MaxClients() - Config()->m_SvReservedSlots);
			str_format(aBuf, sizeof(aBuf), "%s [%d/%d]", Config()->m_SvName, ClientCount, MaxClients);
			p.AddString(aBuf, 64);
		}
	}
	p.AddString(GetMapName(), 32);

	if(Type == SERVERINFO_EXTENDED)
	{
		ADD_INT(p, m_aCurrentMapCrc[MAP_TYPE_SIX]);
		ADD_INT(p, m_aCurrentMapSize[MAP_TYPE_SIX]);
	}

	// gametype
	p.AddString(GameServer()->GameType(), 16);

	// flags
	ADD_INT(p, Config()->m_Password[0] ? SERVER_FLAG_PASSWORD : 0);

	int MaxClients = m_NetServer.MaxClients();
	// How many clients the used serverinfo protocol supports, has to be tracked
	// separately to make sure we don't subtract the reserved slots from it
	int MaxClientsProtocol = MAX_CLIENTS;
	if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
	{
		if(ClientCount >= VANILLA_MAX_CLIENTS)
		{
			if(ClientCount < MaxClients)
				ClientCount = VANILLA_MAX_CLIENTS - 1;
			else
				ClientCount = VANILLA_MAX_CLIENTS;
		}
		MaxClientsProtocol = VANILLA_MAX_CLIENTS;
		if(PlayerCount > ClientCount)
			PlayerCount = ClientCount;
	}

	ADD_INT(p, PlayerCount); // num players
	ADD_INT(p, minimum(MaxClientsProtocol, maximum(MaxClients - maximum(Config()->m_SvSpectatorSlots, Config()->m_SvReservedSlots), PlayerCount))); // max players
	ADD_INT(p, ClientCount); // num clients
	ADD_INT(p, minimum(MaxClientsProtocol, maximum(MaxClients - Config()->m_SvReservedSlots, ClientCount))); // max clients

	if(Type == SERVERINFO_EXTENDED)
		p.AddString("", 0); // extra info, reserved

	const void *pPrefix = p.Data();
	int PrefixSize = p.Size();

	CPacker q;
	int ChunksStored = 0;
	int PlayersStored = 0;

#define SAVE(size) \
	do \
	{ \
		pCache->AddChunk(q.Data(), size); \
		ChunksStored++; \
	} while(0)

#define RESET() \
	do \
	{ \
		q.Reset(); \
		q.AddRaw(pPrefix, PrefixSize); \
	} while(0)

	RESET();

	if(Type == SERVERINFO_64_LEGACY)
		q.AddInt(PlayersStored); // offset

	if(!SendClients)
	{
		SAVE(q.Size());
		return;
	}

	if(Type == SERVERINFO_EXTENDED)
	{
		pPrefix = "";
		PrefixSize = 0;
	}

	int Remaining;
	switch(Type)
	{
	case SERVERINFO_EXTENDED: Remaining = -1; break;
	case SERVERINFO_64_LEGACY: Remaining = 24; break;
	case SERVERINFO_VANILLA: Remaining = VANILLA_MAX_CLIENTS; break;
	case SERVERINFO_INGAME: Remaining = VANILLA_MAX_CLIENTS; break;
	default: dbg_assert(0, "caught earlier, unreachable"); return;
	}

	// Use the following strategy for sending:
	// For vanilla, send the first 16 players.
	// For legacy 64p, send 24 players per packet.
	// For extended, send as much players as possible.

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].IncludedInServerInfo())
		{
			if(Remaining == 0)
			{
				if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
					break;

				// Otherwise we're SERVERINFO_64_LEGACY.
				SAVE(q.Size());
				RESET();
				q.AddInt(PlayersStored); // offset
				Remaining = 24;
			}
			if(Remaining > 0)
			{
				Remaining--;
			}

			int PreviousSize = q.Size();

			q.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
			q.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan

			ADD_INT(q, m_aClients[i].m_Country); // client country (ISO 3166-1 numeric)

			int Score;
			if(m_aClients[i].m_Score.has_value())
			{
				Score = m_aClients[i].m_Score.value();
				if(Score == 9999)
					Score = -10000;
				else if(Score == 0) // 0 time isn't displayed otherwise.
					Score = -1;
				else
					Score = -Score;
			}
			else
			{
				Score = -9999;
			}

			ADD_INT(q, Score); // client score
			ADD_INT(q, GameServer()->IsClientPlayer(i) ? 1 : 0); // is player?
			if(Type == SERVERINFO_EXTENDED)
				q.AddString("", 0); // extra info, reserved

			if(Type == SERVERINFO_EXTENDED)
			{
				if(q.Size() >= NET_MAX_PAYLOAD - 18) // 8 bytes for type, 10 bytes for the largest token
				{
					// Retry current player.
					i--;
					SAVE(PreviousSize);
					RESET();
					ADD_INT(q, ChunksStored);
					q.AddString("", 0); // extra info, reserved
					continue;
				}
			}
			PlayersStored++;
		}
	}

	SAVE(q.Size());
#undef SAVE
#undef RESET
#undef ADD_RAW
#undef ADD_INT
}

void CServer::CacheServerInfoSixup(CCache *pCache, bool SendClients)
{
	pCache->Clear();

	CPacker Packer;
	Packer.Reset();

	// Could be moved to a separate function and cached
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].IncludedInServerInfo())
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	char aVersion[32];
	str_format(aVersion, sizeof(aVersion), "0.7↔%s", GameServer()->Version());
	Packer.AddString(aVersion, 32);
	Packer.AddString(Config()->m_SvName, 64);
	Packer.AddString(Config()->m_SvHostname, 128);
	Packer.AddString(GetMapName(), 32);

	// gametype
	Packer.AddString(GameServer()->GameType(), 16);

	// flags
	int Flags = SERVER_FLAG_TIMESCORE;
	if(Config()->m_Password[0]) // password set
		Flags |= SERVER_FLAG_PASSWORD;
	Packer.AddInt(Flags);

	int MaxClients = m_NetServer.MaxClients();
	Packer.AddInt(Config()->m_SvSkillLevel); // server skill level
	Packer.AddInt(PlayerCount); // num players
	Packer.AddInt(maximum(MaxClients - maximum(Config()->m_SvSpectatorSlots, Config()->m_SvReservedSlots), PlayerCount)); // max players
	Packer.AddInt(ClientCount); // num clients
	Packer.AddInt(maximum(MaxClients - Config()->m_SvReservedSlots, ClientCount)); // max clients

	if(SendClients)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aClients[i].IncludedInServerInfo())
			{
				Packer.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
				Packer.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan
				Packer.AddInt(m_aClients[i].m_Country); // client country (ISO 3166-1 numeric)
				Packer.AddInt(m_aClients[i].m_Score.value_or(-1)); // client score
				Packer.AddInt(GameServer()->IsClientPlayer(i) ? 0 : 1); // flag spectator=1, bot=2 (player=0)
			}
		}
	}

	pCache->AddChunk(Packer.Data(), Packer.Size());
}

void CServer::SendServerInfo(const NETADDR *pAddr, int Token, int Type, bool SendClients)
{
	CPacker p;
	char aBuf[128];
	p.Reset();

	CCache *pCache = &m_aServerInfoCache[GetCacheIndex(Type, SendClients)];

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_format(aBuf, sizeof(aBuf), "%d", x); \
		(p).AddString(aBuf, 0); \
	} while(0)

	CNetChunk Packet;
	Packet.m_ClientId = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;

	for(const auto &Chunk : pCache->m_vCache)
	{
		p.Reset();
		if(Type == SERVERINFO_EXTENDED)
		{
			if(&Chunk == &pCache->m_vCache.front())
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED));
			else
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED_MORE, sizeof(SERVERBROWSE_INFO_EXTENDED_MORE));
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_64_LEGACY)
		{
			ADD_RAW(p, SERVERBROWSE_INFO_64_LEGACY);
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
		{
			ADD_RAW(p, SERVERBROWSE_INFO);
			ADD_INT(p, Token);
		}
		else
		{
			dbg_assert(false, "unknown serverinfo type");
		}

		p.AddRaw(Chunk.m_vData.data(), Chunk.m_vData.size());
		Packet.m_pData = p.Data();
		Packet.m_DataSize = p.Size();
		m_NetServer.Send(&Packet);
	}
}

void CServer::GetServerInfoSixup(CPacker *pPacker, int Token, bool SendClients)
{
	if(Token != -1)
	{
		pPacker->Reset();
		pPacker->AddRaw(SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO));
		pPacker->AddInt(Token);
	}

	SendClients = SendClients && Token != -1;

	CCache::CCacheChunk &FirstChunk = m_aSixupServerInfoCache[SendClients].m_vCache.front();
	pPacker->AddRaw(FirstChunk.m_vData.data(), FirstChunk.m_vData.size());
}

void CServer::FillAntibot(CAntibotRoundData *pData)
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		CAntibotPlayerData *pPlayer = &pData->m_aPlayers[ClientId];
		if(m_aClients[ClientId].m_State == CServer::CClient::STATE_EMPTY)
		{
			pPlayer->m_aAddress[0] = '\0';
		}
		else
		{
			// No need for expensive str_copy since we don't truncate and the string is
			// ASCII anyway
			static_assert(std::size((CAntibotPlayerData{}).m_aAddress) >= NETADDR_MAXSTRSIZE);
			static_assert(std::is_same_v<decltype(CServer{}.ClientAddrStringImpl(ClientId, true)), const std::array<char, NETADDR_MAXSTRSIZE> &>);
			mem_copy(pPlayer->m_aAddress, ClientAddrStringImpl(ClientId, true).data(), NETADDR_MAXSTRSIZE);
		}
	}
}

void CServer::ExpireServerInfo()
{
	m_ServerInfoNeedsUpdate = true;
}

void CServer::UpdateRegisterServerInfo()
{
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].IncludedInServerInfo())
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	int MaxPlayers = maximum(m_NetServer.MaxClients() - maximum(g_Config.m_SvSpectatorSlots, g_Config.m_SvReservedSlots), PlayerCount);
	int MaxClients = maximum(m_NetServer.MaxClients() - g_Config.m_SvReservedSlots, ClientCount);
	char aMapSha256[SHA256_MAXSTRSIZE];

	sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIX], aMapSha256, sizeof(aMapSha256));

	CJsonStringWriter JsonWriter;

	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("max_clients");
	JsonWriter.WriteIntValue(MaxClients);

	JsonWriter.WriteAttribute("max_players");
	JsonWriter.WriteIntValue(MaxPlayers);

	JsonWriter.WriteAttribute("passworded");
	JsonWriter.WriteBoolValue(g_Config.m_Password[0]);

	JsonWriter.WriteAttribute("game_type");
	JsonWriter.WriteStrValue(GameServer()->GameType());

	JsonWriter.WriteAttribute("name");
	JsonWriter.WriteStrValue(g_Config.m_SvName);

	JsonWriter.WriteAttribute("map");
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("name");
	JsonWriter.WriteStrValue(GetMapName());
	JsonWriter.WriteAttribute("sha256");
	JsonWriter.WriteStrValue(aMapSha256);
	JsonWriter.WriteAttribute("size");
	JsonWriter.WriteIntValue(m_aCurrentMapSize[MAP_TYPE_SIX]);
	JsonWriter.EndObject();

	JsonWriter.WriteAttribute("version");
	JsonWriter.WriteStrValue(GameServer()->Version());

	JsonWriter.WriteAttribute("client_score_kind");
	JsonWriter.WriteStrValue("time"); // "points" or "time"

	JsonWriter.WriteAttribute("requires_login");
	JsonWriter.WriteBoolValue(false);

	JsonWriter.WriteAttribute("clients");
	JsonWriter.BeginArray();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].IncludedInServerInfo())
		{
			JsonWriter.BeginObject();

			JsonWriter.WriteAttribute("name");
			JsonWriter.WriteStrValue(ClientName(i));

			JsonWriter.WriteAttribute("clan");
			JsonWriter.WriteStrValue(ClientClan(i));

			JsonWriter.WriteAttribute("country");
			JsonWriter.WriteIntValue(m_aClients[i].m_Country); // ISO 3166-1 numeric

			JsonWriter.WriteAttribute("score");
			JsonWriter.WriteIntValue(m_aClients[i].m_Score.value_or(-9999));

			JsonWriter.WriteAttribute("is_player");
			JsonWriter.WriteBoolValue(GameServer()->IsClientPlayer(i));

			GameServer()->OnUpdatePlayerServerInfo(&JsonWriter, i);

			JsonWriter.EndObject();
		}
	}

	JsonWriter.EndArray();
	JsonWriter.EndObject();

	m_pRegister->OnNewInfo(JsonWriter.GetOutputString().c_str());
}

void CServer::UpdateServerInfo(bool Resend)
{
	if(m_RunServer == UNINITIALIZED)
		return;

	UpdateRegisterServerInfo();

	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 2; j++)
			CacheServerInfo(&m_aServerInfoCache[i * 2 + j], i, j);

	for(int i = 0; i < 2; i++)
		CacheServerInfoSixup(&m_aSixupServerInfoCache[i], i);

	if(Resend)
	{
		for(int i = 0; i < MaxClients(); ++i)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				if(!IsSixup(i))
					SendServerInfo(ClientAddr(i), -1, SERVERINFO_INGAME, false);
				else
				{
					CMsgPacker Msg(protocol7::NETMSG_SERVERINFO, true, true);
					GetServerInfoSixup(&Msg, -1, false);
					SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, i);
				}
			}
		}
	}

	m_ServerInfoNeedsUpdate = false;
}

void CServer::PumpNetwork(bool PacketWaiting)
{
	CNetChunk Packet;
	SECURITY_TOKEN ResponseToken;

	m_NetServer.Update();

	if(PacketWaiting)
	{
		// process packets
		ResponseToken = NET_SECURITY_TOKEN_UNKNOWN;
		while(m_NetServer.Recv(&Packet, &ResponseToken))
		{
			if(Packet.m_ClientId == -1)
			{
				if(ResponseToken == NET_SECURITY_TOKEN_UNKNOWN && m_pRegister->OnPacket(&Packet))
					continue;

				{
					int ExtraToken = 0;
					int Type = -1;
					if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
					{
						if(Packet.m_Flags & NETSENDFLAG_EXTENDED)
						{
							Type = SERVERINFO_EXTENDED;
							ExtraToken = (Packet.m_aExtraData[0] << 8) | Packet.m_aExtraData[1];
						}
						else
							Type = SERVERINFO_VANILLA;
					}
					else if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO_64_LEGACY) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO_64_LEGACY, sizeof(SERVERBROWSE_GETINFO_64_LEGACY)) == 0)
					{
						Type = SERVERINFO_64_LEGACY;
					}
					if(Type == SERVERINFO_VANILLA && ResponseToken != NET_SECURITY_TOKEN_UNKNOWN && Config()->m_SvSixup)
					{
						CUnpacker Unpacker;
						Unpacker.Reset((unsigned char *)Packet.m_pData + sizeof(SERVERBROWSE_GETINFO), Packet.m_DataSize - sizeof(SERVERBROWSE_GETINFO));
						int SrvBrwsToken = Unpacker.GetInt();
						if(Unpacker.Error())
						{
							continue;
						}

						CPacker Packer;
						GetServerInfoSixup(&Packer, SrvBrwsToken, RateLimitServerInfoConnless());

						CNetChunk Response;
						Response.m_ClientId = -1;
						Response.m_Address = Packet.m_Address;
						Response.m_Flags = NETSENDFLAG_CONNLESS;
						Response.m_pData = Packer.Data();
						Response.m_DataSize = Packer.Size();
						m_NetServer.SendConnlessSixup(&Response, ResponseToken);
					}
					else if(Type != -1)
					{
						int Token = ((unsigned char *)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)];
						Token |= ExtraToken << 8;
						SendServerInfoConnless(&Packet.m_Address, Token, Type);
					}
				}
			}
			else
			{
				if(m_aClients[Packet.m_ClientId].m_State == CClient::STATE_REDIRECTED)
					continue;

				int GameFlags = 0;
				if(Packet.m_Flags & NET_CHUNKFLAG_VITAL)
				{
					GameFlags |= MSGFLAG_VITAL;
				}
				if(Antibot()->OnEngineClientMessage(Packet.m_ClientId, Packet.m_pData, Packet.m_DataSize, GameFlags))
				{
					continue;
				}

				ProcessClientPacket(&Packet);
			}
		}
	}
	{
		unsigned char aBuffer[NET_MAX_PAYLOAD];
		int Flags;
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_pData = aBuffer;
		while(Antibot()->OnEngineSimulateClientMessage(&Packet.m_ClientId, aBuffer, sizeof(aBuffer), &Packet.m_DataSize, &Flags))
		{
			Packet.m_Flags = 0;
			if(Flags & MSGFLAG_VITAL)
			{
				Packet.m_Flags |= NET_CHUNKFLAG_VITAL;
			}
			ProcessClientPacket(&Packet);
		}
	}

	m_ServerBan.Update();
	m_Econ.Update();
}

const char *CServer::GetMapName() const
{
	return m_pCurrentMapName;
}

void CServer::ChangeMap(const char *pMap)
{
	str_copy(Config()->m_SvMap, pMap);
	m_MapReload = str_comp(Config()->m_SvMap, m_aCurrentMap) != 0;
}

void CServer::ReloadMap()
{
	m_SameMapReload = true;
}

int CServer::LoadMap(const char *pMapName)
{
	m_MapReload = false;
	m_SameMapReload = false;

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);
	GameServer()->OnMapChange(aBuf, sizeof(aBuf));

	if(!m_pMap->Load(aBuf))
		return 0;

	// reinit snapshot ids
	m_IdPool.TimeoutIds();

	// get the crc of the map
	m_aCurrentMapSha256[MAP_TYPE_SIX] = m_pMap->Sha256();
	m_aCurrentMapCrc[MAP_TYPE_SIX] = m_pMap->Crc();
	char aBufMsg[256];
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIX], aSha256, sizeof(aSha256));
	str_format(aBufMsg, sizeof(aBufMsg), "%s sha256 is %s", aBuf, aSha256);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);

	str_copy(m_aCurrentMap, pMapName);
	m_pCurrentMapName = fs_filename(m_aCurrentMap);

	// load complete map into memory for download
	{
		free(m_apCurrentMapData[MAP_TYPE_SIX]);
		void *pData;
		Storage()->ReadFile(aBuf, IStorage::TYPE_ALL, &pData, &m_aCurrentMapSize[MAP_TYPE_SIX]);
		m_apCurrentMapData[MAP_TYPE_SIX] = (unsigned char *)pData;
	}

	if(Config()->m_SvMapsBaseUrl[0])
	{
		char aEscaped[256];
		str_format(aBuf, sizeof(aBuf), "%s_%s.map", pMapName, aSha256);
		EscapeUrl(aEscaped, aBuf);
		str_format(m_aMapDownloadUrl, sizeof(m_aMapDownloadUrl), "%s%s", Config()->m_SvMapsBaseUrl, aEscaped);
	}
	else
	{
		m_aMapDownloadUrl[0] = '\0';
	}

	// load sixup version of the map
	if(Config()->m_SvSixup)
	{
		str_format(aBuf, sizeof(aBuf), "maps7/%s.map", pMapName);
		void *pData;
		if(!Storage()->ReadFile(aBuf, IStorage::TYPE_ALL, &pData, &m_aCurrentMapSize[MAP_TYPE_SIXUP]))
		{
			Config()->m_SvSixup = 0;
			if(m_pRegister)
			{
				m_pRegister->OnConfigChange();
			}
			log_error("sixup", "couldn't load map %s", aBuf);
			log_info("sixup", "disabling 0.7 compatibility");
		}
		else
		{
			free(m_apCurrentMapData[MAP_TYPE_SIXUP]);
			m_apCurrentMapData[MAP_TYPE_SIXUP] = (unsigned char *)pData;

			m_aCurrentMapSha256[MAP_TYPE_SIXUP] = sha256(m_apCurrentMapData[MAP_TYPE_SIXUP], m_aCurrentMapSize[MAP_TYPE_SIXUP]);
			m_aCurrentMapCrc[MAP_TYPE_SIXUP] = crc32(0, m_apCurrentMapData[MAP_TYPE_SIXUP], m_aCurrentMapSize[MAP_TYPE_SIXUP]);
			sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIXUP], aSha256, sizeof(aSha256));
			str_format(aBufMsg, sizeof(aBufMsg), "%s sha256 is %s", aBuf, aSha256);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "sixup", aBufMsg);
		}
	}
	if(!Config()->m_SvSixup)
	{
		free(m_apCurrentMapData[MAP_TYPE_SIXUP]);
		m_apCurrentMapData[MAP_TYPE_SIXUP] = nullptr;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPrevStates[i] = m_aClients[i].m_State;

	return 1;
}

#ifdef CONF_DEBUG
void CServer::UpdateDebugDummies(bool ForceDisconnect)
{
	if(m_PreviousDebugDummies == g_Config.m_DbgDummies && !ForceDisconnect)
		return;

	g_Config.m_DbgDummies = clamp(g_Config.m_DbgDummies, 0, MaxClients());
	for(int DummyIndex = 0; DummyIndex < maximum(m_PreviousDebugDummies, g_Config.m_DbgDummies); ++DummyIndex)
	{
		const bool AddDummy = !ForceDisconnect && DummyIndex < g_Config.m_DbgDummies;
		const int ClientId = MaxClients() - DummyIndex - 1;
		CClient &Client = m_aClients[ClientId];
		if(AddDummy && m_aClients[ClientId].m_State == CClient::STATE_EMPTY)
		{
			NewClientCallback(ClientId, this, false);
			Client.m_DebugDummy = true;

			// See https://en.wikipedia.org/wiki/Unique_local_address
			Client.m_DebugDummyAddr.type = NETTYPE_IPV6;
			Client.m_DebugDummyAddr.ip[0] = 0xfd;
			// Global ID (40 bits): random
			secure_random_fill(&Client.m_DebugDummyAddr.ip[1], 5);
			// Subnet ID (16 bits): constant
			Client.m_DebugDummyAddr.ip[6] = 0xc0;
			Client.m_DebugDummyAddr.ip[7] = 0xde;
			// Interface ID (64 bits): set to client ID
			Client.m_DebugDummyAddr.ip[8] = 0x00;
			Client.m_DebugDummyAddr.ip[9] = 0x00;
			Client.m_DebugDummyAddr.ip[10] = 0x00;
			Client.m_DebugDummyAddr.ip[11] = 0x00;
			uint_to_bytes_be(&Client.m_DebugDummyAddr.ip[12], ClientId);
			// Port: random like normal clients
			Client.m_DebugDummyAddr.port = (secure_rand() % (65535 - 1024)) + 1024;
			net_addr_str(&Client.m_DebugDummyAddr, Client.m_aDebugDummyAddrString.data(), Client.m_aDebugDummyAddrString.size(), true);
			net_addr_str(&Client.m_DebugDummyAddr, Client.m_aDebugDummyAddrStringNoPort.data(), Client.m_aDebugDummyAddrStringNoPort.size(), false);

			GameServer()->OnClientConnected(ClientId, nullptr);
			Client.m_State = CClient::STATE_INGAME;
			str_format(Client.m_aName, sizeof(Client.m_aName), "Debug dummy %d", DummyIndex + 1);
			GameServer()->OnClientEnter(ClientId);
		}
		else if(!AddDummy && Client.m_DebugDummy)
		{
			DelClientCallback(ClientId, "Dropping debug dummy", this);
		}

		if(AddDummy && Client.m_DebugDummy)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (ClientId & 1) ? -1 : 1;
			Client.m_aInputs[0].m_GameTick = Tick() + 1;
			mem_copy(Client.m_aInputs[0].m_aData, &Input, minimum(sizeof(Input), sizeof(Client.m_aInputs[0].m_aData)));
			Client.m_LatestInput = Client.m_aInputs[0];
			Client.m_CurrentInput = 0;
		}
	}

	m_PreviousDebugDummies = ForceDisconnect ? 0 : g_Config.m_DbgDummies;
}
#endif

int CServer::Run()
{
	if(m_RunServer == UNINITIALIZED)
		m_RunServer = RUNNING;

	m_AuthManager.Init();

	if(Config()->m_Debug)
	{
		g_UuidManager.DebugDump();
	}

	{
		int Size = GameServer()->PersistentClientDataSize();
		for(auto &Client : m_aClients)
		{
			Client.m_HasPersistentData = false;
			Client.m_pPersistentData = malloc(Size);
		}
	}
	m_pPersistentData = malloc(GameServer()->PersistentDataSize());

	// load map
	if(!LoadMap(Config()->m_SvMap))
	{
		log_error("server", "failed to load map. mapname='%s'", Config()->m_SvMap);
		return -1;
	}

	if(Config()->m_SvSqliteFile[0] != '\0')
	{
		char aFullPath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE_OR_ABSOLUTE, Config()->m_SvSqliteFile, aFullPath, sizeof(aFullPath));

		if(Config()->m_SvUseSql)
		{
			DbPool()->RegisterSqliteDatabase(CDbConnectionPool::WRITE_BACKUP, aFullPath);
		}
		else
		{
			DbPool()->RegisterSqliteDatabase(CDbConnectionPool::READ, aFullPath);
			DbPool()->RegisterSqliteDatabase(CDbConnectionPool::WRITE, aFullPath);
		}
	}

	// start server
	NETADDR BindAddr;
	if(g_Config.m_Bindaddr[0] == '\0')
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
	}
	else if(net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) != 0)
	{
		log_error("server", "The configured bindaddr '%s' cannot be resolved", g_Config.m_Bindaddr);
		return -1;
	}
	BindAddr.type = Config()->m_SvIpv4Only ? NETTYPE_IPV4 : NETTYPE_ALL;

	int Port = Config()->m_SvPort;
	for(BindAddr.port = Port != 0 ? Port : 8303; !m_NetServer.Open(BindAddr, &m_ServerBan, Config()->m_SvMaxClients, Config()->m_SvMaxClientsPerIp); BindAddr.port++)
	{
		if(Port != 0 || BindAddr.port >= 8310)
		{
			log_error("server", "couldn't open socket. port %d might already be in use", BindAddr.port);
			return -1;
		}
	}

	if(Port == 0)
		log_info("server", "using port %d", BindAddr.port);

#if defined(CONF_UPNP)
	m_UPnP.Open(BindAddr);
#endif

	if(!m_Http.Init(std::chrono::seconds{2}))
	{
		log_error("server", "Failed to initialize the HTTP client.");
		return -1;
	}

	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pRegister = CreateRegister(&g_Config, m_pConsole, m_pEngine, &m_Http, this->Port(), m_NetServer.GetGlobalToken());

	m_NetServer.SetCallbacks(NewClientCallback, NewClientNoAuthCallback, ClientRejoinCallback, DelClientCallback, this);

	m_Econ.Init(Config(), Console(), &m_ServerBan);

	m_Fifo.Init(Console(), Config()->m_SvInputFifo, CFGFLAG_SERVER);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", Config()->m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	Antibot()->Init();
	GameServer()->OnInit(nullptr);
	if(ErrorShutdown())
	{
		m_RunServer = STOPPING;
	}
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "version " GAME_RELEASE_VERSION " on " CONF_PLATFORM_STRING " " CONF_ARCH_STRING);
	if(GIT_SHORTREV_HASH)
	{
		str_format(aBuf, sizeof(aBuf), "git revision hash: %s", GIT_SHORTREV_HASH);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}

	ReadAnnouncementsFile();
	InitMaplist();

	// process pending commands
	m_pConsole->StoreCommands(false);
	m_pRegister->OnConfigChange();

	if(m_AuthManager.IsGenerated())
	{
		log_info("server", "+-------------------------+");
		log_info("server", "| rcon password: '%s' |", Config()->m_SvRconPassword);
		log_info("server", "+-------------------------+");
	}

	// start game
	{
		bool NonActive = false;
		bool PacketWaiting = false;

		m_GameStartTime = time_get();

		UpdateServerInfo();
		while(m_RunServer < STOPPING)
		{
			if(NonActive)
				PumpNetwork(PacketWaiting);

			set_new_tick();

			int64_t t = time_get();
			int NewTicks = 0;

			// load new map
			if(m_MapReload || m_SameMapReload || m_CurrentGameTick >= MAX_TICK) // force reload to make sure the ticks stay within a valid range
			{
				const bool SameMapReload = m_SameMapReload;
				// load map
				if(LoadMap(Config()->m_SvMap))
				{
					// new map loaded

					// ask the game for the data it wants to persist past a map change
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(m_aClients[i].m_State == CClient::STATE_INGAME)
						{
							m_aClients[i].m_HasPersistentData = GameServer()->OnClientDataPersist(i, m_aClients[i].m_pPersistentData);
						}
					}

#ifdef CONF_DEBUG
					UpdateDebugDummies(true);
#endif
					GameServer()->OnShutdown(m_pPersistentData);

					for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
					{
						if(m_aClients[ClientId].m_State <= CClient::STATE_AUTH)
							continue;

						if(SameMapReload)
							SendMapReload(ClientId);

						SendMap(ClientId);
						bool HasPersistentData = m_aClients[ClientId].m_HasPersistentData;
						m_aClients[ClientId].Reset();
						m_aClients[ClientId].m_HasPersistentData = HasPersistentData;
						m_aClients[ClientId].m_State = CClient::STATE_CONNECTING;
					}

					m_GameStartTime = time_get();
					m_CurrentGameTick = MIN_TICK;
					m_ServerInfoFirstRequest = 0;
					Kernel()->ReregisterInterface(GameServer());
					Console()->StoreCommands(true);
					GameServer()->OnInit(m_pPersistentData);
					Console()->StoreCommands(false);
					if(ErrorShutdown())
					{
						break;
					}
					UpdateServerInfo(true);
					for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
					{
						if(m_aClients[ClientId].m_State != CClient::STATE_CONNECTING)
							continue;

						// When doing a map change, a new Teehistorian file is created. For players that are already
						// on the server, no PlayerJoin event is produced in Teehistorian from the network engine.
						// Record PlayerJoin events here to record the Sixup version and player join event.
						GameServer()->TeehistorianRecordPlayerJoin(ClientId, m_aClients[ClientId].m_Sixup);
					}
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "failed to load map. mapname='%s'", Config()->m_SvMap);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					str_copy(Config()->m_SvMap, m_aCurrentMap);
				}
			}

			while(t > TickStartTime(m_CurrentGameTick + 1))
			{
				GameServer()->OnPreTickTeehistorian();

#ifdef CONF_DEBUG
				UpdateDebugDummies(false);
#endif

				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State != CClient::STATE_INGAME)
						continue;
					bool ClientHadInput = false;
					for(auto &Input : m_aClients[c].m_aInputs)
					{
						if(Input.m_GameTick == Tick() + 1)
						{
							GameServer()->OnClientPredictedEarlyInput(c, Input.m_aData);
							ClientHadInput = true;
						}
					}
					if(!ClientHadInput)
						GameServer()->OnClientPredictedEarlyInput(c, nullptr);
				}

				m_CurrentGameTick++;
				NewTicks++;

				// apply new input
				for(int c = 0; c < MAX_CLIENTS; c++)
				{
					if(m_aClients[c].m_State != CClient::STATE_INGAME)
						continue;
					bool ClientHadInput = false;
					for(auto &Input : m_aClients[c].m_aInputs)
					{
						if(Input.m_GameTick == Tick())
						{
							GameServer()->OnClientPredictedInput(c, Input.m_aData);
							ClientHadInput = true;
							break;
						}
					}
					if(!ClientHadInput)
						GameServer()->OnClientPredictedInput(c, nullptr);
				}

				GameServer()->OnTick();
				if(ErrorShutdown())
				{
					break;
				}
			}

			// snap game
			if(NewTicks)
			{
				if(Config()->m_SvHighBandwidth || (m_CurrentGameTick % 2) == 0)
					DoSnapshot();

				const int CommandSendingClientId = Tick() % MAX_CLIENTS;
				UpdateClientRconCommands(CommandSendingClientId);
				UpdateClientMaplistEntries(CommandSendingClientId);

				m_Fifo.Update();

#if defined(CONF_PLATFORM_ANDROID)
				std::vector<std::string> vAndroidCommandQueue = FetchAndroidServerCommandQueue();
				for(const std::string &Command : vAndroidCommandQueue)
				{
					Console()->ExecuteLineFlag(Command.c_str(), CFGFLAG_SERVER, -1);
				}
#endif

				// master server stuff
				m_pRegister->Update();

				if(m_ServerInfoNeedsUpdate)
					UpdateServerInfo();

				Antibot()->OnEngineTick();

				// handle dnsbl
				if(Config()->m_SvDnsbl)
				{
					for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
					{
						if(m_aClients[ClientId].m_State == CClient::STATE_EMPTY)
							continue;

						if(m_aClients[ClientId].m_DnsblState == CClient::DNSBL_STATE_NONE)
						{
							// initiate dnsbl lookup
							InitDnsbl(ClientId);
						}
						else if(m_aClients[ClientId].m_DnsblState == CClient::DNSBL_STATE_PENDING &&
							m_aClients[ClientId].m_pDnsblLookup->State() == IJob::STATE_DONE)
						{
							if(m_aClients[ClientId].m_pDnsblLookup->Result() != 0)
							{
								// entry not found -> whitelisted
								m_aClients[ClientId].m_DnsblState = CClient::DNSBL_STATE_WHITELISTED;

								str_format(aBuf, sizeof(aBuf), "ClientId=%d addr=<{%s}> secure=%s whitelisted", ClientId, ClientAddrString(ClientId, true), m_NetServer.HasSecurityToken(ClientId) ? "yes" : "no");
								Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", aBuf);
							}
							else
							{
								// entry found -> blacklisted
								m_aClients[ClientId].m_DnsblState = CClient::DNSBL_STATE_BLACKLISTED;

								str_format(aBuf, sizeof(aBuf), "ClientId=%d addr=<{%s}> secure=%s blacklisted", ClientId, ClientAddrString(ClientId, true), m_NetServer.HasSecurityToken(ClientId) ? "yes" : "no");
								Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dnsbl", aBuf);

								if(Config()->m_SvDnsblBan)
								{
									m_NetServer.NetBan()->BanAddr(ClientAddr(ClientId), 60, Config()->m_SvDnsblBanReason, true);
								}
							}
						}
					}
				}
				for(int i = 0; i < MAX_CLIENTS; ++i)
				{
					if(m_aClients[i].m_State == CClient::STATE_REDIRECTED)
					{
						if(time_get() > m_aClients[i].m_RedirectDropTime)
						{
							m_NetServer.Drop(i, "redirected");
						}
					}
				}
			}

			if(!NonActive)
				PumpNetwork(PacketWaiting);

			NonActive = true;
			for(const auto &Client : m_aClients)
			{
				if(Client.m_State != CClient::STATE_EMPTY)
				{
					NonActive = false;
					break;
				}
			}

			// wait for incoming data
			if(NonActive)
			{
				if(Config()->m_SvReloadWhenEmpty == 1)
				{
					m_MapReload = true;
					Config()->m_SvReloadWhenEmpty = 0;
				}
				else if(Config()->m_SvReloadWhenEmpty == 2 && !m_ReloadedWhenEmpty)
				{
					m_MapReload = true;
					m_ReloadedWhenEmpty = true;
				}

				if(Config()->m_SvShutdownWhenEmpty)
					m_RunServer = STOPPING;
				else
					PacketWaiting = net_socket_read_wait(m_NetServer.Socket(), 1000000);
			}
			else
			{
				m_ReloadedWhenEmpty = false;

				set_new_tick();
				t = time_get();
				int x = (TickStartTime(m_CurrentGameTick + 1) - t) * 1000000 / time_freq() + 1;

				PacketWaiting = x > 0 ? net_socket_read_wait(m_NetServer.Socket(), x) : true;
			}
			if(IsInterrupted())
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "interrupted");
				break;
			}
		}
	}
	const char *pDisconnectReason = "Server shutdown";
	if(m_aShutdownReason[0])
		pDisconnectReason = m_aShutdownReason;

	if(ErrorShutdown())
	{
		log_info("server", "shutdown from game server (%s)", m_aErrorShutdownReason);
		pDisconnectReason = m_aErrorShutdownReason;
	}
	// disconnect all clients on shutdown
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			m_NetServer.Drop(i, pDisconnectReason);
	}

	m_pRegister->OnShutdown();
	m_Econ.Shutdown();
	m_Fifo.Shutdown();
	Engine()->ShutdownJobs();

	GameServer()->OnShutdown(nullptr);
	m_pMap->Unload();
	DbPool()->OnShutdown();

#if defined(CONF_UPNP)
	m_UPnP.Shutdown();
#endif
	m_NetServer.Close();

	return ErrorShutdown();
}

void CServer::ConKick(IConsole::IResult *pResult, void *pUser)
{
	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pResult->GetString(1));
		((CServer *)pUser)->Kick(pResult->GetInteger(0), aBuf);
	}
	else
		((CServer *)pUser)->Kick(pResult->GetInteger(0), "Kicked by console");
}

void CServer::ConStatus(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[1024];
	CServer *pThis = static_cast<CServer *>(pUser);
	const char *pName = pResult->NumArguments() == 1 ? pResult->GetString(0) : "";

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State == CClient::STATE_EMPTY)
			continue;

		if(!str_utf8_find_nocase(pThis->m_aClients[i].m_aName, pName))
			continue;

		if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
		{
			char aDnsblStr[64];
			aDnsblStr[0] = '\0';
			if(pThis->Config()->m_SvDnsbl)
			{
				const char *pDnsblStr = pThis->m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_WHITELISTED ? "white" :
																pThis->m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_BLACKLISTED ? "black" :
																									pThis->m_aClients[i].m_DnsblState == CClient::DNSBL_STATE_PENDING ? "pending" : "n/a";

				str_format(aDnsblStr, sizeof(aDnsblStr), " dnsbl=%s", pDnsblStr);
			}

			char aAuthStr[128];
			aAuthStr[0] = '\0';
			if(pThis->m_aClients[i].m_AuthKey >= 0)
			{
				const char *pAuthStr = pThis->m_aClients[i].m_Authed == AUTHED_ADMIN ? "(Admin)" :
												       pThis->m_aClients[i].m_Authed == AUTHED_MOD ? "(Mod)" :
																		     pThis->m_aClients[i].m_Authed == AUTHED_HELPER ? "(Helper)" : "";

				str_format(aAuthStr, sizeof(aAuthStr), " key=%s %s", pThis->m_AuthManager.KeyIdent(pThis->m_aClients[i].m_AuthKey), pAuthStr);
			}

			const char *pClientPrefix = "";
			if(pThis->m_aClients[i].m_Sixup)
			{
				pClientPrefix = "0.7:";
			}
			str_format(aBuf, sizeof(aBuf), "id=%d addr=<{%s}> name='%s' client=%s%d secure=%s flags=%d%s%s",
				i, pThis->ClientAddrString(i, true), pThis->m_aClients[i].m_aName, pClientPrefix, pThis->m_aClients[i].m_DDNetVersion,
				pThis->m_NetServer.HasSecurityToken(i) ? "yes" : "no", pThis->m_aClients[i].m_Flags, aDnsblStr, aAuthStr);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "id=%d addr=<{%s}> connecting", i, pThis->ClientAddrString(i, true));
		}
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

static int GetAuthLevel(const char *pLevel)
{
	int Level = -1;
	if(!str_comp_nocase(pLevel, "admin"))
		Level = AUTHED_ADMIN;
	else if(str_startswith(pLevel, "mod"))
		Level = AUTHED_MOD;
	else if(!str_comp_nocase(pLevel, "helper"))
		Level = AUTHED_HELPER;

	return Level;
}

void CServer::AuthRemoveKey(int KeySlot)
{
	m_AuthManager.RemoveKey(KeySlot);
	LogoutKey(KeySlot, "key removal");

	// Update indices.
	for(auto &Client : m_aClients)
	{
		if(Client.m_AuthKey == KeySlot)
		{
			Client.m_AuthKey = -1;
		}
		else if(Client.m_AuthKey > KeySlot)
		{
			--Client.m_AuthKey;
		}
	}
}

void CServer::ConAuthAdd(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	bool NeedUpdate = !pManager->NumNonDefaultKeys();
	if(pManager->AddKey(pIdent, pPw, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
	{
		if(NeedUpdate)
			pThis->SendRconType(-1, true);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key added");
	}
}

void CServer::ConAuthAddHashed(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);
	const char *pSalt = pResult->GetString(3);

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	MD5_DIGEST Hash;
	unsigned char aSalt[SALT_BYTES];

	if(md5_from_str(&Hash, pPw))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed password hash");
		return;
	}
	if(str_hex_decode(aSalt, sizeof(aSalt), pSalt))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed salt hash");
		return;
	}

	bool NeedUpdate = !pManager->NumNonDefaultKeys();

	if(pManager->AddKeyHash(pIdent, Hash, aSalt, Level) < 0)
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident already exists");
	else
	{
		if(NeedUpdate)
			pThis->SendRconType(-1, true);
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key added");
	}
}

void CServer::ConAuthUpdate(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	pManager->UpdateKey(KeySlot, pPw, Level);
	pThis->LogoutKey(KeySlot, "key update");

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key updated");
}

void CServer::ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);
	const char *pLevel = pResult->GetString(1);
	const char *pPw = pResult->GetString(2);
	const char *pSalt = pResult->GetString(3);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	int Level = GetAuthLevel(pLevel);
	if(Level == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "level can be one of {\"admin\", \"mod(erator)\", \"helper\"}");
		return;
	}

	MD5_DIGEST Hash;
	unsigned char aSalt[SALT_BYTES];

	if(md5_from_str(&Hash, pPw))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed password hash");
		return;
	}
	if(str_hex_decode(aSalt, sizeof(aSalt), pSalt))
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "Malformed salt hash");
		return;
	}

	pManager->UpdateKeyHash(KeySlot, Hash, aSalt, Level);
	pThis->LogoutKey(KeySlot, "key update");

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key updated");
}

void CServer::ConAuthRemove(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	const char *pIdent = pResult->GetString(0);

	int KeySlot = pManager->FindKey(pIdent);
	if(KeySlot == -1)
	{
		pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "ident couldn't be found");
		return;
	}

	pThis->AuthRemoveKey(KeySlot);

	if(!pManager->NumNonDefaultKeys())
		pThis->SendRconType(-1, false);

	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", "key removed, all users logged out");
}

static void ListKeysCallback(const char *pIdent, int Level, void *pUser)
{
	static const char LSTRING[][10] = {"helper", "moderator", "admin"};

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s %s", pIdent, LSTRING[Level - 1]);
	((CServer *)pUser)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "auth", aBuf);
}

void CServer::ConAuthList(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	CAuthManager *pManager = &pThis->m_AuthManager;

	pManager->ListKeys(ListKeysCallback, pThis);
}

void CServer::ConShutdown(IConsole::IResult *pResult, void *pUser)
{
	CServer *pThis = static_cast<CServer *>(pUser);
	pThis->m_RunServer = STOPPING;
	const char *pReason = pResult->GetString(0);
	if(pReason[0])
	{
		str_copy(pThis->m_aShutdownReason, pReason);
	}
}

void CServer::DemoRecorder_HandleAutoStart()
{
	if(Config()->m_SvAutoDemoRecord)
	{
		m_aDemoRecorder[RECORDER_AUTO].Stop(IDemoRecorder::EStopMode::KEEP_FILE);

		char aTimestamp[20];
		str_timestamp(aTimestamp, sizeof(aTimestamp));
		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "demos/auto/server/%s_%s.demo", GetMapName(), aTimestamp);
		m_aDemoRecorder[RECORDER_AUTO].Start(
			Storage(),
			m_pConsole,
			aFilename,
			GameServer()->NetVersion(),
			GetMapName(),
			m_aCurrentMapSha256[MAP_TYPE_SIX],
			m_aCurrentMapCrc[MAP_TYPE_SIX],
			"server",
			m_aCurrentMapSize[MAP_TYPE_SIX],
			m_apCurrentMapData[MAP_TYPE_SIX],
			nullptr,
			nullptr,
			nullptr);

		if(Config()->m_SvAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/auto/server", "", ".demo", Config()->m_SvAutoDemoMax);
		}
	}
}

void CServer::SaveDemo(int ClientId, float Time)
{
	if(IsRecording(ClientId))
	{
		char aNewFilename[IO_MAX_PATH_LENGTH];
		str_format(aNewFilename, sizeof(aNewFilename), "demos/%s_%s_%05.2f.demo", GetMapName(), m_aClients[ClientId].m_aName, Time);
		m_aDemoRecorder[ClientId].Stop(IDemoRecorder::EStopMode::KEEP_FILE, aNewFilename);
	}
}

void CServer::StartRecord(int ClientId)
{
	if(Config()->m_SvPlayerDemoRecord)
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "demos/%s_%d_%d_tmp.demo", GetMapName(), m_NetServer.Address().port, ClientId);
		m_aDemoRecorder[ClientId].Start(
			Storage(),
			Console(),
			aFilename,
			GameServer()->NetVersion(),
			GetMapName(),
			m_aCurrentMapSha256[MAP_TYPE_SIX],
			m_aCurrentMapCrc[MAP_TYPE_SIX],
			"server",
			m_aCurrentMapSize[MAP_TYPE_SIX],
			m_apCurrentMapData[MAP_TYPE_SIX],
			nullptr,
			nullptr,
			nullptr);
	}
}

void CServer::StopRecord(int ClientId)
{
	if(IsRecording(ClientId))
	{
		m_aDemoRecorder[ClientId].Stop(IDemoRecorder::EStopMode::REMOVE_FILE);
	}
}

bool CServer::IsRecording(int ClientId)
{
	return m_aDemoRecorder[ClientId].IsRecording();
}

void CServer::StopDemos()
{
	for(int i = 0; i < NUM_RECORDERS; i++)
	{
		if(!m_aDemoRecorder[i].IsRecording())
			continue;

		m_aDemoRecorder[i].Stop(i < MAX_CLIENTS ? IDemoRecorder::EStopMode::REMOVE_FILE : IDemoRecorder::EStopMode::KEEP_FILE);
	}
}

void CServer::ConRecord(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->IsRecording(RECORDER_MANUAL))
	{
		pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Demo recorder already recording");
		return;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	if(pResult->NumArguments())
	{
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	}
	else
	{
		char aTimestamp[20];
		str_timestamp(aTimestamp, sizeof(aTimestamp));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aTimestamp);
	}
	pServer->m_aDemoRecorder[RECORDER_MANUAL].Start(
		pServer->Storage(),
		pServer->Console(),
		aFilename,
		pServer->GameServer()->NetVersion(),
		pServer->GetMapName(),
		pServer->m_aCurrentMapSha256[MAP_TYPE_SIX],
		pServer->m_aCurrentMapCrc[MAP_TYPE_SIX],
		"server",
		pServer->m_aCurrentMapSize[MAP_TYPE_SIX],
		pServer->m_apCurrentMapData[MAP_TYPE_SIX],
		nullptr,
		nullptr,
		nullptr);
}

void CServer::ConStopRecord(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_aDemoRecorder[RECORDER_MANUAL].Stop(IDemoRecorder::EStopMode::KEEP_FILE);
}

void CServer::ConMapReload(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->ReloadMap();
}

void CServer::ConLogout(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->m_RconClientId >= 0 && pServer->m_RconClientId < MAX_CLIENTS &&
		pServer->m_aClients[pServer->m_RconClientId].m_State != CServer::CClient::STATE_EMPTY)
	{
		pServer->LogoutClient(pServer->m_RconClientId, "");
	}
}

void CServer::ConShowIps(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->m_RconClientId >= 0 && pServer->m_RconClientId < MAX_CLIENTS &&
		pServer->m_aClients[pServer->m_RconClientId].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(pResult->NumArguments())
		{
			pServer->m_aClients[pServer->m_RconClientId].m_ShowIps = pResult->GetInteger(0);
		}
		else
		{
			char aStr[9];
			str_format(aStr, sizeof(aStr), "Value: %d", pServer->m_aClients[pServer->m_RconClientId].m_ShowIps);
			pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aStr);
		}
	}
}

void CServer::ConAddSqlServer(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pSelf = (CServer *)pUserData;

	if(!MysqlAvailable())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "can't add MySQL server: compiled without MySQL support");
		return;
	}

	if(!pSelf->Config()->m_SvUseSql)
		return;

	if(pResult->NumArguments() != 7 && pResult->NumArguments() != 8)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "7 or 8 arguments are required");
		return;
	}

	CMysqlConfig Config;
	bool Write;
	if(str_comp_nocase(pResult->GetString(0), "w") == 0)
		Write = false;
	else if(str_comp_nocase(pResult->GetString(0), "r") == 0)
		Write = true;
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "choose either 'r' for SqlReadServer or 'w' for SqlWriteServer");
		return;
	}

	str_copy(Config.m_aDatabase, pResult->GetString(1), sizeof(Config.m_aDatabase));
	str_copy(Config.m_aPrefix, pResult->GetString(2), sizeof(Config.m_aPrefix));
	str_copy(Config.m_aUser, pResult->GetString(3), sizeof(Config.m_aUser));
	str_copy(Config.m_aPass, pResult->GetString(4), sizeof(Config.m_aPass));
	str_copy(Config.m_aIp, pResult->GetString(5), sizeof(Config.m_aIp));
	Config.m_aBindaddr[0] = '\0';
	Config.m_Port = pResult->GetInteger(6);
	Config.m_Setup = pResult->NumArguments() == 8 ? pResult->GetInteger(7) : true;

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"Adding new Sql%sServer: DB: '%s' Prefix: '%s' User: '%s' IP: <{%s}> Port: %d",
		Write ? "Write" : "Read",
		Config.m_aDatabase, Config.m_aPrefix, Config.m_aUser, Config.m_aIp, Config.m_Port);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	pSelf->DbPool()->RegisterMysqlDatabase(Write ? CDbConnectionPool::WRITE : CDbConnectionPool::READ, &Config);
}

void CServer::ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pSelf = (CServer *)pUserData;

	if(str_comp_nocase(pResult->GetString(0), "w") == 0)
	{
		pSelf->DbPool()->Print(pSelf->Console(), CDbConnectionPool::WRITE);
		pSelf->DbPool()->Print(pSelf->Console(), CDbConnectionPool::WRITE_BACKUP);
	}
	else if(str_comp_nocase(pResult->GetString(0), "r") == 0)
	{
		pSelf->DbPool()->Print(pSelf->Console(), CDbConnectionPool::READ);
	}
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "choose either 'r' for SqlReadServer or 'w' for SqlWriteServer");
		return;
	}
}

void CServer::ConReloadAnnouncement(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pThis = static_cast<CServer *>(pUserData);
	pThis->ReadAnnouncementsFile();
}

void CServer::ConReloadMaplist(IConsole::IResult *pResult, void *pUserData)
{
	CServer *pThis = static_cast<CServer *>(pUserData);
	pThis->InitMaplist();
}

void CServer::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		str_clean_whitespaces(pThis->Config()->m_SvName);
		pThis->UpdateServerInfo(true);
	}
}

void CServer::ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->m_NetServer.SetMaxClientsPerIp(pResult->GetInteger(0));
}

void CServer::ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	if(pResult->NumArguments() == 2)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		const IConsole::CCommandInfo *pInfo = pThis->Console()->GetCommandInfo(pResult->GetString(0), CFGFLAG_SERVER, false);
		int OldAccessLevel = 0;
		if(pInfo)
			OldAccessLevel = pInfo->GetAccessLevel();
		pfnCallback(pResult, pCallbackUserData);
		if(pInfo && OldAccessLevel != pInfo->GetAccessLevel())
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(pThis->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
					continue;
				const int ClientAccessLevel = pThis->m_aClients[i].ConsoleAccessLevel();
				if((pInfo->GetAccessLevel() > ClientAccessLevel && ClientAccessLevel < OldAccessLevel) ||
					(pInfo->GetAccessLevel() < ClientAccessLevel && ClientAccessLevel > OldAccessLevel) ||
					(pThis->m_aClients[i].m_pRconCmdToSend && str_comp(pResult->GetString(0), pThis->m_aClients[i].m_pRconCmdToSend->m_pName) >= 0))
					continue;

				if(OldAccessLevel < pInfo->GetAccessLevel())
					pThis->SendRconCmdAdd(pInfo, i);
				else
					pThis->SendRconCmdRem(pInfo, i);
			}
		}
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CServer::LogoutClient(int ClientId, const char *pReason)
{
	if(!IsSixup(ClientId))
	{
		CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS, true);
		Msg.AddInt(0); //authed
		Msg.AddInt(0); //cmdlist
		SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
	}
	else
	{
		CMsgPacker Msg(protocol7::NETMSG_RCON_AUTH_OFF, true, true);
		SendMsg(&Msg, MSGFLAG_VITAL, ClientId);
	}

	m_aClients[ClientId].m_AuthTries = 0;
	m_aClients[ClientId].m_pRconCmdToSend = nullptr;
	m_aClients[ClientId].m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;

	char aBuf[64];
	if(*pReason)
	{
		str_format(aBuf, sizeof(aBuf), "Logged out by %s.", pReason);
		SendRconLine(ClientId, aBuf);
		str_format(aBuf, sizeof(aBuf), "ClientId=%d with key=%s logged out by %s", ClientId, m_AuthManager.KeyIdent(m_aClients[ClientId].m_AuthKey), pReason);
	}
	else
	{
		SendRconLine(ClientId, "Logout successful.");
		str_format(aBuf, sizeof(aBuf), "ClientId=%d with key=%s logged out", ClientId, m_AuthManager.KeyIdent(m_aClients[ClientId].m_AuthKey));
	}

	m_aClients[ClientId].m_Authed = AUTHED_NO;
	m_aClients[ClientId].m_AuthKey = -1;

	GameServer()->OnSetAuthed(ClientId, AUTHED_NO);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CServer::LogoutKey(int Key, const char *pReason)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(m_aClients[i].m_AuthKey == Key)
			LogoutClient(i, pReason);
}

void CServer::ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult)
{
	if(pResult->NumArguments() == 1)
	{
		int KeySlot = m_AuthManager.DefaultKey(Level);
		const char *pNew = pResult->GetString(0);
		if(str_comp(pCurrent, pNew) == 0)
		{
			return;
		}
		if(KeySlot == -1 && pNew[0])
		{
			m_AuthManager.AddDefaultKey(Level, pNew);
		}
		else if(KeySlot >= 0)
		{
			if(!pNew[0])
			{
				AuthRemoveKey(KeySlot);
				// Already logs users out.
			}
			else
			{
				m_AuthManager.UpdateKey(KeySlot, pNew, Level);
				LogoutKey(KeySlot, "key update");
			}
		}
	}
}

void CServer::ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pThis = static_cast<CServer *>(pUserData);
	pThis->ConchainRconPasswordChangeGeneric(AUTHED_ADMIN, pThis->Config()->m_SvRconPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pThis = static_cast<CServer *>(pUserData);
	pThis->ConchainRconPasswordChangeGeneric(AUTHED_MOD, pThis->Config()->m_SvRconModPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pThis = static_cast<CServer *>(pUserData);
	pThis->ConchainRconPasswordChangeGeneric(AUTHED_HELPER, pThis->Config()->m_SvRconHelperPassword, pResult);
	pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->m_MapReload = str_comp(pThis->Config()->m_SvMap, pThis->m_aCurrentMap) != 0;
	}
}

void CServer::ConchainSixupUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CServer *pThis = static_cast<CServer *>(pUserData);
	if(pResult->NumArguments() >= 1 && pThis->m_aCurrentMap[0] != '\0')
		pThis->m_MapReload |= (pThis->m_apCurrentMapData[MAP_TYPE_SIXUP] != nullptr) != (pResult->GetInteger(0) != 0);
}

void CServer::ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pSelf = (CServer *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		pSelf->m_pFileLogger->SetFilter(CLogFilter{IConsole::ToLogLevelFilter(g_Config.m_Loglevel)});
	}
}

void CServer::ConchainStdoutOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pSelf = (CServer *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && pSelf->m_pStdoutLogger)
	{
		pSelf->m_pStdoutLogger->SetFilter(CLogFilter{IConsole::ToLogLevelFilter(g_Config.m_StdoutOutputLevel)});
	}
}

void CServer::ConchainAnnouncementFileName(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CServer *pSelf = (CServer *)pUserData;
	bool Changed = pResult->NumArguments() && str_comp(pResult->GetString(0), g_Config.m_SvAnnouncementFileName);
	pfnCallback(pResult, pCallbackUserData);
	if(Changed)
	{
		pSelf->ReadAnnouncementsFile();
	}
}

#if defined(CONF_FAMILY_UNIX)
void CServer::ConchainConnLoggingServerChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pServer = (CServer *)pUserData;

		// open socket to send new connections
		if(!pServer->m_ConnLoggingSocketCreated)
		{
			pServer->m_ConnLoggingSocket = net_unix_create_unnamed();
			if(pServer->m_ConnLoggingSocket == -1)
			{
				pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Failed to created socket for communication with the connection logging server.");
			}
			else
			{
				pServer->m_ConnLoggingSocketCreated = true;
			}
		}

		// set the destination address for the connection logging
		net_unix_set_addr(&pServer->m_ConnLoggingDestAddr, pResult->GetString(0));
	}
}
#endif

void CServer::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGameServer = Kernel()->RequestInterface<IGameServer>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pAntibot = Kernel()->RequestInterface<IEngineAntibot>();

	Kernel()->RegisterInterface(static_cast<IHttp *>(&m_Http), false);

	// register console commands
	Console()->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason");
	Console()->Register("status", "?r[name]", CFGFLAG_SERVER, ConStatus, this, "List players containing name or all players");
	Console()->Register("shutdown", "?r[reason]", CFGFLAG_SERVER, ConShutdown, this, "Shut down");
	Console()->Register("logout", "", CFGFLAG_SERVER, ConLogout, this, "Logout of rcon");
	Console()->Register("show_ips", "?i[show]", CFGFLAG_SERVER, ConShowIps, this, "Show IP addresses in rcon commands (1 = on, 0 = off)");

	Console()->Register("record", "?s[file]", CFGFLAG_SERVER | CFGFLAG_STORE, ConRecord, this, "Record to a file");
	Console()->Register("stoprecord", "", CFGFLAG_SERVER, ConStopRecord, this, "Stop recording");

	Console()->Register("reload", "", CFGFLAG_SERVER, ConMapReload, this, "Reload the map");

	Console()->Register("add_sqlserver", "s['r'|'w'] s[Database] s[Prefix] s[User] s[Password] s[IP] i[Port] ?i[SetUpDatabase ?]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAddSqlServer, this, "add a sqlserver");
	Console()->Register("dump_sqlservers", "s['r'|'w']", CFGFLAG_SERVER, ConDumpSqlServers, this, "dumps all sqlservers readservers = r, writeservers = w");

	Console()->Register("auth_add", "s[ident] s[level] r[pw]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAuthAdd, this, "Add a rcon key");
	Console()->Register("auth_add_p", "s[ident] s[level] s[hash] s[salt]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAuthAddHashed, this, "Add a prehashed rcon key");
	Console()->Register("auth_change", "s[ident] s[level] r[pw]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAuthUpdate, this, "Update a rcon key");
	Console()->Register("auth_change_p", "s[ident] s[level] s[hash] s[salt]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAuthUpdateHashed, this, "Update a rcon key with prehashed data");
	Console()->Register("auth_remove", "s[ident]", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, ConAuthRemove, this, "Remove a rcon key");
	Console()->Register("auth_list", "", CFGFLAG_SERVER, ConAuthList, this, "List all rcon keys");

	Console()->Register("reload_announcement", "", CFGFLAG_SERVER, ConReloadAnnouncement, this, "Reload the announcements");
	Console()->Register("reload_maplist", "", CFGFLAG_SERVER, ConReloadMaplist, this, "Reload the maplist");

	RustVersionRegister(*Console());

	Console()->Chain("sv_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("password", ConchainSpecialInfoupdate, this);
	Console()->Chain("sv_spectator_slots", ConchainSpecialInfoupdate, this);

	Console()->Chain("sv_max_clients_per_ip", ConchainMaxclientsperipUpdate, this);
	Console()->Chain("access_level", ConchainCommandAccessUpdate, this);

	Console()->Chain("sv_rcon_password", ConchainRconPasswordChange, this);
	Console()->Chain("sv_rcon_mod_password", ConchainRconModPasswordChange, this);
	Console()->Chain("sv_rcon_helper_password", ConchainRconHelperPasswordChange, this);
	Console()->Chain("sv_map", ConchainMapUpdate, this);
	Console()->Chain("sv_sixup", ConchainSixupUpdate, this);

	Console()->Chain("loglevel", ConchainLoglevel, this);
	Console()->Chain("stdout_output_level", ConchainStdoutOutputLevel, this);

	Console()->Chain("sv_announcement_filename", ConchainAnnouncementFileName, this);

#if defined(CONF_FAMILY_UNIX)
	Console()->Chain("sv_conn_logging_server", ConchainConnLoggingServerChange, this);
#endif

	// register console commands in sub parts
	m_ServerBan.InitServerBan(Console(), Storage(), this);
	m_NameBans.InitConsole(Console());
	m_pGameServer->OnConsoleInit();
}

int CServer::SnapNewId()
{
	return m_IdPool.NewId();
}

void CServer::SnapFreeId(int Id)
{
	m_IdPool.FreeId(Id);
}

void *CServer::SnapNewItem(int Type, int Id, int Size)
{
	dbg_assert(Id >= -1 && Id <= 0xffff, "incorrect id");
	return Id < 0 ? nullptr : m_SnapshotBuilder.NewItem(Type, Id, Size);
}

void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

CServer *CreateServer() { return new CServer(); }

// DDRace

void CServer::ReadAnnouncementsFile()
{
	m_vAnnouncements.clear();

	if(g_Config.m_SvAnnouncementFileName[0] == '\0')
		return;

	CLineReader LineReader;
	if(!LineReader.OpenFile(m_pStorage->OpenFile(g_Config.m_SvAnnouncementFileName, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		log_error("server", "Failed load announcements from '%s'", g_Config.m_SvAnnouncementFileName);
		return;
	}
	while(const char *pLine = LineReader.Get())
	{
		if(str_length(pLine) && pLine[0] != '#')
		{
			m_vAnnouncements.emplace_back(pLine);
		}
	}
	log_info("server", "Loaded %" PRIzu " announcements", m_vAnnouncements.size());
}

const char *CServer::GetAnnouncementLine()
{
	if(m_vAnnouncements.empty())
	{
		return nullptr;
	}
	else if(m_vAnnouncements.size() == 1)
	{
		m_AnnouncementLastLine = 0;
	}
	else if(!g_Config.m_SvAnnouncementRandom)
	{
		if(++m_AnnouncementLastLine >= m_vAnnouncements.size())
			m_AnnouncementLastLine %= m_vAnnouncements.size();
	}
	else
	{
		unsigned Rand;
		do
		{
			Rand = rand() % m_vAnnouncements.size();
		} while(Rand == m_AnnouncementLastLine);

		m_AnnouncementLastLine = Rand;
	}

	return m_vAnnouncements[m_AnnouncementLastLine].c_str();
}

struct CSubdirCallbackUserdata
{
	CServer *m_pServer;
	char m_aCurrentFolder[IO_MAX_PATH_LENGTH];
};

int CServer::MaplistEntryCallback(const char *pFilename, int IsDir, int DirType, void *pUser)
{
	CSubdirCallbackUserdata *pUserdata = static_cast<CSubdirCallbackUserdata *>(pUser);
	CServer *pThis = pUserdata->m_pServer;

	if(str_comp(pFilename, ".") == 0 || str_comp(pFilename, "..") == 0)
		return 0;

	char aFilename[IO_MAX_PATH_LENGTH];
	if(pUserdata->m_aCurrentFolder[0] != '\0')
		str_format(aFilename, sizeof(aFilename), "%s/%s", pUserdata->m_aCurrentFolder, pFilename);
	else
		str_copy(aFilename, pFilename);

	if(IsDir)
	{
		CSubdirCallbackUserdata Userdata;
		Userdata.m_pServer = pThis;
		str_copy(Userdata.m_aCurrentFolder, aFilename);
		char aFindPath[IO_MAX_PATH_LENGTH];
		str_format(aFindPath, sizeof(aFindPath), "maps/%s/", aFilename);
		pThis->Storage()->ListDirectory(IStorage::TYPE_ALL, aFindPath, MaplistEntryCallback, &Userdata);
		return 0;
	}

	const char *pSuffix = str_endswith(aFilename, ".map");
	if(!pSuffix) // not ending with .map
		return 0;
	const size_t FilenameLength = pSuffix - aFilename;
	aFilename[FilenameLength] = '\0'; // remove suffix
	if(FilenameLength >= sizeof(CMaplistEntry().m_aName)) // name too long
		return 0;

	pThis->m_vMaplistEntries.emplace_back(aFilename);
	return 0;
}

void CServer::InitMaplist()
{
	m_vMaplistEntries.clear();

	CSubdirCallbackUserdata Userdata;
	Userdata.m_pServer = this;
	Userdata.m_aCurrentFolder[0] = '\0';
	Storage()->ListDirectory(IStorage::TYPE_ALL, "maps/", MaplistEntryCallback, &Userdata);

	std::sort(m_vMaplistEntries.begin(), m_vMaplistEntries.end());
	log_info("server", "Found %d maps for maplist", (int)m_vMaplistEntries.size());

	for(CClient &Client : m_aClients)
	{
		if(Client.m_State != CClient::STATE_INGAME)
			continue;

		// Resend maplist to clients that already got it or are currently getting it
		if(Client.m_MaplistEntryToSend == CClient::MAPLIST_DONE || Client.m_MaplistEntryToSend >= 0)
		{
			Client.m_MaplistEntryToSend = CClient::MAPLIST_UNINITIALIZED;
		}
	}
}

int *CServer::GetIdMap(int ClientId)
{
	return m_aIdMap + VANILLA_MAX_CLIENTS * ClientId;
}

bool CServer::SetTimedOut(int ClientId, int OrigId)
{
	if(!m_NetServer.SetTimedOut(ClientId, OrigId))
	{
		return false;
	}
	m_aClients[ClientId].m_Sixup = m_aClients[OrigId].m_Sixup;

	if(m_aClients[OrigId].m_Authed != AUTHED_NO)
	{
		LogoutClient(ClientId, "Timeout Protection");
	}
	DelClientCallback(OrigId, "Timeout Protection used", this);
	m_aClients[ClientId].m_Authed = AUTHED_NO;
	m_aClients[ClientId].m_Flags = m_aClients[OrigId].m_Flags;
	m_aClients[ClientId].m_DDNetVersion = m_aClients[OrigId].m_DDNetVersion;
	m_aClients[ClientId].m_GotDDNetVersionPacket = m_aClients[OrigId].m_GotDDNetVersionPacket;
	m_aClients[ClientId].m_DDNetVersionSettled = m_aClients[OrigId].m_DDNetVersionSettled;
	return true;
}

void CServer::SetErrorShutdown(const char *pReason)
{
	str_copy(m_aErrorShutdownReason, pReason);
}

void CServer::SetLoggers(std::shared_ptr<ILogger> &&pFileLogger, std::shared_ptr<ILogger> &&pStdoutLogger)
{
	m_pFileLogger = pFileLogger;
	m_pStdoutLogger = pStdoutLogger;
}
