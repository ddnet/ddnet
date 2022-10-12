/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#define _WIN32_WINNT 0x0501

#include <climits>
#include <new>
#include <tuple>

#include <base/hash_ctxt.h>
#include <base/logger.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>

#include <game/client/components/menus.h>
#include <game/generated/protocol.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/discord.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/sound.h>
#include <engine/steam.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <engine/client/notifications.h>
#include <engine/shared/assertion_logger.h>
#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/demo.h>
#include <engine/shared/fifo.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/http.h>
#include <engine/shared/masterserver.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/uuid_manager.h>

#include <game/localization.h>
#include <game/version.h>

#include <engine/client/demoedit.h>

#include "client.h"
#include "friends.h"
#include "serverbrowser.h"

#if defined(CONF_VIDEORECORDER)
#include "video.h"
#endif

#include "SDL.h"
#ifdef main
#undef main
#endif

#include "base/hash.h"

// for msvc
#ifndef PRIu64
#define PRIu64 "I64u"
#endif

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

static const ColorRGBA gs_ClientNetworkPrintColor{0.7f, 1, 0.7f, 1.0f};
static const ColorRGBA gs_ClientNetworkErrPrintColor{1.0f, 0.25f, 0.25f, 1.0f};

void CGraph::Init(float Min, float Max)
{
	SetMin(Min);
	SetMax(Max);
	m_Index = 0;
}

void CGraph::SetMin(float Min)
{
	m_MinRange = m_Min = Min;
}

void CGraph::SetMax(float Max)
{
	m_MaxRange = m_Max = Max;
}

void CGraph::Scale()
{
	m_Min = m_MinRange;
	m_Max = m_MaxRange;
	for(auto Value : m_aValues)
	{
		if(Value > m_Max)
			m_Max = Value;
		else if(Value < m_Min)
			m_Min = Value;
	}
}

void CGraph::Add(float v, float r, float g, float b)
{
	m_Index = (m_Index + 1) % MAX_VALUES;
	InsertAt(m_Index, v, r, g, b);
}

void CGraph::InsertAt(size_t Index, float v, float r, float g, float b)
{
	dbg_assert(Index < MAX_VALUES, "Index out of bounds");
	m_aValues[Index] = v;
	m_aColors[Index][0] = r;
	m_aColors[Index][1] = g;
	m_aColors[Index][2] = b;
}

void CGraph::Render(IGraphics *pGraphics, IGraphics::CTextureHandle FontTexture, float x, float y, float w, float h, const char *pDescription)
{
	//m_pGraphics->BlendNormal();

	pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(0, 0, 0, 0.75f);
	IGraphics::CQuadItem QuadItem(x, y, w, h);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();

	pGraphics->LinesBegin();
	pGraphics->SetColor(0.95f, 0.95f, 0.95f, 1.00f);
	IGraphics::CLineItem LineItem(x, y + h / 2, x + w, y + h / 2);
	pGraphics->LinesDraw(&LineItem, 1);
	pGraphics->SetColor(0.5f, 0.5f, 0.5f, 0.75f);
	IGraphics::CLineItem aLineItems[2] = {
		IGraphics::CLineItem(x, y + (h * 3) / 4, x + w, y + (h * 3) / 4),
		IGraphics::CLineItem(x, y + h / 4, x + w, y + h / 4)};
	pGraphics->LinesDraw(aLineItems, 2);
	for(int i = 1; i < MAX_VALUES; i++)
	{
		float a0 = (i - 1) / (float)MAX_VALUES;
		float a1 = i / (float)MAX_VALUES;
		int i0 = (m_Index + i - 1) % MAX_VALUES;
		int i1 = (m_Index + i) % MAX_VALUES;

		float v0 = (m_aValues[i0] - m_Min) / (m_Max - m_Min);
		float v1 = (m_aValues[i1] - m_Min) / (m_Max - m_Min);

		IGraphics::CColorVertex aColorVertices[2] = {
			IGraphics::CColorVertex(0, m_aColors[i0][0], m_aColors[i0][1], m_aColors[i0][2], 0.75f),
			IGraphics::CColorVertex(1, m_aColors[i1][0], m_aColors[i1][1], m_aColors[i1][2], 0.75f)};
		pGraphics->SetColorVertex(aColorVertices, 2);
		IGraphics::CLineItem LineItem2(x + a0 * w, y + h - v0 * h, x + a1 * w, y + h - v1 * h);
		pGraphics->LinesDraw(&LineItem2, 1);
	}
	pGraphics->LinesEnd();

	pGraphics->TextureSet(FontTexture);
	pGraphics->QuadsBegin();
	pGraphics->QuadsText(x + 2, y + h - 16, 16, pDescription);

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%.2f", m_Max);
	pGraphics->QuadsText(x + w - 8 * str_length(aBuf) - 8, y + 2, 16, aBuf);

	str_format(aBuf, sizeof(aBuf), "%.2f", m_Min);
	pGraphics->QuadsText(x + w - 8 * str_length(aBuf) - 8, y + h - 16, 16, aBuf);
	pGraphics->QuadsEnd();
}

void CSmoothTime::Init(int64_t Target)
{
	m_Snap = time_get();
	m_Current = Target;
	m_Target = Target;
	m_SnapMargin = m_Snap;
	m_CurrentMargin = 0;
	m_TargetMargin = 0;
	m_aAdjustSpeed[0] = 0.3f;
	m_aAdjustSpeed[1] = 0.3f;
	m_Graph.Init(0.0f, 0.5f);
}

void CSmoothTime::SetAdjustSpeed(int Direction, float Value)
{
	m_aAdjustSpeed[Direction] = Value;
}

int64_t CSmoothTime::Get(int64_t Now)
{
	int64_t c = m_Current + (Now - m_Snap);
	int64_t t = m_Target + (Now - m_Snap);

	// it's faster to adjust upward instead of downward
	// we might need to adjust these abit

	float AdjustSpeed = m_aAdjustSpeed[0];
	if(t > c)
		AdjustSpeed = m_aAdjustSpeed[1];

	float a = ((Now - m_Snap) / (float)time_freq()) * AdjustSpeed;
	if(a > 1.0f)
		a = 1.0f;

	int64_t r = c + (int64_t)((t - c) * a);

	m_Graph.Add(a + 0.5f, 1, 1, 1);

	return r + GetMargin(Now);
}

void CSmoothTime::UpdateInt(int64_t Target)
{
	int64_t Now = time_get();
	m_Current = Get(Now) - GetMargin(Now);
	m_Snap = Now;
	m_Target = Target - GetMargin(Now);
}

void CSmoothTime::Update(CGraph *pGraph, int64_t Target, int TimeLeft, int AdjustDirection)
{
	int UpdateTimer = 1;

	if(TimeLeft < 0)
	{
		int IsSpike = 0;
		if(TimeLeft < -50)
		{
			IsSpike = 1;

			m_SpikeCounter += 5;
			if(m_SpikeCounter > 50)
				m_SpikeCounter = 50;
		}

		if(IsSpike && m_SpikeCounter < 15)
		{
			// ignore this ping spike
			UpdateTimer = 0;
			pGraph->Add(TimeLeft, 1, 1, 0);
		}
		else
		{
			pGraph->Add(TimeLeft, 1, 0, 0);
			if(m_aAdjustSpeed[AdjustDirection] < 30.0f)
				m_aAdjustSpeed[AdjustDirection] *= 2.0f;
		}
	}
	else
	{
		if(m_SpikeCounter)
			m_SpikeCounter--;

		pGraph->Add(TimeLeft, 0, 1, 0);

		m_aAdjustSpeed[AdjustDirection] *= 0.95f;
		if(m_aAdjustSpeed[AdjustDirection] < 2.0f)
			m_aAdjustSpeed[AdjustDirection] = 2.0f;
	}

	if(UpdateTimer)
		UpdateInt(Target);
}

int64_t CSmoothTime::GetMargin(int64_t Now)
{
	int64_t TimePassed = Now - m_SnapMargin;
	int64_t Diff = m_TargetMargin - m_CurrentMargin;

	float a = clamp(TimePassed / (float)time_freq(), -1.f, 1.f);
	int64_t Lim = maximum((int64_t)(a * absolute(Diff)), 1 + TimePassed / 100);
	return m_CurrentMargin + (int64_t)clamp(Diff, -Lim, Lim);
}

void CSmoothTime::UpdateMargin(int64_t TargetMargin)
{
	int64_t Now = time_get();
	m_CurrentMargin = GetMargin(Now);
	m_SnapMargin = Now;
	m_TargetMargin = TargetMargin;
}

CClient::CClient() :
	m_DemoPlayer(&m_SnapshotDelta, [&]() { UpdateDemoIntraTimers(); })
{
	for(auto &DemoRecorder : m_aDemoRecorder)
		DemoRecorder = CDemoRecorder(&m_SnapshotDelta);

	m_pEditor = 0;
	m_pInput = 0;
	m_pGraphics = 0;
	m_pSound = 0;
	m_pGameClient = 0;
	m_pMap = 0;
	m_pConfigManager = 0;
	m_pConfig = 0;
	m_pConsole = 0;

	m_RenderFrameTime = 0.0001f;
	m_RenderFrameTimeLow = 1.0f;
	m_RenderFrameTimeHigh = 0.0f;
	m_RenderFrames = 0;
	m_LastRenderTime = time_get();

	m_GameTickSpeed = SERVER_TICK_SPEED;

	m_SnapCrcErrors = 0;
	m_AutoScreenshotRecycle = false;
	m_AutoStatScreenshotRecycle = false;
	m_AutoCSVRecycle = false;
	m_EditorActive = false;

	m_aAckGameTick[0] = -1;
	m_aAckGameTick[1] = -1;
	m_aCurrentRecvTick[0] = 0;
	m_aCurrentRecvTick[1] = 0;
	m_aRconAuthed[0] = 0;
	m_aRconAuthed[1] = 0;
	m_aRconPassword[0] = '\0';
	m_aPassword[0] = '\0';

	// version-checking
	m_aVersionStr[0] = '0';
	m_aVersionStr[1] = '\0';

	// pinging
	m_PingStartTime = 0;

	m_aCurrentMap[0] = 0;

	m_aCmdConnect[0] = 0;

	// map download
	m_aMapdownloadFilename[0] = 0;
	m_aMapdownloadFilenameTemp[0] = 0;
	m_aMapdownloadName[0] = 0;
	m_pMapdownloadTask = NULL;
	m_MapdownloadFileTemp = 0;
	m_MapdownloadChunk = 0;
	m_MapdownloadSha256Present = false;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadCrc = 0;
	m_MapdownloadAmount = -1;
	m_MapdownloadTotalsize = -1;

	m_MapDetailsPresent = false;
	m_aMapDetailsName[0] = 0;
	m_MapDetailsSha256 = SHA256_ZEROED;
	m_MapDetailsCrc = 0;
	m_aMapDetailsUrl[0] = 0;

	IStorage::FormatTmpPath(m_aDDNetInfoTmp, sizeof(m_aDDNetInfoTmp), DDNET_INFO);
	m_pDDNetInfoTask = NULL;
	m_aNews[0] = '\0';
	m_aMapDownloadUrl[0] = '\0';
	m_Points = -1;

	m_CurrentServerInfoRequestTime = -1;
	m_CurrentServerPingInfoType = -1;
	m_CurrentServerPingBasicToken = -1;
	m_CurrentServerPingToken = -1;
	mem_zero(&m_CurrentServerPingUuid, sizeof(m_CurrentServerPingUuid));
	m_CurrentServerCurrentPingTime = -1;
	m_CurrentServerNextPingTime = -1;

	m_aCurrentInput[0] = 0;
	m_aCurrentInput[1] = 0;
	m_LastDummy = false;

	mem_zero(&m_aInputs, sizeof(m_aInputs));

	m_State = IClient::STATE_OFFLINE;
	m_StateStartTime = time_get();
	m_aConnectAddressStr[0] = 0;

	mem_zero(m_aapSnapshots, sizeof(m_aapSnapshots));
	m_aSnapshotStorage[0].Init();
	m_aSnapshotStorage[1].Init();
	m_aReceivedSnapshots[0] = 0;
	m_aReceivedSnapshots[1] = 0;
	m_aSnapshotParts[0] = 0;
	m_aSnapshotParts[1] = 0;

	m_VersionInfo.m_State = CVersionInfo::STATE_INIT;

	if(g_Config.m_ClDummy == 0)
		m_LastDummyConnectTime = 0;

	m_ReconnectTime = 0;

	m_GenerateTimeoutSeed = true;

	m_FrameTimeAvg = 0.0001f;
	m_BenchmarkFile = 0;
	m_BenchmarkStopTime = 0;

	mem_zero(&m_Checksum, sizeof(m_Checksum));
}

// ----- send functions -----
static inline bool RepackMsg(const CMsgPacker *pMsg, CPacker &Packer)
{
	Packer.Reset();
	if(pMsg->m_MsgID < OFFSET_UUID)
	{
		Packer.AddInt((pMsg->m_MsgID << 1) | (pMsg->m_System ? 1 : 0));
	}
	else
	{
		Packer.AddInt((0 << 1) | (pMsg->m_System ? 1 : 0)); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(pMsg->m_MsgID, &Packer);
	}
	Packer.AddRaw(pMsg->Data(), pMsg->Size());

	return false;
}

int CClient::SendMsg(int Conn, CMsgPacker *pMsg, int Flags)
{
	CNetChunk Packet;

	if(State() == IClient::STATE_OFFLINE)
		return 0;

	// repack message (inefficient)
	CPacker Pack;
	if(RepackMsg(pMsg, Pack))
		return 0;

	mem_zero(&Packet, sizeof(CNetChunk));
	Packet.m_ClientID = 0;
	Packet.m_pData = Pack.Data();
	Packet.m_DataSize = Pack.Size();

	if(Flags & MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags & MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if((Flags & MSGFLAG_RECORD) && Conn == g_Config.m_ClDummy)
	{
		for(auto &i : m_aDemoRecorder)
			if(i.IsRecording())
				i.RecordMessage(Packet.m_pData, Packet.m_DataSize);
	}

	if(!(Flags & MSGFLAG_NOSEND))
	{
		m_aNetClient[Conn].Send(&Packet);
	}

	return 0;
}

int CClient::SendMsgActive(CMsgPacker *pMsg, int Flags)
{
	return SendMsg(g_Config.m_ClDummy, pMsg, Flags);
}

void CClient::SendInfo()
{
	CMsgPacker MsgVer(NETMSG_CLIENTVER, true);
	MsgVer.AddRaw(&m_ConnectionID, sizeof(m_ConnectionID));
	MsgVer.AddInt(GameClient()->DDNetVersion());
	MsgVer.AddString(GameClient()->DDNetVersionStr(), 0);
	SendMsg(CONN_MAIN, &MsgVer, MSGFLAG_VITAL);

	CMsgPacker Msg(NETMSG_INFO, true);
	Msg.AddString(GameClient()->NetVersion(), 128);
	Msg.AddString(m_aPassword, 128);
	SendMsg(CONN_MAIN, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendEnterGame(int Conn)
{
	CMsgPacker Msg(NETMSG_ENTERGAME, true);
	SendMsg(Conn, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendReady()
{
	CMsgPacker Msg(NETMSG_READY, true);
	SendMsg(CONN_MAIN, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendMapRequest()
{
	if(m_MapdownloadFileTemp)
	{
		io_close(m_MapdownloadFileTemp);
		Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
	}
	m_MapdownloadFileTemp = Storage()->OpenFile(m_aMapdownloadFilenameTemp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA, true);
	Msg.AddInt(m_MapdownloadChunk);
	SendMsg(CONN_MAIN, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::RconAuth(const char *pName, const char *pPassword)
{
	if(RconAuthed())
		return;

	if(pPassword != m_aRconPassword)
		str_copy(m_aRconPassword, pPassword);

	CMsgPacker Msg(NETMSG_RCON_AUTH, true);
	Msg.AddString(pName, 32);
	Msg.AddString(pPassword, 32);
	Msg.AddInt(1);
	SendMsgActive(&Msg, MSGFLAG_VITAL);
}

void CClient::Rcon(const char *pCmd)
{
	CMsgPacker Msg(NETMSG_RCON_CMD, true);
	Msg.AddString(pCmd, 256);
	SendMsgActive(&Msg, MSGFLAG_VITAL);
}

bool CClient::ConnectionProblems() const
{
	return m_aNetClient[g_Config.m_ClDummy].GotProblems(MaxLatencyTicks() * time_freq() / SERVER_TICK_SPEED) != 0;
}

void CClient::DirectInput(int *pInput, int Size)
{
	CMsgPacker Msg(NETMSG_INPUT, true);
	Msg.AddInt(m_aAckGameTick[g_Config.m_ClDummy]);
	Msg.AddInt(m_aPredTick[g_Config.m_ClDummy]);
	Msg.AddInt(Size);

	for(int i = 0; i < Size / 4; i++)
		Msg.AddInt(pInput[i]);

	SendMsgActive(&Msg, 0);
}

void CClient::SendInput()
{
	int64_t Now = time_get();

	if(m_aPredTick[g_Config.m_ClDummy] <= 0)
		return;

	bool Force = false;
	// fetch input
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		if(!m_DummyConnected && Dummy != 0)
		{
			break;
		}
		int i = g_Config.m_ClDummy ^ Dummy;
		int Size = GameClient()->OnSnapInput(m_aInputs[i][m_aCurrentInput[i]].m_aData, Dummy, Force);

		if(Size)
		{
			// pack input
			CMsgPacker Msg(NETMSG_INPUT, true);
			Msg.AddInt(m_aAckGameTick[i]);
			Msg.AddInt(m_aPredTick[g_Config.m_ClDummy]);
			Msg.AddInt(Size);

			m_aInputs[i][m_aCurrentInput[i]].m_Tick = m_aPredTick[g_Config.m_ClDummy];
			m_aInputs[i][m_aCurrentInput[i]].m_PredictedTime = m_PredictedTime.Get(Now);
			m_aInputs[i][m_aCurrentInput[i]].m_PredictionMargin = m_PredictedTime.GetMargin(Now);
			m_aInputs[i][m_aCurrentInput[i]].m_Time = Now;

			// pack it
			for(int k = 0; k < Size / 4; k++)
				Msg.AddInt(m_aInputs[i][m_aCurrentInput[i]].m_aData[k]);

			m_aCurrentInput[i]++;
			m_aCurrentInput[i] %= 200;

			SendMsg(i, &Msg, MSGFLAG_FLUSH);
			// ugly workaround for dummy. we need to send input with dummy to prevent
			// prediction time resets. but if we do it too often, then it's
			// impossible to use grenade with frozen dummy that gets hammered...
			if(g_Config.m_ClDummyCopyMoves || m_aCurrentInput[i] % 2)
				Force = true;
		}
	}
}

const char *CClient::LatestVersion() const
{
	return m_aVersionStr;
}

// TODO: OPT: do this a lot smarter!
int *CClient::GetInput(int Tick, int IsDummy) const
{
	int Best = -1;
	const int d = IsDummy ^ g_Config.m_ClDummy;
	for(int i = 0; i < 200; i++)
	{
		if(m_aInputs[d][i].m_Tick <= Tick && (Best == -1 || m_aInputs[d][Best].m_Tick < m_aInputs[d][i].m_Tick))
			Best = i;
	}

	if(Best != -1)
		return (int *)m_aInputs[d][Best].m_aData;
	return 0;
}

// ------ state handling -----
void CClient::SetState(EClientState s)
{
	if(m_State == IClient::STATE_QUITTING || m_State == IClient::STATE_RESTARTING)
		return;

	int Old = m_State;
	if(g_Config.m_Debug)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "state change. last=%d current=%d", m_State, s);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
	}
	m_State = s;
	if(Old != s)
	{
		m_StateStartTime = time_get();
		GameClient()->OnStateChange(m_State, Old);

		if(s == IClient::STATE_OFFLINE && m_ReconnectTime == 0)
		{
			if(g_Config.m_ClReconnectFull > 0 && (str_find_nocase(ErrorString(), "full") || str_find_nocase(ErrorString(), "reserved")))
				m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectFull;
			else if(g_Config.m_ClReconnectTimeout > 0 && (str_find_nocase(ErrorString(), "Timeout") || str_find_nocase(ErrorString(), "Too weak connection")))
				m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectTimeout;
		}

		if(s == IClient::STATE_ONLINE)
		{
			const bool AnnounceAddr = m_ServerBrowser.IsRegistered(ServerAddress());
			Discord()->SetGameInfo(ServerAddress(), m_aCurrentMap, AnnounceAddr);
			Steam()->SetGameInfo(ServerAddress(), m_aCurrentMap, AnnounceAddr);
		}
		else if(Old == IClient::STATE_ONLINE)
		{
			Discord()->ClearGameInfo();
			Steam()->ClearGameInfo();
		}
	}
}

// called when the map is loaded and we should init for a new round
void CClient::OnEnterGame(bool Dummy)
{
	// reset input
	for(int i = 0; i < 200; i++)
	{
		m_aInputs[Dummy][i].m_Tick = -1;
	}
	m_aCurrentInput[Dummy] = 0;

	// reset snapshots
	m_aapSnapshots[Dummy][SNAP_CURRENT] = 0;
	m_aapSnapshots[Dummy][SNAP_PREV] = 0;
	m_aSnapshotStorage[Dummy].PurgeAll();
	// Also make gameclient aware that snapshots have been purged
	GameClient()->InvalidateSnapshot();
	m_aReceivedSnapshots[Dummy] = 0;
	m_aSnapshotParts[Dummy] = 0;
	m_aPredTick[Dummy] = 0;
	m_aAckGameTick[Dummy] = -1;
	m_aCurrentRecvTick[Dummy] = 0;
	m_aCurGameTick[Dummy] = 0;
	m_aPrevGameTick[Dummy] = 0;

	if(Dummy == 0)
	{
		m_LastDummyConnectTime = 0;
	}

	GameClient()->OnEnterGame();
}

void CClient::EnterGame(int Conn)
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
		return;

	m_aCodeRunAfterJoin[Conn] = false;

	// now we will wait for two snapshots
	// to finish the connection
	SendEnterGame(Conn);
	OnEnterGame(Conn);

	ServerInfoRequest(); // fresh one for timeout protection
	m_CurrentServerNextPingTime = time_get() + time_freq() / 2;
}

void GenerateTimeoutCode(char *pBuffer, unsigned Size, char *pSeed, const NETADDR *pAddrs, int NumAddrs, bool Dummy)
{
	MD5_CTX Md5;
	md5_init(&Md5);
	const char *pDummy = Dummy ? "dummy" : "normal";
	md5_update(&Md5, (unsigned char *)pDummy, str_length(pDummy) + 1);
	md5_update(&Md5, (unsigned char *)pSeed, str_length(pSeed) + 1);
	for(int i = 0; i < NumAddrs; i++)
	{
		md5_update(&Md5, (unsigned char *)&pAddrs[i], sizeof(pAddrs[i]));
	}
	MD5_DIGEST Digest = md5_finish(&Md5);

	unsigned short aRandom[8];
	mem_copy(aRandom, Digest.data, sizeof(aRandom));
	generate_password(pBuffer, Size, aRandom, 8);
}

void CClient::GenerateTimeoutSeed()
{
	secure_random_password(g_Config.m_ClTimeoutSeed, sizeof(g_Config.m_ClTimeoutSeed), 16);
}

void CClient::GenerateTimeoutCodes(const NETADDR *pAddrs, int NumAddrs)
{
	if(g_Config.m_ClTimeoutSeed[0])
	{
		for(int i = 0; i < 2; i++)
		{
			GenerateTimeoutCode(m_aTimeoutCodes[i], sizeof(m_aTimeoutCodes[i]), g_Config.m_ClTimeoutSeed, pAddrs, NumAddrs, i);

			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "timeout code '%s' (%s)", m_aTimeoutCodes[i], i == 0 ? "normal" : "dummy");
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
		}
	}
	else
	{
		str_copy(m_aTimeoutCodes[0], g_Config.m_ClTimeoutCode);
		str_copy(m_aTimeoutCodes[1], g_Config.m_ClDummyTimeoutCode);
	}
}

void CClient::Connect(const char *pAddress, const char *pPassword)
{
	Disconnect();

	m_ConnectionID = RandomUuid();
	if(pAddress != m_aConnectAddressStr)
		str_copy(m_aConnectAddressStr, pAddress);

	char aMsg[512];
	str_format(aMsg, sizeof(aMsg), "connecting to '%s'", m_aConnectAddressStr);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aMsg, gs_ClientNetworkPrintColor);

	ServerInfoRequest();

	int NumConnectAddrs = 0;
	NETADDR aConnectAddrs[MAX_SERVER_ADDRESSES];
	mem_zero(aConnectAddrs, sizeof(aConnectAddrs));
	const char *pNextAddr = pAddress;
	char aBuffer[128];
	while((pNextAddr = str_next_token(pNextAddr, ",", aBuffer, sizeof(aBuffer))))
	{
		NETADDR NextAddr;
		if(net_host_lookup(aBuffer, &NextAddr, m_aNetClient[CONN_MAIN].NetType()) != 0)
		{
			log_error("client", "could not find address of %s", aBuffer);
			continue;
		}
		if(NumConnectAddrs == (int)std::size(aConnectAddrs))
		{
			log_warn("client", "too many connect addresses, ignoring %s", aBuffer);
			continue;
		}
		if(NextAddr.port == 0)
		{
			NextAddr.port = 8303;
		}
		char aNextAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&NextAddr, aNextAddr, sizeof(aNextAddr), true);
		log_debug("client", "resolved connect address '%s' to %s", aBuffer, aNextAddr);
		aConnectAddrs[NumConnectAddrs] = NextAddr;
		NumConnectAddrs += 1;
	}

	if(NumConnectAddrs == 0)
	{
		log_error("client", "could not find any connect address, defaulting to localhost for whatever reason...");
		net_host_lookup("localhost", &aConnectAddrs[0], m_aNetClient[CONN_MAIN].NetType());
		NumConnectAddrs = 1;
	}

	if(m_SendPassword)
	{
		str_copy(m_aPassword, g_Config.m_Password);
		m_SendPassword = false;
	}
	else if(!pPassword)
		m_aPassword[0] = 0;
	else
		str_copy(m_aPassword, pPassword);

	m_CanReceiveServerCapabilities = true;
	// Deregister Rcon commands from last connected server, might not have called
	// DisconnectWithReason if the server was shut down
	m_aRconAuthed[0] = 0;
	m_UseTempRconCommands = 0;
	m_pConsole->DeregisterTempAll();

	m_aNetClient[CONN_MAIN].Connect(aConnectAddrs, NumConnectAddrs);
	m_aNetClient[CONN_MAIN].RefreshStun();
	SetState(IClient::STATE_CONNECTING);

	for(int i = 0; i < RECORDER_MAX; i++)
		if(m_aDemoRecorder[i].IsRecording())
			DemoRecorder_Stop(i);

	m_InputtimeMarginGraph.Init(-150.0f, 150.0f);
	m_GametimeMarginGraph.Init(-150.0f, 150.0f);

	GenerateTimeoutCodes(aConnectAddrs, NumConnectAddrs);
}

void CClient::DisconnectWithReason(const char *pReason)
{
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "disconnecting. reason='%s'", pReason ? pReason : "unknown");
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkPrintColor);

	// stop demo playback and recorder
	m_DemoPlayer.Stop();
	for(int i = 0; i < RECORDER_MAX; i++)
		DemoRecorder_Stop(i);

	//
	m_aRconAuthed[0] = 0;
	m_ServerSentCapabilities = false;
	m_UseTempRconCommands = 0;
	m_pConsole->DeregisterTempAll();
	m_aNetClient[CONN_MAIN].Disconnect(pReason);
	SetState(IClient::STATE_OFFLINE);
	m_pMap->Unload();
	m_CurrentServerPingInfoType = -1;
	m_CurrentServerPingBasicToken = -1;
	m_CurrentServerPingToken = -1;
	mem_zero(&m_CurrentServerPingUuid, sizeof(m_CurrentServerPingUuid));
	m_CurrentServerCurrentPingTime = -1;
	m_CurrentServerNextPingTime = -1;

	// disable all downloads
	m_MapdownloadChunk = 0;
	if(m_pMapdownloadTask)
		m_pMapdownloadTask->Abort();
	if(m_MapdownloadFileTemp)
	{
		io_close(m_MapdownloadFileTemp);
		Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
	}
	m_MapdownloadFileTemp = 0;
	m_MapdownloadSha256Present = false;
	m_MapdownloadSha256 = SHA256_ZEROED;
	m_MapdownloadCrc = 0;
	m_MapdownloadTotalsize = -1;
	m_MapdownloadAmount = 0;
	m_MapDetailsPresent = false;

	// clear the current server info
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));

	// clear snapshots
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = 0;
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV] = 0;
	m_aReceivedSnapshots[g_Config.m_ClDummy] = 0;
}

void CClient::Disconnect()
{
	m_ButtonRender = false;
	if(m_DummyConnected)
		DummyDisconnect(0);
	if(m_State != IClient::STATE_OFFLINE)
		DisconnectWithReason(0);

	// make sure to remove replay tmp demo
	if(g_Config.m_ClReplays)
	{
		Storage()->RemoveFile((&m_aDemoRecorder[RECORDER_REPLAYS])->GetCurrentFilename(), IStorage::TYPE_SAVE);
	}
}

bool CClient::DummyConnected()
{
	return m_DummyConnected;
}

bool CClient::DummyConnecting()
{
	return !m_DummyConnected && m_LastDummyConnectTime > 0 && m_LastDummyConnectTime + GameTickSpeed() * 5 > GameTick(g_Config.m_ClDummy);
}

void CClient::DummyConnect()
{
	if(m_LastDummyConnectTime > 0 && m_LastDummyConnectTime + GameTickSpeed() * 5 > GameTick(g_Config.m_ClDummy))
		return;

	if(m_aNetClient[CONN_MAIN].State() != NET_CONNSTATE_ONLINE && m_aNetClient[CONN_MAIN].State() != NET_CONNSTATE_PENDING)
		return;

	if(m_DummyConnected || !DummyAllowed())
		return;

	m_LastDummyConnectTime = GameTick(g_Config.m_ClDummy);

	m_aRconAuthed[1] = 0;

	m_DummySendConnInfo = true;

	g_Config.m_ClDummyCopyMoves = 0;
	g_Config.m_ClDummyHammer = 0;

	// connect to the server
	m_aNetClient[CONN_DUMMY].Connect(m_aNetClient[CONN_MAIN].ServerAddress(), 1);
}

void CClient::DummyDisconnect(const char *pReason)
{
	if(!m_DummyConnected)
		return;

	m_aNetClient[CONN_DUMMY].Disconnect(pReason);
	g_Config.m_ClDummy = 0;
	m_aRconAuthed[1] = 0;
	m_aapSnapshots[1][SNAP_CURRENT] = 0;
	m_aapSnapshots[1][SNAP_PREV] = 0;
	m_aReceivedSnapshots[1] = 0;
	m_DummyConnected = false;
	GameClient()->OnDummyDisconnect();
}

bool CClient::DummyAllowed()
{
	return m_ServerCapabilities.m_AllowDummy;
}

int CClient::GetCurrentRaceTime()
{
	if(GameClient()->GetLastRaceTick() < 0)
		return 0;
	return (GameTick(g_Config.m_ClDummy) - GameClient()->GetLastRaceTick()) / 50;
}

void CClient::GetServerInfo(CServerInfo *pServerInfo) const
{
	mem_copy(pServerInfo, &m_CurrentServerInfo, sizeof(m_CurrentServerInfo));

	if(m_DemoPlayer.IsPlaying() && g_Config.m_ClDemoAssumeRace)
		str_copy(pServerInfo->m_aGameType, "DDraceNetwork");
}

void CClient::ServerInfoRequest()
{
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	m_CurrentServerInfoRequestTime = 0;
}

int CClient::LoadData()
{
	m_DebugFont = Graphics()->LoadTexture("debug_font.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	return 1;
}

// ---

void *CClient::SnapGetItem(int SnapID, int Index, CSnapItem *pItem) const
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	const CSnapshotItem *pSnapshotItem = m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItem(Index);
	pItem->m_DataSize = m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemSize(Index);
	pItem->m_Type = m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemType(Index);
	pItem->m_ID = pSnapshotItem->ID();
	return (void *)pSnapshotItem->Data();
}

int CClient::SnapItemSize(int SnapID, int Index) const
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	return m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->GetItemSize(Index);
}

const void *CClient::SnapFindItem(int SnapID, int Type, int ID) const
{
	if(!m_aapSnapshots[g_Config.m_ClDummy][SnapID])
		return 0x0;

	return m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->FindItem(Type, ID);
}

int CClient::SnapNumItems(int SnapID) const
{
	dbg_assert(SnapID >= 0 && SnapID < NUM_SNAPSHOT_TYPES, "invalid SnapID");
	if(!m_aapSnapshots[g_Config.m_ClDummy][SnapID])
		return 0;
	return m_aapSnapshots[g_Config.m_ClDummy][SnapID]->m_pAltSnap->NumItems();
}

void CClient::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

void CClient::DebugRender()
{
	static NETSTATS Prev, Current;
	static int64_t LastSnap = 0;
	static float FrameTimeAvg = 0;
	char aBuffer[512];

	if(!g_Config.m_Debug)
		return;

	//m_pGraphics->BlendNormal();
	Graphics()->TextureSet(m_DebugFont);
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();

	if(time_get() - LastSnap > time_freq())
	{
		LastSnap = time_get();
		Prev = Current;
		net_stats(&Current);
	}

	/*
		eth = 14
		ip = 20
		udp = 8
		total = 42
	*/
	FrameTimeAvg = FrameTimeAvg * 0.9f + m_RenderFrameTime * 0.1f;
	str_format(aBuffer, sizeof(aBuffer), "ticks: %8d %8d gfx mem(tex/buff/stream/staging): (%" PRIu64 "k/%" PRIu64 "k/%" PRIu64 "k/%" PRIu64 "k) fps: %3d",
		m_aCurGameTick[g_Config.m_ClDummy], m_aPredTick[g_Config.m_ClDummy],
		(Graphics()->TextureMemoryUsage() / 1024),
		(Graphics()->BufferMemoryUsage() / 1024),
		(Graphics()->StreamedMemoryUsage() / 1024),
		(Graphics()->StagingMemoryUsage() / 1024),
		(int)(1.0f / FrameTimeAvg + 0.5f));
	Graphics()->QuadsText(2, 2, 16, aBuffer);

	{
		uint64_t SendPackets = (Current.sent_packets - Prev.sent_packets);
		uint64_t SendBytes = (Current.sent_bytes - Prev.sent_bytes);
		uint64_t SendTotal = SendBytes + SendPackets * 42;
		uint64_t RecvPackets = (Current.recv_packets - Prev.recv_packets);
		uint64_t RecvBytes = (Current.recv_bytes - Prev.recv_bytes);
		uint64_t RecvTotal = RecvBytes + RecvPackets * 42;

		if(!SendPackets)
			SendPackets++;
		if(!RecvPackets)
			RecvPackets++;
		str_format(aBuffer, sizeof(aBuffer), "send: %3" PRIu64 " %5" PRIu64 "+%4" PRIu64 "=%5" PRIu64 " (%3" PRIu64 " kbps) avg: %5" PRIu64 "\nrecv: %3" PRIu64 " %5" PRIu64 "+%4" PRIu64 "=%5" PRIu64 " (%3" PRIu64 " kbps) avg: %5" PRIu64,
			SendPackets, SendBytes, SendPackets * 42, SendTotal, (SendTotal * 8) / 1024, SendBytes / SendPackets,
			RecvPackets, RecvBytes, RecvPackets * 42, RecvTotal, (RecvTotal * 8) / 1024, RecvBytes / RecvPackets);
		Graphics()->QuadsText(2, 14, 16, aBuffer);
	}

	// render rates
	{
		int y = 0;
		str_format(aBuffer, sizeof(aBuffer), "%5s %20s: %8s %8s %8s", "ID", "Name", "Rate", "Updates", "R/U");
		Graphics()->QuadsText(2, 100 + y * 12, 16, aBuffer);
		y++;
		for(int i = 0; i < NUM_NETOBJTYPES; i++)
		{
			if(m_SnapshotDelta.GetDataRate(i))
			{
				str_format(aBuffer, sizeof(aBuffer), "%5d %20s: %8d %8d %8d", i, GameClient()->GetItemName(i), m_SnapshotDelta.GetDataRate(i) / 8, m_SnapshotDelta.GetDataUpdates(i),
					(m_SnapshotDelta.GetDataRate(i) / m_SnapshotDelta.GetDataUpdates(i)) / 8);
				Graphics()->QuadsText(2, 100 + y * 12, 16, aBuffer);
				y++;
			}
		}
		for(int i = CSnapshot::MAX_TYPE; i > (CSnapshot::MAX_TYPE - 64); i--)
		{
			if(m_SnapshotDelta.GetDataRate(i) && m_aapSnapshots[g_Config.m_ClDummy][IClient::SNAP_CURRENT])
			{
				int Type = m_aapSnapshots[g_Config.m_ClDummy][IClient::SNAP_CURRENT]->m_pAltSnap->GetExternalItemType(i);
				if(Type == UUID_INVALID)
				{
					str_format(aBuffer, sizeof(aBuffer), "%5d %20s: %8d %8d %8d", i, "Unknown UUID", m_SnapshotDelta.GetDataRate(i) / 8, m_SnapshotDelta.GetDataUpdates(i),
						(m_SnapshotDelta.GetDataRate(i) / m_SnapshotDelta.GetDataUpdates(i)) / 8);
					Graphics()->QuadsText(2, 100 + y * 12, 16, aBuffer);
					y++;
				}
				else if(Type != i)
				{
					str_format(aBuffer, sizeof(aBuffer), "%5d %20s: %8d %8d %8d", Type, GameClient()->GetItemName(Type), m_SnapshotDelta.GetDataRate(i) / 8, m_SnapshotDelta.GetDataUpdates(i),
						(m_SnapshotDelta.GetDataRate(i) / m_SnapshotDelta.GetDataUpdates(i)) / 8);
					Graphics()->QuadsText(2, 100 + y * 12, 16, aBuffer);
					y++;
				}
			}
		}
	}

	str_format(aBuffer, sizeof(aBuffer), "pred: %d ms", GetPredictionTime());
	Graphics()->QuadsText(2, 70, 16, aBuffer);
	Graphics()->QuadsEnd();

	// render graphs
	if(g_Config.m_DbgGraphs)
	{
		//Graphics()->MapScreen(0,0,400.0f,300.0f);
		float w = Graphics()->ScreenWidth() / 4.0f;
		float h = Graphics()->ScreenHeight() / 6.0f;
		float sp = Graphics()->ScreenWidth() / 100.0f;
		float x = Graphics()->ScreenWidth() - w - sp;

		m_FpsGraph.Scale();
		m_FpsGraph.Render(Graphics(), m_DebugFont, x, sp * 5, w, h, "FPS");
		m_InputtimeMarginGraph.Scale();
		m_InputtimeMarginGraph.Render(Graphics(), m_DebugFont, x, sp * 5 + h + sp, w, h, "Prediction Margin");
		m_GametimeMarginGraph.Scale();
		m_GametimeMarginGraph.Render(Graphics(), m_DebugFont, x, sp * 5 + h + sp + h + sp, w, h, "Gametime Margin");
	}
}

void CClient::Restart()
{
	SetState(IClient::STATE_RESTARTING);
}

void CClient::Quit()
{
	SetState(IClient::STATE_QUITTING);
}

const char *CClient::PlayerName() const
{
	if(g_Config.m_PlayerName[0])
	{
		return g_Config.m_PlayerName;
	}
	if(g_Config.m_SteamName[0])
	{
		return g_Config.m_SteamName;
	}
	return "nameless tee";
}

const char *CClient::DummyName() const
{
	if(g_Config.m_ClDummyName[0])
	{
		return g_Config.m_ClDummyName;
	}
	const char *pBase = 0;
	if(g_Config.m_PlayerName[0])
	{
		pBase = g_Config.m_PlayerName;
	}
	else if(g_Config.m_SteamName[0])
	{
		pBase = g_Config.m_SteamName;
	}
	if(pBase)
	{
		static char aDummyNameBuf[16];
		str_format(aDummyNameBuf, sizeof(aDummyNameBuf), "[D] %s", pBase);
		return aDummyNameBuf;
	}
	return "brainless tee";
}

const char *CClient::ErrorString() const
{
	return m_aNetClient[CONN_MAIN].ErrorString();
}

void CClient::Render()
{
	if(g_Config.m_ClOverlayEntities)
	{
		ColorRGBA bg = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClBackgroundEntitiesColor));
		Graphics()->Clear(bg.r, bg.g, bg.b);
	}
	else
	{
		ColorRGBA bg = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClBackgroundColor));
		Graphics()->Clear(bg.r, bg.g, bg.b);
	}

	GameClient()->OnRender();
	DebugRender();

	if(State() == IClient::STATE_ONLINE && g_Config.m_ClAntiPingLimit)
	{
		int64_t Now = time_get();
		g_Config.m_ClAntiPing = (m_PredictedTime.Get(Now) - m_aGameTime[g_Config.m_ClDummy].Get(Now)) * 1000 / (float)time_freq() > g_Config.m_ClAntiPingLimit;
	}
}

const char *CClient::LoadMap(const char *pName, const char *pFilename, SHA256_DIGEST *pWantedSha256, unsigned WantedCrc)
{
	static char s_aErrorMsg[128];

	SetState(IClient::STATE_LOADING);
	SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_LOADING_MAP);

	if((bool)m_MapLoadingCBFunc)
		m_MapLoadingCBFunc();

	if(!m_pMap->Load(pFilename))
	{
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map '%s' not found", pFilename);
		return s_aErrorMsg;
	}

	if(pWantedSha256 && m_pMap->Sha256() != *pWantedSha256)
	{
		char aWanted[SHA256_MAXSTRSIZE];
		char aGot[SHA256_MAXSTRSIZE];
		sha256_str(*pWantedSha256, aWanted, sizeof(aWanted));
		sha256_str(m_pMap->Sha256(), aGot, sizeof(aWanted));
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map differs from the server. %s != %s", aGot, aWanted);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", s_aErrorMsg);
		m_pMap->Unload();
		return s_aErrorMsg;
	}

	// Only check CRC if we don't have the secure SHA256.
	if(!pWantedSha256 && m_pMap->Crc() != WantedCrc)
	{
		str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "map differs from the server. %08x != %08x", m_pMap->Crc(), WantedCrc);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", s_aErrorMsg);
		m_pMap->Unload();
		return s_aErrorMsg;
	}

	// stop demo recording if we loaded a new map
	for(int i = 0; i < RECORDER_MAX; i++)
		DemoRecorder_Stop(i, i == RECORDER_REPLAYS);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", pFilename);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	m_aReceivedSnapshots[g_Config.m_ClDummy] = 0;

	str_copy(m_aCurrentMap, pName);
	str_copy(m_aCurrentMapPath, pFilename);

	return 0;
}

static void FormatMapDownloadFilename(const char *pName, const SHA256_DIGEST *pSha256, int Crc, bool Temp, char *pBuffer, int BufferSize)
{
	char aSuffix[32];
	if(Temp)
	{
		IStorage::FormatTmpPath(aSuffix, sizeof(aSuffix), "");
	}
	else
	{
		str_copy(aSuffix, ".map");
	}

	if(pSha256)
	{
		char aSha256[SHA256_MAXSTRSIZE];
		sha256_str(*pSha256, aSha256, sizeof(aSha256));
		str_format(pBuffer, BufferSize, "downloadedmaps/%s_%s%s", pName, aSha256, aSuffix);
	}
	else
	{
		str_format(pBuffer, BufferSize, "downloadedmaps/%s_%08x%s", pName, Crc, aSuffix);
	}
}

const char *CClient::LoadMapSearch(const char *pMapName, SHA256_DIGEST *pWantedSha256, int WantedCrc)
{
	char aBuf[512];
	char aWanted[SHA256_MAXSTRSIZE + 16];
	aWanted[0] = 0;
	if(pWantedSha256)
	{
		char aWantedSha256[SHA256_MAXSTRSIZE];
		sha256_str(*pWantedSha256, aWantedSha256, sizeof(aWantedSha256));
		str_format(aWanted, sizeof(aWanted), "sha256=%s ", aWantedSha256);
	}
	str_format(aBuf, sizeof(aBuf), "loading map, map=%s wanted %scrc=%08x", pMapName, aWanted, WantedCrc);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
	SetState(IClient::STATE_LOADING);
	SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_LOADING_MAP);

	// try the normal maps folder
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);
	const char *pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return nullptr;

	// try the downloaded maps
	FormatMapDownloadFilename(pMapName, pWantedSha256, WantedCrc, false, aBuf, sizeof(aBuf));
	pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
	if(!pError)
		return nullptr;

	// backward compatibility with old names
	if(pWantedSha256)
	{
		FormatMapDownloadFilename(pMapName, 0, WantedCrc, false, aBuf, sizeof(aBuf));
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
		if(!pError)
			return nullptr;
	}

	// search for the map within subfolders
	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s.map", pMapName);
	if(Storage()->FindFile(aFilename, "maps", IStorage::TYPE_ALL, aBuf, sizeof(aBuf)))
	{
		pError = LoadMap(pMapName, aBuf, pWantedSha256, WantedCrc);
		if(!pError)
			return nullptr;
	}

	static char s_aErrorMsg[256];
	str_format(s_aErrorMsg, sizeof(s_aErrorMsg), "Could not find map '%s'", pMapName);
	return s_aErrorMsg;
}

void CClient::ProcessConnlessPacket(CNetChunk *pPacket)
{
	// server info
	if(pPacket->m_DataSize >= (int)sizeof(SERVERBROWSE_INFO))
	{
		int Type = -1;
		if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO)) == 0)
			Type = SERVERINFO_VANILLA;
		else if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED)) == 0)
			Type = SERVERINFO_EXTENDED;
		else if(mem_comp(pPacket->m_pData, SERVERBROWSE_INFO_EXTENDED_MORE, sizeof(SERVERBROWSE_INFO_EXTENDED_MORE)) == 0)
			Type = SERVERINFO_EXTENDED_MORE;

		if(Type != -1)
		{
			void *pData = (unsigned char *)pPacket->m_pData + sizeof(SERVERBROWSE_INFO);
			int DataSize = pPacket->m_DataSize - sizeof(SERVERBROWSE_INFO);
			ProcessServerInfo(Type, &pPacket->m_Address, pData, DataSize);
		}
	}
}

static int SavedServerInfoType(int Type)
{
	if(Type == SERVERINFO_EXTENDED_MORE)
		return SERVERINFO_EXTENDED;

	return Type;
}

void CClient::ProcessServerInfo(int RawType, NETADDR *pFrom, const void *pData, int DataSize)
{
	CServerBrowser::CServerEntry *pEntry = m_ServerBrowser.Find(*pFrom);

	CServerInfo Info = {0};
	int SavedType = SavedServerInfoType(RawType);
	if(SavedType == SERVERINFO_EXTENDED && pEntry && pEntry->m_GotInfo && SavedType == pEntry->m_Info.m_Type)
	{
		Info = pEntry->m_Info;
	}

	Info.m_Type = SavedType;

	net_addr_str(pFrom, Info.m_aAddress, sizeof(Info.m_aAddress), true);

	CUnpacker Up;
	Up.Reset(pData, DataSize);

#define GET_STRING(array) str_copy(array, Up.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES), sizeof(array))
#define GET_INT(integer) (integer) = str_toint(Up.GetString())

	int Token;
	int PacketNo = 0; // Only used if SavedType == SERVERINFO_EXTENDED

	GET_INT(Token);
	if(RawType != SERVERINFO_EXTENDED_MORE)
	{
		GET_STRING(Info.m_aVersion);
		GET_STRING(Info.m_aName);
		GET_STRING(Info.m_aMap);

		if(SavedType == SERVERINFO_EXTENDED)
		{
			GET_INT(Info.m_MapCrc);
			GET_INT(Info.m_MapSize);
		}

		GET_STRING(Info.m_aGameType);
		GET_INT(Info.m_Flags);
		GET_INT(Info.m_NumPlayers);
		GET_INT(Info.m_MaxPlayers);
		GET_INT(Info.m_NumClients);
		GET_INT(Info.m_MaxClients);
		if(Info.m_aMap[0])
			Info.m_HasRank = m_ServerBrowser.HasRank(Info.m_aMap);

		// don't add invalid info to the server browser list
		if(Info.m_NumClients < 0 || Info.m_MaxClients < 0 ||
			Info.m_NumPlayers < 0 || Info.m_MaxPlayers < 0 ||
			Info.m_NumPlayers > Info.m_NumClients || Info.m_MaxPlayers > Info.m_MaxClients)
		{
			return;
		}

		switch(SavedType)
		{
		case SERVERINFO_VANILLA:
			if(Info.m_MaxPlayers > VANILLA_MAX_CLIENTS ||
				Info.m_MaxClients > VANILLA_MAX_CLIENTS)
			{
				return;
			}
			break;
		case SERVERINFO_64_LEGACY:
			if(Info.m_MaxPlayers > MAX_CLIENTS ||
				Info.m_MaxClients > MAX_CLIENTS)
			{
				return;
			}
			break;
		case SERVERINFO_EXTENDED:
			if(Info.m_NumPlayers > Info.m_NumClients)
				return;
			break;
		default:
			dbg_assert(false, "unknown serverinfo type");
		}

		if(SavedType == SERVERINFO_EXTENDED)
			PacketNo = 0;
	}
	else
	{
		GET_INT(PacketNo);
		// 0 needs to be excluded because that's reserved for the main packet.
		if(PacketNo <= 0 || PacketNo >= 64)
			return;
	}

	bool DuplicatedPacket = false;
	if(SavedType == SERVERINFO_EXTENDED)
	{
		Up.GetString(); // extra info, reserved

		uint64_t Flag = (uint64_t)1 << PacketNo;
		DuplicatedPacket = Info.m_ReceivedPackets & Flag;
		Info.m_ReceivedPackets |= Flag;
	}

	bool IgnoreError = false;
	for(int i = 0; i < MAX_CLIENTS && Info.m_NumReceivedClients < MAX_CLIENTS && !Up.Error(); i++)
	{
		CServerInfo::CClient *pClient = &Info.m_aClients[Info.m_NumReceivedClients];
		GET_STRING(pClient->m_aName);
		if(Up.Error())
		{
			// Packet end, no problem unless it happens during one
			// player info, so ignore the error.
			IgnoreError = true;
			break;
		}
		GET_STRING(pClient->m_aClan);
		GET_INT(pClient->m_Country);
		GET_INT(pClient->m_Score);
		GET_INT(pClient->m_Player);
		if(SavedType == SERVERINFO_EXTENDED)
		{
			Up.GetString(); // extra info, reserved
		}
		if(!Up.Error())
		{
			if(SavedType == SERVERINFO_64_LEGACY)
			{
				uint64_t Flag = (uint64_t)1 << i;
				if(!(Info.m_ReceivedPackets & Flag))
				{
					Info.m_ReceivedPackets |= Flag;
					Info.m_NumReceivedClients++;
				}
			}
			else
			{
				Info.m_NumReceivedClients++;
			}
		}
	}

	str_clean_whitespaces(Info.m_aName);

	if(!Up.Error() || IgnoreError)
	{
		if(!DuplicatedPacket && (!pEntry || !pEntry->m_GotInfo || SavedType >= pEntry->m_Info.m_Type))
		{
			m_ServerBrowser.OnServerInfoUpdate(*pFrom, Token, &Info);
		}

		// Player info is irrelevant for the client (while connected),
		// it gets its info from elsewhere.
		//
		// SERVERINFO_EXTENDED_MORE doesn't carry any server
		// information, so just skip it.
		if(net_addr_comp(&ServerAddress(), pFrom) == 0 && RawType != SERVERINFO_EXTENDED_MORE)
		{
			// Only accept server info that has a type that is
			// newer or equal to something the server already sent
			// us.
			if(SavedType >= m_CurrentServerInfo.m_Type)
			{
				mem_copy(&m_CurrentServerInfo, &Info, sizeof(m_CurrentServerInfo));
				m_CurrentServerInfo.m_NumAddresses = 1;
				m_CurrentServerInfo.m_aAddresses[0] = ServerAddress();
				m_CurrentServerInfoRequestTime = -1;
			}

			bool ValidPong = false;
			if(!m_ServerCapabilities.m_PingEx && m_CurrentServerCurrentPingTime >= 0 && SavedType >= m_CurrentServerPingInfoType)
			{
				if(RawType == SERVERINFO_VANILLA)
				{
					ValidPong = Token == m_CurrentServerPingBasicToken;
				}
				else if(RawType == SERVERINFO_EXTENDED)
				{
					ValidPong = Token == m_CurrentServerPingToken;
				}
			}
			if(ValidPong)
			{
				int LatencyMs = (time_get() - m_CurrentServerCurrentPingTime) * 1000 / time_freq();
				m_ServerBrowser.SetCurrentServerPing(ServerAddress(), LatencyMs);
				m_CurrentServerPingInfoType = SavedType;
				m_CurrentServerCurrentPingTime = -1;

				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "got pong from current server, latency=%dms", LatencyMs);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
			}
		}
	}

#undef GET_STRING
#undef GET_INT
}

static CServerCapabilities GetServerCapabilities(int Version, int Flags)
{
	CServerCapabilities Result;
	bool DDNet = false;
	if(Version >= 1)
	{
		DDNet = Flags & SERVERCAPFLAG_DDNET;
	}
	Result.m_ChatTimeoutCode = DDNet;
	Result.m_AnyPlayerFlag = DDNet;
	Result.m_PingEx = false;
	Result.m_AllowDummy = true;
	Result.m_SyncWeaponInput = false;
	if(Version >= 1)
	{
		Result.m_ChatTimeoutCode = Flags & SERVERCAPFLAG_CHATTIMEOUTCODE;
	}
	if(Version >= 2)
	{
		Result.m_AnyPlayerFlag = Flags & SERVERCAPFLAG_ANYPLAYERFLAG;
	}
	if(Version >= 3)
	{
		Result.m_PingEx = Flags & SERVERCAPFLAG_PINGEX;
	}
	if(Version >= 4)
	{
		Result.m_AllowDummy = Flags & SERVERCAPFLAG_ALLOWDUMMY;
	}
	if(Version >= 5)
	{
		Result.m_SyncWeaponInput = Flags & SERVERCAPFLAG_SYNCWEAPONINPUT;
	}
	return Result;
}

void CClient::ProcessServerPacket(CNetChunk *pPacket, int Conn, bool Dummy)
{
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX, true);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}
	else if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(Conn, &Packer, MSGFLAG_VITAL);
	}

	if(Sys)
	{
		// system message
		if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_DETAILS)
		{
			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES);
			SHA256_DIGEST *pMapSha256 = (SHA256_DIGEST *)Unpacker.GetRaw(sizeof(*pMapSha256));
			int MapCrc = Unpacker.GetInt();
			int MapSize = Unpacker.GetInt();

			if(Unpacker.Error())
			{
				return;
			}

			const char *pMapUrl = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error())
			{
				pMapUrl = "";
			}

			m_MapDetailsPresent = true;
			(void)MapSize;
			str_copy(m_aMapDetailsName, pMap);
			m_MapDetailsSha256 = *pMapSha256;
			m_MapDetailsCrc = MapCrc;
			str_copy(m_aMapDetailsUrl, pMapUrl);
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_CAPABILITIES)
		{
			if(!m_CanReceiveServerCapabilities)
			{
				return;
			}
			int Version = Unpacker.GetInt();
			int Flags = Unpacker.GetInt();
			if(Version <= 0)
			{
				return;
			}
			m_ServerCapabilities = GetServerCapabilities(Version, Flags);
			m_CanReceiveServerCapabilities = false;
			m_ServerSentCapabilities = true;
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_CHANGE)
		{
			if(m_CanReceiveServerCapabilities)
			{
				m_ServerCapabilities = GetServerCapabilities(0, 0);
				m_CanReceiveServerCapabilities = false;
			}
			bool MapDetailsWerePresent = m_MapDetailsPresent;
			m_MapDetailsPresent = false;

			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES);
			int MapCrc = Unpacker.GetInt();
			int MapSize = Unpacker.GetInt();
			const char *pError = 0;

			if(Unpacker.Error())
				return;

			if(m_DummyConnected)
				DummyDisconnect(0);

			for(int i = 0; pMap[i]; i++) // protect the player from nasty map names
			{
				if(pMap[i] == '/' || pMap[i] == '\\')
					pError = "strange character in map name";
			}

			if(MapSize < 0)
				pError = "invalid map size";

			if(pError)
				DisconnectWithReason(pError);
			else
			{
				SHA256_DIGEST *pMapSha256 = 0;
				const char *pMapUrl = nullptr;
				if(MapDetailsWerePresent && str_comp(m_aMapDetailsName, pMap) == 0 && m_MapDetailsCrc == MapCrc)
				{
					pMapSha256 = &m_MapDetailsSha256;
					pMapUrl = m_aMapDetailsUrl[0] ? m_aMapDetailsUrl : nullptr;
				}
				pError = LoadMapSearch(pMap, pMapSha256, MapCrc);

				if(!pError)
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
					SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_SENDING_READY);
					SendReady();
				}
				else
				{
					if(m_MapdownloadFileTemp)
					{
						io_close(m_MapdownloadFileTemp);
						Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
					}

					// start map download
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, false, m_aMapdownloadFilename, sizeof(m_aMapdownloadFilename));
					FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, true, m_aMapdownloadFilenameTemp, sizeof(m_aMapdownloadFilenameTemp));

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "starting to download map to '%s'", m_aMapdownloadFilenameTemp);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", aBuf);

					m_MapdownloadChunk = 0;
					str_copy(m_aMapdownloadName, pMap);

					m_MapdownloadSha256Present = (bool)pMapSha256;
					m_MapdownloadSha256 = pMapSha256 ? *pMapSha256 : SHA256_ZEROED;
					m_MapdownloadCrc = MapCrc;
					m_MapdownloadTotalsize = MapSize;
					m_MapdownloadAmount = 0;

					ResetMapDownload();

					if(pMapSha256)
					{
						char aUrl[256];
						char aEscaped[256];
						EscapeUrl(aEscaped, sizeof(aEscaped), m_aMapdownloadFilename + 15); // cut off downloadedmaps/
						bool UseConfigUrl = str_comp(g_Config.m_ClMapDownloadUrl, "https://maps.ddnet.org") != 0 || m_aMapDownloadUrl[0] == '\0';
						str_format(aUrl, sizeof(aUrl), "%s/%s", UseConfigUrl ? g_Config.m_ClMapDownloadUrl : m_aMapDownloadUrl, aEscaped);

						m_pMapdownloadTask = HttpGetFile(pMapUrl ? pMapUrl : aUrl, Storage(), m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
						m_pMapdownloadTask->Timeout(CTimeout{g_Config.m_ClMapDownloadConnectTimeoutMs, 0, g_Config.m_ClMapDownloadLowSpeedLimit, g_Config.m_ClMapDownloadLowSpeedTime});
						m_pMapdownloadTask->MaxResponseSize(1024 * 1024 * 1024); // 1 GiB
						Engine()->AddJob(m_pMapdownloadTask);
					}
					else
						SendMapRequest();
				}
			}
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_MAP_DATA)
		{
			int Last = Unpacker.GetInt();
			int MapCRC = Unpacker.GetInt();
			int Chunk = Unpacker.GetInt();
			int Size = Unpacker.GetInt();
			const unsigned char *pData = Unpacker.GetRaw(Size);

			// check for errors
			if(Unpacker.Error() || Size <= 0 || MapCRC != m_MapdownloadCrc || Chunk != m_MapdownloadChunk || !m_MapdownloadFileTemp)
				return;

			io_write(m_MapdownloadFileTemp, pData, Size);

			m_MapdownloadAmount += Size;

			if(Last)
			{
				if(m_MapdownloadFileTemp)
				{
					io_close(m_MapdownloadFileTemp);
					m_MapdownloadFileTemp = 0;
				}
				FinishMapDownload();
			}
			else
			{
				// request new chunk
				m_MapdownloadChunk++;

				CMsgPacker MsgP(NETMSG_REQUEST_MAP_DATA, true);
				MsgP.AddInt(m_MapdownloadChunk);
				SendMsg(CONN_MAIN, &MsgP, MSGFLAG_VITAL | MSGFLAG_FLUSH);

				if(g_Config.m_Debug)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "requested chunk %d", m_MapdownloadChunk);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client/network", aBuf);
				}
			}
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_CON_READY)
		{
			GameClient()->OnConnected();
		}
		else if(Conn == CONN_DUMMY && Msg == NETMSG_CON_READY)
		{
			m_DummyConnected = true;
			g_Config.m_ClDummy = 1;
			Rcon("crashmeplx");
			if(m_aRconAuthed[0])
				RconAuth("", m_aRconPassword);
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker MsgP(NETMSG_PING_REPLY, true);
			SendMsg(Conn, &MsgP, 0);
		}
		else if(Msg == NETMSG_PINGEX)
		{
			CUuid *pID = (CUuid *)Unpacker.GetRaw(sizeof(*pID));
			if(Unpacker.Error())
			{
				return;
			}
			CMsgPacker MsgP(NETMSG_PONGEX, true);
			MsgP.AddRaw(pID, sizeof(*pID));
			SendMsg(Conn, &MsgP, MSGFLAG_FLUSH);
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_PONGEX)
		{
			CUuid *pID = (CUuid *)Unpacker.GetRaw(sizeof(*pID));
			if(Unpacker.Error())
			{
				return;
			}
			if(m_ServerCapabilities.m_PingEx && m_CurrentServerCurrentPingTime >= 0 && *pID == m_CurrentServerPingUuid)
			{
				int LatencyMs = (time_get() - m_CurrentServerCurrentPingTime) * 1000 / time_freq();
				m_ServerBrowser.SetCurrentServerPing(ServerAddress(), LatencyMs);
				m_CurrentServerCurrentPingTime = -1;

				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "got pong from current server, latency=%dms", LatencyMs);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
			}
		}
		else if(Msg == NETMSG_CHECKSUM_REQUEST)
		{
			CUuid *pUuid = (CUuid *)Unpacker.GetRaw(sizeof(*pUuid));
			if(Unpacker.Error())
			{
				return;
			}
			int ResultCheck = HandleChecksum(Conn, *pUuid, &Unpacker);
			if(ResultCheck)
			{
				CMsgPacker MsgP(NETMSG_CHECKSUM_ERROR, true);
				MsgP.AddRaw(pUuid, sizeof(*pUuid));
				MsgP.AddInt(ResultCheck);
				SendMsg(Conn, &MsgP, MSGFLAG_VITAL);
			}
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_ADD)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pHelp = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pParams = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->RegisterTemp(pName, pParams, CFGFLAG_SERVER, pHelp);
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_REM)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(Unpacker.Error() == 0)
				m_pConsole->DeregisterTemp(pName);
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_AUTH_STATUS)
		{
			int ResultInt = Unpacker.GetInt();
			if(Unpacker.Error() == 0)
				m_aRconAuthed[Conn] = ResultInt;
			if(Conn == CONN_MAIN)
			{
				int Old = m_UseTempRconCommands;
				m_UseTempRconCommands = Unpacker.GetInt();
				if(Unpacker.Error() != 0)
					m_UseTempRconCommands = 0;
				if(Old != 0 && m_UseTempRconCommands == 0)
					m_pConsole->DeregisterTempAll();
			}
		}
		else if(!Dummy && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_LINE)
		{
			const char *pLine = Unpacker.GetString();
			if(Unpacker.Error() == 0)
				GameClient()->OnRconLine(pLine);
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_PING_REPLY)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "latency %.2f", (time_get() - m_PingStartTime) * 1000 / (float)time_freq());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client/network", aBuf);
		}
		else if(Msg == NETMSG_INPUTTIMING)
		{
			int InputPredTick = Unpacker.GetInt();
			int TimeLeft = Unpacker.GetInt();
			int64_t Now = time_get();

			// adjust our prediction time
			int64_t Target = 0;
			for(int k = 0; k < 200; k++)
			{
				if(m_aInputs[Conn][k].m_Tick == InputPredTick)
				{
					Target = m_aInputs[Conn][k].m_PredictedTime + (Now - m_aInputs[Conn][k].m_Time);
					Target = Target - (int64_t)((TimeLeft / 1000.0f) * time_freq()) + m_aInputs[Conn][k].m_PredictionMargin;
					break;
				}
			}

			if(Target)
				m_PredictedTime.Update(&m_InputtimeMarginGraph, Target, TimeLeft, 1);
		}
		else if(Msg == NETMSG_SNAP || Msg == NETMSG_SNAPSINGLE || Msg == NETMSG_SNAPEMPTY)
		{
			int GameTick = Unpacker.GetInt();
			int DeltaTick = GameTick - Unpacker.GetInt();

			// only allow packets from the server we actually want
			if(net_addr_comp(&pPacket->m_Address, &ServerAddress()))
				return;

			// we are not allowed to process snapshot yet
			if(State() < IClient::STATE_LOADING)
				return;

			int NumParts = 1;
			int Part = 0;
			if(Msg == NETMSG_SNAP)
			{
				NumParts = Unpacker.GetInt();
				Part = Unpacker.GetInt();
			}

			unsigned int Crc = 0;
			int PartSize = 0;
			if(Msg != NETMSG_SNAPEMPTY)
			{
				Crc = Unpacker.GetInt();
				PartSize = Unpacker.GetInt();
			}

			const char *pData = (const char *)Unpacker.GetRaw(PartSize);

			if(Unpacker.Error() || NumParts < 1 || NumParts > CSnapshot::MAX_PARTS || Part < 0 || Part >= NumParts || PartSize < 0 || PartSize > MAX_SNAPSHOT_PACKSIZE)
				return;

			// Check m_aAckGameTick to see if we already got a snapshot for that tick
			if(GameTick >= m_aCurrentRecvTick[Conn] && GameTick > m_aAckGameTick[Conn])
			{
				if(GameTick != m_aCurrentRecvTick[Conn])
				{
					m_aSnapshotParts[Conn] = 0;
					m_aCurrentRecvTick[Conn] = GameTick;
					m_aSnapshotIncomingDataSize[Conn] = 0;
				}

				mem_copy((char *)m_aaSnapshotIncomingData[Conn] + Part * MAX_SNAPSHOT_PACKSIZE, pData, clamp(PartSize, 0, (int)sizeof(m_aaSnapshotIncomingData[Conn]) - Part * MAX_SNAPSHOT_PACKSIZE));
				m_aSnapshotParts[Conn] |= (uint64_t)(1) << Part;

				if(Part == NumParts - 1)
				{
					m_aSnapshotIncomingDataSize[Conn] = (NumParts - 1) * MAX_SNAPSHOT_PACKSIZE + PartSize;
				}

				if((NumParts < CSnapshot::MAX_PARTS && m_aSnapshotParts[Conn] == (((uint64_t)(1) << NumParts) - 1)) ||
					(NumParts == CSnapshot::MAX_PARTS && m_aSnapshotParts[Conn] == std::numeric_limits<uint64_t>::max()))
				{
					static CSnapshot Emptysnap;
					CSnapshot *pDeltaShot = &Emptysnap;
					unsigned char aTmpBuffer2[CSnapshot::MAX_SIZE];
					unsigned char aTmpBuffer3[CSnapshot::MAX_SIZE];
					CSnapshot *pTmpBuffer3 = (CSnapshot *)aTmpBuffer3; // Fix compiler warning for strict-aliasing

					// reset snapshoting
					m_aSnapshotParts[Conn] = 0;

					// find snapshot that we should use as delta
					Emptysnap.Clear();

					// find delta
					if(DeltaTick >= 0)
					{
						int DeltashotSize = m_aSnapshotStorage[Conn].Get(DeltaTick, 0, &pDeltaShot, 0);

						if(DeltashotSize < 0)
						{
							// couldn't find the delta snapshots that the server used
							// to compress this snapshot. force the server to resync
							if(g_Config.m_Debug)
							{
								char aBuf[256];
								str_format(aBuf, sizeof(aBuf), "error, couldn't find the delta snapshot");
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
							}

							// ack snapshot
							m_aAckGameTick[Conn] = -1;
							SendInput();
							return;
						}
					}

					// decompress snapshot
					const void *pDeltaData = m_SnapshotDelta.EmptyDelta();
					int DeltaSize = sizeof(int) * 3;

					if(m_aSnapshotIncomingDataSize[Conn])
					{
						int IntSize = CVariableInt::Decompress(m_aaSnapshotIncomingData[Conn], m_aSnapshotIncomingDataSize[Conn], aTmpBuffer2, sizeof(aTmpBuffer2));

						if(IntSize < 0) // failure during decompression
							return;

						pDeltaData = aTmpBuffer2;
						DeltaSize = IntSize;
					}

					// unpack delta
					const int SnapSize = m_SnapshotDelta.UnpackDelta(pDeltaShot, pTmpBuffer3, pDeltaData, DeltaSize);
					if(SnapSize < 0)
					{
						dbg_msg("client", "delta unpack failed. error=%d", SnapSize);
						return;
					}
					if(!pTmpBuffer3->IsValid(SnapSize))
					{
						dbg_msg("client", "snapshot invalid. SnapSize=%d, DeltaSize=%d", SnapSize, DeltaSize);
						return;
					}

					if(Msg != NETMSG_SNAPEMPTY && pTmpBuffer3->Crc() != Crc)
					{
						if(g_Config.m_Debug)
						{
							char aBuf[256];
							str_format(aBuf, sizeof(aBuf), "snapshot crc error #%d - tick=%d wantedcrc=%d gotcrc=%d compressed_size=%d delta_tick=%d",
								m_SnapCrcErrors, GameTick, Crc, pTmpBuffer3->Crc(), m_aSnapshotIncomingDataSize[Conn], DeltaTick);
							m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
						}

						m_SnapCrcErrors++;
						if(m_SnapCrcErrors > 10)
						{
							// to many errors, send reset
							m_aAckGameTick[Conn] = -1;
							SendInput();
							m_SnapCrcErrors = 0;
						}
						return;
					}
					else
					{
						if(m_SnapCrcErrors)
							m_SnapCrcErrors--;
					}

					// purge old snapshots
					int PurgeTick = DeltaTick;
					if(m_aapSnapshots[Conn][SNAP_PREV] && m_aapSnapshots[Conn][SNAP_PREV]->m_Tick < PurgeTick)
						PurgeTick = m_aapSnapshots[Conn][SNAP_PREV]->m_Tick;
					if(m_aapSnapshots[Conn][SNAP_CURRENT] && m_aapSnapshots[Conn][SNAP_CURRENT]->m_Tick < PurgeTick)
						PurgeTick = m_aapSnapshots[Conn][SNAP_CURRENT]->m_Tick;
					m_aSnapshotStorage[Conn].PurgeUntil(PurgeTick);

					// create a verified and unpacked snapshot
					unsigned char aAltSnapBuffer[CSnapshot::MAX_SIZE];
					CSnapshot *pAltSnapBuffer = (CSnapshot *)aAltSnapBuffer;
					const int AltSnapSize = UnpackAndValidateSnapshot(pTmpBuffer3, pAltSnapBuffer);
					if(AltSnapSize < 0)
					{
						dbg_msg("client", "unpack snapshot and validate failed. error=%d", AltSnapSize);
						return;
					}

					// add new
					m_aSnapshotStorage[Conn].Add(GameTick, time_get(), SnapSize, pTmpBuffer3, AltSnapSize, pAltSnapBuffer);

					if(!Dummy)
					{
						// for antiping: if the projectile netobjects from the server contains extra data, this is removed and the original content restored before recording demo
						unsigned char aExtraInfoRemoved[CSnapshot::MAX_SIZE];
						mem_copy(aExtraInfoRemoved, pTmpBuffer3, SnapSize);
						SnapshotRemoveExtraProjectileInfo(aExtraInfoRemoved);

						// add snapshot to demo
						for(auto &DemoRecorder : m_aDemoRecorder)
						{
							if(DemoRecorder.IsRecording())
							{
								// write snapshot
								DemoRecorder.RecordSnapshot(GameTick, aExtraInfoRemoved, SnapSize);
							}
						}
					}

					// apply snapshot, cycle pointers
					m_aReceivedSnapshots[Conn]++;

					// we got two snapshots until we see us self as connected
					if(m_aReceivedSnapshots[Conn] == 2)
					{
						// start at 200ms and work from there
						if(!Dummy)
						{
							m_PredictedTime.Init(GameTick * time_freq() / 50);
							m_PredictedTime.SetAdjustSpeed(1, 1000.0f);
							m_PredictedTime.UpdateMargin(PredictionMargin() * time_freq() / 1000);
						}
						m_aGameTime[Conn].Init((GameTick - 1) * time_freq() / 50);
						m_aapSnapshots[Conn][SNAP_PREV] = m_aSnapshotStorage[Conn].m_pFirst;
						m_aapSnapshots[Conn][SNAP_CURRENT] = m_aSnapshotStorage[Conn].m_pLast;
						if(!Dummy)
						{
							m_LocalStartTime = time_get();
#if defined(CONF_VIDEORECORDER)
							IVideo::SetLocalStartTime(m_LocalStartTime);
#endif
							GameClient()->OnNewSnapshot();
						}
						SetState(IClient::STATE_ONLINE);
						if(!Dummy)
						{
							DemoRecorder_HandleAutoStart();
						}
					}

					// adjust game time
					if(m_aReceivedSnapshots[Conn] > 2)
					{
						int64_t Now = m_aGameTime[Conn].Get(time_get());
						int64_t TickStart = GameTick * time_freq() / 50;
						int64_t TimeLeft = (TickStart - Now) * 1000 / time_freq();
						m_aGameTime[Conn].Update(&m_GametimeMarginGraph, (GameTick - 1) * time_freq() / 50, TimeLeft, 0);
					}

					if(m_aReceivedSnapshots[Conn] > 50 && !m_aCodeRunAfterJoin[Conn])
					{
						if(m_ServerCapabilities.m_ChatTimeoutCode)
						{
							CNetMsg_Cl_Say MsgP;
							MsgP.m_Team = 0;
							char aBuf[128];
							char aBufMsg[256];
							if(!g_Config.m_ClRunOnJoin[0] && !g_Config.m_ClDummyDefaultEyes && !g_Config.m_ClPlayerDefaultEyes)
								str_format(aBufMsg, sizeof(aBufMsg), "/timeout %s", m_aTimeoutCodes[Conn]);
							else
								str_format(aBufMsg, sizeof(aBufMsg), "/mc;timeout %s", m_aTimeoutCodes[Conn]);

							if(g_Config.m_ClRunOnJoin[0])
							{
								str_format(aBuf, sizeof(aBuf), ";%s", g_Config.m_ClRunOnJoin);
								str_append(aBufMsg, aBuf, sizeof(aBufMsg));
							}
							if(g_Config.m_ClDummyDefaultEyes || g_Config.m_ClPlayerDefaultEyes)
							{
								int Emote = ((g_Config.m_ClDummy) ? !Dummy : Dummy) ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
								char aBufEmote[128];
								aBufEmote[0] = '\0';
								switch(Emote)
								{
								case EMOTE_NORMAL:
									break;
								case EMOTE_PAIN:
									str_format(aBufEmote, sizeof(aBufEmote), "emote pain %d", g_Config.m_ClEyeDuration);
									break;
								case EMOTE_HAPPY:
									str_format(aBufEmote, sizeof(aBufEmote), "emote happy %d", g_Config.m_ClEyeDuration);
									break;
								case EMOTE_SURPRISE:
									str_format(aBufEmote, sizeof(aBufEmote), "emote surprise %d", g_Config.m_ClEyeDuration);
									break;
								case EMOTE_ANGRY:
									str_format(aBufEmote, sizeof(aBufEmote), "emote angry %d", g_Config.m_ClEyeDuration);
									break;
								case EMOTE_BLINK:
									str_format(aBufEmote, sizeof(aBufEmote), "emote blink %d", g_Config.m_ClEyeDuration);
									break;
								}
								if(aBufEmote[0])
								{
									str_format(aBuf, sizeof(aBuf), ";%s", aBufEmote);
									str_append(aBufMsg, aBuf, sizeof(aBufMsg));
								}
							}
							MsgP.m_pMessage = aBufMsg;
							CMsgPacker PackerTimeout(MsgP.MsgID(), false);
							MsgP.Pack(&PackerTimeout);
							SendMsg(Conn, &PackerTimeout, MSGFLAG_VITAL);
						}
						m_aCodeRunAfterJoin[Conn] = true;
					}

					// ack snapshot
					m_aAckGameTick[Conn] = GameTick;
				}
			}
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_RCONTYPE)
		{
			bool UsernameReq = Unpacker.GetInt() & 1;
			GameClient()->OnRconType(UsernameReq);
		}
	}
	else
	{
		if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0)
		{
			// game message
			if(!Dummy)
			{
				for(auto &DemoRecorder : m_aDemoRecorder)
					if(DemoRecorder.IsRecording())
						DemoRecorder.RecordMessage(pPacket->m_pData, pPacket->m_DataSize);
			}

			GameClient()->OnMessage(Msg, &Unpacker, Conn, Dummy);
		}
	}
}

int CClient::UnpackAndValidateSnapshot(CSnapshot *pFrom, CSnapshot *pTo)
{
	CUnpacker Unpacker;
	CSnapshotBuilder Builder;
	Builder.Init();
	CNetObjHandler *pNetObjHandler = GameClient()->GetNetObjHandler();

	int Num = pFrom->NumItems();
	for(int Index = 0; Index < Num; Index++)
	{
		const CSnapshotItem *pFromItem = pFrom->GetItem(Index);
		const int FromItemSize = pFrom->GetItemSize(Index);
		const int ItemType = pFrom->GetItemType(Index);
		const void *pData = pFromItem->Data();
		Unpacker.Reset(pData, FromItemSize);

		void *pRawObj = pNetObjHandler->SecureUnpackObj(ItemType, &Unpacker);
		if(!pRawObj)
		{
			if(g_Config.m_Debug && ItemType != UUID_UNKNOWN)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "dropped weird object '%s' (%d), failed on '%s'", pNetObjHandler->GetObjName(ItemType), ItemType, pNetObjHandler->FailedObjOn());
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);
			}
			continue;
		}
		const int ItemSize = pNetObjHandler->GetUnpackedObjSize(ItemType);

		void *pObj = Builder.NewItem(pFromItem->Type(), pFromItem->ID(), ItemSize);
		if(!pObj)
			return -4;

		mem_copy(pObj, pRawObj, ItemSize);
	}

	return Builder.Finish(pTo);
}

void CClient::ResetMapDownload()
{
	if(m_pMapdownloadTask)
	{
		m_pMapdownloadTask->Abort();
		m_pMapdownloadTask = NULL;
	}
	m_MapdownloadFileTemp = 0;
	m_MapdownloadAmount = 0;
}

void CClient::FinishMapDownload()
{
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "download complete, loading map");

	int Prev = m_MapdownloadTotalsize;
	m_MapdownloadTotalsize = -1;
	SHA256_DIGEST *pSha256 = m_MapdownloadSha256Present ? &m_MapdownloadSha256 : 0;

	Storage()->RemoveFile(m_aMapdownloadFilename, IStorage::TYPE_SAVE);
	Storage()->RenameFile(m_aMapdownloadFilenameTemp, m_aMapdownloadFilename, IStorage::TYPE_SAVE);

	// load map
	const char *pError = LoadMap(m_aMapdownloadName, m_aMapdownloadFilename, pSha256, m_MapdownloadCrc);
	if(!pError)
	{
		ResetMapDownload();
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
		SendReady();
	}
	else if(m_pMapdownloadTask) // fallback
	{
		ResetMapDownload();
		m_MapdownloadTotalsize = Prev;
		SendMapRequest();
	}
	else
	{
		if(m_MapdownloadFileTemp)
		{
			io_close(m_MapdownloadFileTemp);
			m_MapdownloadFileTemp = 0;
			Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
		}
		ResetMapDownload();
		DisconnectWithReason(pError);
	}
}

void CClient::ResetDDNetInfo()
{
	if(m_pDDNetInfoTask)
	{
		m_pDDNetInfoTask->Abort();
		m_pDDNetInfoTask = NULL;
	}
}

bool CClient::IsDDNetInfoChanged()
{
	IOHANDLE OldFile = m_pStorage->OpenFile(DDNET_INFO, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_SAVE);

	if(!OldFile)
		return true;

	IOHANDLE NewFile = m_pStorage->OpenFile(m_aDDNetInfoTmp, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_SAVE);

	if(NewFile)
	{
		char aOldData[4096];
		char aNewData[4096];
		unsigned OldBytes;
		unsigned NewBytes;

		do
		{
			OldBytes = io_read(OldFile, aOldData, sizeof(aOldData));
			NewBytes = io_read(NewFile, aNewData, sizeof(aNewData));

			if(OldBytes != NewBytes || mem_comp(aOldData, aNewData, OldBytes) != 0)
			{
				io_close(NewFile);
				io_close(OldFile);
				return true;
			}
		} while(OldBytes > 0);

		io_close(NewFile);
	}

	io_close(OldFile);
	return false;
}

void CClient::FinishDDNetInfo()
{
	ResetDDNetInfo();
	if(IsDDNetInfoChanged())
	{
		m_pStorage->RenameFile(m_aDDNetInfoTmp, DDNET_INFO, IStorage::TYPE_SAVE);
		LoadDDNetInfo();

		if(g_Config.m_UiPage == CMenus::PAGE_DDNET)
			m_ServerBrowser.Refresh(IServerBrowser::TYPE_DDNET);
		else if(g_Config.m_UiPage == CMenus::PAGE_KOG)
			m_ServerBrowser.Refresh(IServerBrowser::TYPE_KOG);
	}
	else
	{
		m_pStorage->RemoveFile(m_aDDNetInfoTmp, IStorage::TYPE_SAVE);
	}
}

typedef std::tuple<int, int, int> TVersion;
static const TVersion gs_InvalidVersion = std::make_tuple(-1, -1, -1);

TVersion ToVersion(char *pStr)
{
	int aVersion[3] = {0, 0, 0};
	const char *p = strtok(pStr, ".");

	for(int i = 0; i < 3 && p; ++i)
	{
		if(!str_isallnum(p))
			return gs_InvalidVersion;

		aVersion[i] = str_toint(p);
		p = strtok(NULL, ".");
	}

	if(p)
		return gs_InvalidVersion;

	return std::make_tuple(aVersion[0], aVersion[1], aVersion[2]);
}

void CClient::LoadDDNetInfo()
{
	const json_value *pDDNetInfo = m_ServerBrowser.LoadDDNetInfo();

	if(!pDDNetInfo)
		return;

	const json_value &DDNetInfo = *pDDNetInfo;
	const json_value &CurrentVersion = DDNetInfo["version"];
	if(CurrentVersion.type == json_string)
	{
		char aNewVersionStr[64];
		str_copy(aNewVersionStr, CurrentVersion);
		char aCurVersionStr[64];
		str_copy(aCurVersionStr, GAME_RELEASE_VERSION);
		if(ToVersion(aNewVersionStr) > ToVersion(aCurVersionStr))
		{
			str_copy(m_aVersionStr, CurrentVersion);
		}
		else
		{
			m_aVersionStr[0] = '0';
			m_aVersionStr[1] = '\0';
		}
	}

	const json_value &News = DDNetInfo["news"];
	if(News.type == json_string)
	{
		// Only mark news button if something new was added to the news
		if(m_aNews[0] && str_find(m_aNews, News) == nullptr)
			g_Config.m_UiUnreadNews = true;

		str_copy(m_aNews, News);
	}

	const json_value &MapDownloadUrl = DDNetInfo["map-download-url"];
	if(MapDownloadUrl.type == json_string)
	{
		str_copy(m_aMapDownloadUrl, MapDownloadUrl);
	}

	const json_value &Points = DDNetInfo["points"];
	if(Points.type == json_integer)
	{
		m_Points = Points.u.integer;
	}

	const json_value &StunServersIpv6 = DDNetInfo["stun-servers-ipv6"];
	if(StunServersIpv6.type == json_array && StunServersIpv6[0].type == json_string)
	{
		NETADDR Addr;
		if(!net_addr_from_str(&Addr, StunServersIpv6[0]))
		{
			m_aNetClient->FeedStunServer(Addr);
		}
	}
	const json_value &StunServersIpv4 = DDNetInfo["stun-servers-ipv4"];
	if(StunServersIpv4.type == json_array && StunServersIpv4[0].type == json_string)
	{
		NETADDR Addr;
		if(!net_addr_from_str(&Addr, StunServersIpv4[0]))
		{
			m_aNetClient->FeedStunServer(Addr);
		}
	}
	const json_value &ConnectingIp = DDNetInfo["connecting-ip"];
	if(ConnectingIp.type == json_string)
	{
		NETADDR Addr;
		if(!net_addr_from_str(&Addr, ConnectingIp))
		{
			m_HaveGlobalTcpAddr = true;
			m_GlobalTcpAddr = Addr;
			log_debug("info", "got global tcp ip address: %s", (const char *)ConnectingIp);
		}
	}
	const json_value &WarnPngliteIncompatibleImages = DDNetInfo["warn-pnglite-incompatible-images"];
	Graphics()->WarnPngliteIncompatibleImages(WarnPngliteIncompatibleImages.type == json_boolean && (bool)WarnPngliteIncompatibleImages);
}

int CClient::ConnectNetTypes() const
{
	const NETADDR *pConnectAddrs;
	int NumConnectAddrs;
	m_aNetClient[CONN_MAIN].ConnectAddresses(&pConnectAddrs, &NumConnectAddrs);
	int NetType = 0;
	for(int i = 0; i < NumConnectAddrs; i++)
	{
		NetType |= pConnectAddrs[i].type;
	}
	return NetType;
}

void CClient::PumpNetwork()
{
	for(auto &NetClient : m_aNetClient)
	{
		NetClient.Update();
	}

	if(State() != IClient::STATE_DEMOPLAYBACK)
	{
		// check for errors
		if(State() != IClient::STATE_OFFLINE && State() < IClient::STATE_QUITTING && m_aNetClient[CONN_MAIN].State() == NETSTATE_OFFLINE)
		{
			SetState(IClient::STATE_OFFLINE);
			Disconnect();
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "offline error='%s'", m_aNetClient[CONN_MAIN].ErrorString());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkErrPrintColor);
		}

		if(State() != IClient::STATE_OFFLINE && State() < IClient::STATE_QUITTING && m_DummyConnected &&
			m_aNetClient[CONN_DUMMY].State() == NETSTATE_OFFLINE)
		{
			DummyDisconnect(0);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "offline dummy error='%s'", m_aNetClient[CONN_DUMMY].ErrorString());
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkErrPrintColor);
		}

		//
		if(State() == IClient::STATE_CONNECTING && m_aNetClient[CONN_MAIN].State() == NETSTATE_ONLINE)
		{
			// we switched to online
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "connected, sending info", gs_ClientNetworkPrintColor);
			SetState(IClient::STATE_LOADING);
			SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_INITIAL);
			SendInfo();
		}
	}

	// process packets
	CNetChunk Packet;
	for(int i = 0; i < NUM_CONNS; i++)
	{
		while(m_aNetClient[i].Recv(&Packet))
		{
			if(Packet.m_ClientID == -1)
			{
				ProcessConnlessPacket(&Packet);
				continue;
			}
			if(i > 1)
			{
				continue;
			}
			ProcessServerPacket(&Packet, i, g_Config.m_ClDummy ^ i);
		}
	}
}

void CClient::OnDemoPlayerSnapshot(void *pData, int Size)
{
	// update ticks, they could have changed
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	m_aCurGameTick[g_Config.m_ClDummy] = pInfo->m_Info.m_CurrentTick;
	m_aPrevGameTick[g_Config.m_ClDummy] = pInfo->m_PreviousTick;

	// create a verified and unpacked snapshot
	unsigned char aAltSnapBuffer[CSnapshot::MAX_SIZE];
	CSnapshot *pAltSnapBuffer = (CSnapshot *)aAltSnapBuffer;
	const int AltSnapSize = UnpackAndValidateSnapshot((CSnapshot *)pData, pAltSnapBuffer);
	if(AltSnapSize < 0)
	{
		dbg_msg("client", "unpack snapshot and validate failed. error=%d", AltSnapSize);
		return;
	}

	// handle snapshots after validation
	std::swap(m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV], m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]);
	mem_copy(m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pSnap, pData, Size);
	mem_copy(m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pAltSnap, pAltSnapBuffer, AltSnapSize);

	GameClient()->OnNewSnapshot();
}

void CClient::OnDemoPlayerMessage(void *pData, int Size)
{
	CUnpacker Unpacker;
	Unpacker.Reset(pData, Size);
	CMsgPacker Packer(NETMSG_EX, true);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(!Sys)
		GameClient()->OnMessage(Msg, &Unpacker, CONN_MAIN, false);
}
/*
const IDemoPlayer::CInfo *client_demoplayer_getinfo()
{
	static DEMOPLAYBACK_INFO ret;
	const DEMOREC_PLAYBACKINFO *info = m_DemoPlayer.Info();
	ret.first_tick = info->first_tick;
	ret.last_tick = info->last_tick;
	ret.current_tick = info->current_tick;
	ret.paused = info->paused;
	ret.speed = info->speed;
	return &ret;
}*/

/*
void DemoPlayer()->SetPos(float percent)
{
	demorec_playback_set(percent);
}

void DemoPlayer()->SetSpeed(float speed)
{
	demorec_playback_setspeed(speed);
}

void DemoPlayer()->SetPause(int paused)
{
	if(paused)
		demorec_playback_pause();
	else
		demorec_playback_unpause();
}*/

void CClient::UpdateDemoIntraTimers()
{
	// update timers
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	m_aCurGameTick[g_Config.m_ClDummy] = pInfo->m_Info.m_CurrentTick;
	m_aPrevGameTick[g_Config.m_ClDummy] = pInfo->m_PreviousTick;
	m_aGameIntraTick[g_Config.m_ClDummy] = pInfo->m_IntraTick;
	m_aGameTickTime[g_Config.m_ClDummy] = pInfo->m_TickTime;
	m_aGameIntraTickSincePrev[g_Config.m_ClDummy] = pInfo->m_IntraTickSincePrev;
};

void CClient::Update()
{
	if(State() == IClient::STATE_DEMOPLAYBACK)
	{
#if defined(CONF_VIDEORECORDER)
		if(m_DemoPlayer.IsPlaying() && IVideo::Current())
		{
			IVideo::Current()->NextVideoFrame();
			IVideo::Current()->NextAudioFrameTimeline(Sound()->GetSoundMixFunc());
		}
		else if(m_ButtonRender)
			Disconnect();
#endif

		m_DemoPlayer.Update();

		if(m_DemoPlayer.IsPlaying())
		{
			// update timers
			const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
			m_aCurGameTick[g_Config.m_ClDummy] = pInfo->m_Info.m_CurrentTick;
			m_aPrevGameTick[g_Config.m_ClDummy] = pInfo->m_PreviousTick;
			m_aGameIntraTick[g_Config.m_ClDummy] = pInfo->m_IntraTick;
			m_aGameTickTime[g_Config.m_ClDummy] = pInfo->m_TickTime;
		}
		else
		{
			// disconnect on error
			Disconnect();
		}
	}
	else if(State() == IClient::STATE_ONLINE)
	{
		if(m_LastDummy != (bool)g_Config.m_ClDummy)
		{
			// Invalidate references to !m_ClDummy snapshots
			GameClient()->InvalidateSnapshot();
			GameClient()->OnDummySwap();
		}

		if(m_aReceivedSnapshots[!g_Config.m_ClDummy] >= 3)
		{
			// switch dummy snapshot
			int64_t Now = m_aGameTime[!g_Config.m_ClDummy].Get(time_get());
			while(true)
			{
				CSnapshotStorage::CHolder *pCur = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT];
				int64_t TickStart = (pCur->m_Tick) * time_freq() / 50;

				if(TickStart < Now)
				{
					CSnapshotStorage::CHolder *pNext = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;
					if(pNext)
					{
						m_aapSnapshots[!g_Config.m_ClDummy][SNAP_PREV] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT];
						m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT] = pNext;

						// set ticks
						m_aCurGameTick[!g_Config.m_ClDummy] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
						m_aPrevGameTick[!g_Config.m_ClDummy] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_PREV]->m_Tick;
					}
					else
						break;
				}
				else
					break;
			}
		}

		if(m_aReceivedSnapshots[g_Config.m_ClDummy] >= 3)
		{
			// switch snapshot
			bool Repredict = false;
			int64_t Now = m_aGameTime[g_Config.m_ClDummy].Get(time_get());
			int64_t PredNow = m_PredictedTime.Get(time_get());

			if(m_LastDummy != (bool)g_Config.m_ClDummy && m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV])
			{
				// Load snapshot for m_ClDummy
				GameClient()->OnNewSnapshot();
				Repredict = true;
			}

			while(true)
			{
				CSnapshotStorage::CHolder *pCur = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
				int64_t TickStart = (pCur->m_Tick) * time_freq() / 50;

				if(TickStart < Now)
				{
					CSnapshotStorage::CHolder *pNext = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;
					if(pNext)
					{
						m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
						m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = pNext;

						// set ticks
						m_aCurGameTick[g_Config.m_ClDummy] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
						m_aPrevGameTick[g_Config.m_ClDummy] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick;

						if(m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV])
						{
							GameClient()->OnNewSnapshot();
							Repredict = true;
						}
					}
					else
						break;
				}
				else
					break;
			}

			if(m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] && m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV])
			{
				int64_t CurTickStart = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick * time_freq() / SERVER_TICK_SPEED;
				int64_t PrevTickStart = m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick * time_freq() / SERVER_TICK_SPEED;
				int PrevPredTick = (int)(PredNow * SERVER_TICK_SPEED / time_freq());
				int NewPredTick = PrevPredTick + 1;

				m_aGameIntraTick[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)(CurTickStart - PrevTickStart);
				m_aGameTickTime[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)time_freq();
				m_aGameIntraTickSincePrev[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)(time_freq() / SERVER_TICK_SPEED);

				int64_t CurPredTickStart = NewPredTick * time_freq() / SERVER_TICK_SPEED;
				int64_t PrevPredTickStart = PrevPredTick * time_freq() / SERVER_TICK_SPEED;
				m_aPredIntraTick[g_Config.m_ClDummy] = (PredNow - PrevPredTickStart) / (float)(CurPredTickStart - PrevPredTickStart);

				if(absolute(NewPredTick - m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick) > MaxLatencyTicks())
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", "prediction time reset!");
					m_PredictedTime.Init(CurTickStart + 2 * time_freq() / SERVER_TICK_SPEED);
				}

				if(NewPredTick > m_aPredTick[g_Config.m_ClDummy])
				{
					m_aPredTick[g_Config.m_ClDummy] = NewPredTick;
					Repredict = true;

					// send input
					SendInput();
				}
			}

			// only do sane predictions
			if(Repredict)
			{
				if(m_aPredTick[g_Config.m_ClDummy] > m_aCurGameTick[g_Config.m_ClDummy] && m_aPredTick[g_Config.m_ClDummy] < m_aCurGameTick[g_Config.m_ClDummy] + MaxLatencyTicks())
					GameClient()->OnPredict();
			}

			// fetch server info if we don't have it
			if(State() >= IClient::STATE_LOADING &&
				m_CurrentServerInfoRequestTime >= 0 &&
				time_get() > m_CurrentServerInfoRequestTime)
			{
				m_ServerBrowser.RequestCurrentServer(ServerAddress());
				m_CurrentServerInfoRequestTime = time_get() + time_freq() * 2;
			}

			// periodically ping server
			if(State() == IClient::STATE_ONLINE &&
				m_CurrentServerNextPingTime >= 0 &&
				time_get() > m_CurrentServerNextPingTime)
			{
				int64_t NowPing = time_get();
				int64_t Freq = time_freq();

				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "pinging current server%s", !m_ServerCapabilities.m_PingEx ? ", using fallback via server info" : "");
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);

				m_CurrentServerPingUuid = RandomUuid();
				if(!m_ServerCapabilities.m_PingEx)
				{
					m_ServerBrowser.RequestCurrentServerWithRandomToken(ServerAddress(), &m_CurrentServerPingBasicToken, &m_CurrentServerPingToken);
				}
				else
				{
					CMsgPacker Msg(NETMSG_PINGEX, true);
					Msg.AddRaw(&m_CurrentServerPingUuid, sizeof(m_CurrentServerPingUuid));
					SendMsg(CONN_MAIN, &Msg, MSGFLAG_FLUSH);
				}
				m_CurrentServerCurrentPingTime = NowPing;
				m_CurrentServerNextPingTime = NowPing + 600 * Freq; // ping every 10 minutes
			}
		}

		m_LastDummy = (bool)g_Config.m_ClDummy;
	}

	// STRESS TEST: join the server again
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
	{
		static int64_t ActionTaken = 0;
		int64_t Now = time_get();
		if(State() == IClient::STATE_OFFLINE)
		{
			if(Now > ActionTaken + time_freq() * 2)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "reconnecting!");
				Connect(g_Config.m_DbgStressServer);
				ActionTaken = Now;
			}
		}
		else
		{
			if(Now > ActionTaken + time_freq() * (10 + g_Config.m_DbgStress))
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "disconnecting!");
				Disconnect();
				ActionTaken = Now;
			}
		}
	}
#endif

	// pump the network
	PumpNetwork();

	if(m_pMapdownloadTask)
	{
		if(m_pMapdownloadTask->State() == HTTP_DONE)
			FinishMapDownload();
		else if(m_pMapdownloadTask->State() == HTTP_ERROR || m_pMapdownloadTask->State() == HTTP_ABORTED)
		{
			dbg_msg("webdl", "http failed, falling back to gameserver");
			ResetMapDownload();
			SendMapRequest();
		}
	}

	if(m_pDDNetInfoTask)
	{
		if(m_pDDNetInfoTask->State() == HTTP_DONE)
			FinishDDNetInfo();
		else if(m_pDDNetInfoTask->State() == HTTP_ERROR)
		{
			Storage()->RemoveFile(m_aDDNetInfoTmp, IStorage::TYPE_SAVE);
			ResetDDNetInfo();
		}
		else if(m_pDDNetInfoTask->State() == HTTP_ABORTED)
		{
			Storage()->RemoveFile(m_aDDNetInfoTmp, IStorage::TYPE_SAVE);
			m_pDDNetInfoTask = NULL;
		}
	}

	if(State() == IClient::STATE_ONLINE)
	{
		if(!m_lpEditJobs.empty())
		{
			std::shared_ptr<CDemoEdit> e = m_lpEditJobs.front();
			if(e->Status() == IJob::STATE_DONE)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "Successfully saved the replay to %s!", e->Destination());
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", aBuf);

				GameClient()->Echo(Localize("Successfully saved the replay!"));

				m_lpEditJobs.pop_front();
			}
		}
	}

	// update the server browser
	m_ServerBrowser.Update(m_ResortServerBrowser);
	m_ResortServerBrowser = false;

	// update editor/gameclient
	if(m_EditorActive)
		m_pEditor->OnUpdate();
	else
		GameClient()->OnUpdate();

	Discord()->Update();
	Steam()->Update();
	if(Steam()->GetConnectAddress())
	{
		HandleConnectAddress(Steam()->GetConnectAddress());
		Steam()->ClearConnectAddress();
	}

	if(m_ReconnectTime > 0 && time_get() > m_ReconnectTime)
	{
		if(State() != STATE_ONLINE)
			Connect(m_aConnectAddressStr);
		m_ReconnectTime = 0;
	}

	m_PredictedTime.UpdateMargin(PredictionMargin() * time_freq() / 1000);
}

void CClient::RegisterInterfaces()
{
	Kernel()->RegisterInterface(static_cast<IDemoRecorder *>(&m_aDemoRecorder[RECORDER_MANUAL]), false);
	Kernel()->RegisterInterface(static_cast<IDemoPlayer *>(&m_DemoPlayer), false);
	Kernel()->RegisterInterface(static_cast<IGhostRecorder *>(&m_GhostRecorder), false);
	Kernel()->RegisterInterface(static_cast<IGhostLoader *>(&m_GhostLoader), false);
	Kernel()->RegisterInterface(static_cast<IServerBrowser *>(&m_ServerBrowser), false);
#if defined(CONF_AUTOUPDATE)
	Kernel()->RegisterInterface(static_cast<IUpdater *>(&m_Updater), false);
#endif
	Kernel()->RegisterInterface(static_cast<IFriends *>(&m_Friends), false);
	Kernel()->ReregisterInterface(static_cast<IFriends *>(&m_Foes));
}

void CClient::InitInterfaces()
{
	// fetch interfaces
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	m_pFavorites = Kernel()->RequestInterface<IFavorites>();
	//m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
	m_pSound = Kernel()->RequestInterface<IEngineSound>();
	m_pGameClient = Kernel()->RequestInterface<IGameClient>();
	m_pInput = Kernel()->RequestInterface<IEngineInput>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	m_pConfig = m_pConfigManager->Values();
#if defined(CONF_AUTOUPDATE)
	m_pUpdater = Kernel()->RequestInterface<IUpdater>();
#endif
	m_pDiscord = Kernel()->RequestInterface<IDiscord>();
	m_pSteam = Kernel()->RequestInterface<ISteam>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_DemoEditor.Init(m_pGameClient->NetVersion(), &m_SnapshotDelta, m_pConsole, m_pStorage);

	m_ServerBrowser.SetBaseInfo(&m_aNetClient[CONN_CONTACT], m_pGameClient->NetVersion());

	HttpInit(m_pStorage);

#if defined(CONF_AUTOUPDATE)
	m_Updater.Init();
#endif

	m_pConfigManager->RegisterCallback(IFavorites::ConfigSaveCallback, m_pFavorites);
	m_Friends.Init();
	m_Foes.Init(true);

	m_GhostRecorder.Init();
	m_GhostLoader.Init();
}

void CClient::Run()
{
	m_LocalStartTime = time_get();
#if defined(CONF_VIDEORECORDER)
	IVideo::SetLocalStartTime(m_LocalStartTime);
#endif
	m_aSnapshotParts[0] = 0;
	m_aSnapshotParts[1] = 0;

	if(m_GenerateTimeoutSeed)
	{
		GenerateTimeoutSeed();
	}

	unsigned int Seed;
	secure_random_fill(&Seed, sizeof(Seed));
	srand(Seed);

	if(g_Config.m_Debug)
	{
		g_UuidManager.DebugDump();
	}

	// init graphics
	{
		m_pGraphics = CreateEngineGraphicsThreaded();

		bool RegisterFail = false;
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(m_pGraphics); // IEngineGraphics
		RegisterFail = RegisterFail || !Kernel()->RegisterInterface(static_cast<IGraphics *>(m_pGraphics), false);

		if(RegisterFail || m_pGraphics->Init() != 0)
		{
			dbg_msg("client", "couldn't init graphics");
			return;
		}
	}

	// make sure the first frame just clears everything to prevent undesired colors when waiting for io
	Graphics()->Clear(0, 0, 0);
	Graphics()->Swap();

	// init sound, allowed to fail
	m_SoundInitFailed = Sound()->Init() != 0;

#if defined(CONF_VIDEORECORDER)
	// init video recorder aka ffmpeg
	CVideo::Init();
#endif

#ifndef CONF_WEBASM
	// open socket
	{
		NETADDR BindAddr;
		if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
		{
			// got bindaddr
			BindAddr.type = NETTYPE_ALL;
		}
		else
		{
			mem_zero(&BindAddr, sizeof(BindAddr));
			BindAddr.type = NETTYPE_ALL;
		}
		for(unsigned int i = 0; i < std::size(m_aNetClient); i++)
		{
			int &PortRef = i == CONN_MAIN ? g_Config.m_ClPort : i == CONN_DUMMY ? g_Config.m_ClDummyPort : g_Config.m_ClContactPort;
			if(PortRef < 1024) // Reject users setting ports that we don't want to use
			{
				PortRef = 0;
			}
			BindAddr.port = PortRef;
			while(BindAddr.port == 0 || !m_aNetClient[i].Open(BindAddr))
			{
				BindAddr.port = (secure_rand() % 64511) + 1024;
			}
		}
	}
#endif

	// init font rendering
	Kernel()->RequestInterface<IEngineTextRender>()->Init();

	// init the input
	Input()->Init();

	// init the editor
	m_pEditor->Init();

	// load and save a map to fix it
	/*if(m_pEditor->Load(arg, IStorage::TYPE_ALL))
		m_pEditor->Save(arg);
	return;*/

	m_ServerBrowser.OnInit();
	// loads the existing ddnet info file if it exists
	LoadDDNetInfo();

	// load data
	if(!LoadData())
		return;

	if(Steam()->GetPlayerName())
	{
		str_copy(g_Config.m_SteamName, Steam()->GetPlayerName());
	}

	GameClient()->OnInit();

	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "version " GAME_RELEASE_VERSION " on " CONF_PLATFORM_STRING " " CONF_ARCH_STRING, ColorRGBA(0.7f, 0.7f, 1, 1.0f));
	if(GIT_SHORTREV_HASH)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "git revision hash: %s", GIT_SHORTREV_HASH);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf, ColorRGBA(0.7f, 0.7f, 1, 1.0f));
	}

	// connect to the server if wanted
	/*
	if(config.cl_connect[0] != 0)
		Connect(config.cl_connect);
	config.cl_connect[0] = 0;
	*/

	//
	m_FpsGraph.Init(0.0f, 120.0f);

	// never start with the editor
	g_Config.m_ClEditor = 0;

	// process pending commands
	m_pConsole->StoreCommands(false);

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Init(m_pConsole, g_Config.m_ClInputFifo, CFGFLAG_CLIENT);
#endif

	InitChecksum();
	m_pConsole->InitChecksum(ChecksumData());

	// request the new ddnet info from server if already past the welcome dialog
	if(g_Config.m_ClShowWelcome)
		g_Config.m_ClShowWelcome = 0;
	else
		RequestDDNetInfo();

	bool LastD = false;
	bool LastE = false;
	bool LastG = false;

	auto LastTime = time_get_nanoseconds();
	int64_t LastRenderTime = time_get();

	while(true)
	{
		set_new_tick();

		// handle pending connects
		if(m_aCmdConnect[0])
		{
			str_copy(g_Config.m_UiServerAddress, m_aCmdConnect);
			Connect(m_aCmdConnect);
			m_aCmdConnect[0] = 0;
		}

		// handle pending demo play
		if(m_aCmdPlayDemo[0])
		{
			const char *pError = DemoPlayer_Play(m_aCmdPlayDemo, IStorage::TYPE_ALL);
			if(pError && !fs_is_relative_path(m_aCmdPlayDemo))
				pError = DemoPlayer_Play(m_aCmdPlayDemo, IStorage::TYPE_ABSOLUTE);
			if(pError)
				dbg_msg("demo_player", "playing passed demo file '%s' failed: %s", m_aCmdPlayDemo, pError);
			m_aCmdPlayDemo[0] = 0;
		}

		// handle pending map edits
		if(m_aCmdEditMap[0])
		{
			int Result = m_pEditor->Load(m_aCmdEditMap, IStorage::TYPE_ALL);
			if(!Result && !fs_is_relative_path(m_aCmdEditMap))
				Result = m_pEditor->Load(m_aCmdEditMap, IStorage::TYPE_ABSOLUTE);
			if(Result)
				g_Config.m_ClEditor = true;
			else
				dbg_msg("editor", "editing passed map file '%s' failed", m_aCmdEditMap);
			m_aCmdEditMap[0] = 0;
		}

		// progress on dummy connect if security token handshake skipped/passed
		if(m_DummySendConnInfo && !m_aNetClient[CONN_DUMMY].SecurityTokenUnknown())
		{
			m_DummySendConnInfo = false;

			// send client info
			CMsgPacker MsgVer(NETMSG_CLIENTVER, true);
			MsgVer.AddRaw(&m_ConnectionID, sizeof(m_ConnectionID));
			MsgVer.AddInt(GameClient()->DDNetVersion());
			MsgVer.AddString(GameClient()->DDNetVersionStr(), 0);
			SendMsg(CONN_DUMMY, &MsgVer, MSGFLAG_VITAL);

			CMsgPacker MsgInfo(NETMSG_INFO, true);
			MsgInfo.AddString(GameClient()->NetVersion(), 128);
			MsgInfo.AddString(m_aPassword, 128);
			SendMsg(CONN_DUMMY, &MsgInfo, MSGFLAG_VITAL | MSGFLAG_FLUSH);

			// update netclient
			m_aNetClient[CONN_DUMMY].Update();

			// send ready
			CMsgPacker MsgReady(NETMSG_READY, true);
			SendMsg(CONN_DUMMY, &MsgReady, MSGFLAG_VITAL | MSGFLAG_FLUSH);

			// startinfo
			GameClient()->SendDummyInfo(true);

			// send enter game an finish the connection
			CMsgPacker MsgEnter(NETMSG_ENTERGAME, true);
			SendMsg(CONN_DUMMY, &MsgEnter, MSGFLAG_VITAL | MSGFLAG_FLUSH);
		}

		// update input
		if(Input()->Update())
		{
			if(State() == IClient::STATE_QUITTING)
				break;
			else
				SetState(IClient::STATE_QUITTING); // SDL_QUIT
		}
#if defined(CONF_AUTOUPDATE)
		Updater()->Update();
#endif

		// update sound
		Sound()->Update();

		if(CtrlShiftKey(KEY_D, LastD))
			g_Config.m_Debug ^= 1;

		if(CtrlShiftKey(KEY_G, LastG))
			g_Config.m_DbgGraphs ^= 1;

		if(CtrlShiftKey(KEY_E, LastE))
		{
			g_Config.m_ClEditor = g_Config.m_ClEditor ^ 1;
			Input()->MouseModeRelative();
			Input()->SetIMEState(true);
		}

		// render
		{
			if(g_Config.m_ClEditor)
			{
				if(!m_EditorActive)
				{
					Input()->MouseModeRelative();
					GameClient()->OnActivateEditor();
					m_pEditor->ResetMentions();
					m_EditorActive = true;
				}
			}
			else if(m_EditorActive)
				m_EditorActive = false;

			Update();
			int64_t Now = time_get();

			bool IsRenderActive = (g_Config.m_GfxBackgroundRender || m_pGraphics->WindowOpen());

			bool AsyncRenderOld = g_Config.m_GfxAsyncRenderOld;

			int GfxRefreshRate = g_Config.m_GfxRefreshRate;

#if defined(CONF_VIDEORECORDER)
			// keep rendering synced
			if(IVideo::Current())
			{
				AsyncRenderOld = false;
				GfxRefreshRate = 0;
			}
#endif

			if(IsRenderActive &&
				(!AsyncRenderOld || m_pGraphics->IsIdle()) &&
				(!GfxRefreshRate || (time_freq() / (int64_t)g_Config.m_GfxRefreshRate) <= Now - LastRenderTime))
			{
				m_RenderFrames++;

				// update frametime
				m_RenderFrameTime = (Now - m_LastRenderTime) / (float)time_freq();
				if(m_RenderFrameTime < m_RenderFrameTimeLow)
					m_RenderFrameTimeLow = m_RenderFrameTime;
				if(m_RenderFrameTime > m_RenderFrameTimeHigh)
					m_RenderFrameTimeHigh = m_RenderFrameTime;
				m_FpsGraph.Add(1.0f / m_RenderFrameTime, 1, 1, 1);

				if(m_BenchmarkFile)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Frametime %d us\n", (int)(m_RenderFrameTime * 1000000));
					io_write(m_BenchmarkFile, aBuf, str_length(aBuf));
					if(time_get() > m_BenchmarkStopTime)
					{
						io_close(m_BenchmarkFile);
						m_BenchmarkFile = 0;
						Quit();
					}
				}

				m_FrameTimeAvg = m_FrameTimeAvg * 0.9f + m_RenderFrameTime * 0.1f;

				// keep the overflow time - it's used to make sure the gfx refreshrate is reached
				int64_t AdditionalTime = g_Config.m_GfxRefreshRate ? ((Now - LastRenderTime) - (time_freq() / (int64_t)g_Config.m_GfxRefreshRate)) : 0;
				// if the value is over the frametime of a 60 fps frame, reset the additional time (drop the frames, that are lost already)
				if(AdditionalTime > (time_freq() / 60))
					AdditionalTime = (time_freq() / 60);
				LastRenderTime = Now - AdditionalTime;
				m_LastRenderTime = Now;

#ifdef CONF_DEBUG
				if(g_Config.m_DbgStress)
				{
					if((m_RenderFrames % 10) == 0)
					{
						if(!m_EditorActive)
							Render();
						else
						{
							m_pEditor->OnRender();
							DebugRender();
						}
						m_pGraphics->Swap();
					}
				}
				else
#endif
				{
					if(!m_EditorActive)
						Render();
					else
					{
						m_pEditor->OnRender();
						DebugRender();
					}
					m_pGraphics->Swap();
				}
			}
			else if(!IsRenderActive)
			{
				// if the client does not render, it should reset its render time to a time where it would render the first frame, when it wakes up again
				LastRenderTime = g_Config.m_GfxRefreshRate ? (Now - (time_freq() / (int64_t)g_Config.m_GfxRefreshRate)) : Now;
			}

			if(Input()->VideoRestartNeeded())
			{
				m_pGraphics->Init();
				LoadData();
				GameClient()->OnInit();
			}
		}

		AutoScreenshot_Cleanup();
		AutoStatScreenshot_Cleanup();
		AutoCSV_Cleanup();

		// check conditions
		if(State() == IClient::STATE_QUITTING || State() == IClient::STATE_RESTARTING)
		{
			static bool s_SavedConfig = false;
			if(!s_SavedConfig)
			{
				// write down the config and quit
				if(!m_pConfigManager->Save())
					m_vWarnings.emplace_back(SWarning(Localize("Saving ddnet-settings.cfg failed")));
				s_SavedConfig = true;
			}

			IOHANDLE File = m_pStorage->OpenFile(m_aDDNetInfoTmp, IOFLAG_READ | IOFLAG_SKIP_BOM, IStorage::TYPE_SAVE);
			if(File)
			{
				io_close(File);
				m_pStorage->RemoveFile(m_aDDNetInfoTmp, IStorage::TYPE_SAVE);
			}

			if(m_vWarnings.empty() && !GameClient()->IsDisplayingWarning())
				break;
		}

#if defined(CONF_FAMILY_UNIX)
		m_Fifo.Update();
#endif

		// beNice
		auto Now = time_get_nanoseconds();
		decltype(Now) SleepTimeInNanoSeconds{0};
		bool Slept = false;
		if(
#ifdef CONF_DEBUG
			g_Config.m_DbgStress ||
#endif
			(g_Config.m_ClRefreshRateInactive && !m_pGraphics->WindowActive()))
		{
			SleepTimeInNanoSeconds = (std::chrono::nanoseconds(1s) / (int64_t)g_Config.m_ClRefreshRateInactive) - (Now - LastTime);
			std::this_thread::sleep_for(SleepTimeInNanoSeconds);
			Slept = true;
		}
		else if(g_Config.m_ClRefreshRate)
		{
			SleepTimeInNanoSeconds = (std::chrono::nanoseconds(1s) / (int64_t)g_Config.m_ClRefreshRate) - (Now - LastTime);
			if(SleepTimeInNanoSeconds > 0ns)
				net_socket_read_wait(m_aNetClient[CONN_MAIN].m_Socket, SleepTimeInNanoSeconds);
			Slept = true;
		}
		if(Slept)
		{
			// if the diff gets too small it shouldn't get even smaller (drop the updates, that could not be handled)
			if(SleepTimeInNanoSeconds < -16666666ns)
				SleepTimeInNanoSeconds = -16666666ns;
			// don't go higher than the frametime of a 60 fps frame
			else if(SleepTimeInNanoSeconds > 16666666ns)
				SleepTimeInNanoSeconds = 16666666ns;
			// the time diff between the time that was used actually used and the time the thread should sleep/wait
			// will be calculated in the sleep time of the next update tick by faking the time it should have slept/wait.
			// so two cases (and the case it slept exactly the time it should):
			//	- the thread slept/waited too long, then it adjust the time to sleep/wait less in the next update tick
			//	- the thread slept/waited too less, then it adjust the time to sleep/wait more in the next update tick
			LastTime = Now + SleepTimeInNanoSeconds;
		}
		else
			LastTime = Now;

		if(g_Config.m_DbgHitch)
		{
			std::this_thread::sleep_for(g_Config.m_DbgHitch * 1ms);
			g_Config.m_DbgHitch = 0;
		}

		// update local time
		m_LocalTime = (time_get() - m_LocalStartTime) / (float)time_freq();
	}

#if defined(CONF_FAMILY_UNIX)
	m_Fifo.Shutdown();
#endif

	GameClient()->OnShutdown();
	Disconnect();

	// close socket
	for(unsigned int i = 0; i < std::size(m_aNetClient); i++)
		m_aNetClient[i].Close();

	delete m_pEditor;
}

bool CClient::CtrlShiftKey(int Key, bool &Last)
{
	if(Input()->ModifierIsPressed() && (Input()->KeyIsPressed(KEY_LSHIFT) || Input()->KeyIsPressed(KEY_RSHIFT)) && !Last && Input()->KeyIsPressed(Key))
	{
		Last = true;
		return true;
	}
	else if(Last && !Input()->KeyIsPressed(Key))
		Last = false;

	return false;
}

void CClient::Con_Connect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->HandleConnectLink(pResult->GetString(0));
}

void CClient::Con_Disconnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Disconnect();
}

void CClient::Con_DummyConnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DummyConnect();
}

void CClient::Con_DummyDisconnect(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DummyDisconnect(0);
}

void CClient::Con_DummyResetInput(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->GameClient()->DummyResetInput();
}

void CClient::Con_Quit(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Quit();
}

void CClient::Con_Minimize(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Graphics()->Minimize();
}

void CClient::Con_Ping(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;

	CMsgPacker Msg(NETMSG_PING, true);
	pSelf->SendMsg(CONN_MAIN, &Msg, 0);
	pSelf->m_PingStartTime = time_get();
}

void CClient::AutoScreenshot_Start()
{
	if(g_Config.m_ClAutoScreenshot)
	{
		Graphics()->TakeScreenshot("auto/autoscreen");
		m_AutoScreenshotRecycle = true;
	}
}

void CClient::AutoStatScreenshot_Start()
{
	if(g_Config.m_ClAutoStatboardScreenshot)
	{
		Graphics()->TakeScreenshot("auto/stats/autoscreen");
		m_AutoStatScreenshotRecycle = true;
	}
}

void CClient::AutoScreenshot_Cleanup()
{
	if(m_AutoScreenshotRecycle)
	{
		if(g_Config.m_ClAutoScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto", "autoscreen", ".png", g_Config.m_ClAutoScreenshotMax);
		}
		m_AutoScreenshotRecycle = false;
	}
}

void CClient::AutoStatScreenshot_Cleanup()
{
	if(m_AutoStatScreenshotRecycle)
	{
		if(g_Config.m_ClAutoStatboardScreenshotMax)
		{
			// clean up auto taken screens
			CFileCollection AutoScreens;
			AutoScreens.Init(Storage(), "screenshots/auto/stats", "autoscreen", ".png", g_Config.m_ClAutoStatboardScreenshotMax);
		}
		m_AutoStatScreenshotRecycle = false;
	}
}

void CClient::AutoCSV_Start()
{
	if(g_Config.m_ClAutoCSV)
		m_AutoCSVRecycle = true;
}

void CClient::AutoCSV_Cleanup()
{
	if(m_AutoCSVRecycle)
	{
		if(g_Config.m_ClAutoCSVMax)
		{
			// clean up auto csvs
			CFileCollection AutoRecord;
			AutoRecord.Init(Storage(), "record/csv", "autorecord", ".csv", g_Config.m_ClAutoCSVMax);
		}
		m_AutoCSVRecycle = false;
	}
}

void CClient::Con_Screenshot(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Graphics()->TakeScreenshot(0);
}

void CClient::Con_Reset(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->m_pConfigManager->Reset(pResult->GetString(0));
}

#if defined(CONF_VIDEORECORDER)

void CClient::Con_StartVideo(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;

	if(pSelf->State() != IClient::STATE_DEMOPLAYBACK)
	{
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "videorecorder", "Can not start videorecorder outside of demoplayer.");
		return;
	}

	if(!IVideo::Current())
	{
		// wait for idle, so there is no data race
		pSelf->Graphics()->WaitForIdle();
		// pause the sound device while creating the video instance
		pSelf->Sound()->PauseAudioDevice();
		new CVideo((CGraphics_Threaded *)pSelf->m_pGraphics, pSelf->Sound(), pSelf->Storage(), pSelf->Graphics()->ScreenWidth(), pSelf->Graphics()->ScreenHeight(), "");
		pSelf->Sound()->UnpauseAudioDevice();
		IVideo::Current()->Start();
		bool paused = pSelf->m_DemoPlayer.Info()->m_Info.m_Paused;
		if(paused)
			IVideo::Current()->Pause(true);
	}
	else
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "videorecorder", "Videorecorder already running.");
}

void CClient::StartVideo(IConsole::IResult *pResult, void *pUserData, const char *pVideoName)
{
	CClient *pSelf = (CClient *)pUserData;

	if(pSelf->State() != IClient::STATE_DEMOPLAYBACK)
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "videorecorder", "Can not start videorecorder outside of demoplayer.");

	pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "demo_render", pVideoName);
	if(!IVideo::Current())
	{
		// wait for idle, so there is no data race
		pSelf->Graphics()->WaitForIdle();
		// pause the sound device while creating the video instance
		pSelf->Sound()->PauseAudioDevice();
		new CVideo((CGraphics_Threaded *)pSelf->m_pGraphics, pSelf->Sound(), pSelf->Storage(), pSelf->Graphics()->ScreenWidth(), pSelf->Graphics()->ScreenHeight(), pVideoName);
		pSelf->Sound()->UnpauseAudioDevice();
		IVideo::Current()->Start();
	}
	else
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "videorecorder", "Videorecorder already running.");
}

void CClient::Con_StopVideo(IConsole::IResult *pResult, void *pUserData)
{
	if(IVideo::Current())
		IVideo::Current()->Stop();
}

#endif

void CClient::Con_Rcon(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Rcon(pResult->GetString(0));
}

void CClient::Con_RconAuth(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->RconAuth("", pResult->GetString(0));
}

void CClient::Con_RconLogin(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->RconAuth(pResult->GetString(0), pResult->GetString(1));
}

void CClient::Con_BeginFavoriteGroup(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->m_FavoritesGroup)
	{
		log_error("client", "opening favorites group while there is already one, discarding old one");
		for(int i = 0; i < pSelf->m_FavoritesGroupNum; i++)
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&pSelf->m_aFavoritesGroupAddresses[i], aAddr, sizeof(aAddr), true);
			log_warn("client", "discarding %s", aAddr);
		}
	}
	pSelf->m_FavoritesGroup = true;
	pSelf->m_FavoritesGroupAllowPing = false;
	pSelf->m_FavoritesGroupNum = 0;
}

void CClient::Con_EndFavoriteGroup(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(!pSelf->m_FavoritesGroup)
	{
		log_error("client", "closing favorites group while there is none, ignoring");
		return;
	}
	log_info("client", "adding group of %d favorites", pSelf->m_FavoritesGroupNum);
	pSelf->m_pFavorites->Add(pSelf->m_aFavoritesGroupAddresses, pSelf->m_FavoritesGroupNum);
	if(pSelf->m_FavoritesGroupAllowPing)
	{
		pSelf->m_pFavorites->AllowPing(pSelf->m_aFavoritesGroupAddresses, pSelf->m_FavoritesGroupNum, true);
	}
	pSelf->m_FavoritesGroup = false;
}

void CClient::Con_AddFavorite(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "invalid address '%s'", pResult->GetString(0));
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf);
		return;
	}
	bool AllowPing = pResult->NumArguments() > 1 && str_find(pResult->GetString(1), "allow_ping");
	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&Addr, aAddr, sizeof(aAddr), true);
	if(pSelf->m_FavoritesGroup)
	{
		if(pSelf->m_FavoritesGroupNum == (int)std::size(pSelf->m_aFavoritesGroupAddresses))
		{
			log_error("client", "discarding %s because groups can have at most a size of %d", aAddr, pSelf->m_FavoritesGroupNum);
			return;
		}
		log_info("client", "adding %s to favorites group", aAddr);
		pSelf->m_aFavoritesGroupAddresses[pSelf->m_FavoritesGroupNum] = Addr;
		pSelf->m_FavoritesGroupAllowPing = pSelf->m_FavoritesGroupAllowPing || AllowPing;
		pSelf->m_FavoritesGroupNum += 1;
	}
	else
	{
		log_info("client", "adding %s to favorites", aAddr);
		pSelf->m_pFavorites->Add(&Addr, 1);
		if(AllowPing)
		{
			pSelf->m_pFavorites->AllowPing(&Addr, 1, true);
		}
	}
}

void CClient::Con_RemoveFavorite(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	NETADDR Addr;
	if(net_addr_from_str(&Addr, pResult->GetString(0)) == 0)
		pSelf->m_pFavorites->Remove(&Addr, 1);
}

void CClient::DemoSliceBegin()
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	g_Config.m_ClDemoSliceBegin = pInfo->m_Info.m_CurrentTick;
}

void CClient::DemoSliceEnd()
{
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	g_Config.m_ClDemoSliceEnd = pInfo->m_Info.m_CurrentTick;
}

void CClient::Con_DemoSliceBegin(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoSliceBegin();
}

void CClient::Con_DemoSliceEnd(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoSliceEnd();
}

void CClient::Con_SaveReplay(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pResult->NumArguments())
	{
		int Length = pResult->GetInteger(0);
		if(Length <= 0)
			pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: length must be greater than 0 second.");
		else
		{
			if(pResult->NumArguments() >= 2)
				pSelf->SaveReplay(Length, pResult->GetString(1));
			else
				pSelf->SaveReplay(Length);
		}
	}
	else
		pSelf->SaveReplay(g_Config.m_ClReplayLength);
}

void CClient::SaveReplay(const int Length, const char *pFilename)
{
	if(!g_Config.m_ClReplays)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "Feature is disabled. Please enable it via configuration.");
		GameClient()->Echo(Localize("Replay feature is disabled!"));
		return;
	}

	if(!DemoRecorder(RECORDER_REPLAYS)->IsRecording())
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: demorecorder isn't recording. Try to rejoin to fix that.");
	else if(DemoRecorder(RECORDER_REPLAYS)->Length() < 1)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: demorecorder isn't recording for at least 1 second.");
	else
	{
		// First we stop the recorder to slice correctly the demo after
		DemoRecorder_Stop(RECORDER_REPLAYS);

		char aDate[64];
		str_timestamp(aDate, sizeof(aDate));

		char aFilename[IO_MAX_PATH_LENGTH];
		if(str_comp(pFilename, "") == 0)
			str_format(aFilename, sizeof(aFilename), "demos/replays/%s_%s (replay).demo", m_aCurrentMap, aDate);
		else
			str_format(aFilename, sizeof(aFilename), "demos/replays/%s.demo", pFilename);

		char *pSrc = (&m_aDemoRecorder[RECORDER_REPLAYS])->GetCurrentFilename();

		// Slice the demo to get only the last cl_replay_length seconds
		const int EndTick = GameTick(g_Config.m_ClDummy);
		const int StartTick = EndTick - Length * GameTickSpeed();

		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "Saving replay...");

		// Create a job to do this slicing in background because it can be a bit long depending on the file size
		std::shared_ptr<CDemoEdit> pDemoEditTask = std::make_shared<CDemoEdit>(GameClient()->NetVersion(), &m_SnapshotDelta, m_pStorage, pSrc, aFilename, StartTick, EndTick);
		Engine()->AddJob(pDemoEditTask);
		m_lpEditJobs.push_back(pDemoEditTask);

		// And we restart the recorder
		DemoRecorder_StartReplayRecorder();
	}
}

void CClient::DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser)
{
	if(m_DemoPlayer.IsPlaying())
	{
		const char *pDemoFileName = m_DemoPlayer.GetDemoFileName();
		m_DemoEditor.Slice(pDemoFileName, pDstPath, g_Config.m_ClDemoSliceBegin, g_Config.m_ClDemoSliceEnd, pfnFilter, pUser);
	}
}

const char *CClient::DemoPlayer_Play(const char *pFilename, int StorageType)
{
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, StorageType);
	if(!File)
		return "error opening demo file";

	io_close(File);

	Disconnect();
	m_aNetClient[CONN_MAIN].ResetErrorString();

	// try to start playback
	m_DemoPlayer.SetListener(this);

	if(m_DemoPlayer.Load(Storage(), m_pConsole, pFilename, StorageType))
		return "error loading demo";

	// load map
	const CMapInfo *pMapInfo = m_DemoPlayer.GetMapInfo();
	int Crc = pMapInfo->m_Crc;
	SHA256_DIGEST Sha = pMapInfo->m_Sha256;
	const char *pError = LoadMapSearch(pMapInfo->m_aName, Sha != SHA256_ZEROED ? &Sha : nullptr, Crc);
	if(pError)
	{
		if(!m_DemoPlayer.ExtractMap(Storage()))
		{
			DisconnectWithReason(pError);
			return pError;
		}

		Sha = m_DemoPlayer.GetMapInfo()->m_Sha256;
		pError = LoadMapSearch(pMapInfo->m_aName, &Sha, Crc);
		if(pError)
		{
			DisconnectWithReason(pError);
			return pError;
		}
	}

	// setup current info
	str_copy(m_CurrentServerInfo.m_aMap, pMapInfo->m_aName);
	m_CurrentServerInfo.m_MapCrc = pMapInfo->m_Crc;
	m_CurrentServerInfo.m_MapSize = pMapInfo->m_Size;

	GameClient()->OnConnected();

	// setup buffers
	mem_zero(m_aaapDemorecSnapshotData, sizeof(m_aaapDemorecSnapshotData));

	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = &m_aDemorecSnapshotHolders[SNAP_CURRENT];
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV] = &m_aDemorecSnapshotHolders[SNAP_PREV];

	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pSnap = (CSnapshot *)m_aaapDemorecSnapshotData[SNAP_CURRENT][0];
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pAltSnap = (CSnapshot *)m_aaapDemorecSnapshotData[SNAP_CURRENT][1];
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_SnapSize = 0;
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_AltSnapSize = 0;
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick = -1;

	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_pSnap = (CSnapshot *)m_aaapDemorecSnapshotData[SNAP_PREV][0];
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_pAltSnap = (CSnapshot *)m_aaapDemorecSnapshotData[SNAP_PREV][1];
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_SnapSize = 0;
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_AltSnapSize = 0;
	m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick = -1;

	// enter demo playback state
	SetState(IClient::STATE_DEMOPLAYBACK);

	m_DemoPlayer.Play();
	GameClient()->OnEnterGame();

	return 0;
}

#if defined(CONF_VIDEORECORDER)
const char *CClient::DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex)
{
	const char *pError = DemoPlayer_Play(pFilename, StorageType);
	if(pError)
		return pError;
	m_ButtonRender = true;

	this->CClient::StartVideo(NULL, this, pVideoName);
	m_DemoPlayer.Play();
	m_DemoPlayer.SetSpeed(g_aSpeeds[SpeedIndex]);
	//m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "demo_recorder", "demo eof");
	return 0;
}
#endif

void CClient::Con_Play(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->HandleDemoPath(pResult->GetString(0));
}

void CClient::Con_DemoPlay(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->m_DemoPlayer.IsPlaying())
	{
		if(pSelf->m_DemoPlayer.BaseInfo()->m_Paused)
		{
			pSelf->m_DemoPlayer.Unpause();
		}
		else
		{
			pSelf->m_DemoPlayer.Pause();
		}
	}
}

void CClient::Con_DemoSpeed(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->m_DemoPlayer.SetSpeed(pResult->GetFloat(0));
}

void CClient::DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder)
{
	if(State() != IClient::STATE_ONLINE)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
	else
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		if(WithTimestamp)
		{
			char aDate[20];
			str_timestamp(aDate, sizeof(aDate));
			str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", pFilename, aDate);
		}
		else
			str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pFilename);

		SHA256_DIGEST Sha256 = m_pMap->Sha256();
		m_aDemoRecorder[Recorder].Start(Storage(), m_pConsole, aFilename, GameClient()->NetVersion(), m_aCurrentMap, &Sha256, m_pMap->Crc(), "client", m_pMap->MapSize(), 0, m_pMap->File());
	}
}

void CClient::DemoRecorder_HandleAutoStart()
{
	if(g_Config.m_ClAutoDemoRecord)
	{
		DemoRecorder_Stop(RECORDER_AUTO);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "auto/%s", m_aCurrentMap);
		DemoRecorder_Start(aBuf, true, RECORDER_AUTO);
		if(g_Config.m_ClAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/auto", "" /* empty for wild card */, ".demo", g_Config.m_ClAutoDemoMax);
		}
	}
	if(!DemoRecorder(RECORDER_REPLAYS)->IsRecording())
	{
		DemoRecorder_StartReplayRecorder();
	}
}

void CClient::DemoRecorder_StartReplayRecorder()
{
	if(g_Config.m_ClReplays)
	{
		DemoRecorder_Stop(RECORDER_REPLAYS);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "replays/replay_tmp-%s", m_aCurrentMap);
		DemoRecorder_Start(aBuf, true, RECORDER_REPLAYS);
	}
}

void CClient::DemoRecorder_Stop(int Recorder, bool RemoveFile)
{
	m_aDemoRecorder[Recorder].Stop();
	if(RemoveFile)
	{
		const char *pFilename = (&m_aDemoRecorder[Recorder])->GetCurrentFilename();
		if(pFilename[0] != '\0')
			Storage()->RemoveFile(pFilename, IStorage::TYPE_SAVE);
	}
}

void CClient::DemoRecorder_AddDemoMarker(int Recorder)
{
	m_aDemoRecorder[Recorder].AddDemoMarker();
}

class IDemoRecorder *CClient::DemoRecorder(int Recorder)
{
	return &m_aDemoRecorder[Recorder];
}

void CClient::Con_Record(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pResult->NumArguments())
		pSelf->DemoRecorder_Start(pResult->GetString(0), false, RECORDER_MANUAL);
	else
		pSelf->DemoRecorder_Start(pSelf->m_aCurrentMap, true, RECORDER_MANUAL);
}

void CClient::Con_StopRecord(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoRecorder_Stop(RECORDER_MANUAL);
}

void CClient::Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_MANUAL);
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_RACE);
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_AUTO);
	pSelf->DemoRecorder_AddDemoMarker(RECORDER_REPLAYS);
}

void CClient::Con_BenchmarkQuit(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	int Seconds = pResult->GetInteger(0);
	const char *pFilename = pResult->GetString(1);
	pSelf->BenchmarkQuit(Seconds, pFilename);
}

void CClient::BenchmarkQuit(int Seconds, const char *pFilename)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	m_BenchmarkFile = Storage()->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_ABSOLUTE, aBuf, sizeof(aBuf));
	m_BenchmarkStopTime = time_get() + time_freq() * Seconds;
}

void CClient::UpdateAndSwap()
{
	Input()->Update();
	Graphics()->Swap();
	Graphics()->Clear(0, 0, 0);
}

void CClient::ServerBrowserUpdate()
{
	m_ResortServerBrowser = true;
}

void CClient::ConchainServerBrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CClient *)pUserData)->ServerBrowserUpdate();
}

void CClient::InitChecksum()
{
	CChecksumData *pData = &m_Checksum.m_Data;
	pData->m_SizeofData = sizeof(*pData);
	str_copy(pData->m_aVersionStr, GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")");
	pData->m_Start = time_get();
	os_version_str(pData->m_aOsVersion, sizeof(pData->m_aOsVersion));
	secure_random_fill(&pData->m_Random, sizeof(pData->m_Random));
	pData->m_Version = GameClient()->DDNetVersion();
	pData->m_SizeofClient = sizeof(*this);
	pData->m_SizeofConfig = sizeof(pData->m_Config);
}

#ifndef DDNET_CHECKSUM_SALT
// salt@checksum.ddnet.org: db877f2b-2ddb-3ba6-9f67-a6d169ec671d
#define DDNET_CHECKSUM_SALT \
	{ \
		{ \
			0xdb, 0x87, 0x7f, 0x2b, 0x2d, 0xdb, 0x3b, 0xa6, \
				0x9f, 0x67, 0xa6, 0xd1, 0x69, 0xec, 0x67, 0x1d, \
		} \
	}
#endif

int CClient::HandleChecksum(int Conn, CUuid Uuid, CUnpacker *pUnpacker)
{
	int Start = pUnpacker->GetInt();
	int Length = pUnpacker->GetInt();
	if(pUnpacker->Error())
	{
		return 1;
	}
	if(Start < 0 || Length < 0 || Start > INT_MAX - Length)
	{
		return 2;
	}
	int End = Start + Length;
	int ChecksumBytesEnd = minimum(End, (int)sizeof(m_Checksum.m_aBytes));
	int FileStart = maximum(Start, (int)sizeof(m_Checksum.m_aBytes));
	unsigned char aStartBytes[4];
	unsigned char aEndBytes[4];
	int_to_bytes_be(aStartBytes, Start);
	int_to_bytes_be(aEndBytes, End);

	if(Start <= (int)sizeof(m_Checksum.m_aBytes))
	{
		mem_zero(&m_Checksum.m_Data.m_Config, sizeof(m_Checksum.m_Data.m_Config));
#define CHECKSUM_RECORD(Flags) (((Flags)&CFGFLAG_CLIENT) == 0 || ((Flags)&CFGFLAG_INSENSITIVE) != 0)
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	if(CHECKSUM_RECORD(Flags)) \
	{ \
		m_Checksum.m_Data.m_Config.m_##Name = g_Config.m_##Name; \
	}
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	if(CHECKSUM_RECORD(Flags)) \
	{ \
		m_Checksum.m_Data.m_Config.m_##Name = g_Config.m_##Name; \
	}
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	if(CHECKSUM_RECORD(Flags)) \
	{ \
		str_copy(m_Checksum.m_Data.m_Config.m_##Name, g_Config.m_##Name, sizeof(m_Checksum.m_Data.m_Config.m_##Name)); \
	}
#include <engine/shared/config_variables.h>
#undef CHECKSUM_RECORD
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
	}
	if(End > (int)sizeof(m_Checksum.m_aBytes))
	{
		if(m_OwnExecutableSize == 0)
		{
			m_OwnExecutable = io_current_exe();
			// io_length returns -1 on error.
			m_OwnExecutableSize = m_OwnExecutable ? io_length(m_OwnExecutable) : -1;
		}
		// Own executable not available.
		if(m_OwnExecutableSize < 0)
		{
			return 3;
		}
		if(End - (int)sizeof(m_Checksum.m_aBytes) > m_OwnExecutableSize)
		{
			return 4;
		}
	}

	SHA256_CTX Sha256Ctxt;
	sha256_init(&Sha256Ctxt);
	CUuid Salt = DDNET_CHECKSUM_SALT;
	sha256_update(&Sha256Ctxt, &Salt, sizeof(Salt));
	sha256_update(&Sha256Ctxt, &Uuid, sizeof(Uuid));
	sha256_update(&Sha256Ctxt, aStartBytes, sizeof(aStartBytes));
	sha256_update(&Sha256Ctxt, aEndBytes, sizeof(aEndBytes));
	if(Start < (int)sizeof(m_Checksum.m_aBytes))
	{
		sha256_update(&Sha256Ctxt, m_Checksum.m_aBytes + Start, ChecksumBytesEnd - Start);
	}
	if(End > (int)sizeof(m_Checksum.m_aBytes))
	{
		unsigned char aBuf[2048];
		if(io_seek(m_OwnExecutable, FileStart - sizeof(m_Checksum.m_aBytes), IOSEEK_START))
		{
			return 5;
		}
		for(int i = FileStart; i < End; i += sizeof(aBuf))
		{
			int Read = io_read(m_OwnExecutable, aBuf, minimum((int)sizeof(aBuf), End - i));
			sha256_update(&Sha256Ctxt, aBuf, Read);
		}
	}
	SHA256_DIGEST Sha256 = sha256_finish(&Sha256Ctxt);

	CMsgPacker Msg(NETMSG_CHECKSUM_RESPONSE, true);
	Msg.AddRaw(&Uuid, sizeof(Uuid));
	Msg.AddRaw(&Sha256, sizeof(Sha256));
	SendMsg(Conn, &Msg, MSGFLAG_VITAL);

	return 0;
}

void CClient::SwitchWindowScreen(int Index)
{
	// Todo SDL: remove this when fixed (changing screen when in fullscreen is bugged)
	if(g_Config.m_GfxFullscreen)
	{
		SetWindowParams(0, g_Config.m_GfxBorderless, g_Config.m_GfxFullscreen != 3);
		if(Graphics()->SetWindowScreen(Index))
			g_Config.m_GfxScreen = Index;
		SetWindowParams(g_Config.m_GfxFullscreen, g_Config.m_GfxBorderless, g_Config.m_GfxFullscreen != 3);
	}
	else
	{
		if(Graphics()->SetWindowScreen(Index))
			g_Config.m_GfxScreen = Index;
	}
}

void CClient::ConchainWindowScreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(g_Config.m_GfxScreen != pResult->GetInteger(0))
			pSelf->SwitchWindowScreen(pResult->GetInteger(0));
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::SetWindowParams(int FullscreenMode, bool IsBorderless, bool AllowResizing)
{
	g_Config.m_GfxFullscreen = clamp(FullscreenMode, 0, 3);
	g_Config.m_GfxBorderless = (int)IsBorderless;
	Graphics()->SetWindowParams(FullscreenMode, IsBorderless, AllowResizing);
}

void CClient::ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(g_Config.m_GfxFullscreen != pResult->GetInteger(0))
			pSelf->SetWindowParams(pResult->GetInteger(0), g_Config.m_GfxBorderless, pResult->GetInteger(0) != 3);
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ConchainWindowBordered(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(!g_Config.m_GfxFullscreen && (g_Config.m_GfxBorderless != pResult->GetInteger(0)))
			pSelf->SetWindowParams(g_Config.m_GfxFullscreen, !g_Config.m_GfxBorderless, g_Config.m_GfxFullscreen != 3);
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowVSync()
{
	if(Graphics()->SetVSync(g_Config.m_GfxVsync ^ 1))
		g_Config.m_GfxVsync ^= 1;
}

void CClient::LoadFont()
{
	static CFont *pDefaultFont = 0;
	char aFilename[IO_MAX_PATH_LENGTH];
	char aBuff[1024];
	const char *pFontFile = "fonts/DejaVuSans.ttf";
	const char *apFallbackFontFiles[] =
		{
			"fonts/GlowSansJCompressed-Book.otf",
			"fonts/SourceHanSansSC-Regular.otf",
		};
	IOHANDLE File = Storage()->OpenFile(pFontFile, IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
	if(File)
	{
		IEngineTextRender *pTextRender = Kernel()->RequestInterface<IEngineTextRender>();
		pDefaultFont = pTextRender->GetFont(aFilename);
		if(pDefaultFont == NULL)
		{
			void *pBuf;
			unsigned Size;
			io_read_all(File, &pBuf, &Size);
			pDefaultFont = pTextRender->LoadFont(aFilename, (unsigned char *)pBuf, Size);
		}
		io_close(File);

		for(auto &pFallbackFontFile : apFallbackFontFiles)
		{
			bool FontLoaded = false;
			File = Storage()->OpenFile(pFallbackFontFile, IOFLAG_READ, IStorage::TYPE_ALL, aFilename, sizeof(aFilename));
			if(File)
			{
				void *pBuf;
				unsigned Size;
				io_read_all(File, &pBuf, &Size);
				io_close(File);
				FontLoaded = pTextRender->LoadFallbackFont(pDefaultFont, aFilename, (unsigned char *)pBuf, Size);
			}

			if(!FontLoaded)
			{
				str_format(aBuff, std::size(aBuff), "failed to load the fallback font. filename='%s'", pFallbackFontFile);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", aBuff);
			}
		}

		pTextRender->SetDefaultFont(pDefaultFont);
	}

	if(!pDefaultFont)
	{
		str_format(aBuff, std::size(aBuff), "failed to load font. filename='%s'", pFontFile);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gameclient", aBuff);
	}
}

void CClient::Notify(const char *pTitle, const char *pMessage)
{
	if(m_pGraphics->WindowActive() || !g_Config.m_ClShowNotifications)
		return;

	NotificationsNotify(pTitle, pMessage);
	Graphics()->NotifyWindow();
}

void CClient::ConchainWindowVSync(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(g_Config.m_GfxVsync != pResult->GetInteger(0))
			pSelf->ToggleWindowVSync();
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ConchainTimeoutSeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		pSelf->m_GenerateTimeoutSeed = false;
}

void CClient::ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	log_set_loglevel((LEVEL)g_Config.m_Loglevel);
}

void CClient::ConchainPassword(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && pSelf->m_LocalStartTime) //won't set m_SendPassword before game has started
		pSelf->m_SendPassword = true;
}

void CClient::ConchainReplays(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		int Status = pResult->GetInteger(0);
		if(Status == 0)
		{
			// stop recording and remove the tmp demo file
			pSelf->DemoRecorder_Stop(RECORDER_REPLAYS, true);
		}
		else
		{
			// start recording
			pSelf->DemoRecorder_HandleAutoStart();
		}
	}
}

void CClient::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	// register server dummy commands for tab completion
	m_pConsole->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, 0, 0, "Kick player with specified id for any reason");
	m_pConsole->Register("ban", "s[ip|id] ?i[minutes] r[reason]", CFGFLAG_SERVER, 0, 0, "Ban player with ip/id for x minutes for any reason");
	m_pConsole->Register("unban", "r[ip]", CFGFLAG_SERVER, 0, 0, "Unban ip");
	m_pConsole->Register("bans", "?i[page]", CFGFLAG_SERVER, 0, 0, "Show banlist (page 0 by default, 20 entries per page)");
	m_pConsole->Register("status", "?r[name]", CFGFLAG_SERVER, 0, 0, "List players containing name or all players");
	m_pConsole->Register("shutdown", "", CFGFLAG_SERVER, 0, 0, "Shut down");
	m_pConsole->Register("record", "r[file]", CFGFLAG_SERVER, 0, 0, "Record to a file");
	m_pConsole->Register("stoprecord", "", CFGFLAG_SERVER, 0, 0, "Stop recording");
	m_pConsole->Register("reload", "", CFGFLAG_SERVER, 0, 0, "Reload the map");

	m_pConsole->Register("dummy_connect", "", CFGFLAG_CLIENT, Con_DummyConnect, this, "Connect dummy");
	m_pConsole->Register("dummy_disconnect", "", CFGFLAG_CLIENT, Con_DummyDisconnect, this, "Disconnect dummy");
	m_pConsole->Register("dummy_reset", "", CFGFLAG_CLIENT, Con_DummyResetInput, this, "Reset dummy");

	m_pConsole->Register("quit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("exit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit Teeworlds");
	m_pConsole->Register("minimize", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Minimize, this, "Minimize Teeworlds");
	m_pConsole->Register("connect", "r[host|ip]", CFGFLAG_CLIENT, Con_Connect, this, "Connect to the specified host/ip");
	m_pConsole->Register("disconnect", "", CFGFLAG_CLIENT, Con_Disconnect, this, "Disconnect from the server");
	m_pConsole->Register("ping", "", CFGFLAG_CLIENT, Con_Ping, this, "Ping the current server");
	m_pConsole->Register("screenshot", "", CFGFLAG_CLIENT, Con_Screenshot, this, "Take a screenshot");
	m_pConsole->Register("reset", "s[config-name]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Reset, this, "Reset a config its default value");

#if defined(CONF_VIDEORECORDER)
	m_pConsole->Register("start_video", "", CFGFLAG_CLIENT, Con_StartVideo, this, "Start recording a video");
	m_pConsole->Register("stop_video", "", CFGFLAG_CLIENT, Con_StopVideo, this, "Stop recording a video");
#endif

	m_pConsole->Register("rcon", "r[rcon-command]", CFGFLAG_CLIENT, Con_Rcon, this, "Send specified command to rcon");
	m_pConsole->Register("rcon_auth", "r[password]", CFGFLAG_CLIENT, Con_RconAuth, this, "Authenticate to rcon");
	m_pConsole->Register("rcon_login", "s[username] r[password]", CFGFLAG_CLIENT, Con_RconLogin, this, "Authenticate to rcon with a username");
	m_pConsole->Register("play", "r[file]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Play, this, "Play the file specified");
	m_pConsole->Register("record", "?r[file]", CFGFLAG_CLIENT, Con_Record, this, "Record to the file");
	m_pConsole->Register("stoprecord", "", CFGFLAG_CLIENT, Con_StopRecord, this, "Stop recording");
	m_pConsole->Register("add_demomarker", "", CFGFLAG_CLIENT, Con_AddDemoMarker, this, "Add demo timeline marker");
	m_pConsole->Register("begin_favorite_group", "", CFGFLAG_CLIENT, Con_BeginFavoriteGroup, this, "Use this before `add_favorite` to group favorites. End with `end_favorite_group`");
	m_pConsole->Register("end_favorite_group", "", CFGFLAG_CLIENT, Con_EndFavoriteGroup, this, "Use this after `add_favorite` to group favorites. Start with `begin_favorite_group`");
	m_pConsole->Register("add_favorite", "s[host|ip] ?s['allow_ping']", CFGFLAG_CLIENT, Con_AddFavorite, this, "Add a server as a favorite");
	m_pConsole->Register("remove_favorite", "r[host|ip]", CFGFLAG_CLIENT, Con_RemoveFavorite, this, "Remove a server from favorites");
	m_pConsole->Register("demo_slice_start", "", CFGFLAG_CLIENT, Con_DemoSliceBegin, this, "");
	m_pConsole->Register("demo_slice_end", "", CFGFLAG_CLIENT, Con_DemoSliceEnd, this, "");
	m_pConsole->Register("demo_play", "", CFGFLAG_CLIENT, Con_DemoPlay, this, "Play demo");
	m_pConsole->Register("demo_speed", "i[speed]", CFGFLAG_CLIENT, Con_DemoSpeed, this, "Set demo speed");

	m_pConsole->Register("save_replay", "?i[length] ?s[filename]", CFGFLAG_CLIENT, Con_SaveReplay, this, "Save a replay of the last defined amount of seconds");
	m_pConsole->Register("benchmark_quit", "i[seconds] r[file]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_BenchmarkQuit, this, "Benchmark frame times for number of seconds to file, then quit");

	m_pConsole->Chain("cl_timeout_seed", ConchainTimeoutSeed, this);
	m_pConsole->Chain("cl_replays", ConchainReplays, this);

	m_pConsole->Chain("loglevel", ConchainLoglevel, this);
	m_pConsole->Chain("password", ConchainPassword, this);

	// used for server browser update
	m_pConsole->Chain("br_filter_string", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_gametype", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_serveraddress", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("add_favorite", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("remove_favorite", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("end_favorite_group", ConchainServerBrowserUpdate, this);

	m_pConsole->Chain("gfx_screen", ConchainWindowScreen, this);
	m_pConsole->Chain("gfx_fullscreen", ConchainFullscreen, this);
	m_pConsole->Chain("gfx_borderless", ConchainWindowBordered, this);
	m_pConsole->Chain("gfx_vsync", ConchainWindowVSync, this);

	// DDRace

#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help) m_pConsole->Register(name, params, flags, 0, 0, help);
#include <game/ddracecommands.h>
#undef CONSOLE_COMMAND
}

static CClient *CreateClient()
{
	CClient *pClient = static_cast<CClient *>(malloc(sizeof(*pClient)));
	mem_zero(pClient, sizeof(CClient));
	return new(pClient) CClient;
}

void CClient::HandleConnectAddress(const NETADDR *pAddr)
{
	net_addr_str(pAddr, m_aCmdConnect, sizeof(m_aCmdConnect), true);
}

void CClient::HandleConnectLink(const char *pLink)
{
	if(str_startswith(pLink, CONNECTLINK))
		str_copy(m_aCmdConnect, pLink + sizeof(CONNECTLINK) - 1);
	else
		str_copy(m_aCmdConnect, pLink);
}

void CClient::HandleDemoPath(const char *pPath)
{
	str_copy(m_aCmdPlayDemo, pPath);
}

void CClient::HandleMapPath(const char *pPath)
{
	str_copy(m_aCmdEditMap, pPath);
}

static bool UnknownArgumentCallback(const char *pCommand, void *pUser)
{
	CClient *pClient = static_cast<CClient *>(pUser);
	if(str_startswith(pCommand, CONNECTLINK))
	{
		pClient->HandleConnectLink(pCommand);
		return true;
	}
	else if(str_endswith(pCommand, ".demo"))
	{
		pClient->HandleDemoPath(pCommand);
		return true;
	}
	else if(str_endswith(pCommand, ".map"))
	{
		pClient->HandleMapPath(pCommand);
		return true;
	}
	return false;
}

/*
	Server Time
	Client Mirror Time
	Client Predicted Time

	Snapshot Latency
		Downstream latency

	Prediction Latency
		Upstream latency
*/

#if defined(CONF_PLATFORM_MACOS)
extern "C" int TWMain(int argc, const char **argv)
#elif defined(CONF_PLATFORM_ANDROID)
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char *argv[]);
extern "C" void InitAndroid();

int SDL_main(int argc, char *argv2[])
#else
int main(int argc, const char **argv)
#endif
{
#if defined(CONF_PLATFORM_ANDROID)
	const char **argv = const_cast<const char **>(argv2);
#elif defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(true);
#endif
	CCmdlineFix CmdlineFix(&argc, &argv);
	bool Silent = false;
	bool RandInitFailed = false;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0)
		{
			Silent = true;
		}
	}

#if defined(CONF_PLATFORM_ANDROID)
	InitAndroid();
#endif

#if defined(CONF_EXCEPTION_HANDLING)
	init_exception_handler();
#endif

	std::vector<std::shared_ptr<ILogger>> vpLoggers;
#if defined(CONF_PLATFORM_ANDROID)
	vpLoggers.push_back(std::shared_ptr<ILogger>(log_logger_android()));
#else
	if(!Silent)
	{
		vpLoggers.push_back(std::shared_ptr<ILogger>(log_logger_stdout()));
	}
#endif
	std::shared_ptr<CFutureLogger> pFutureFileLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureFileLogger);
	std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureConsoleLogger);
	std::shared_ptr<CFutureLogger> pFutureAssertionLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureAssertionLogger);
	log_set_global_logger(log_logger_collection(std::move(vpLoggers)).release());

	if(secure_random_init() != 0)
	{
		RandInitFailed = true;
	}

	NotificationsInit();

	CClient *pClient = CreateClient();
	IKernel *pKernel = IKernel::Create();
	pKernel->RegisterInterface(pClient, false);
	pClient->RegisterInterfaces();

	// create the components
	IEngine *pEngine = CreateEngine(GAME_NAME, pFutureConsoleLogger, 2);
	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT);
	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_CLIENT, argc, (const char **)argv);
	IConfigManager *pConfigManager = CreateConfigManager();
	IEngineSound *pEngineSound = CreateEngineSound();
	IEngineInput *pEngineInput = CreateEngineInput();
	IEngineTextRender *pEngineTextRender = CreateEngineTextRender();
	IEngineMap *pEngineMap = CreateEngineMap();
	IDiscord *pDiscord = CreateDiscord();
	ISteam *pSteam = CreateSteam();

	pFutureAssertionLogger->Set(CreateAssertionLogger(pStorage, GAME_NAME));
#if defined(CONF_EXCEPTION_HANDLING)
	char aBufPath[IO_MAX_PATH_LENGTH];
	char aBufName[IO_MAX_PATH_LENGTH];
	char aDate[64];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aBufName, sizeof(aBufName), "dumps/" GAME_NAME "_crash_log_%d_%s.RTP", pid(), aDate);
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, aBufName, aBufPath, sizeof(aBufPath));
	set_exception_handler_log_file(aBufPath);
#endif

	if(RandInitFailed)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineSound); // IEngineSound
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ISound *>(pEngineSound), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineInput); // IEngineInput
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IInput *>(pEngineInput), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineTextRender); // IEngineTextRender
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<ITextRender *>(pEngineTextRender), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngineMap); // IEngineMap
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateEditor(), false);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateFavorites().release());
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(CreateGameClient());
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pDiscord);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pSteam);

		if(RegisterFail)
		{
			delete pKernel;
			pClient->~CClient();
			free(pClient);
			return -1;
		}
	}

	pEngine->Init();
	pConfigManager->Init();
	pConsole->Init();

	// register all console commands
	pClient->RegisterCommands();

	pKernel->RequestInterface<IGameClient>()->OnConsoleInit();

	// init client's interfaces
	pClient->InitInterfaces();

	// execute config file
	IOHANDLE File = pStorage->OpenFile(CONFIG_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(CONFIG_FILE);
	}

	// execute autoexec file
	File = pStorage->OpenFile(AUTOEXEC_CLIENT_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(AUTOEXEC_CLIENT_FILE);
	}
	else // fallback
	{
		pConsole->ExecuteFile(AUTOEXEC_FILE);
	}

	if(g_Config.m_ClConfigVersion < 1)
	{
		if(g_Config.m_ClAntiPing == 0)
		{
			g_Config.m_ClAntiPingPlayers = 1;
			g_Config.m_ClAntiPingGrenade = 1;
			g_Config.m_ClAntiPingWeapons = 1;
		}
	}
	g_Config.m_ClConfigVersion = 1;

	// parse the command line arguments
	pConsole->SetUnknownCommandCallback(UnknownArgumentCallback, pClient);
	pConsole->ParseArguments(argc - 1, (const char **)&argv[1]);
	pConsole->SetUnknownCommandCallback(IConsole::EmptyUnknownCommandCallback, nullptr);

	if(pSteam->GetConnectAddress())
	{
		pClient->HandleConnectAddress(pSteam->GetConnectAddress());
		pSteam->ClearConnectAddress();
	}

	log_set_loglevel((LEVEL)g_Config.m_Loglevel);
	if(g_Config.m_Logfile[0])
	{
		IOHANDLE Logfile = io_open(g_Config.m_Logfile, IOFLAG_WRITE);
		if(Logfile)
		{
			pFutureFileLogger->Set(log_logger_file(Logfile));
		}
		else
		{
			dbg_msg("client", "failed to open '%s' for logging", g_Config.m_Logfile);
		}
	}

	// init SDL
	if(SDL_Init(0) < 0)
	{
		dbg_msg("client", "unable to init SDL base: %s", SDL_GetError());
		return -1;
	}

#ifndef CONF_PLATFORM_ANDROID
	atexit(SDL_Quit);
#endif

	// run the client
	dbg_msg("client", "starting...");
	pClient->Run();

	bool Restarting = pClient->State() == CClient::STATE_RESTARTING;

	pClient->~CClient();
	free(pClient);

	NotificationsUninit();
	secure_random_uninit();

	if(Restarting)
	{
		char aBuf[512];
		shell_execute(pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aBuf, sizeof aBuf));
	}

	pKernel->Shutdown();
	delete pKernel;

	// shutdown SDL
	SDL_Quit();

#ifdef CONF_PLATFORM_ANDROID
	// properly close this native thread, so globals are destructed
	std::exit(0);
#endif

	return 0;
}

// DDRace

const char *CClient::GetCurrentMap() const
{
	return m_aCurrentMap;
}

const char *CClient::GetCurrentMapPath() const
{
	return m_aCurrentMapPath;
}

SHA256_DIGEST CClient::GetCurrentMapSha256() const
{
	return m_pMap->Sha256();
}

unsigned CClient::GetCurrentMapCrc() const
{
	return m_pMap->Crc();
}

void CClient::RaceRecord_Start(const char *pFilename)
{
	if(State() != IClient::STATE_ONLINE)
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
	else
	{
		SHA256_DIGEST Sha256 = m_pMap->Sha256();
		m_aDemoRecorder[RECORDER_RACE].Start(Storage(), m_pConsole, pFilename, GameClient()->NetVersion(), m_aCurrentMap, &Sha256, m_pMap->Crc(), "client", m_pMap->MapSize(), 0, m_pMap->File());
	}
}

void CClient::RaceRecord_Stop()
{
	if(m_aDemoRecorder[RECORDER_RACE].IsRecording())
		m_aDemoRecorder[RECORDER_RACE].Stop();
}

bool CClient::RaceRecord_IsRecording()
{
	return m_aDemoRecorder[RECORDER_RACE].IsRecording();
}

void CClient::RequestDDNetInfo()
{
	char aUrl[256];
	str_copy(aUrl, "https://info.ddnet.org/info");

	if(g_Config.m_BrIndicateFinished)
	{
		char aEscaped[128];
		EscapeUrl(aEscaped, sizeof(aEscaped), PlayerName());
		str_append(aUrl, "?name=", sizeof(aUrl));
		str_append(aUrl, aEscaped, sizeof(aUrl));
	}

	// Use ipv4 so we can know the ingame ip addresses of players before they join game servers
	m_pDDNetInfoTask = HttpGetFile(aUrl, Storage(), m_aDDNetInfoTmp, IStorage::TYPE_SAVE);
	m_pDDNetInfoTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pDDNetInfoTask->IpResolve(IPRESOLVE::V4);
	Engine()->AddJob(m_pDDNetInfoTask);
}

int CClient::GetPredictionTime()
{
	int64_t Now = time_get();
	return (int)((m_PredictedTime.Get(Now) - m_aGameTime[g_Config.m_ClDummy].Get(Now)) * 1000 / (float)time_freq());
}

void CClient::GetSmoothTick(int *pSmoothTick, float *pSmoothIntraTick, float MixAmount)
{
	int64_t GameTime = m_aGameTime[g_Config.m_ClDummy].Get(time_get());
	int64_t PredTime = m_PredictedTime.Get(time_get());
	int64_t SmoothTime = clamp(GameTime + (int64_t)(MixAmount * (PredTime - GameTime)), GameTime, PredTime);

	*pSmoothTick = (int)(SmoothTime * 50 / time_freq()) + 1;
	*pSmoothIntraTick = (SmoothTime - (*pSmoothTick - 1) * time_freq() / 50) / (float)(time_freq() / 50);
}

SWarning *CClient::GetCurWarning()
{
	if(m_vWarnings.empty())
	{
		return NULL;
	}
	else if(m_vWarnings[0].m_WasShown)
	{
		m_vWarnings.erase(m_vWarnings.begin());
		return NULL;
	}
	else
	{
		return m_vWarnings.data();
	}
}

int CClient::MaxLatencyTicks() const
{
	return SERVER_TICK_SPEED + (PredictionMargin() * SERVER_TICK_SPEED) / 1000;
}

int CClient::PredictionMargin() const
{
	return m_ServerCapabilities.m_SyncWeaponInput ? g_Config.m_ClPredictionMargin : 10;
}

int CClient::UdpConnectivity(int NetType)
{
	static const int NETTYPES[2] = {NETTYPE_IPV6, NETTYPE_IPV4};
	int Connectivity = CONNECTIVITY_UNKNOWN;
	for(int PossibleNetType : NETTYPES)
	{
		if((NetType & PossibleNetType) == 0)
		{
			continue;
		}
		NETADDR GlobalUdpAddr;
		int NewConnectivity;
		switch(m_aNetClient[CONN_MAIN].GetConnectivity(PossibleNetType, &GlobalUdpAddr))
		{
		case CONNECTIVITY::UNKNOWN:
			NewConnectivity = CONNECTIVITY_UNKNOWN;
			break;
		case CONNECTIVITY::CHECKING:
			NewConnectivity = CONNECTIVITY_CHECKING;
			break;
		case CONNECTIVITY::UNREACHABLE:
			NewConnectivity = CONNECTIVITY_UNREACHABLE;
			break;
		case CONNECTIVITY::REACHABLE:
			NewConnectivity = CONNECTIVITY_REACHABLE;
			break;
		case CONNECTIVITY::ADDRESS_KNOWN:
			GlobalUdpAddr.port = 0;
			if(m_HaveGlobalTcpAddr && NetType == (int)m_GlobalTcpAddr.type && net_addr_comp(&m_GlobalTcpAddr, &GlobalUdpAddr) != 0)
			{
				NewConnectivity = CONNECTIVITY_DIFFERING_UDP_TCP_IP_ADDRESSES;
				break;
			}
			NewConnectivity = CONNECTIVITY_REACHABLE;
			break;
		default:
			dbg_assert(0, "invalid connectivity value");
			return CONNECTIVITY_UNKNOWN;
		}
		Connectivity = std::max(Connectivity, NewConnectivity);
	}
	return Connectivity;
}
