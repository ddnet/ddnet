/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CLIENT_H
#define ENGINE_CLIENT_CLIENT_H

#include <deque>
#include <memory>
#include <mutex>

#include <base/hash.h>

#include <engine/client.h>
#include <engine/client/checksum.h>
#include <engine/client/friends.h>
#include <engine/client/ghost.h>
#include <engine/client/serverbrowser.h>
#include <engine/client/updater.h>
#include <engine/editor.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/demo.h>
#include <engine/shared/fifo.h>
#include <engine/shared/http.h>
#include <engine/shared/network.h>
#include <engine/textrender.h>
#include <engine/warning.h>

#include "graph.h"
#include "smooth_time.h"

class CDemoEdit;
class IDemoRecorder;
class CMsgPacker;
class CUnpacker;
class IConfigManager;
class IDiscord;
class IEngine;
class IEngineInput;
class IEngineMap;
class IEngineSound;
class IFriends;
class ILogger;
class ISteam;
class INotifications;
class IStorage;
class IUpdater;

#define CONNECTLINK_DOUBLE_SLASH "ddnet://"
#define CONNECTLINK_NO_SLASH "ddnet:"

class CServerCapabilities
{
public:
	bool m_ChatTimeoutCode = false;
	bool m_AnyPlayerFlag = false;
	bool m_PingEx = false;
	bool m_AllowDummy = false;
	bool m_SyncWeaponInput = false;
};

class CClient : public IClient, public CDemoPlayer::IListener
{
	// needed interfaces
	IConfigManager *m_pConfigManager = nullptr;
	CConfig *m_pConfig = nullptr;
	IConsole *m_pConsole = nullptr;
	IDiscord *m_pDiscord = nullptr;
	IEditor *m_pEditor = nullptr;
	IEngine *m_pEngine = nullptr;
	IFavorites *m_pFavorites = nullptr;
	IGameClient *m_pGameClient = nullptr;
	IEngineGraphics *m_pGraphics = nullptr;
	IEngineInput *m_pInput = nullptr;
	IEngineMap *m_pMap = nullptr;
	IEngineSound *m_pSound = nullptr;
	ISteam *m_pSteam = nullptr;
	INotifications *m_pNotifications = nullptr;
	IStorage *m_pStorage = nullptr;
	IEngineTextRender *m_pTextRender = nullptr;
	IUpdater *m_pUpdater = nullptr;
	CHttp m_Http;

	CNetClient m_aNetClient[NUM_CONNS];
	CDemoPlayer m_DemoPlayer;
	CDemoRecorder m_aDemoRecorder[RECORDER_MAX];
	CDemoEditor m_DemoEditor;
	CGhostRecorder m_GhostRecorder;
	CGhostLoader m_GhostLoader;
	CServerBrowser m_ServerBrowser;
	CUpdater m_Updater;
	CFriends m_Friends;
	CFriends m_Foes;

	char m_aConnectAddressStr[MAX_SERVER_ADDRESSES * NETADDR_MAXSTRSIZE] = "";

	CUuid m_ConnectionId = UUID_ZEROED;
	bool m_Sixup;

	bool m_HaveGlobalTcpAddr = false;
	NETADDR m_GlobalTcpAddr = NETADDR_ZEROED;

	uint64_t m_aSnapshotParts[NUM_DUMMIES] = {0, 0};
	int64_t m_LocalStartTime = 0;
	int64_t m_GlobalStartTime = 0;

	IGraphics::CTextureHandle m_DebugFont;

	int64_t m_LastRenderTime;

	int m_SnapCrcErrors = 0;
	bool m_AutoScreenshotRecycle = false;
	bool m_AutoStatScreenshotRecycle = false;
	bool m_AutoCSVRecycle = false;
	bool m_EditorActive = false;

	int m_aAckGameTick[NUM_DUMMIES] = {-1, -1};
	int m_aCurrentRecvTick[NUM_DUMMIES] = {0, 0};
	int m_aRconAuthed[NUM_DUMMIES] = {0, 0};
	char m_aRconUsername[32] = "";
	char m_aRconPassword[sizeof(g_Config.m_SvRconPassword)] = "";
	int m_UseTempRconCommands = 0;
	int m_ExpectedRconCommands = -1;
	int m_GotRconCommands = 0;
	char m_aPassword[sizeof(g_Config.m_Password)] = "";
	bool m_SendPassword = false;

	int m_ExpectedMaplistEntries = -1;
	std::vector<std::string> m_vMaplistEntries;

	// version-checking
	char m_aVersionStr[10] = "0";

	// pinging
	int64_t m_PingStartTime = 0;

	char m_aCurrentMap[IO_MAX_PATH_LENGTH] = "";
	char m_aCurrentMapPath[IO_MAX_PATH_LENGTH] = "";

	char m_aTimeoutCodes[NUM_DUMMIES][32] = {"", ""};
	bool m_aCodeRunAfterJoin[NUM_DUMMIES] = {false, false};
	bool m_GenerateTimeoutSeed = true;

	char m_aCmdConnect[256] = "";
	char m_aCmdPlayDemo[IO_MAX_PATH_LENGTH] = "";
	char m_aCmdEditMap[IO_MAX_PATH_LENGTH] = "";

	// map download
	char m_aMapDownloadUrl[256] = "";
	std::shared_ptr<CHttpRequest> m_pMapdownloadTask = nullptr;
	char m_aMapdownloadFilename[256] = "";
	char m_aMapdownloadFilenameTemp[256] = "";
	char m_aMapdownloadName[256] = "";
	IOHANDLE m_MapdownloadFileTemp = nullptr;
	int m_MapdownloadChunk = 0;
	int m_MapdownloadCrc = 0;
	int m_MapdownloadAmount = -1;
	int m_MapdownloadTotalsize = -1;
	bool m_MapdownloadSha256Present = false;
	SHA256_DIGEST m_MapdownloadSha256 = SHA256_ZEROED;

	bool m_MapDetailsPresent = false;
	char m_aMapDetailsName[256] = "";
	int m_MapDetailsCrc = 0;
	SHA256_DIGEST m_MapDetailsSha256 = SHA256_ZEROED;
	char m_aMapDetailsUrl[256] = "";

	std::shared_ptr<CHttpRequest> m_pDDNetInfoTask = nullptr;

	// time
	CSmoothTime m_aGameTime[NUM_DUMMIES];
	CSmoothTime m_PredictedTime;

	// input
	struct // TODO: handle input better
	{
		int m_aData[MAX_INPUT_SIZE]; // the input data
		int m_Tick; // the tick that the input is for
		int64_t m_PredictedTime; // prediction latency when we sent this input
		int64_t m_PredictionMargin; // prediction margin when we sent this input
		int64_t m_Time;
	} m_aInputs[NUM_DUMMIES][200];

	int m_aCurrentInput[NUM_DUMMIES] = {0, 0};
	bool m_LastDummy = false;
	bool m_DummySendConnInfo = false;
	bool m_DummyConnecting = false;
	bool m_DummyConnected = false;
	float m_LastDummyConnectTime = 0.0f;
	bool m_DummyReconnectOnReload = false;
	bool m_DummyDeactivateOnReconnect = false;

	// graphs
	CGraph m_InputtimeMarginGraph;
	CGraph m_aGametimeMarginGraphs[NUM_DUMMIES];
	CGraph m_FpsGraph;

	// the game snapshots are modifiable by the game
	CSnapshotStorage m_aSnapshotStorage[NUM_DUMMIES];
	CSnapshotStorage::CHolder *m_aapSnapshots[NUM_DUMMIES][NUM_SNAPSHOT_TYPES];

	int m_aReceivedSnapshots[NUM_DUMMIES] = {0, 0};
	char m_aaSnapshotIncomingData[NUM_DUMMIES][CSnapshot::MAX_SIZE];
	int m_aSnapshotIncomingDataSize[NUM_DUMMIES] = {0, 0};

	CSnapshotStorage::CHolder m_aDemorecSnapshotHolders[NUM_SNAPSHOT_TYPES];
	char m_aaaDemorecSnapshotData[NUM_SNAPSHOT_TYPES][2][CSnapshot::MAX_SIZE];

	CSnapshotDelta m_SnapshotDelta;

	std::deque<std::shared_ptr<CDemoEdit>> m_EditJobs;

	//
	bool m_CanReceiveServerCapabilities = false;
	bool m_ServerSentCapabilities = false;
	CServerCapabilities m_ServerCapabilities;

	bool ServerCapAnyPlayerFlag() const override { return m_ServerCapabilities.m_AnyPlayerFlag; }

	CServerInfo m_CurrentServerInfo;
	int64_t m_CurrentServerInfoRequestTime = -1; // >= 0 should request, == -1 got info

	int m_CurrentServerPingInfoType = -1;
	int m_CurrentServerPingBasicToken = -1;
	int m_CurrentServerPingToken = -1;
	CUuid m_CurrentServerPingUuid = UUID_ZEROED;
	int64_t m_CurrentServerCurrentPingTime = -1; // >= 0 request running
	int64_t m_CurrentServerNextPingTime = -1; // >= 0 should request

	// version info
	struct CVersionInfo
	{
		enum
		{
			STATE_INIT = 0,
			STATE_START,
			STATE_READY,
		};

		int m_State = STATE_INIT;
	} m_VersionInfo;

	std::mutex m_WarningsMutex;
	std::vector<SWarning> m_vWarnings;
	std::vector<SWarning> m_vQuittingWarnings;

	CFifo m_Fifo;

	IOHANDLE m_BenchmarkFile = nullptr;
	int64_t m_BenchmarkStopTime = 0;

	CChecksum m_Checksum;
	int64_t m_OwnExecutableSize = 0;
	IOHANDLE m_OwnExecutable = nullptr;

	// favorite command handling
	bool m_FavoritesGroup = false;
	bool m_FavoritesGroupAllowPing = false;
	int m_FavoritesGroupNum = 0;
	NETADDR m_aFavoritesGroupAddresses[MAX_SERVER_ADDRESSES];

	void UpdateDemoIntraTimers();
	int MaxLatencyTicks() const;
	int PredictionMargin() const;

	std::shared_ptr<ILogger> m_pFileLogger = nullptr;
	std::shared_ptr<ILogger> m_pStdoutLogger = nullptr;

	// For DummyName function
	char m_aAutomaticDummyName[MAX_NAME_LENGTH];

public:
	IConfigManager *ConfigManager() { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IDiscord *Discord() { return m_pDiscord; }
	IEngine *Engine() { return m_pEngine; }
	IGameClient *GameClient() { return m_pGameClient; }
	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineInput *Input() { return m_pInput; }
	IEngineSound *Sound() { return m_pSound; }
	ISteam *Steam() { return m_pSteam; }
	INotifications *Notifications() { return m_pNotifications; }
	IStorage *Storage() { return m_pStorage; }
	IEngineTextRender *TextRender() { return m_pTextRender; }
	IUpdater *Updater() { return m_pUpdater; }
	IHttp *Http() { return &m_Http; }

	CClient();

	// ----- send functions -----
	int SendMsg(int Conn, CMsgPacker *pMsg, int Flags) override;
	// Send via the currently active client (main/dummy)
	int SendMsgActive(CMsgPacker *pMsg, int Flags) override;

	void SendInfo(int Conn);
	void SendEnterGame(int Conn);
	void SendReady(int Conn);
	void SendMapRequest();

	bool RconAuthed() const override { return m_aRconAuthed[g_Config.m_ClDummy] != 0; }
	bool UseTempRconCommands() const override { return m_UseTempRconCommands != 0; }
	void RconAuth(const char *pName, const char *pPassword, bool Dummy = g_Config.m_ClDummy) override;
	void Rcon(const char *pCmd) override;
	bool ReceivingRconCommands() const override { return m_ExpectedRconCommands > 0; }
	float GotRconCommandsPercentage() const override;
	bool ReceivingMaplist() const override { return m_ExpectedMaplistEntries > 0; }
	float GotMaplistPercentage() const override;
	const std::vector<std::string> &MaplistEntries() const override { return m_vMaplistEntries; }

	bool ConnectionProblems() const override;

	IGraphics::CTextureHandle GetDebugFont() const override { return m_DebugFont; }

	void SendInput();

	// TODO: OPT: do this a lot smarter!
	int *GetInput(int Tick, int IsDummy) const override;

	const char *LatestVersion() const override;

	// ------ state handling -----
	void SetState(EClientState State);

	// called when the map is loaded and we should init for a new round
	void OnEnterGame(bool Dummy);
	void EnterGame(int Conn) override;

	void Connect(const char *pAddress, const char *pPassword = nullptr) override;
	void DisconnectWithReason(const char *pReason);
	void Disconnect() override;

	void DummyDisconnect(const char *pReason) override;
	void DummyConnect() override;
	bool DummyConnected() const override;
	bool DummyConnecting() const override;
	bool DummyConnectingDelayed() const override;
	bool DummyAllowed() const override;

	void GetServerInfo(CServerInfo *pServerInfo) const override;
	void ServerInfoRequest();

	void LoadDebugFont();

	// ---

	int GetPredictionTime() override;
	CSnapItem SnapGetItem(int SnapId, int Index) const override;
	int GetPredictionTick() override;
	const void *SnapFindItem(int SnapId, int Type, int Id) const override;
	int SnapNumItems(int SnapId) const override;
	void SnapSetStaticsize(int ItemType, int Size) override;
	void SnapSetStaticsize7(int ItemType, int Size) override;

	void Render();
	void DebugRender();

	void Restart() override;
	void Quit() override;

	const char *PlayerName() const override;
	const char *DummyName() override;
	const char *ErrorString() const override;

	const char *LoadMap(const char *pName, const char *pFilename, SHA256_DIGEST *pWantedSha256, unsigned WantedCrc);
	const char *LoadMapSearch(const char *pMapName, SHA256_DIGEST *pWantedSha256, int WantedCrc);

	int TranslateSysMsg(int *pMsgId, bool System, CUnpacker *pUnpacker, CPacker *pPacker, CNetChunk *pPacket, bool *pIsExMsg);

	void PreprocessConnlessPacket7(CNetChunk *pPacket);
	void ProcessConnlessPacket(CNetChunk *pPacket);
	void ProcessServerInfo(int Type, NETADDR *pFrom, const void *pData, int DataSize);
	void ProcessServerPacket(CNetChunk *pPacket, int Conn, bool Dummy);

	int UnpackAndValidateSnapshot(CSnapshot *pFrom, CSnapshot *pTo);

	void ResetMapDownload(bool ResetActive);
	void FinishMapDownload();

	void RequestDDNetInfo() override;
	void ResetDDNetInfoTask();
	void LoadDDNetInfo();

	bool IsSixup() const override { return m_Sixup; }

	const NETADDR &ServerAddress() const override { return *m_aNetClient[CONN_MAIN].ServerAddress(); }
	int ConnectNetTypes() const override;
	const char *ConnectAddressString() const override { return m_aConnectAddressStr; }
	const char *MapDownloadName() const override { return m_aMapdownloadName; }
	int MapDownloadAmount() const override { return !m_pMapdownloadTask ? m_MapdownloadAmount : (int)m_pMapdownloadTask->Current(); }
	int MapDownloadTotalsize() const override { return !m_pMapdownloadTask ? m_MapdownloadTotalsize : (int)m_pMapdownloadTask->Size(); }

	void PumpNetwork();

	void OnDemoPlayerSnapshot(void *pData, int Size) override;
	void OnDemoPlayerMessage(void *pData, int Size) override;

	void Update();

	void RegisterInterfaces();
	void InitInterfaces();

	void Run();

	bool InitNetworkClient(char *pError, size_t ErrorSize);
	bool CtrlShiftKey(int Key, bool &Last);

	static void Con_Connect(IConsole::IResult *pResult, void *pUserData);
	static void Con_Disconnect(IConsole::IResult *pResult, void *pUserData);

	static void Con_DummyConnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_DummyDisconnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_DummyResetInput(IConsole::IResult *pResult, void *pUserData);

	static void Con_Quit(IConsole::IResult *pResult, void *pUserData);
	static void Con_Restart(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoPlay(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSpeed(IConsole::IResult *pResult, void *pUserData);
	static void Con_Minimize(IConsole::IResult *pResult, void *pUserData);
	static void Con_Ping(IConsole::IResult *pResult, void *pUserData);
	static void Con_Screenshot(IConsole::IResult *pResult, void *pUserData);

#if defined(CONF_VIDEORECORDER)
	void StartVideo(const char *pFilename, bool WithTimestamp);
	static void Con_StartVideo(IConsole::IResult *pResult, void *pUserData);
	static void Con_StopVideo(IConsole::IResult *pResult, void *pUserData);
	const char *DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex, bool StartPaused = false) override;
#endif

	static void Con_Rcon(IConsole::IResult *pResult, void *pUserData);
	static void Con_RconAuth(IConsole::IResult *pResult, void *pUserData);
	static void Con_RconLogin(IConsole::IResult *pResult, void *pUserData);
	static void Con_BeginFavoriteGroup(IConsole::IResult *pResult, void *pUserData);
	static void Con_EndFavoriteGroup(IConsole::IResult *pResult, void *pUserData);
	static void Con_AddFavorite(IConsole::IResult *pResult, void *pUserData);
	static void Con_RemoveFavorite(IConsole::IResult *pResult, void *pUserData);
	static void Con_Play(IConsole::IResult *pResult, void *pUserData);
	static void Con_Record(IConsole::IResult *pResult, void *pUserData);
	static void Con_StopRecord(IConsole::IResult *pResult, void *pUserData);
	static void Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData);
	static void Con_BenchmarkQuit(IConsole::IResult *pResult, void *pUserData);
	static void ConchainServerBrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainWindowResize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainTimeoutSeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainPassword(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainReplays(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainStdoutOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void Con_DemoSlice(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceBegin(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceEnd(IConsole::IResult *pResult, void *pUserData);
	static void Con_SaveReplay(IConsole::IResult *pResult, void *pUserData);

	void RegisterCommands();

	const char *DemoPlayer_Play(const char *pFilename, int StorageType) override;
	void DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder, bool Verbose = false) override;
	void DemoRecorder_HandleAutoStart() override;
	void DemoRecorder_UpdateReplayRecorder() override;
	void DemoRecorder_AddDemoMarker(int Recorder);
	IDemoRecorder *DemoRecorder(int Recorder) override;

	void AutoScreenshot_Start() override;
	void AutoStatScreenshot_Start() override;
	void AutoScreenshot_Cleanup();
	void AutoStatScreenshot_Cleanup();

	void AutoCSV_Start() override;
	void AutoCSV_Cleanup();

	void ServerBrowserUpdate() override;

	void HandleConnectAddress(const NETADDR *pAddr);
	void HandleConnectLink(const char *pLink);
	void HandleDemoPath(const char *pPath);
	void HandleMapPath(const char *pPath);

	virtual void InitChecksum();
	virtual int HandleChecksum(int Conn, CUuid Uuid, CUnpacker *pUnpacker);

	// gfx
	void SwitchWindowScreen(int Index) override;
	void SetWindowParams(int FullscreenMode, bool IsBorderless) override;
	void ToggleWindowVSync() override;
	void Notify(const char *pTitle, const char *pMessage) override;
	void OnWindowResize() override;
	void BenchmarkQuit(int Seconds, const char *pFilename);

	void UpdateAndSwap() override;

	// DDRace

	void GenerateTimeoutSeed() override;
	void GenerateTimeoutCodes(const NETADDR *pAddrs, int NumAddrs);

	const char *GetCurrentMap() const override;
	const char *GetCurrentMapPath() const override;
	SHA256_DIGEST GetCurrentMapSha256() const override;
	unsigned GetCurrentMapCrc() const override;

	void RaceRecord_Start(const char *pFilename) override;
	void RaceRecord_Stop() override;
	bool RaceRecord_IsRecording() override;

	void DemoSliceBegin() override;
	void DemoSliceEnd() override;
	void DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser) override;
	virtual void SaveReplay(int Length, const char *pFilename = "");

	bool EditorHasUnsavedData() const override { return m_pEditor->HasUnsavedData(); }

	IFriends *Foes() override { return &m_Foes; }

	void GetSmoothTick(int *pSmoothTick, float *pSmoothIntraTick, float MixAmount) override;

	void AddWarning(const SWarning &Warning) override;
	std::optional<SWarning> CurrentWarning() override;
	std::vector<SWarning> &&QuittingWarnings() { return std::move(m_vQuittingWarnings); }

	CChecksumData *ChecksumData() override { return &m_Checksum.m_Data; }
	int UdpConnectivity(int NetType) override;

	bool ViewLink(const char *pLink) override;
	bool ViewFile(const char *pFilename) override;

#if defined(CONF_FAMILY_WINDOWS)
	void ShellRegister() override;
	void ShellUnregister() override;
#endif

	void ShowMessageBox(const char *pTitle, const char *pMessage, EMessageBoxType Type = MESSAGE_BOX_TYPE_ERROR) override;
	void GetGpuInfoString(char (&aGpuInfo)[256]) override;
	void SetLoggers(std::shared_ptr<ILogger> &&pFileLogger, std::shared_ptr<ILogger> &&pStdoutLogger);
};

#endif
