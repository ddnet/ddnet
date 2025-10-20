/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include "antibot.h"
#include "authmanager.h"
#include "name_ban.h"
#include "snap_id_pool.h"

#include <base/hash.h>

#include <engine/console.h>
#include <engine/server.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/fifo.h>
#include <engine/shared/http.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/uuid_manager.h>

#include <memory>
#include <optional>
#include <vector>

#if defined(CONF_UPNP)
#include "upnp.h"
#endif

class CConfig;
class CHostLookup;
class CLogMessage;
class CMsgPacker;
class CPacker;
class IEngine;
class IEngineMap;
class ILogger;

class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T>
	int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason, bool VerbatimReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorage *pStorage, class CServer *pServer);

	int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason, bool VerbatimReason) override;
	int BanRange(const CNetRange *pRange, int Seconds, const char *pReason) override;

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
	static void ConBanRegion(class IConsole::IResult *pResult, void *pUser);
	static void ConBanRegionRange(class IConsole::IResult *pResult, void *pUser);
};

class CServer : public IServer
{
	friend class CServerLogger;

	class IGameServer *m_pGameServer;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IEngineAntibot *m_pAntibot;
	class IRegister *m_pRegister;
	IEngine *m_pEngine;

#if defined(CONF_UPNP)
	CUPnP m_UPnP;
#endif

#if defined(CONF_FAMILY_UNIX)
	UNIXSOCKETADDR m_ConnLoggingDestAddr;
	bool m_ConnLoggingSocketCreated;
	UNIXSOCKET m_ConnLoggingSocket;
#endif

	class CDbConnectionPool *m_pConnectionPool;

#ifdef CONF_DEBUG
	int m_PreviousDebugDummies = 0;
	void UpdateDebugDummies(bool ForceDisconnect);
#endif

public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	const CConfig *Config() const { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }
	class IEngineAntibot *Antibot() { return m_pAntibot; }
	class CDbConnectionPool *DbPool() { return m_pConnectionPool; }
	IEngine *Engine() { return m_pEngine; }

	enum
	{
		MAX_RCONCMD_SEND = 16,
	};

	enum class EDnsblState
	{
		NONE,
		PENDING,
		BLACKLISTED,
		WHITELISTED,
	};

	static const char *DnsblStateStr(EDnsblState State);

	class CClient
	{
	public:
		enum
		{
			STATE_REDIRECTED = -1,
			STATE_EMPTY,
			STATE_PREAUTH,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_READY,
			STATE_INGAME,
		};

		enum
		{
			SNAPRATE_INIT = 0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER,
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		double m_Traffic;
		int64_t m_TrafficSince;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CNetMsg_Sv_PreInput m_LastPreInput = {};
		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		std::optional<int> m_Score;
		int m_AuthKey;
		int m_AuthTries;
		bool m_AuthHidden;
		int m_NextMapChunk;
		int m_Flags;
		bool m_ShowIps;
		bool m_DebugDummy;
		bool m_ForceHighBandwidthOnSpectate;
		NETADDR m_DebugDummyAddr;
		std::array<char, NETADDR_MAXSTRSIZE> m_aDebugDummyAddrString;
		std::array<char, NETADDR_MAXSTRSIZE> m_aDebugDummyAddrStringNoPort;

		const IConsole::ICommandInfo *m_pRconCmdToSend;
		enum
		{
			MAPLIST_UNINITIALIZED = -1,
			MAPLIST_DISABLED = -2,
			MAPLIST_DONE = -3,
		};
		int m_MaplistEntryToSend;

		bool m_HasPersistentData;
		void *m_pPersistentData;

		void Reset();

		// DDRace

		bool m_GotDDNetVersionPacket;
		bool m_DDNetVersionSettled;
		int m_DDNetVersion;
		char m_aDDNetVersionStr[64];
		CUuid m_ConnectionId;
		int64_t m_RedirectDropTime;

		int m_aIdMap[LEGACY_MAX_CLIENTS];
		int m_aReverseIdMap[MAX_CLIENTS];

		// DNSBL
		EDnsblState m_DnsblState;
		std::shared_ptr<CHostLookup> m_pDnsblLookup;

		bool m_Sixup;

		bool IncludedInServerInfo() const
		{
			return m_State != STATE_EMPTY && !m_DebugDummy;
		}
	};

	IConsole::EAccessLevel ConsoleAccessLevel(int ClientId) const;

	CClient m_aClients[MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIdPool m_IdPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
	CFifo m_Fifo;
	CServerBan m_ServerBan;
	CHttp m_Http;

	IEngineMap *m_pMap;

	int64_t m_GameStartTime;
	//int m_CurrentGameTick;

	enum
	{
		UNINITIALIZED = 0,
		RUNNING = 1,
		STOPPING = 2
	};

	int m_RunServer;

	bool m_MapReload;
	bool m_SameMapReload;
	bool m_ReloadedWhenEmpty;
	int m_RconClientId;
	int m_RconAuthLevel;
	int m_PrintCBIndex;
	char m_aShutdownReason[128];
	void *m_pPersistentData;

	enum
	{
		MAP_TYPE_SIX = 0,
		MAP_TYPE_SIXUP,
		NUM_MAP_TYPES
	};

	enum
	{
		RECORDER_MANUAL = MAX_CLIENTS,
		RECORDER_AUTO = MAX_CLIENTS + 1,
		NUM_RECORDERS = MAX_CLIENTS + 2,
	};

	char m_aCurrentMap[IO_MAX_PATH_LENGTH];
	const char *m_pCurrentMapName;
	SHA256_DIGEST m_aCurrentMapSha256[NUM_MAP_TYPES];
	unsigned m_aCurrentMapCrc[NUM_MAP_TYPES];
	unsigned char *m_apCurrentMapData[NUM_MAP_TYPES];
	unsigned int m_aCurrentMapSize[NUM_MAP_TYPES];
	char m_aMapDownloadUrl[256];

	CDemoRecorder m_aDemoRecorder[NUM_RECORDERS];
	CAuthManager m_AuthManager;

	int64_t m_ServerInfoFirstRequest;
	int m_ServerInfoNumRequests;

	char m_aErrorShutdownReason[128];

	CNameBans m_NameBans;

	size_t m_AnnouncementLastLine;
	std::vector<std::string> m_vAnnouncements;

	std::shared_ptr<ILogger> m_pFileLogger = nullptr;
	std::shared_ptr<ILogger> m_pStdoutLogger = nullptr;

	CServer();
	~CServer() override;

	bool IsClientNameAvailable(int ClientId, const char *pNameRequest);
	bool SetClientNameImpl(int ClientId, const char *pNameRequest, bool Set);
	bool SetClientClanImpl(int ClientId, const char *pClanRequest, bool Set);

	bool WouldClientNameChange(int ClientId, const char *pNameRequest) override;
	bool WouldClientClanChange(int ClientId, const char *pClanRequest) override;
	void SetClientName(int ClientId, const char *pName) override;
	void SetClientClan(int ClientId, const char *pClan) override;
	void SetClientCountry(int ClientId, int Country) override;
	void SetClientScore(int ClientId, std::optional<int> Score) override;
	void SetClientFlags(int ClientId, int Flags) override;

	void Kick(int ClientId, const char *pReason) override;
	void Ban(int ClientId, int Seconds, const char *pReason, bool VerbatimReason) override;
	void ReconnectClient(int ClientId);
	void RedirectClient(int ClientId, int Port) override;

	void DemoRecorder_HandleAutoStart() override;

	//int Tick()
	int64_t TickStartTime(int Tick);
	//int TickSpeed()

	int Init();

	void SendLogLine(const CLogMessage *pMessage);
	void SetRconCid(int ClientId) override;
	int GetAuthedState(int ClientId) const override;
	bool IsRconAuthed(int ClientId) const override;
	bool IsRconAuthedAdmin(int ClientId) const override;
	const char *GetAuthName(int ClientId) const override;
	bool HasAuthHidden(int ClientId) const override;
	void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc) override;
	bool GetClientInfo(int ClientId, CClientInfo *pInfo) const override;
	void SetClientDDNetVersion(int ClientId, int DDNetVersion) override;
	const NETADDR *ClientAddr(int ClientId) const override;
	const std::array<char, NETADDR_MAXSTRSIZE> &ClientAddrStringImpl(int ClientId, bool IncludePort) const override;
	const char *ClientName(int ClientId) const override;
	const char *ClientClan(int ClientId) const override;
	int ClientCountry(int ClientId) const override;
	bool ClientSlotEmpty(int ClientId) const override;
	bool ClientIngame(int ClientId) const override;
	int Port() const override;
	int MaxClients() const override;
	int ClientCount() const override;
	int DistinctClientCount() const override;

	int GetClientVersion(int ClientId) const override;
	int SendMsg(CMsgPacker *pMsg, int Flags, int ClientId) override;

	void DoSnapshot();

	static int NewClientCallback(int ClientId, void *pUser, bool Sixup);
	static int NewClientNoAuthCallback(int ClientId, void *pUser);
	static int DelClientCallback(int ClientId, const char *pReason, void *pUser);

	static int ClientRejoinCallback(int ClientId, void *pUser);

	void SendRconType(int ClientId, bool UsernameReq);
	void SendCapabilities(int ClientId);
	void SendMap(int ClientId);
	void SendMapData(int ClientId, int Chunk);
	void SendMapReload(int ClientId);
	void SendConnectionReady(int ClientId);
	void SendRconLine(int ClientId, const char *pLine);
	// Accepts -1 as ClientId to mean "all clients with at least auth level admin"
	void SendRconLogLine(int ClientId, const CLogMessage *pMessage);

	void SendRconCmdAdd(const IConsole::ICommandInfo *pCommandInfo, int ClientId);
	void SendRconCmdRem(const IConsole::ICommandInfo *pCommandInfo, int ClientId);
	void SendRconCmdGroupStart(int ClientId);
	void SendRconCmdGroupEnd(int ClientId);
	int NumRconCommands(int ClientId);
	void UpdateClientRconCommands(int ClientId);

	class CMaplistEntry
	{
	public:
		char m_aName[128];

		CMaplistEntry() = default;
		CMaplistEntry(const char *pName);
		bool operator<(const CMaplistEntry &Other) const;
	};
	std::vector<CMaplistEntry> m_vMaplistEntries;
	void SendMaplistGroupStart(int ClientId);
	void SendMaplistGroupEnd(int ClientId);
	void UpdateClientMaplistEntries(int ClientId);

	bool CheckReservedSlotAuth(int ClientId, const char *pPassword);
	void ProcessClientPacket(CNetChunk *pPacket);

	class CCache
	{
	public:
		class CCacheChunk
		{
		public:
			CCacheChunk(const void *pData, int Size);
			CCacheChunk(const CCacheChunk &) = delete;
			CCacheChunk(CCacheChunk &&) = default;

			std::vector<uint8_t> m_vData;
		};

		std::vector<CCacheChunk> m_vCache;

		CCache();
		~CCache();

		void AddChunk(const void *pData, int Size);
		void Clear();
	};
	CCache m_aServerInfoCache[3 * 2];
	CCache m_aSixupServerInfoCache[2];
	bool m_ServerInfoNeedsUpdate;

	void FillAntibot(CAntibotRoundData *pData) override;

	void ExpireServerInfo() override;
	void CacheServerInfo(CCache *pCache, int Type, bool SendClients);
	void CacheServerInfoSixup(CCache *pCache, bool SendClients, int MaxConsideredClients);
	void SendServerInfo(const NETADDR *pAddr, int Token, int Type, bool SendClients);
	void GetServerInfoSixup(CPacker *pPacker, bool SendClients);
	bool RateLimitServerInfoConnless();
	void SendServerInfoConnless(const NETADDR *pAddr, int Token, int Type);
	void UpdateRegisterServerInfo();
	void UpdateServerInfo(bool Resend = false);

	void PumpNetwork(bool PacketWaiting);

	void ChangeMap(const char *pMap) override;
	const char *GetMapName() const override;
	void ReloadMap() override;
	int LoadMap(const char *pMapName);

	void SaveDemo(int ClientId, float Time) override;
	void StartRecord(int ClientId) override;
	void StopRecord(int ClientId) override;
	bool IsRecording(int ClientId) override;
	void StopDemos() override;

	int Run();

	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);
	static void ConShowIps(IConsole::IResult *pResult, void *pUser);
	static void ConHideAuthStatus(IConsole::IResult *pResult, void *pUser);
	static void ConForceHighBandwidthOnSpectate(IConsole::IResult *pResult, void *pUser);

	static void ConAuthAdd(IConsole::IResult *pResult, void *pUser);
	static void ConAuthAddHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdate(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthRemove(IConsole::IResult *pResult, void *pUser);
	static void ConAuthList(IConsole::IResult *pResult, void *pUser);

	// console commands for sqlmasters
	static void ConAddSqlServer(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData);

	static void ConReloadAnnouncement(IConsole::IResult *pResult, void *pUserData);
	static void ConReloadMaplist(IConsole::IResult *pResult, void *pUserData);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void LogoutClient(int ClientId, const char *pReason);
	void LogoutKey(int Key, const char *pReason);

	void ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult);
	static void ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSixupUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRegisterCommunityTokenRedact(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainStdoutOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAnnouncementFileName(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainInputFifo(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

#if defined(CONF_FAMILY_UNIX)
	static void ConchainConnLoggingServerChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
#endif

	void RegisterCommands();

	int SnapNewId() override;
	void SnapFreeId(int Id) override;
	void *SnapNewItem(int Type, int Id, int Size) override;
	void SnapSetStaticsize(int ItemType, int Size) override;

	// DDRace

	int m_aPrevStates[MAX_CLIENTS];
	const char *GetAnnouncementLine() override;
	void ReadAnnouncementsFile();

	static int MaplistEntryCallback(const char *pFilename, int IsDir, int DirType, void *pUser);
	void InitMaplist();

	int *GetIdMap(int ClientId) override;
	int *GetReverseIdMap(int ClientId) override;

	void InitDnsbl(int ClientId);
	bool DnsblWhite(int ClientId) override
	{
		return m_aClients[ClientId].m_DnsblState == EDnsblState::NONE ||
		       m_aClients[ClientId].m_DnsblState == EDnsblState::WHITELISTED;
	}
	bool DnsblPending(int ClientId) override
	{
		return m_aClients[ClientId].m_DnsblState == EDnsblState::PENDING;
	}
	bool DnsblBlack(int ClientId) override
	{
		return m_aClients[ClientId].m_DnsblState == EDnsblState::BLACKLISTED;
	}

	void AuthRemoveKey(int KeySlot);
	bool ClientPrevIngame(int ClientId) override { return m_aPrevStates[ClientId] == CClient::STATE_INGAME; }
	const char *GetNetErrorString(int ClientId) override { return m_NetServer.ErrorString(ClientId); }
	void ResetNetErrorString(int ClientId) override { m_NetServer.ResetErrorString(ClientId); }
	bool SetTimedOut(int ClientId, int OrigId) override;
	void SetTimeoutProtected(int ClientId) override { m_NetServer.SetTimeoutProtected(ClientId); }

	void SendMsgRaw(int ClientId, const void *pData, int Size, int Flags) override;

	bool ErrorShutdown() const { return m_aErrorShutdownReason[0] != 0; }
	void SetErrorShutdown(const char *pReason) override;

	bool IsSixup(int ClientId) const override { return ClientId != SERVER_DEMO_CLIENT && m_aClients[ClientId].m_Sixup; }

	void SetLoggers(std::shared_ptr<ILogger> &&pFileLogger, std::shared_ptr<ILogger> &&pStdoutLogger);

#ifdef CONF_FAMILY_UNIX
	enum CONN_LOGGING_CMD
	{
		OPEN_SESSION = 1,
		CLOSE_SESSION = 2,
	};

	void SendConnLoggingCommand(CONN_LOGGING_CMD Cmd, const NETADDR *pAddr);
#endif
};

bool IsInterrupted();

extern CServer *CreateServer();
#endif
