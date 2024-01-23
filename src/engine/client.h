/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_CLIENT_H
#define ENGINE_CLIENT_H
#include "kernel.h"

#include "graphics.h"
#include "message.h"
#include <base/hash.h>

#include <game/generated/protocol.h>

#include <engine/friends.h>
#include <functional>

struct SWarning;

enum
{
	RECORDER_MANUAL = 0,
	RECORDER_AUTO = 1,
	RECORDER_RACE = 2,
	RECORDER_REPLAYS = 3,
	RECORDER_MAX = 4,

	NUM_DUMMIES = 2,
};

typedef bool (*CLIENTFUNC_FILTER)(const void *pData, int DataSize, void *pUser);
struct CChecksumData;

class IClient : public IInterface
{
	MACRO_INTERFACE("client")
public:
	/* Constants: Client States
		STATE_OFFLINE - The client is offline.
		STATE_CONNECTING - The client is trying to connect to a server.
		STATE_LOADING - The client has connected to a server and is loading resources.
		STATE_ONLINE - The client is connected to a server and running the game.
		STATE_DEMOPLAYBACK - The client is playing a demo
		STATE_QUITTING - The client is quitting.
	*/

	enum EClientState
	{
		STATE_OFFLINE = 0,
		STATE_CONNECTING,
		STATE_LOADING,
		STATE_ONLINE,
		STATE_DEMOPLAYBACK,
		STATE_QUITTING,
		STATE_RESTARTING,
	};

	/**
	* More precise state for @see STATE_LOADING
	* Sets what is actually happening in the client right now
	*/
	enum ELoadingStateDetail
	{
		LOADING_STATE_DETAIL_INITIAL,
		LOADING_STATE_DETAIL_LOADING_MAP,
		LOADING_STATE_DETAIL_SENDING_READY,
		LOADING_STATE_DETAIL_GETTING_READY,
	};

	typedef std::function<void()> TMapLoadingCallbackFunc;

protected:
	// quick access to state of the client
	EClientState m_State = IClient::STATE_OFFLINE;
	ELoadingStateDetail m_LoadingStateDetail = LOADING_STATE_DETAIL_INITIAL;
	int64_t m_StateStartTime;

	// quick access to time variables
	int m_aPrevGameTick[NUM_DUMMIES] = {0, 0};
	int m_aCurGameTick[NUM_DUMMIES] = {0, 0};
	float m_aGameIntraTick[NUM_DUMMIES] = {0.0f, 0.0f};
	float m_aGameTickTime[NUM_DUMMIES] = {0.0f, 0.0f};
	float m_aGameIntraTickSincePrev[NUM_DUMMIES] = {0.0f, 0.0f};

	int m_aPredTick[NUM_DUMMIES] = {0, 0};
	float m_aPredIntraTick[NUM_DUMMIES] = {0.0f, 0.0f};

	float m_LocalTime = 0.0f;
	float m_GlobalTime = 0.0f;
	float m_RenderFrameTime = 0.0001f;
	float m_FrameTimeAvg = 0.0001f;

	TMapLoadingCallbackFunc m_MapLoadingCBFunc = nullptr;

	char m_aNews[3000] = "";
	int m_Points = -1;
	int64_t m_ReconnectTime = 0;

public:
	class CSnapItem
	{
	public:
		int m_Type;
		int m_ID;
		int m_DataSize;
	};

	enum
	{
		CONN_MAIN = 0,
		CONN_DUMMY,
		CONN_CONTACT,
		NUM_CONNS,
	};

	enum
	{
		CONNECTIVITY_UNKNOWN,
		CONNECTIVITY_CHECKING,
		CONNECTIVITY_UNREACHABLE,
		CONNECTIVITY_REACHABLE,
		// Different global IP address has been detected for UDP and
		// TCP connections.
		CONNECTIVITY_DIFFERING_UDP_TCP_IP_ADDRESSES,
	};

	//
	inline EClientState State() const { return m_State; }
	inline ELoadingStateDetail LoadingStateDetail() const { return m_LoadingStateDetail; }
	inline int64_t StateStartTime() const { return m_StateStartTime; }
	void SetLoadingStateDetail(ELoadingStateDetail LoadingStateDetail) { m_LoadingStateDetail = LoadingStateDetail; }

	void SetMapLoadingCBFunc(TMapLoadingCallbackFunc &&Func) { m_MapLoadingCBFunc = std::move(Func); }

	// tick time access
	inline int PrevGameTick(int Conn) const { return m_aPrevGameTick[Conn]; }
	inline int GameTick(int Conn) const { return m_aCurGameTick[Conn]; }
	inline int PredGameTick(int Conn) const { return m_aPredTick[Conn]; }
	inline float IntraGameTick(int Conn) const { return m_aGameIntraTick[Conn]; }
	inline float PredIntraGameTick(int Conn) const { return m_aPredIntraTick[Conn]; }
	inline float IntraGameTickSincePrev(int Conn) const { return m_aGameIntraTickSincePrev[Conn]; }
	inline float GameTickTime(int Conn) const { return m_aGameTickTime[Conn]; }
	inline int GameTickSpeed() const { return SERVER_TICK_SPEED; }

	// other time access
	inline float RenderFrameTime() const { return m_RenderFrameTime; }
	inline float LocalTime() const { return m_LocalTime; }
	inline float GlobalTime() const { return m_GlobalTime; }
	inline float FrameTimeAvg() const { return m_FrameTimeAvg; }

	// actions
	virtual void Connect(const char *pAddress, const char *pPassword = nullptr) = 0;
	virtual void Disconnect() = 0;

	// dummy
	virtual void DummyDisconnect(const char *pReason) = 0;
	virtual void DummyConnect() = 0;
	virtual bool DummyConnected() = 0;
	virtual bool DummyConnecting() = 0;
	virtual bool DummyAllowed() = 0;

	virtual void Restart() = 0;
	virtual void Quit() = 0;
	virtual const char *DemoPlayer_Play(const char *pFilename, int StorageType) = 0;
#if defined(CONF_VIDEORECORDER)
	virtual const char *DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex, bool StartPaused = false) = 0;
#endif
	virtual void DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder, bool Verbose = false) = 0;
	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual void DemoRecorder_UpdateReplayRecorder() = 0;
	virtual class IDemoRecorder *DemoRecorder(int Recorder) = 0;
	virtual void AutoScreenshot_Start() = 0;
	virtual void AutoStatScreenshot_Start() = 0;
	virtual void AutoCSV_Start() = 0;
	virtual void ServerBrowserUpdate() = 0;

	// gfx
	virtual void SwitchWindowScreen(int Index) = 0;
	virtual void SetWindowParams(int FullscreenMode, bool IsBorderless) = 0;
	virtual void ToggleWindowVSync() = 0;
	virtual void Notify(const char *pTitle, const char *pMessage) = 0;
	virtual void OnWindowResize() = 0;

	virtual void UpdateAndSwap() = 0;

	// networking
	virtual void EnterGame(int Conn) = 0;

	//
	virtual const NETADDR &ServerAddress() const = 0;
	virtual int ConnectNetTypes() const = 0;
	virtual const char *ConnectAddressString() const = 0;
	virtual const char *MapDownloadName() const = 0;
	virtual int MapDownloadAmount() const = 0;
	virtual int MapDownloadTotalsize() const = 0;

	// input
	virtual int *GetInput(int Tick, int IsDummy = 0) const = 0;

	// remote console
	virtual void RconAuth(const char *pUsername, const char *pPassword) = 0;
	virtual bool RconAuthed() const = 0;
	virtual bool UseTempRconCommands() const = 0;
	virtual void Rcon(const char *pLine) = 0;

	// server info
	virtual void GetServerInfo(class CServerInfo *pServerInfo) const = 0;

	virtual int GetPredictionTime() = 0;

	// snapshot interface

	enum
	{
		SNAP_CURRENT = 0,
		SNAP_PREV = 1,
		NUM_SNAPSHOT_TYPES = 2,
	};

	// TODO: Refactor: should redo this a bit i think, too many virtual calls
	virtual int SnapNumItems(int SnapID) const = 0;
	virtual const void *SnapFindItem(int SnapID, int Type, int ID) const = 0;
	virtual void *SnapGetItem(int SnapID, int Index, CSnapItem *pItem) const = 0;
	virtual int SnapItemSize(int SnapID, int Index) const = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	virtual int SendMsg(int Conn, CMsgPacker *pMsg, int Flags) = 0;
	virtual int SendMsgActive(CMsgPacker *pMsg, int Flags) = 0;

	template<class T>
	int SendPackMsgActive(T *pMsg, int Flags)
	{
		CMsgPacker Packer(T::ms_MsgID, false);
		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsgActive(&Packer, Flags);
	}

	//
	virtual const char *PlayerName() const = 0;
	virtual const char *DummyName() const = 0;
	virtual const char *ErrorString() const = 0;
	virtual const char *LatestVersion() const = 0;
	virtual bool ConnectionProblems() const = 0;

	virtual bool SoundInitFailed() const = 0;

	virtual IGraphics::CTextureHandle GetDebugFont() const = 0; // TODO: remove this function

	//DDRace

	virtual const char *GetCurrentMap() const = 0;
	virtual const char *GetCurrentMapPath() const = 0;
	virtual SHA256_DIGEST GetCurrentMapSha256() const = 0;
	virtual unsigned GetCurrentMapCrc() const = 0;

	const char *News() const { return m_aNews; }
	int Points() const { return m_Points; }
	int64_t ReconnectTime() const { return m_ReconnectTime; }
	void SetReconnectTime(int64_t ReconnectTime) { m_ReconnectTime = ReconnectTime; }
	virtual int GetCurrentRaceTime() = 0;

	virtual void RaceRecord_Start(const char *pFilename) = 0;
	virtual void RaceRecord_Stop() = 0;
	virtual bool RaceRecord_IsRecording() = 0;

	virtual void DemoSliceBegin() = 0;
	virtual void DemoSliceEnd() = 0;
	virtual void DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser) = 0;

	virtual void RequestDDNetInfo() = 0;
	virtual bool EditorHasUnsavedData() const = 0;

	virtual void GenerateTimeoutSeed() = 0;

	virtual IFriends *Foes() = 0;

	virtual void GetSmoothTick(int *pSmoothTick, float *pSmoothIntraTick, float MixAmount) = 0;

	virtual void AddWarning(const SWarning &Warning) = 0;
	virtual SWarning *GetCurWarning() = 0;

	virtual CChecksumData *ChecksumData() = 0;
	virtual bool InfoTaskRunning() = 0;
	virtual int UdpConnectivity(int NetType) = 0;

#if defined(CONF_FAMILY_WINDOWS)
	virtual void ShellRegister() = 0;
	virtual void ShellUnregister() = 0;
#endif

	enum EMessageBoxType
	{
		MESSAGE_BOX_TYPE_ERROR,
		MESSAGE_BOX_TYPE_WARNING,
		MESSAGE_BOX_TYPE_INFO,
	};
	virtual void ShowMessageBox(const char *pTitle, const char *pMessage, EMessageBoxType Type = MESSAGE_BOX_TYPE_ERROR) = 0;
	virtual void GetGPUInfoString(char (&aGPUInfo)[256]) = 0;
};

class IGameClient : public IInterface
{
	MACRO_INTERFACE("gameclient")
protected:
public:
	virtual void OnConsoleInit() = 0;

	virtual void OnRconType(bool UsernameReq) = 0;
	virtual void OnRconLine(const char *pLine) = 0;
	virtual void OnInit() = 0;
	virtual void InvalidateSnapshot() = 0;
	virtual void OnNewSnapshot() = 0;
	virtual void OnEnterGame() = 0;
	virtual void OnShutdown() = 0;
	virtual void OnRender() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnStateChange(int NewState, int OldState) = 0;
	virtual void OnConnected() = 0;
	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int Conn, bool Dummy) = 0;
	virtual void OnPredict() = 0;
	virtual void OnActivateEditor() = 0;
	virtual void OnWindowResize() = 0;

	virtual int OnSnapInput(int *pData, bool Dummy, bool Force) = 0;
	virtual void OnDummySwap() = 0;
	virtual void SendDummyInfo(bool Start) = 0;
	virtual int GetLastRaceTick() const = 0;

	virtual const char *GetItemName(int Type) const = 0;
	virtual const char *Version() const = 0;
	virtual const char *NetVersion() const = 0;
	virtual int DDNetVersion() const = 0;
	virtual const char *DDNetVersionStr() const = 0;

	virtual void OnDummyDisconnect() = 0;
	virtual void DummyResetInput() = 0;
	virtual void Echo(const char *pString) = 0;
	virtual bool CanDisplayWarning() const = 0;
	virtual bool IsDisplayingWarning() const = 0;

	virtual CNetObjHandler *GetNetObjHandler() = 0;
};

void SnapshotRemoveExtraProjectileInfo(unsigned char *pData);

extern IGameClient *CreateGameClient();
#endif
