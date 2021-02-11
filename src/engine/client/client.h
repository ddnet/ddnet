/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_CLIENT_H
#define ENGINE_CLIENT_CLIENT_H

#include <list>
#include <memory>

#include <base/hash.h>
#include <engine/client.h>
#include <engine/client/demoedit.h>
#include <engine/client/friends.h>
#include <engine/client/ghost.h>
#include <engine/client/http.h>
#include <engine/client/serverbrowser.h>
#include <engine/client/updater.h>
#include <engine/discord.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/shared/config.h>
#include <engine/shared/demo.h>
#include <engine/shared/fifo.h>
#include <engine/shared/network.h>
#include <engine/sound.h>
#include <engine/steam.h>
#include <engine/warning.h>

#define CONNECTLINK "ddnet:"

class CGraph
{
public:
	enum
	{
		// restrictions: Must be power of two
		MAX_VALUES = 128,
	};

	float m_Min, m_Max;
	float m_aValues[MAX_VALUES];
	float m_aColors[MAX_VALUES][3];
	int m_Index;

	void Init(float Min, float Max);

	void ScaleMax();
	void ScaleMin();

	void Add(float v, float r, float g, float b);
	void Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription);
};

class CSmoothTime
{
	int64 m_Snap;
	int64 m_Current;
	int64 m_Target;

	CGraph m_Graph;

	int m_SpikeCounter;

	float m_aAdjustSpeed[2]; // 0 = down, 1 = up
public:
	void Init(int64 Target);
	void SetAdjustSpeed(int Direction, float Value);

	int64 Get(int64 Now);

	void UpdateInt(int64 Target);
	void Update(CGraph *pGraph, int64 Target, int TimeLeft, int AdjustDirection);
};

class CServerCapabilities
{
public:
	bool m_ChatTimeoutCode;
};

class CClient : public IClient, public CDemoPlayer::IListener
{
	// needed interfaces
	IEngine *m_pEngine;
	IEditor *m_pEditor;
	IEngineInput *m_pInput;
	IEngineGraphics *m_pGraphics;
	IEngineSound *m_pSound;
	IGameClient *m_pGameClient;
	IEngineMap *m_pMap;
	IConfigManager *m_pConfigManager;
	CConfig *m_pConfig;
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	IUpdater *m_pUpdater;
	IDiscord *m_pDiscord;
	ISteam *m_pSteam;
	IEngineMasterServer *m_pMasterServer;

	enum
	{
		NUM_SNAPSHOT_TYPES = 2,
		PREDICTION_MARGIN = 1000 / 50 / 2, // magic network prediction value
	};

	enum
	{
		CLIENT_MAIN = 0,
		CLIENT_DUMMY,
		CLIENT_CONTACT,
		NUM_CLIENTS,
	};

	class CNetClient m_NetClient[NUM_CLIENTS];
	class CDemoPlayer m_DemoPlayer;
	class CDemoRecorder m_DemoRecorder[RECORDER_MAX];
	class CDemoEditor m_DemoEditor;
	class CGhostRecorder m_GhostRecorder;
	class CGhostLoader m_GhostLoader;
	class CServerBrowser m_ServerBrowser;
	class CUpdater m_Updater;
	class CFriends m_Friends;
	class CFriends m_Foes;

	char m_aServerAddressStr[256];

	CUuid m_ConnectionID;

	unsigned m_SnapshotParts[NUM_DUMMIES];
	int64 m_LocalStartTime;

	IGraphics::CTextureHandle m_DebugFont;
	int m_DebugSoundIndex = 0;

	int64 m_LastRenderTime;
	float m_RenderFrameTimeLow;
	float m_RenderFrameTimeHigh;
	int m_RenderFrames;

	NETADDR m_ServerAddress;
	int m_SnapCrcErrors;
	bool m_AutoScreenshotRecycle;
	bool m_AutoStatScreenshotRecycle;
	bool m_AutoCSVRecycle;
	bool m_EditorActive;
	bool m_SoundInitFailed;
	bool m_ResortServerBrowser;

	int m_AckGameTick[NUM_DUMMIES];
	int m_CurrentRecvTick[NUM_DUMMIES];
	int m_RconAuthed[NUM_DUMMIES];
	char m_RconPassword[32];
	int m_UseTempRconCommands;
	char m_Password[32];
	bool m_SendPassword;
	bool m_ButtonRender = false;

	// version-checking
	char m_aVersionStr[10];

	// pinging
	int64 m_PingStartTime;

	char m_aCurrentMap[MAX_PATH_LENGTH];
	char m_aCurrentMapPath[MAX_PATH_LENGTH];

	char m_aTimeoutCodes[NUM_DUMMIES][32];
	bool m_aTimeoutCodeSent[NUM_DUMMIES];
	bool m_GenerateTimeoutSeed;

	//
	char m_aCmdConnect[256];
	char m_aCmdPlayDemo[MAX_PATH_LENGTH];
	char m_aCmdEditMap[MAX_PATH_LENGTH];

	// map download
	std::shared_ptr<CGetFile> m_pMapdownloadTask;
	char m_aMapdownloadFilename[256];
	char m_aMapdownloadName[256];
	IOHANDLE m_MapdownloadFile;
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

	char m_aDDNetInfoTmp[64];
	std::shared_ptr<CGetFile> m_pDDNetInfoTask;

	char m_aDummyNameBuf[16];

	// time
	CSmoothTime m_GameTime[NUM_DUMMIES];
	CSmoothTime m_PredictedTime;

	// input
	struct // TODO: handle input better
	{
		int m_aData[MAX_INPUT_SIZE]; // the input data
		int m_Tick; // the tick that the input is for
		int64 m_PredictedTime; // prediction latency when we sent this input
		int64 m_Time;
	} m_aInputs[NUM_DUMMIES][200];

	int m_CurrentInput[NUM_DUMMIES];
	bool m_LastDummy;
	bool m_DummySendConnInfo;

	// graphs
	CGraph m_InputtimeMarginGraph;
	CGraph m_GametimeMarginGraph;
	CGraph m_FpsGraph;

	// the game snapshots are modifiable by the game
	class CSnapshotStorage m_SnapshotStorage[NUM_DUMMIES];
	CSnapshotStorage::CHolder *m_aSnapshots[NUM_DUMMIES][NUM_SNAPSHOT_TYPES];

	int m_ReceivedSnapshots[NUM_DUMMIES];
	char m_aSnapshotIncomingData[CSnapshot::MAX_SIZE];

	class CSnapshotStorage::CHolder m_aDemorecSnapshotHolders[NUM_SNAPSHOT_TYPES];
	char *m_aDemorecSnapshotData[NUM_SNAPSHOT_TYPES][2][CSnapshot::MAX_SIZE];

	class CSnapshotDelta m_SnapshotDelta;

	std::list<std::shared_ptr<CDemoEdit>> m_EditJobs;

	//
	bool m_CanReceiveServerCapabilities;
	bool m_ServerSentCapabilities;
	CServerCapabilities m_ServerCapabilities;

	bool ShouldSendChatTimeoutCodeHeuristic();

	class CServerInfo m_CurrentServerInfo;
	int64 m_CurrentServerInfoRequestTime; // >= 0 should request, == -1 got info

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

	volatile int m_GfxState;
	static void GraphicsThreadProxy(void *pThis) { ((CClient *)pThis)->GraphicsThread(); }
	void GraphicsThread();

	std::vector<SWarning> m_Warnings;

#if defined(CONF_FAMILY_UNIX)
	CFifo m_Fifo;
#endif

	IOHANDLE m_BenchmarkFile;
	int64 m_BenchmarkStopTime;

public:
	IEngine *Engine() { return m_pEngine; }
	IEngineGraphics *Graphics() { return m_pGraphics; }
	IEngineInput *Input() { return m_pInput; }
	IEngineSound *Sound() { return m_pSound; }
	IGameClient *GameClient() { return m_pGameClient; }
	IEngineMasterServer *MasterServer() { return m_pMasterServer; }
	IConfigManager *ConfigManager() { return m_pConfigManager; }
	CConfig *Config() { return m_pConfig; }
	IStorage *Storage() { return m_pStorage; }
	IUpdater *Updater() { return m_pUpdater; }
	IDiscord *Discord() { return m_pDiscord; }
	ISteam *Steam() { return m_pSteam; }

	CClient();

	// ----- send functions -----
	virtual int SendMsg(CMsgPacker *pMsg, int Flags);
	virtual int SendMsgY(CMsgPacker *pMsg, int Flags, int NetClient = 1);

	void SendInfo();
	void SendEnterGame();
	void SendReady();
	void SendMapRequest();

	virtual bool RconAuthed() { return m_RconAuthed[g_Config.m_ClDummy] != 0; }
	virtual bool UseTempRconCommands() { return m_UseTempRconCommands != 0; }
	void RconAuth(const char *pName, const char *pPassword);
	virtual void Rcon(const char *pCmd);

	virtual bool ConnectionProblems();

	virtual bool SoundInitFailed() { return m_SoundInitFailed; }

	virtual IGraphics::CTextureHandle GetDebugFont() { return m_DebugFont; }

	void DirectInput(int *pInput, int Size);
	void SendInput();

	// TODO: OPT: do this a lot smarter!
	virtual int *GetInput(int Tick, int IsDummy);
	virtual int *GetDirectInput(int Tick, int IsDummy);

	const char *LatestVersion();

	// ------ state handling -----
	void SetState(int s);

	// called when the map is loaded and we should init for a new round
	void OnEnterGame();
	virtual void EnterGame();

	virtual void Connect(const char *pAddress, const char *pPassword = NULL);
	void DisconnectWithReason(const char *pReason);
	virtual void Disconnect();

	virtual void DummyDisconnect(const char *pReason);
	virtual void DummyConnect();
	virtual bool DummyConnected();
	virtual bool DummyConnecting();
	int m_DummyConnected;
	int m_LastDummyConnectTime;

	virtual void GetServerInfo(CServerInfo *pServerInfo);
	void ServerInfoRequest();

	int LoadData();

	// ---

	int GetPredictionTime();
	void *SnapGetItem(int SnapID, int Index, CSnapItem *pItem);
	int SnapItemSize(int SnapID, int Index);
	void SnapInvalidateItem(int SnapID, int Index);
	void *SnapFindItem(int SnapID, int Type, int ID);
	int SnapNumItems(int SnapID);
	void SnapSetStaticsize(int ItemType, int Size);

	void Render();
	void DebugRender();

	virtual void Restart();
	virtual void Quit();

	virtual const char *PlayerName();
	virtual const char *DummyName();
	virtual const char *ErrorString();

	const char *LoadMap(const char *pName, const char *pFilename, SHA256_DIGEST *pWantedSha256, unsigned WantedCrc);
	const char *LoadMapSearch(const char *pMapName, SHA256_DIGEST *pWantedSha256, int WantedCrc);

	static bool PlayerScoreNameLess(const CServerInfo::CClient &p0, const CServerInfo::CClient &p1);

	void ProcessConnlessPacket(CNetChunk *pPacket);
	void ProcessServerInfo(int Type, NETADDR *pFrom, const void *pData, int DataSize);
	void ProcessServerPacket(CNetChunk *pPacket);
	void ProcessServerPacketDummy(CNetChunk *pPacket);

	void ResetMapDownload();
	void FinishMapDownload();

	void RequestDDNetInfo();
	void ResetDDNetInfo();
	bool IsDDNetInfoChanged();
	void FinishDDNetInfo();
	void LoadDDNetInfo();

	virtual const char *MapDownloadName() { return m_aMapdownloadName; }
	virtual int MapDownloadAmount() { return !m_pMapdownloadTask ? m_MapdownloadAmount : (int)m_pMapdownloadTask->Current(); }
	virtual int MapDownloadTotalsize() { return !m_pMapdownloadTask ? m_MapdownloadTotalsize : (int)m_pMapdownloadTask->Size(); }

	void PumpNetwork();

	virtual void OnDemoPlayerSnapshot(void *pData, int Size);
	virtual void OnDemoPlayerMessage(void *pData, int Size);

	void Update();

	void RegisterInterfaces();
	void InitInterfaces();

	void Run();

	bool CtrlShiftKey(int Key, bool &Last);

	static void Con_Connect(IConsole::IResult *pResult, void *pUserData);
	static void Con_Disconnect(IConsole::IResult *pResult, void *pUserData);

	static void Con_DummyConnect(IConsole::IResult *pResult, void *pUserData);
	static void Con_DummyDisconnect(IConsole::IResult *pResult, void *pUserData);

	static void Con_Quit(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoPlay(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSpeed(IConsole::IResult *pResult, void *pUserData);
	static void Con_Minimize(IConsole::IResult *pResult, void *pUserData);
	static void Con_Ping(IConsole::IResult *pResult, void *pUserData);
	static void Con_Screenshot(IConsole::IResult *pResult, void *pUserData);

#if defined(CONF_VIDEORECORDER)
	static void StartVideo(IConsole::IResult *pResult, void *pUserData, const char *pVideName);
	static void Con_StartVideo(IConsole::IResult *pResult, void *pUserData);
	static void Con_StopVideo(IConsole::IResult *pResult, void *pUserData);
	const char *DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex);
#endif

	static void Con_Rcon(IConsole::IResult *pResult, void *pUserData);
	static void Con_RconAuth(IConsole::IResult *pResult, void *pUserData);
	static void Con_RconLogin(IConsole::IResult *pResult, void *pUserData);
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
	static void ConchainPassword(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainReplays(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void Con_DemoSlice(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceBegin(IConsole::IResult *pResult, void *pUserData);
	static void Con_DemoSliceEnd(IConsole::IResult *pResult, void *pUserData);
	static void Con_SaveReplay(IConsole::IResult *pResult, void *pUserData);

	void RegisterCommands();

	const char *DemoPlayer_Play(const char *pFilename, int StorageType);
	void DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder);
	void DemoRecorder_HandleAutoStart();
	void DemoRecorder_StartReplayRecorder();
	void DemoRecorder_Stop(int Recorder, bool RemoveFile = false);
	void DemoRecorder_AddDemoMarker(int Recorder);
	class IDemoRecorder *DemoRecorder(int Recorder);

	void AutoScreenshot_Start();
	void AutoStatScreenshot_Start();
	void AutoScreenshot_Cleanup();
	void AutoStatScreenshot_Cleanup();

	void AutoCSV_Start();
	void AutoCSV_Cleanup();

	void ServerBrowserUpdate();

	void HandleConnectAddress(const NETADDR *pAddr);
	void HandleConnectLink(const char *pLink);
	void HandleDemoPath(const char *pPath);
	void HandleMapPath(const char *pPath);

	// gfx
	void SwitchWindowScreen(int Index);
	void ToggleFullscreen();
	void ToggleWindowBordered();
	void ToggleWindowVSync();
	void LoadFont();
	void Notify(const char *pTitle, const char *pMessage);
	void BenchmarkQuit(int Seconds, const char *pFilename);

	// DDRace

	virtual void GenerateTimeoutSeed();
	void GenerateTimeoutCodes();

	virtual int GetCurrentRaceTime();

	virtual const char *GetCurrentMap();
	virtual const char *GetCurrentMapPath();
	virtual SHA256_DIGEST GetCurrentMapSha256();
	virtual unsigned GetCurrentMapCrc();

	virtual void RaceRecord_Start(const char *pFilename);
	virtual void RaceRecord_Stop();
	virtual bool RaceRecord_IsRecording();

	virtual void DemoSliceBegin();
	virtual void DemoSliceEnd();
	virtual void DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser);
	virtual void SaveReplay(const int Length);

	virtual bool EditorHasUnsavedData() { return m_pEditor->HasUnsavedData(); }

	virtual IFriends *Foes() { return &m_Foes; }

	virtual void GetSmoothTick(int *pSmoothTick, float *pSmoothIntraTick, float MixAmount);

	virtual SWarning *GetCurWarning();
};

#endif
