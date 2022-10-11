/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CLIENT_H
#define ENGINE_CLIENT_CLIENT_H

#include <list>
#include <memory>

#include <base/hash.h>
#include <engine/client.h>
#include <engine/client/checksum.h>
#include <engine/client/friends.h>
#include <engine/client/ghost.h>
#include <engine/client/serverbrowser.h>
#include <engine/client/updater.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/demo.h>
#include <engine/shared/fifo.h>
#include <engine/shared/http.h>
#include <engine/shared/network.h>
#include <engine/warning.h>

class CDemoEdit;
class IDemoRecorder;
class CMsgPacker;
class CUnpacker;
class IConfigManager;
class IDiscord;
class IEngineInput;
class IEngineMap;
class IEngineSound;
class IFriends;
class ISteam;
class IStorage;
class IUpdater;

#define CONNECTLINK "ddnet:"

class CGraph
{
public:
	enum
	{
		MAX_VALUES = 128,
	};

private:
	float m_Min, m_Max;
	float m_MinRange, m_MaxRange;
	float m_aValues[MAX_VALUES];
	float m_aColors[MAX_VALUES][3];
	size_t m_Index;

public:
	void Init(float Min, float Max);
	void SetMin(float Min);
	void SetMax(float Max);

	void Scale();
	void Add(float v, float r, float g, float b);
	void InsertAt(size_t Index, float v, float r, float g, float b);
	void Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription);
};

class CSmoothTime
{
	int64_t m_Snap;
	int64_t m_Current;
	int64_t m_Target;

	int64_t m_SnapMargin;
	int64_t m_CurrentMargin;
	int64_t m_TargetMargin;

	CGraph m_Graph;

	int m_SpikeCounter;

	float m_aAdjustSpeed[2]; // 0 = down, 1 = up
public:
	void Init(int64_t Target);
	void SetAdjustSpeed(int Direction, float Value);

	int64_t Get(int64_t Now);

	void UpdateInt(int64_t Target);
	void Update(CGraph *pGraph, int64_t Target, int TimeLeft, int AdjustDirection);

	int64_t GetMargin(int64_t Now);
	void UpdateMargin(int64_t TargetMargin);
};

class CServerCapabilities
{
public:
	bool m_ChatTimeoutCode;
	bool m_AnyPlayerFlag;
	bool m_PingEx;
	bool m_AllowDummy;
	bool m_SyncWeaponInput;
};

class CClient : public IClient, public CDemoPlayer::IListener
{
	// needed interfaces
	IEngine *m_pEngine;
	IEditor *m_pEditor;
	IEngineInput *m_pInput;
	IEngineGraphics *m_pGraphics;
	IEngineSound *m_pSound;
	IFavorites *m_pFavorites;
	IGameClient *m_pGameClient;
	IEngineMap *m_pMap;
	IConfigManager *m_pConfigManager;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	IUpdater *m_pUpdater;
	IDiscord *m_pDiscord;
	ISteam *m_pSteam;

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

	char m_aConnectAddressStr[MAX_SERVER_ADDRESSES * NETADDR_MAXSTRSIZE];

	CUuid m_ConnectionID;

	bool m_HaveGlobalTcpAddr = false;
	NETADDR m_GlobalTcpAddr;

	uint64_t m_aSnapshotParts[NUM_DUMMIES];
	int64_t m_LocalStartTime;

	IGraphics::CTextureHandle m_DebugFont;
	int m_DebugSoundIndex = 0;

	int64_t m_LastRenderTime;
	float m_RenderFrameTimeLow;
	float m_RenderFrameTimeHigh;
	int m_RenderFrames;

	int m_SnapCrcErrors;
	bool m_AutoScreenshotRecycle;
	bool m_AutoStatScreenshotRecycle;
	bool m_AutoCSVRecycle;
	bool m_EditorActive;
	bool m_SoundInitFailed;
	bool m_ResortServerBrowser;

	int m_aAckGameTick[NUM_DUMMIES];
	int m_aCurrentRecvTick[NUM_DUMMIES];
	int m_aRconAuthed[NUM_DUMMIES];
	char m_aRconPassword[32];
	int m_UseTempRconCommands;
	char m_aPassword[32];
	bool m_SendPassword;
	bool m_ButtonRender = false;

	// version-checking
	char m_aVersionStr[10];

	// pinging
	int64_t m_PingStartTime;

	char m_aCurrentMap[IO_MAX_PATH_LENGTH];
	char m_aCurrentMapPath[IO_MAX_PATH_LENGTH];

	char m_aTimeoutCodes[NUM_DUMMIES][32];
	bool m_aCodeRunAfterJoin[NUM_DUMMIES];
	bool m_GenerateTimeoutSeed;

	//
	char m_aCmdConnect[256];
	char m_aCmdPlayDemo[IO_MAX_PATH_LENGTH];
	char m_aCmdEditMap[IO_MAX_PATH_LENGTH];

	// map download
	std::shared_ptr<CHttpRequest> m_pMapdownloadTask;
	char m_aMapdownloadFilename[256];
	char m_aMapdownloadFilenameTemp[256];
	char m_aMapdownloadName[256];
	IOHANDLE m_MapdownloadFileTemp;
	int m_MapdownloadChunk;
	int m_MapdownloadCrc;
	int m_MapdownloadAmount;
	int m_MapdownloadTotalsize;
	bool m_MapdownloadSha256Present;
	SHA256_DIGEST m_MapdownloadSha256;

	bool m_MapDetailsPresent;
	char m_aMapDetailsName[256];
	int m_MapDetailsCrc;
	SHA256_DIGEST m_MapDetailsSha256;
	char m_aMapDetailsUrl[256];

	char m_aDDNetInfoTmp[64];
	std::shared_ptr<CHttpRequest> m_pDDNetInfoTask;

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

	int m_aCurrentInput[NUM_DUMMIES];
	bool m_LastDummy;
	bool m_DummySendConnInfo;

	// graphs
	CGraph m_InputtimeMarginGraph;
	CGraph m_GametimeMarginGraph;
	CGraph m_FpsGraph;

	// the game snapshots are modifiable by the game
	CSnapshotStorage m_aSnapshotStorage[NUM_DUMMIES];
	CSnapshotStorage::CHolder *m_aapSnapshots[NUM_DUMMIES][NUM_SNAPSHOT_TYPES];

	int m_aReceivedSnapshots[NUM_DUMMIES];
	char m_aaSnapshotIncomingData[NUM_DUMMIES][CSnapshot::MAX_SIZE];
	int m_aSnapshotIncomingDataSize[NUM_DUMMIES];

	CSnapshotStorage::CHolder m_aDemorecSnapshotHolders[NUM_SNAPSHOT_TYPES];
	char *m_aaapDemorecSnapshotData[NUM_SNAPSHOT_TYPES][2][CSnapshot::MAX_SIZE];

	CSnapshotDelta m_SnapshotDelta;

	std::list<std::shared_ptr<CDemoEdit>> m_lpEditJobs;

	//
	bool m_CanReceiveServerCapabilities;
	bool m_ServerSentCapabilities;
	CServerCapabilities m_ServerCapabilities;

	CServerInfo m_CurrentServerInfo;
	int64_t m_CurrentServerInfoRequestTime; // >= 0 should request, == -1 got info

	int m_CurrentServerPingInfoType;
	int m_CurrentServerPingBasicToken;
	int m_CurrentServerPingToken;
	CUuid m_CurrentServerPingUuid;
	int64_t m_CurrentServerCurrentPingTime; // >= 0 request running
	int64_t m_CurrentServerNextPingTime; // >= 0 should request

	// version info
	struct CVersionInfo
	{
		enum
		{
			STATE_INIT = 0,
			STATE_START,
			STATE_READY,
		};

		int m_State;
		class CHostLookup m_VersionServeraddr;
	} m_VersionInfo;

	static void GraphicsThreadProxy(void *pThis) { ((CClient *)pThis)->GraphicsThread(); }
	void GraphicsThread();

	std::vector<SWarning> m_vWarnings;

#if defined(CONF_FAMILY_UNIX)
	CFifo m_Fifo;
#endif

	IOHANDLE m_BenchmarkFile;
	int64_t m_BenchmarkStopTime;

	CChecksum m_Checksum;
	int m_OwnExecutableSize = 0;
	IOHANDLE m_OwnExecutable;

	// favorite command handling
	bool m_FavoritesGroup = false;
	bool m_FavoritesGroupAllowPing = false;
	int m_FavoritesGroupNum = 0;
	NETADDR m_aFavoritesGroupAddresses[MAX_SERVER_ADDRESSES];

	void UpdateDemoIntraTimers();
	int MaxLatencyTicks() const;
	int PredictionMargin() const;

public:
	IEngine *Engine() { return m_pEngine; }
	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineInput *Input() { return m_pInput; }
	IEngineSound *Sound() { return m_pSound; }
	IGameClient *GameClient() { return m_pGameClient; }
	IConfigManager *ConfigManager() { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IStorage *Storage() { return m_pStorage; }
	IUpdater *Updater() { return m_pUpdater; }
	IDiscord *Discord() { return m_pDiscord; }
	ISteam *Steam() { return m_pSteam; }

	CClient();

	// ----- send functions -----
	int SendMsg(int Conn, CMsgPacker *pMsg, int Flags) override;
	// Send via the currently active client (main/dummy)
	int SendMsgActive(CMsgPacker *pMsg, int Flags) override;

	void SendInfo();
	void SendEnterGame(int Conn);
	void SendReady();
	void SendMapRequest();

	bool RconAuthed() const override { return m_aRconAuthed[g_Config.m_ClDummy] != 0; }
	bool UseTempRconCommands() const override { return m_UseTempRconCommands != 0; }
	void RconAuth(const char *pName, const char *pPassword) override;
	void Rcon(const char *pCmd) override;

	bool ConnectionProblems() const override;

	bool SoundInitFailed() const override { return m_SoundInitFailed; }

	IGraphics::CTextureHandle GetDebugFont() const override { return m_DebugFont; }

	void DirectInput(int *pInput, int Size);
	void SendInput();

	// TODO: OPT: do this a lot smarter!
	int *GetInput(int Tick, int IsDummy) const override;

	const char *LatestVersion() const override;

	// ------ state handling -----
	void SetState(EClientState s);

	// called when the map is loaded and we should init for a new round
	void OnEnterGame(bool Dummy);
	void EnterGame(int Conn) override;

	void Connect(const char *pAddress, const char *pPassword = nullptr) override;
	void DisconnectWithReason(const char *pReason);
	void Disconnect() override;

	void DummyDisconnect(const char *pReason) override;
	void DummyConnect() override;
	bool DummyConnected() override;
	bool DummyConnecting() override;
	bool DummyAllowed() override;
	int m_DummyConnected;
	int m_LastDummyConnectTime;

	void GetServerInfo(CServerInfo *pServerInfo) const override;
	void ServerInfoRequest();

	int LoadData();

	// ---

	int GetPredictionTime() override;
	void *SnapGetItem(int SnapID, int Index, CSnapItem *pItem) const override;
	int SnapItemSize(int SnapID, int Index) const override;
	const void *SnapFindItem(int SnapID, int Type, int ID) const override;
	int SnapNumItems(int SnapID) const override;
	void SnapSetStaticsize(int ItemType, int Size) override;

	void Render();
	void DebugRender();

	void Restart() override;
	void Quit() override;

	const char *PlayerName() const override;
	const char *DummyName() const override;
	const char *ErrorString() const override;

	const char *LoadMap(const char *pName, const char *pFilename, SHA256_DIGEST *pWantedSha256, unsigned WantedCrc);
	const char *LoadMapSearch(const char *pMapName, SHA256_DIGEST *pWantedSha256, int WantedCrc);

	void ProcessConnlessPacket(CNetChunk *pPacket);
	void ProcessServerInfo(int Type, NETADDR *pFrom, const void *pData, int DataSize);
	void ProcessServerPacket(CNetChunk *pPacket, int Conn, bool Dummy);

	int UnpackAndValidateSnapshot(CSnapshot *pFrom, CSnapshot *pTo);

	void ResetMapDownload();
	void FinishMapDownload();

	void RequestDDNetInfo() override;
	void ResetDDNetInfo();
	bool IsDDNetInfoChanged();
	void FinishDDNetInfo();
	void LoadDDNetInfo();

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

	bool CtrlShiftKey(int Key, bool &Last);

	static void Con_Connect(IConsole::IResult *pResult, void *pUserData);
	static void Con_Disconnect(IConsole::IResult *pResult, void *pUserData);

	static void Con_DummyConnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_DummyDisconnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_DummyResetInput(IConsole::IResult *pResult, void *pUserData);

	static void Con_Quit(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoPlay(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSpeed(IConsole::IResult *pResult, void *pUserData);
	static void Con_Minimize(IConsole::IResult *pResult, void *pUserData);
	static void Con_Ping(IConsole::IResult *pResult, void *pUserData);
	static void Con_Screenshot(IConsole::IResult *pResult, void *pUserData);
	static void Con_Reset(IConsole::IResult *pResult, void *pUserData);

#if defined(CONF_VIDEORECORDER)
	static void StartVideo(IConsole::IResult *pResult, void *pUserData, const char *pVideoName);
	static void Con_StartVideo(IConsole::IResult *pResult, void *pUserData);
	static void Con_StopVideo(IConsole::IResult *pResult, void *pUserData);
	const char *DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex) override;
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
	static void ConchainTimeoutSeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainPassword(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainReplays(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void Con_DemoSlice(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceBegin(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceEnd(IConsole::IResult *pResult, void *pUserData);
	static void Con_SaveReplay(IConsole::IResult *pResult, void *pUserData);

	void RegisterCommands();

	const char *DemoPlayer_Play(const char *pFilename, int StorageType) override;
	void DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder) override;
	void DemoRecorder_HandleAutoStart() override;
	void DemoRecorder_StartReplayRecorder();
	void DemoRecorder_Stop(int Recorder, bool RemoveFile = false) override;
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
	void SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing) override;
	void ToggleWindowVSync() override;
	void LoadFont() override;
	void Notify(const char *pTitle, const char *pMessage) override;
	void BenchmarkQuit(int Seconds, const char *pFilename);

	void UpdateAndSwap() override;

	// DDRace

	void GenerateTimeoutSeed() override;
	void GenerateTimeoutCodes(const NETADDR *pAddrs, int NumAddrs);

	int GetCurrentRaceTime() override;

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

	SWarning *GetCurWarning() override;
	CChecksumData *ChecksumData() override { return &m_Checksum.m_Data; }
	bool InfoTaskRunning() override { return m_pDDNetInfoTask != nullptr; }
	int UdpConnectivity(int NetType) override;
};

#endif
