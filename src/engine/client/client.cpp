/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/hash.h>
#include <base/hash_ctxt.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>

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
#include <engine/shared/protocol7.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/rust_version.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/uuid_manager.h>

#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include <engine/shared/protocolglue.h>

#include <game/localization.h>
#include <game/version.h>

#include "client.h"
#include "demoedit.h"
#include "friends.h"
#include "notifications.h"
#include "serverbrowser.h"

#if defined(CONF_VIDEORECORDER)
#include "video.h"
#endif

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

#include "SDL.h"
#ifdef main
#undef main
#endif

#include <chrono>
#include <limits>
#include <new>
#include <stack>
#include <thread>
#include <tuple>

using namespace std::chrono_literals;

static const ColorRGBA gs_ClientNetworkPrintColor{0.7f, 1, 0.7f, 1.0f};
static const ColorRGBA gs_ClientNetworkErrPrintColor{1.0f, 0.25f, 0.25f, 1.0f};

CClient::CClient() :
	m_DemoPlayer(&m_SnapshotDelta, true, [&]() { UpdateDemoIntraTimers(); }),
	m_InputtimeMarginGraph(128),
	m_GametimeMarginGraph(128),
	m_FpsGraph(4096)
{
	m_StateStartTime = time_get();
	for(auto &DemoRecorder : m_aDemoRecorder)
		DemoRecorder = CDemoRecorder(&m_SnapshotDelta);
	m_LastRenderTime = time_get();
	mem_zero(m_aInputs, sizeof(m_aInputs));
	mem_zero(m_aapSnapshots, sizeof(m_aapSnapshots));
	for(auto &SnapshotStorage : m_aSnapshotStorage)
		SnapshotStorage.Init();
	mem_zero(m_aDemorecSnapshotHolders, sizeof(m_aDemorecSnapshotHolders));
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	mem_zero(&m_Checksum, sizeof(m_Checksum));
	for(auto &GameTime : m_aGameTime)
		GameTime.Init(0);
	m_PredictedTime.Init(0);

	m_Sixup = false;
}

// ----- send functions -----
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
			else if(MsgId == NETMSG_INFO || MsgId == NETMSG_REQUEST_MAP_DATA)
				;
			else if(MsgId == NETMSG_READY)
				MsgId = protocol7::NETMSG_READY;
			else if(MsgId == NETMSG_RCON_CMD)
				MsgId = protocol7::NETMSG_RCON_CMD;
			else if(MsgId == NETMSG_ENTERGAME)
				MsgId = protocol7::NETMSG_ENTERGAME;
			else if(MsgId == NETMSG_INPUT)
				MsgId = protocol7::NETMSG_INPUT;
			else if(MsgId == NETMSG_RCON_AUTH)
				MsgId = protocol7::NETMSG_RCON_AUTH;
			else if(MsgId == NETMSGTYPE_CL_SETTEAM)
				MsgId = protocol7::NETMSGTYPE_CL_SETTEAM;
			else if(MsgId == NETMSGTYPE_CL_VOTE)
				MsgId = protocol7::NETMSGTYPE_CL_VOTE;
			else if(MsgId == NETMSG_PING)
				MsgId = protocol7::NETMSG_PING;
			else
			{
				dbg_msg("net", "0.7 DROP send sys %d", MsgId);
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

	if(pMsg->m_MsgId < OFFSET_UUID)
	{
		Packer.AddInt((MsgId << 1) | (pMsg->m_System ? 1 : 0));
	}
	else
	{
		Packer.AddInt(pMsg->m_System ? 1 : 0); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(pMsg->m_MsgId, &Packer);
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
	if(RepackMsg(pMsg, Pack, IsSixup()))
		return 0;

	mem_zero(&Packet, sizeof(CNetChunk));
	Packet.m_ClientId = 0;
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

void CClient::SendInfo(int Conn)
{
	CMsgPacker MsgVer(NETMSG_CLIENTVER, true);
	MsgVer.AddRaw(&m_ConnectionId, sizeof(m_ConnectionId));
	MsgVer.AddInt(GameClient()->DDNetVersion());
	MsgVer.AddString(GameClient()->DDNetVersionStr());
	SendMsg(Conn, &MsgVer, MSGFLAG_VITAL);

	if(IsSixup())
	{
		CMsgPacker Msg(NETMSG_INFO, true);
		Msg.AddString(GAME_NETVERSION7, 128);
		Msg.AddString(Config()->m_Password);
		Msg.AddInt(GameClient()->ClientVersion7());
		SendMsg(Conn, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
		return;
	}

	CMsgPacker Msg(NETMSG_INFO, true);
	Msg.AddString(GameClient()->NetVersion());
	Msg.AddString(m_aPassword);
	SendMsg(Conn, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendEnterGame(int Conn)
{
	CMsgPacker Msg(NETMSG_ENTERGAME, true);
	SendMsg(Conn, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendReady(int Conn)
{
	CMsgPacker Msg(NETMSG_READY, true);
	SendMsg(Conn, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
}

void CClient::SendMapRequest()
{
	dbg_assert(!m_MapdownloadFileTemp, "Map download already in progress");
	m_MapdownloadFileTemp = Storage()->OpenFile(m_aMapdownloadFilenameTemp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(IsSixup())
	{
		CMsgPacker MsgP(protocol7::NETMSG_REQUEST_MAP_DATA, true, true);
		SendMsg(CONN_MAIN, &MsgP, MSGFLAG_VITAL | MSGFLAG_FLUSH);
	}
	else
	{
		CMsgPacker Msg(NETMSG_REQUEST_MAP_DATA, true);
		Msg.AddInt(m_MapdownloadChunk);
		SendMsg(CONN_MAIN, &Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH);
	}
}

void CClient::RconAuth(const char *pName, const char *pPassword, bool Dummy)
{
	if(m_aRconAuthed[Dummy] != 0)
		return;

	if(pName != m_aRconUsername)
		str_copy(m_aRconUsername, pName);
	if(pPassword != m_aRconPassword)
		str_copy(m_aRconPassword, pPassword);

	if(IsSixup())
	{
		CMsgPacker Msg7(protocol7::NETMSG_RCON_AUTH, true, true);
		Msg7.AddString(pPassword);
		SendMsg(Dummy, &Msg7, MSGFLAG_VITAL);
		return;
	}

	CMsgPacker Msg(NETMSG_RCON_AUTH, true);
	Msg.AddString(pName);
	Msg.AddString(pPassword);
	Msg.AddInt(1);
	SendMsg(Dummy, &Msg, MSGFLAG_VITAL);
}

void CClient::Rcon(const char *pCmd)
{
	CMsgPacker Msg(NETMSG_RCON_CMD, true);
	Msg.AddString(pCmd);
	SendMsgActive(&Msg, MSGFLAG_VITAL);
}

float CClient::GotRconCommandsPercentage() const
{
	if(m_ExpectedRconCommands < 1)
		return -1.0f;
	if(m_GotRconCommands > m_ExpectedRconCommands)
		return -1.0f;

	return (float)m_GotRconCommands / (float)m_ExpectedRconCommands;
}

bool CClient::ConnectionProblems() const
{
	return m_aNetClient[g_Config.m_ClDummy].GotProblems(MaxLatencyTicks() * time_freq() / GameTickSpeed()) != 0;
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
		if(!DummyConnected() && Dummy != 0)
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
			m_aInputs[i][m_aCurrentInput[i]].m_PredictionMargin = PredictionMargin() * time_freq() / 1000;
			m_aInputs[i][m_aCurrentInput[i]].m_Time = Now;

			// pack it
			for(int k = 0; k < Size / 4; k++)
			{
				static const int FlagsOffset = offsetof(CNetObj_PlayerInput, m_PlayerFlags) / sizeof(int);
				if(k == FlagsOffset && IsSixup())
				{
					int PlayerFlags = m_aInputs[i][m_aCurrentInput[i]].m_aData[k];
					Msg.AddInt(PlayerFlags_SixToSeven(PlayerFlags));
				}
				else
				{
					Msg.AddInt(m_aInputs[i][m_aCurrentInput[i]].m_aData[k]);
				}
			}

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
		if(m_aInputs[d][i].m_Tick != -1 && m_aInputs[d][i].m_Tick <= Tick && (Best == -1 || m_aInputs[d][Best].m_Tick < m_aInputs[d][i].m_Tick))
			Best = i;
	}

	if(Best != -1)
		return (int *)m_aInputs[d][Best].m_aData;
	return 0;
}

// ------ state handling -----
void CClient::SetState(EClientState State)
{
	if(m_State == IClient::STATE_QUITTING || m_State == IClient::STATE_RESTARTING)
		return;
	if(m_State == State)
		return;

	if(g_Config.m_Debug)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "state change. last=%d current=%d", m_State, State);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", aBuf);
	}

	const EClientState OldState = m_State;
	m_State = State;

	m_StateStartTime = time_get();
	GameClient()->OnStateChange(m_State, OldState);

	if(State == IClient::STATE_OFFLINE && m_ReconnectTime == 0)
	{
		if(g_Config.m_ClReconnectFull > 0 && (str_find_nocase(ErrorString(), "full") || str_find_nocase(ErrorString(), "reserved")))
			m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectFull;
		else if(g_Config.m_ClReconnectTimeout > 0 && (str_find_nocase(ErrorString(), "Timeout") || str_find_nocase(ErrorString(), "Too weak connection")))
			m_ReconnectTime = time_get() + time_freq() * g_Config.m_ClReconnectTimeout;
	}

	if(State == IClient::STATE_ONLINE)
	{
		const bool AnnounceAddr = m_ServerBrowser.IsRegistered(ServerAddress());
		Discord()->SetGameInfo(ServerAddress(), m_aCurrentMap, AnnounceAddr);
		Steam()->SetGameInfo(ServerAddress(), m_aCurrentMap, AnnounceAddr);
	}
	else if(OldState == IClient::STATE_ONLINE)
	{
		Discord()->ClearGameInfo();
		Steam()->ClearGameInfo();
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
	m_aapSnapshots[Dummy][SNAP_CURRENT] = nullptr;
	m_aapSnapshots[Dummy][SNAP_PREV] = nullptr;
	m_aSnapshotStorage[Dummy].PurgeAll();
	m_aReceivedSnapshots[Dummy] = 0;
	m_aSnapshotParts[Dummy] = 0;
	m_aSnapshotIncomingDataSize[Dummy] = 0;
	m_SnapCrcErrors = 0;
	// Also make gameclient aware that snapshots have been purged
	GameClient()->InvalidateSnapshot();

	// reset times
	m_aAckGameTick[Dummy] = -1;
	m_aCurrentRecvTick[Dummy] = 0;
	m_aPrevGameTick[Dummy] = 0;
	m_aCurGameTick[Dummy] = 0;
	m_aGameIntraTick[Dummy] = 0.0f;
	m_aGameTickTime[Dummy] = 0.0f;
	m_aGameIntraTickSincePrev[Dummy] = 0.0f;
	m_aPredTick[Dummy] = 0;
	m_aPredIntraTick[Dummy] = 0.0f;
	m_aGameTime[Dummy].Init(0);
	m_PredictedTime.Init(0);

	if(!Dummy)
	{
		m_LastDummyConnectTime = 0.0f;
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
	// Disconnect will not change the state if we are already quitting/restarting
	if(m_State == IClient::STATE_QUITTING || m_State == IClient::STATE_RESTARTING)
		return;
	Disconnect();
	dbg_assert(m_State == IClient::STATE_OFFLINE, "Disconnect must ensure that client is offline");

	char aLastAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&ServerAddress(), aLastAddr, sizeof(aLastAddr), true);

	if(pAddress != m_aConnectAddressStr)
		str_copy(m_aConnectAddressStr, pAddress);

	char aMsg[512];
	str_format(aMsg, sizeof(aMsg), "connecting to '%s'", m_aConnectAddressStr);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aMsg, gs_ClientNetworkPrintColor);

	int NumConnectAddrs = 0;
	NETADDR aConnectAddrs[MAX_SERVER_ADDRESSES];
	mem_zero(aConnectAddrs, sizeof(aConnectAddrs));
	const char *pNextAddr = pAddress;
	char aBuffer[128];
	bool OnlySixup = true;
	while((pNextAddr = str_next_token(pNextAddr, ",", aBuffer, sizeof(aBuffer))))
	{
		NETADDR NextAddr;
		char aHost[128];
		int url = net_addr_from_url(&NextAddr, aBuffer, aHost, sizeof(aHost));
		bool Sixup = NextAddr.type & NETTYPE_TW7;
		if(url > 0)
			str_copy(aHost, aBuffer);

		if(net_host_lookup(aHost, &NextAddr, m_aNetClient[CONN_MAIN].NetType()) != 0)
		{
			log_error("client", "could not find address of %s", aHost);
			continue;
		}
		if(NumConnectAddrs == (int)std::size(aConnectAddrs))
		{
			log_warn("client", "too many connect addresses, ignoring %s", aHost);
			continue;
		}
		if(NextAddr.port == 0)
		{
			NextAddr.port = 8303;
		}
		char aNextAddr[NETADDR_MAXSTRSIZE];
		if(Sixup)
			NextAddr.type |= NETTYPE_TW7;
		else
			OnlySixup = false;
		net_addr_str(&NextAddr, aNextAddr, sizeof(aNextAddr), true);
		log_debug("client", "resolved connect address '%s' to %s", aBuffer, aNextAddr);

		if(!str_comp(aNextAddr, aLastAddr))
		{
			m_SendPassword = true;
		}

		aConnectAddrs[NumConnectAddrs] = NextAddr;
		NumConnectAddrs += 1;
	}

	if(NumConnectAddrs == 0)
	{
		log_error("client", "could not find any connect address");
		char aWarning[256];
		str_format(aWarning, sizeof(aWarning), Localize("Could not resolve connect address '%s'. See local console for details."), m_aConnectAddressStr);
		SWarning Warning(Localize("Connect address error"), aWarning);
		Warning.m_AutoHide = false;
		AddWarning(Warning);
		return;
	}

	m_ConnectionId = RandomUuid();
	ServerInfoRequest();

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

	m_Sixup = OnlySixup;
	if(m_Sixup)
	{
		m_aNetClient[CONN_MAIN].Connect7(aConnectAddrs, NumConnectAddrs);
	}
	else
		m_aNetClient[CONN_MAIN].Connect(aConnectAddrs, NumConnectAddrs);

	m_aNetClient[CONN_MAIN].RefreshStun();
	SetState(IClient::STATE_CONNECTING);

	m_InputtimeMarginGraph.Init(-150.0f, 150.0f);
	m_GametimeMarginGraph.Init(-150.0f, 150.0f);

	GenerateTimeoutCodes(aConnectAddrs, NumConnectAddrs);
}

void CClient::DisconnectWithReason(const char *pReason)
{
	if(pReason != nullptr && pReason[0] == '\0')
		pReason = nullptr;

	DummyDisconnect(pReason);

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "disconnecting. reason='%s'", pReason ? pReason : "unknown");
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkPrintColor);

	// stop demo playback and recorder
	// make sure to remove replay tmp demo
	m_DemoPlayer.Stop();
	for(int Recorder = 0; Recorder < RECORDER_MAX; Recorder++)
	{
		DemoRecorder(Recorder)->Stop(Recorder == RECORDER_REPLAYS ? IDemoRecorder::EStopMode::REMOVE_FILE : IDemoRecorder::EStopMode::KEEP_FILE);
	}

	m_aRconAuthed[0] = 0;
	mem_zero(m_aRconUsername, sizeof(m_aRconUsername));
	mem_zero(m_aRconPassword, sizeof(m_aRconPassword));
	m_MapDetailsPresent = false;
	m_ServerSentCapabilities = false;
	m_UseTempRconCommands = 0;
	m_ExpectedRconCommands = -1;
	m_GotRconCommands = 0;
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

	ResetMapDownload(true);

	// clear the current server info
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));

	// clear snapshots
	m_aapSnapshots[0][SNAP_CURRENT] = 0;
	m_aapSnapshots[0][SNAP_PREV] = 0;
	m_aReceivedSnapshots[0] = 0;
	m_LastDummy = false;

	// 0.7
	m_TranslationContext.Reset();
	m_Sixup = false;
}

void CClient::Disconnect()
{
	if(m_State != IClient::STATE_OFFLINE)
	{
		DisconnectWithReason(nullptr);
	}
}

bool CClient::DummyConnected() const
{
	return m_DummyConnected;
}

bool CClient::DummyConnecting() const
{
	return m_DummyConnecting;
}

bool CClient::DummyConnectingDelayed() const
{
	return !DummyConnected() && !DummyConnecting() && m_LastDummyConnectTime > 0.0f && m_LastDummyConnectTime + 5.0f > GlobalTime();
}

void CClient::DummyConnect()
{
	if(m_aNetClient[CONN_MAIN].State() != NETSTATE_ONLINE)
	{
		log_info("client", "Not online.");
		return;
	}

	if(!DummyAllowed())
	{
		log_info("client", "Dummy is not allowed on this server.");
		return;
	}
	if(DummyConnected() || DummyConnecting())
	{
		log_info("client", "Dummy is already connected/connecting.");
		return;
	}
	if(DummyConnectingDelayed())
	{
		log_info("client", "Wait before connecting dummy again.");
		return;
	}

	m_LastDummyConnectTime = GlobalTime();
	m_aRconAuthed[1] = 0;
	m_DummySendConnInfo = true;

	g_Config.m_ClDummyCopyMoves = 0;
	g_Config.m_ClDummyHammer = 0;

	m_DummyConnecting = true;
	// connect to the server
	if(IsSixup())
		m_aNetClient[CONN_DUMMY].Connect7(m_aNetClient[CONN_MAIN].ServerAddress(), 1);
	else
		m_aNetClient[CONN_DUMMY].Connect(m_aNetClient[CONN_MAIN].ServerAddress(), 1);
}

void CClient::DummyDisconnect(const char *pReason)
{
	m_aNetClient[CONN_DUMMY].Disconnect(pReason);
	g_Config.m_ClDummy = 0;

	m_aRconAuthed[1] = 0;
	m_aapSnapshots[1][SNAP_CURRENT] = 0;
	m_aapSnapshots[1][SNAP_PREV] = 0;
	m_aReceivedSnapshots[1] = 0;
	m_DummyConnected = false;
	m_DummyConnecting = false;
	m_DummyReconnectOnReload = false;
	m_DummyDeactivateOnReconnect = false;
	GameClient()->OnDummyDisconnect();
}

bool CClient::DummyAllowed() const
{
	return m_ServerCapabilities.m_AllowDummy;
}

void CClient::GetServerInfo(CServerInfo *pServerInfo) const
{
	mem_copy(pServerInfo, &m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
}

void CClient::ServerInfoRequest()
{
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	m_CurrentServerInfoRequestTime = 0;
}

void CClient::LoadDebugFont()
{
	m_DebugFont = Graphics()->LoadTexture("debug_font.png", IStorage::TYPE_ALL);
}

// ---

IClient::CSnapItem CClient::SnapGetItem(int SnapId, int Index) const
{
	dbg_assert(SnapId >= 0 && SnapId < NUM_SNAPSHOT_TYPES, "invalid SnapId");
	const CSnapshot *pSnapshot = m_aapSnapshots[g_Config.m_ClDummy][SnapId]->m_pAltSnap;
	const CSnapshotItem *pSnapshotItem = pSnapshot->GetItem(Index);
	CSnapItem Item;
	Item.m_Type = pSnapshot->GetItemType(Index);
	Item.m_Id = pSnapshotItem->Id();
	Item.m_pData = pSnapshotItem->Data();
	Item.m_DataSize = pSnapshot->GetItemSize(Index);
	return Item;
}

const void *CClient::SnapFindItem(int SnapId, int Type, int Id) const
{
	if(!m_aapSnapshots[g_Config.m_ClDummy][SnapId])
		return nullptr;

	return m_aapSnapshots[g_Config.m_ClDummy][SnapId]->m_pAltSnap->FindItem(Type, Id);
}

int CClient::SnapNumItems(int SnapId) const
{
	dbg_assert(SnapId >= 0 && SnapId < NUM_SNAPSHOT_TYPES, "invalid SnapId");
	if(!m_aapSnapshots[g_Config.m_ClDummy][SnapId])
		return 0;
	return m_aapSnapshots[g_Config.m_ClDummy][SnapId]->m_pAltSnap->NumItems();
}

void CClient::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

void CClient::SnapSetStaticsize7(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize7(ItemType, Size);
}

void CClient::DebugRender()
{
	if(!g_Config.m_Debug)
		return;

	static NETSTATS s_Prev, s_Current;
	static int64_t s_LastSnapTime = 0;
	static float s_FrameTimeAvg = 0;
	char aBuffer[512];

	Graphics()->TextureSet(m_DebugFont);
	Graphics()->MapScreen(0, 0, Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	Graphics()->QuadsBegin();

	if(time_get() - s_LastSnapTime > time_freq())
	{
		s_LastSnapTime = time_get();
		s_Prev = s_Current;
		net_stats(&s_Current);
	}

	/*
		eth = 14
		ip = 20
		udp = 8
		total = 42
	*/
	s_FrameTimeAvg = s_FrameTimeAvg * 0.9f + m_RenderFrameTime * 0.1f;
	str_format(aBuffer, sizeof(aBuffer), "ticks: %8d %8d gfx mem(tex/buff/stream/staging): (%" PRIu64 " KiB/%" PRIu64 " KiB/%" PRIu64 " KiB/%" PRIu64 " KiB) fps: %3d",
		m_aCurGameTick[g_Config.m_ClDummy], m_aPredTick[g_Config.m_ClDummy],
		(Graphics()->TextureMemoryUsage() / 1024),
		(Graphics()->BufferMemoryUsage() / 1024),
		(Graphics()->StreamedMemoryUsage() / 1024),
		(Graphics()->StagingMemoryUsage() / 1024),
		(int)(1.0f / s_FrameTimeAvg + 0.5f));
	Graphics()->QuadsText(2, 2, 16, aBuffer);

	{
		uint64_t SendPackets = (s_Current.sent_packets - s_Prev.sent_packets);
		uint64_t SendBytes = (s_Current.sent_bytes - s_Prev.sent_bytes);
		uint64_t SendTotal = SendBytes + SendPackets * 42;
		uint64_t RecvPackets = (s_Current.recv_packets - s_Prev.recv_packets);
		uint64_t RecvBytes = (s_Current.recv_bytes - s_Prev.recv_bytes);
		uint64_t RecvTotal = RecvBytes + RecvPackets * 42;

		if(!SendPackets)
			SendPackets++;
		if(!RecvPackets)
			RecvPackets++;
		str_format(aBuffer, sizeof(aBuffer), "send: %3" PRIu64 " %5" PRIu64 "+%4" PRIu64 "=%5" PRIu64 " (%3" PRIu64 " Kibit/s) avg: %5" PRIu64 "\nrecv: %3" PRIu64 " %5" PRIu64 "+%4" PRIu64 "=%5" PRIu64 " (%3" PRIu64 " Kibit/s) avg: %5" PRIu64,
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
		float w = Graphics()->ScreenWidth() / 4.0f;
		float h = Graphics()->ScreenHeight() / 6.0f;
		float sp = Graphics()->ScreenWidth() / 100.0f;
		float x = Graphics()->ScreenWidth() - w - sp;

		m_FpsGraph.Scale(time_freq());
		m_FpsGraph.Render(Graphics(), TextRender(), x, sp * 5, w, h, "FPS");
		m_InputtimeMarginGraph.Scale(5 * time_freq());
		m_InputtimeMarginGraph.Render(Graphics(), TextRender(), x, sp * 6 + h, w, h, "Prediction Margin");
		m_GametimeMarginGraph.Scale(5 * time_freq());
		m_GametimeMarginGraph.Render(Graphics(), TextRender(), x, sp * 7 + h * 2, w, h, "Gametime Margin");
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

const char *CClient::DummyName()
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
		str_format(m_aAutomaticDummyName, sizeof(m_aAutomaticDummyName), "[D] %s", pBase);
		return m_aAutomaticDummyName;
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
	if((bool)m_LoadingCallback)
		m_LoadingCallback(IClient::LOADING_CALLBACK_DETAIL_MAP);

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
	for(int Recorder = 0; Recorder < RECORDER_MAX; Recorder++)
	{
		DemoRecorder(Recorder)->Stop(Recorder == RECORDER_REPLAYS ? IDemoRecorder::EStopMode::REMOVE_FILE : IDemoRecorder::EStopMode::KEEP_FILE);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "loaded map '%s'", pFilename);
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", aBuf);

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

		// don't add invalid info to the server browser list
		if(Info.m_NumClients < 0 || Info.m_MaxClients < 0 ||
			Info.m_NumPlayers < 0 || Info.m_MaxPlayers < 0 ||
			Info.m_NumPlayers > Info.m_NumClients || Info.m_MaxPlayers > Info.m_MaxClients)
		{
			return;
		}

		m_ServerBrowser.UpdateServerCommunity(&Info);
		m_ServerBrowser.UpdateServerRank(&Info);

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

static CServerCapabilities GetServerCapabilities(int Version, int Flags, bool Sixup)
{
	CServerCapabilities Result;
	bool DDNet = false;
	if(Version >= 1)
	{
		DDNet = Flags & SERVERCAPFLAG_DDNET;
	}
	Result.m_ChatTimeoutCode = DDNet;
	Result.m_AnyPlayerFlag = !Sixup;
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

	int Result = UnpackMessageId(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}
	else if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(Conn, &Packer, MSGFLAG_VITAL);
	}

	// allocates the memory for the translated data
	CPacker Packer6;
	if(IsSixup())
	{
		bool IsExMsg = false;
		int Success = !TranslateSysMsg(&Msg, Sys, &Unpacker, &Packer6, pPacket, &IsExMsg);
		if(Msg < 0)
			return;
		if(Success && !IsExMsg)
		{
			Unpacker.Reset(Packer6.Data(), Packer6.Size());
		}
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
			if(Unpacker.Error() || Version <= 0)
			{
				return;
			}
			m_ServerCapabilities = GetServerCapabilities(Version, Flags, IsSixup());
			m_CanReceiveServerCapabilities = false;
			m_ServerSentCapabilities = true;
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_CHANGE)
		{
			if(m_CanReceiveServerCapabilities)
			{
				m_ServerCapabilities = GetServerCapabilities(0, 0, IsSixup());
				m_CanReceiveServerCapabilities = false;
			}
			bool MapDetailsWerePresent = m_MapDetailsPresent;
			m_MapDetailsPresent = false;

			const char *pMap = Unpacker.GetString(CUnpacker::SANITIZE_CC | CUnpacker::SKIP_START_WHITESPACES);
			int MapCrc = Unpacker.GetInt();
			int MapSize = Unpacker.GetInt();
			if(Unpacker.Error())
			{
				return;
			}
			if(MapSize < 0 || MapSize > 1024 * 1024 * 1024) // 1 GiB
			{
				DisconnectWithReason("invalid map size");
				return;
			}

			for(int i = 0; pMap[i]; i++) // protect the player from nasty map names
			{
				if(pMap[i] == '/' || pMap[i] == '\\')
				{
					DisconnectWithReason("strange character in map name");
					return;
				}
			}

			if(m_DummyConnected && !m_DummyReconnectOnReload)
			{
				DummyDisconnect(0);
			}

			ResetMapDownload(true);

			SHA256_DIGEST *pMapSha256 = nullptr;
			const char *pMapUrl = nullptr;
			if(MapDetailsWerePresent && str_comp(m_aMapDetailsName, pMap) == 0 && m_MapDetailsCrc == MapCrc)
			{
				pMapSha256 = &m_MapDetailsSha256;
				pMapUrl = m_aMapDetailsUrl[0] ? m_aMapDetailsUrl : nullptr;
			}

			if(LoadMapSearch(pMap, pMapSha256, MapCrc) == nullptr)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
				SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_SENDING_READY);
				SendReady(CONN_MAIN);
			}
			else
			{
				// start map download
				FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, false, m_aMapdownloadFilename, sizeof(m_aMapdownloadFilename));
				FormatMapDownloadFilename(pMap, pMapSha256, MapCrc, true, m_aMapdownloadFilenameTemp, sizeof(m_aMapdownloadFilenameTemp));

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "starting to download map to '%s'", m_aMapdownloadFilenameTemp);
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", aBuf);

				str_copy(m_aMapdownloadName, pMap);
				m_MapdownloadSha256Present = (bool)pMapSha256;
				m_MapdownloadSha256 = pMapSha256 ? *pMapSha256 : SHA256_ZEROED;
				m_MapdownloadCrc = MapCrc;
				m_MapdownloadTotalsize = MapSize;

				if(pMapSha256)
				{
					char aUrl[256];
					char aEscaped[256];
					EscapeUrl(aEscaped, sizeof(aEscaped), m_aMapdownloadFilename + 15); // cut off downloadedmaps/
					bool UseConfigUrl = str_comp(g_Config.m_ClMapDownloadUrl, "https://maps.ddnet.org") != 0 || m_aMapDownloadUrl[0] == '\0';
					str_format(aUrl, sizeof(aUrl), "%s/%s", UseConfigUrl ? g_Config.m_ClMapDownloadUrl : m_aMapDownloadUrl, aEscaped);

					m_pMapdownloadTask = HttpGetFile(pMapUrl ? pMapUrl : aUrl, Storage(), m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
					m_pMapdownloadTask->Timeout(CTimeout{g_Config.m_ClMapDownloadConnectTimeoutMs, 0, g_Config.m_ClMapDownloadLowSpeedLimit, g_Config.m_ClMapDownloadLowSpeedTime});
					m_pMapdownloadTask->MaxResponseSize(MapSize);
					m_pMapdownloadTask->ExpectSha256(*pMapSha256);
					Http()->Run(m_pMapdownloadTask);
				}
				else
				{
					SendMapRequest();
				}
			}
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_MAP_DATA)
		{
			if(!m_MapdownloadFileTemp)
			{
				return;
			}
			int Last = -1;
			int MapCRC = -1;
			int Chunk = -1;
			int Size = -1;

			if(IsSixup())
			{
				MapCRC = m_MapdownloadCrc;
				Chunk = m_MapdownloadChunk;
				Size = minimum(m_TranslationContext.m_MapDownloadChunkSize, m_TranslationContext.m_MapdownloadTotalsize - m_MapdownloadAmount);
			}
			else
			{
				Last = Unpacker.GetInt();
				MapCRC = Unpacker.GetInt();
				Chunk = Unpacker.GetInt();
				Size = Unpacker.GetInt();
			}

			const unsigned char *pData = Unpacker.GetRaw(Size);
			if(Unpacker.Error() || Size <= 0 || MapCRC != m_MapdownloadCrc || Chunk != m_MapdownloadChunk)
			{
				return;
			}

			io_write(m_MapdownloadFileTemp, pData, Size);

			m_MapdownloadAmount += Size;

			if(IsSixup())
				Last = m_MapdownloadAmount == m_TranslationContext.m_MapdownloadTotalsize;

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

				if(IsSixup() && (m_MapdownloadChunk % m_TranslationContext.m_MapDownloadChunksPerRequest == 0))
				{
					CMsgPacker MsgP(protocol7::NETMSG_REQUEST_MAP_DATA, true, true);
					SendMsg(CONN_MAIN, &MsgP, MSGFLAG_VITAL | MSGFLAG_FLUSH);
				}
				else
				{
					CMsgPacker MsgP(NETMSG_REQUEST_MAP_DATA, true);
					MsgP.AddInt(m_MapdownloadChunk);
					SendMsg(CONN_MAIN, &MsgP, MSGFLAG_VITAL | MSGFLAG_FLUSH);
				}

				if(g_Config.m_Debug)
				{
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "requested chunk %d", m_MapdownloadChunk);
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client/network", aBuf);
				}
			}
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_MAP_RELOAD)
		{
			if(m_DummyConnected)
			{
				m_DummyReconnectOnReload = true;
				m_DummyDeactivateOnReconnect = g_Config.m_ClDummy == 0;
				g_Config.m_ClDummy = 0;
			}
			else
				m_DummyDeactivateOnReconnect = false;
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_CON_READY)
		{
			GameClient()->OnConnected();
			if(m_DummyReconnectOnReload)
			{
				m_DummySendConnInfo = true;
				m_DummyReconnectOnReload = false;
			}
		}
		else if(Conn == CONN_DUMMY && Msg == NETMSG_CON_READY)
		{
			m_DummyConnected = true;
			m_DummyConnecting = false;
			g_Config.m_ClDummy = 1;
			Rcon("crashmeplx");
			if(m_aRconAuthed[0] && !m_aRconAuthed[1])
				RconAuth(m_aRconUsername, m_aRconPassword);
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker MsgP(NETMSG_PING_REPLY, true);
			int Vital = (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 ? MSGFLAG_VITAL : 0;
			SendMsg(Conn, &MsgP, MSGFLAG_FLUSH | Vital);
		}
		else if(Msg == NETMSG_PINGEX)
		{
			CUuid *pId = (CUuid *)Unpacker.GetRaw(sizeof(*pId));
			if(Unpacker.Error())
			{
				return;
			}
			CMsgPacker MsgP(NETMSG_PONGEX, true);
			MsgP.AddRaw(pId, sizeof(*pId));
			int Vital = (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 ? MSGFLAG_VITAL : 0;
			SendMsg(Conn, &MsgP, MSGFLAG_FLUSH | Vital);
		}
		else if(Conn == CONN_MAIN && Msg == NETMSG_PONGEX)
		{
			CUuid *pId = (CUuid *)Unpacker.GetRaw(sizeof(*pId));
			if(Unpacker.Error())
			{
				return;
			}
			if(m_ServerCapabilities.m_PingEx && m_CurrentServerCurrentPingTime >= 0 && *pId == m_CurrentServerPingUuid)
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
		else if(Msg == NETMSG_REDIRECT)
		{
			int RedirectPort = Unpacker.GetInt();
			if(Unpacker.Error())
			{
				return;
			}
			char aAddr[NETADDR_MAXSTRSIZE];
			NETADDR ServerAddr = ServerAddress();
			ServerAddr.port = RedirectPort;
			net_addr_str(&ServerAddr, aAddr, sizeof(aAddr), true);
			Connect(aAddr);
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_ADD)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pHelp = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			const char *pParams = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(!Unpacker.Error())
			{
				m_pConsole->RegisterTemp(pName, pParams, CFGFLAG_SERVER, pHelp);
			}
			m_GotRconCommands++;
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_REM)
		{
			const char *pName = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(!Unpacker.Error())
			{
				m_pConsole->DeregisterTemp(pName);
			}
		}
		else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_AUTH_STATUS)
		{
			int ResultInt = Unpacker.GetInt();
			if(!Unpacker.Error())
			{
				m_aRconAuthed[Conn] = ResultInt;

				if(m_aRconAuthed[Conn])
					RconAuth(m_aRconUsername, m_aRconPassword, g_Config.m_ClDummy ^ 1);
			}
			if(Conn == CONN_MAIN)
			{
				int Old = m_UseTempRconCommands;
				m_UseTempRconCommands = Unpacker.GetInt();
				if(Unpacker.Error())
				{
					m_UseTempRconCommands = 0;
				}
				if(Old != 0 && m_UseTempRconCommands == 0)
				{
					m_pConsole->DeregisterTempAll();
					m_ExpectedRconCommands = -1;
				}
			}
		}
		else if(!Dummy && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_LINE)
		{
			const char *pLine = Unpacker.GetString();
			if(!Unpacker.Error())
			{
				GameClient()->OnRconLine(pLine);
			}
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
			if(Unpacker.Error())
			{
				return;
			}

			int64_t Now = time_get();

			// adjust our prediction time
			int64_t Target = 0;
			for(int k = 0; k < 200; k++)
			{
				if(m_aInputs[Conn][k].m_Tick == InputPredTick)
				{
					Target = m_aInputs[Conn][k].m_PredictedTime + (Now - m_aInputs[Conn][k].m_Time);
					Target = Target - (int64_t)((TimeLeft / 1000.0f) * time_freq());
					break;
				}
			}

			if(Target)
				m_PredictedTime.Update(&m_InputtimeMarginGraph, Target, TimeLeft, CSmoothTime::ADJUSTDIRECTION_UP);
		}
		else if(Msg == NETMSG_SNAP || Msg == NETMSG_SNAPSINGLE || Msg == NETMSG_SNAPEMPTY)
		{
			// we are not allowed to process snapshot yet
			if(State() < IClient::STATE_LOADING)
			{
				return;
			}

			int GameTick = Unpacker.GetInt();
			int DeltaTick = GameTick - Unpacker.GetInt();

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
			{
				return;
			}

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
					unsigned char aTmpBuffer2[CSnapshot::MAX_SIZE];
					unsigned char aTmpBuffer3[CSnapshot::MAX_SIZE];
					CSnapshot *pTmpBuffer3 = (CSnapshot *)aTmpBuffer3; // Fix compiler warning for strict-aliasing

					// reset snapshoting
					m_aSnapshotParts[Conn] = 0;

					// find snapshot that we should use as delta
					const CSnapshot *pDeltaShot = CSnapshot::EmptySnapshot();
					if(DeltaTick >= 0)
					{
						int DeltashotSize = m_aSnapshotStorage[Conn].Get(DeltaTick, nullptr, &pDeltaShot, nullptr);

						if(DeltashotSize < 0)
						{
							// couldn't find the delta snapshots that the server used
							// to compress this snapshot. force the server to resync
							if(g_Config.m_Debug)
							{
								m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "client", "error, couldn't find the delta snapshot");
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
					const int SnapSize = m_SnapshotDelta.UnpackDelta(pDeltaShot, pTmpBuffer3, pDeltaData, DeltaSize, IsSixup());
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
						log_error("client", "snapshot crc error #%d - tick=%d wantedcrc=%d gotcrc=%d compressed_size=%d delta_tick=%d",
							m_SnapCrcErrors, GameTick, Crc, pTmpBuffer3->Crc(), m_aSnapshotIncomingDataSize[Conn], DeltaTick);

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
					int AltSnapSize = -1;
					unsigned char aAltSnapBuffer[CSnapshot::MAX_SIZE];
					CSnapshot *pAltSnapBuffer = (CSnapshot *)aAltSnapBuffer;

					if(IsSixup())
					{
						unsigned char aTmpTransSnapBuffer[CSnapshot::MAX_SIZE];
						CSnapshot *pTmpTransSnapBuffer = (CSnapshot *)aTmpTransSnapBuffer;
						mem_copy(pTmpTransSnapBuffer, pTmpBuffer3, CSnapshot::MAX_SIZE);
						AltSnapSize = GameClient()->TranslateSnap(pAltSnapBuffer, pTmpTransSnapBuffer, Conn, Dummy);
					}
					else
					{
						AltSnapSize = UnpackAndValidateSnapshot(pTmpBuffer3, pAltSnapBuffer);
					}

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
						SnapshotRemoveExtraProjectileInfo(pTmpBuffer3);

						unsigned char aSnapSeven[CSnapshot::MAX_SIZE];
						CSnapshot *pSnapSeven = (CSnapshot *)aSnapSeven;
						int DemoSnapSize = SnapSize;
						if(IsSixup())
						{
							DemoSnapSize = GameClient()->OnDemoRecSnap7(pTmpBuffer3, pSnapSeven, Conn);
							if(DemoSnapSize < 0)
							{
								dbg_msg("sixup", "demo snapshot failed. error=%d", DemoSnapSize);
							}
						}

						if(DemoSnapSize >= 0)
						{
							// add snapshot to demo
							for(auto &DemoRecorder : m_aDemoRecorder)
							{
								if(DemoRecorder.IsRecording())
								{
									// write snapshot
									DemoRecorder.RecordSnapshot(GameTick, IsSixup() ? pSnapSeven : pTmpBuffer3, DemoSnapSize);
								}
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
							m_PredictedTime.Init(GameTick * time_freq() / GameTickSpeed());
							m_PredictedTime.SetAdjustSpeed(CSmoothTime::ADJUSTDIRECTION_UP, 1000.0f);
							m_PredictedTime.UpdateMargin(PredictionMargin() * time_freq() / 1000);
						}
						m_aGameTime[Conn].Init((GameTick - 1) * time_freq() / GameTickSpeed());
						m_aapSnapshots[Conn][SNAP_PREV] = m_aSnapshotStorage[Conn].m_pFirst;
						m_aapSnapshots[Conn][SNAP_CURRENT] = m_aSnapshotStorage[Conn].m_pLast;
						m_aPrevGameTick[Conn] = m_aapSnapshots[Conn][SNAP_PREV]->m_Tick;
						m_aCurGameTick[Conn] = m_aapSnapshots[Conn][SNAP_CURRENT]->m_Tick;
						if(Conn == CONN_MAIN)
						{
							m_LocalStartTime = time_get();
#if defined(CONF_VIDEORECORDER)
							IVideo::SetLocalStartTime(m_LocalStartTime);
#endif
						}
						if(!Dummy)
						{
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
						int64_t TickStart = GameTick * time_freq() / GameTickSpeed();
						int64_t TimeLeft = (TickStart - Now) * 1000 / time_freq();
						m_aGameTime[Conn].Update(&m_GametimeMarginGraph, (GameTick - 1) * time_freq() / GameTickSpeed(), TimeLeft, CSmoothTime::ADJUSTDIRECTION_DOWN);
					}

					if(m_aReceivedSnapshots[Conn] > GameTickSpeed() && !m_aCodeRunAfterJoin[Conn])
					{
						if(m_ServerCapabilities.m_ChatTimeoutCode)
						{
							char aBuf[128];
							char aBufMsg[256];
							if(!g_Config.m_ClRunOnJoin[0] && !g_Config.m_ClDummyDefaultEyes && !g_Config.m_ClPlayerDefaultEyes)
								str_format(aBufMsg, sizeof(aBufMsg), "/timeout %s", m_aTimeoutCodes[Conn]);
							else
								str_format(aBufMsg, sizeof(aBufMsg), "/mc;timeout %s", m_aTimeoutCodes[Conn]);

							if(g_Config.m_ClRunOnJoin[0])
							{
								str_format(aBuf, sizeof(aBuf), ";%s", g_Config.m_ClRunOnJoin);
								str_append(aBufMsg, aBuf);
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
									str_append(aBufMsg, aBuf);
								}
							}
							if(IsSixup())
							{
								protocol7::CNetMsg_Cl_Say Msg7;
								Msg7.m_Mode = protocol7::CHAT_ALL;
								Msg7.m_Target = -1;
								Msg7.m_pMessage = aBufMsg;
								SendPackMsg(Conn, &Msg7, MSGFLAG_VITAL, true);
							}
							else
							{
								CNetMsg_Cl_Say MsgP;
								MsgP.m_Team = 0;
								MsgP.m_pMessage = aBufMsg;
								CMsgPacker PackerTimeout(&MsgP);
								MsgP.Pack(&PackerTimeout);
								SendMsg(Conn, &PackerTimeout, MSGFLAG_VITAL);
							}
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
			if(!Unpacker.Error())
			{
				GameClient()->OnRconType(UsernameReq);
			}
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_GROUP_START)
		{
			int ExpectedRconCommands = Unpacker.GetInt();
			if(Unpacker.Error())
				return;

			m_ExpectedRconCommands = ExpectedRconCommands;
			m_GotRconCommands = 0;
		}
		else if(Conn == CONN_MAIN && (pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Msg == NETMSG_RCON_CMD_GROUP_END)
		{
			m_ExpectedRconCommands = -1;
		}
	}
	else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0)
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

		void *pObj = Builder.NewItem(pFromItem->Type(), pFromItem->Id(), ItemSize);
		if(!pObj)
			return -4;

		mem_copy(pObj, pRawObj, ItemSize);
	}

	return Builder.Finish(pTo);
}

void CClient::ResetMapDownload(bool ResetActive)
{
	if(m_pMapdownloadTask)
	{
		m_pMapdownloadTask->Abort();
		m_pMapdownloadTask = nullptr;
	}

	if(m_MapdownloadFileTemp)
	{
		io_close(m_MapdownloadFileTemp);
		m_MapdownloadFileTemp = 0;
	}

	if(Storage()->FileExists(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE))
	{
		Storage()->RemoveFile(m_aMapdownloadFilenameTemp, IStorage::TYPE_SAVE);
	}

	if(ResetActive)
	{
		m_MapdownloadChunk = 0;
		m_MapdownloadSha256Present = false;
		m_MapdownloadSha256 = SHA256_ZEROED;
		m_MapdownloadCrc = 0;
		m_MapdownloadTotalsize = -1;
		m_MapdownloadAmount = 0;
		m_aMapdownloadFilename[0] = '\0';
		m_aMapdownloadFilenameTemp[0] = '\0';
		m_aMapdownloadName[0] = '\0';
	}
}

void CClient::FinishMapDownload()
{
	m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "download complete, loading map");

	SHA256_DIGEST *pSha256 = m_MapdownloadSha256Present ? &m_MapdownloadSha256 : nullptr;

	bool FileSuccess = true;
	if(Storage()->FileExists(m_aMapdownloadFilename, IStorage::TYPE_SAVE))
		FileSuccess &= Storage()->RemoveFile(m_aMapdownloadFilename, IStorage::TYPE_SAVE);
	FileSuccess &= Storage()->RenameFile(m_aMapdownloadFilenameTemp, m_aMapdownloadFilename, IStorage::TYPE_SAVE);
	if(!FileSuccess)
	{
		char aError[128 + IO_MAX_PATH_LENGTH];
		str_format(aError, sizeof(aError), Localize("Could not save downloaded map. Try manually deleting this file: %s"), m_aMapdownloadFilename);
		DisconnectWithReason(aError);
		return;
	}

	const char *pError = LoadMap(m_aMapdownloadName, m_aMapdownloadFilename, pSha256, m_MapdownloadCrc);
	if(!pError)
	{
		ResetMapDownload(true);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client/network", "loading done");
		SendReady(CONN_MAIN);
	}
	else if(m_pMapdownloadTask) // fallback
	{
		ResetMapDownload(false);
		SendMapRequest();
	}
	else
	{
		DisconnectWithReason(pError);
	}
}

void CClient::ResetDDNetInfoTask()
{
	if(m_pDDNetInfoTask)
	{
		m_pDDNetInfoTask->Abort();
		m_pDDNetInfoTask = NULL;
	}
}

void CClient::FinishDDNetInfo()
{
	if(m_ServerBrowser.DDNetInfoSha256() == m_pDDNetInfoTask->ResultSha256())
	{
		log_debug("client/info", "DDNet info already up-to-date");
		return;
	}

	char aTempFilename[IO_MAX_PATH_LENGTH];
	IStorage::FormatTmpPath(aTempFilename, sizeof(aTempFilename), DDNET_INFO_FILE);
	IOHANDLE File = Storage()->OpenFile(aTempFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("client/info", "Failed to open temporary DDNet info '%s' for writing", aTempFilename);
		return;
	}

	unsigned char *pResult;
	size_t ResultLength;
	m_pDDNetInfoTask->Result(&pResult, &ResultLength);
	bool Error = io_write(File, pResult, ResultLength) != ResultLength;
	Error |= io_close(File) != 0;
	if(Error)
	{
		log_error("client/info", "Error writing temporary DDNet info to file '%s'", aTempFilename);
		return;
	}

	if(Storage()->FileExists(DDNET_INFO_FILE, IStorage::TYPE_SAVE) && !Storage()->RemoveFile(DDNET_INFO_FILE, IStorage::TYPE_SAVE))
	{
		log_error("client/info", "Failed to remove old DDNet info '%s'", DDNET_INFO_FILE);
		Storage()->RemoveFile(aTempFilename, IStorage::TYPE_SAVE);
		return;
	}
	if(!Storage()->RenameFile(aTempFilename, DDNET_INFO_FILE, IStorage::TYPE_SAVE))
	{
		log_error("client/info", "Failed to rename temporary DDNet info '%s' to '%s'", aTempFilename, DDNET_INFO_FILE);
		Storage()->RemoveFile(aTempFilename, IStorage::TYPE_SAVE);
		return;
	}

	log_debug("client/info", "Loading new DDNet info");
	LoadDDNetInfo();
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
			m_aNetClient[CONN_MAIN].FeedStunServer(Addr);
		}
	}
	const json_value &StunServersIpv4 = DDNetInfo["stun-servers-ipv4"];
	if(StunServersIpv4.type == json_array && StunServersIpv4[0].type == json_string)
	{
		NETADDR Addr;
		if(!net_addr_from_str(&Addr, StunServersIpv4[0]))
		{
			m_aNetClient[CONN_MAIN].FeedStunServer(Addr);
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
		// check for errors of main and dummy
		if(State() != IClient::STATE_OFFLINE && State() < IClient::STATE_QUITTING)
		{
			if(m_aNetClient[CONN_MAIN].State() == NETSTATE_OFFLINE)
			{
				// This will also disconnect the dummy, so the branch below is an `else if`
				Disconnect();
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "offline error='%s'", m_aNetClient[CONN_MAIN].ErrorString());
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkErrPrintColor);
			}
			else if((DummyConnecting() || DummyConnected()) && m_aNetClient[CONN_DUMMY].State() == NETSTATE_OFFLINE)
			{
				const bool WasConnecting = DummyConnecting();
				DummyDisconnect(nullptr);
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "offline dummy error='%s'", m_aNetClient[CONN_DUMMY].ErrorString());
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, gs_ClientNetworkErrPrintColor);
				if(WasConnecting)
				{
					str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Could not connect dummy"), m_aNetClient[CONN_DUMMY].ErrorString());
					GameClient()->Echo(aBuf);
				}
			}
		}

		// check if main was connected
		if(State() == IClient::STATE_CONNECTING && m_aNetClient[CONN_MAIN].State() == NETSTATE_ONLINE)
		{
			// we switched to online
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "connected, sending info", gs_ClientNetworkPrintColor);
			SetState(IClient::STATE_LOADING);
			SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_INITIAL);
			SendInfo(CONN_MAIN);
		}
	}

	// process packets
	CNetChunk Packet;
	SECURITY_TOKEN ResponseToken;
	for(int Conn = 0; Conn < NUM_CONNS; Conn++)
	{
		while(m_aNetClient[Conn].Recv(&Packet, &ResponseToken, IsSixup()))
		{
			if(Packet.m_ClientId == -1)
			{
				ProcessConnlessPacket(&Packet);
				continue;
			}
			if(Conn == CONN_MAIN || Conn == CONN_DUMMY)
			{
				ProcessServerPacket(&Packet, Conn, g_Config.m_ClDummy ^ Conn);
			}
		}
	}
}

void CClient::OnDemoPlayerSnapshot(void *pData, int Size)
{
	// update ticks, they could have changed
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	m_aCurGameTick[0] = pInfo->m_Info.m_CurrentTick;
	m_aPrevGameTick[0] = pInfo->m_PreviousTick;

	// create a verified and unpacked snapshot
	unsigned char aAltSnapBuffer[CSnapshot::MAX_SIZE];
	CSnapshot *pAltSnapBuffer = (CSnapshot *)aAltSnapBuffer;
	int AltSnapSize;

	if(IsSixup())
	{
		AltSnapSize = GameClient()->TranslateSnap(pAltSnapBuffer, (CSnapshot *)pData, CONN_MAIN, false);
		if(AltSnapSize < 0)
		{
			dbg_msg("sixup", "failed to translate snapshot. error=%d", AltSnapSize);
			return;
		}
	}
	else
	{
		AltSnapSize = UnpackAndValidateSnapshot((CSnapshot *)pData, pAltSnapBuffer);
		if(AltSnapSize < 0)
		{
			dbg_msg("client", "unpack snapshot and validate failed. error=%d", AltSnapSize);
			return;
		}
	}

	// handle snapshots after validation
	std::swap(m_aapSnapshots[0][SNAP_PREV], m_aapSnapshots[0][SNAP_CURRENT]);
	mem_copy(m_aapSnapshots[0][SNAP_CURRENT]->m_pSnap, pData, Size);
	mem_copy(m_aapSnapshots[0][SNAP_CURRENT]->m_pAltSnap, pAltSnapBuffer, AltSnapSize);

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

	int Result = UnpackMessageId(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(!Sys)
		GameClient()->OnMessage(Msg, &Unpacker, CONN_MAIN, false);
}

void CClient::UpdateDemoIntraTimers()
{
	// update timers
	const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
	m_aCurGameTick[0] = pInfo->m_Info.m_CurrentTick;
	m_aPrevGameTick[0] = pInfo->m_PreviousTick;
	m_aGameIntraTick[0] = pInfo->m_IntraTick;
	m_aGameTickTime[0] = pInfo->m_TickTime;
	m_aGameIntraTickSincePrev[0] = pInfo->m_IntraTickSincePrev;
};

void CClient::Update()
{
	PumpNetwork();

	if(State() == IClient::STATE_DEMOPLAYBACK)
	{
		if(m_DemoPlayer.IsPlaying())
		{
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				IVideo::Current()->NextVideoFrame();
				IVideo::Current()->NextAudioFrameTimeline([this](short *pFinalOut, unsigned Frames) {
					Sound()->Mix(pFinalOut, Frames);
				});
			}
#endif

			m_DemoPlayer.Update();

			// update timers
			const CDemoPlayer::CPlaybackInfo *pInfo = m_DemoPlayer.Info();
			m_aCurGameTick[0] = pInfo->m_Info.m_CurrentTick;
			m_aPrevGameTick[0] = pInfo->m_PreviousTick;
			m_aGameIntraTick[0] = pInfo->m_IntraTick;
			m_aGameTickTime[0] = pInfo->m_TickTime;
		}
		else
		{
			// Disconnect when demo playback stopped, either due to playback error
			// or because the end of the demo was reached when rendering it.
			DisconnectWithReason(m_DemoPlayer.ErrorMessage());
			if(m_DemoPlayer.ErrorMessage()[0] != '\0')
			{
				SWarning Warning(Localize("Error playing demo"), m_DemoPlayer.ErrorMessage());
				Warning.m_AutoHide = false;
				m_vWarnings.emplace_back(Warning);
			}
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

		if(m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT])
		{
			// switch dummy snapshot
			int64_t Now = m_aGameTime[!g_Config.m_ClDummy].Get(time_get());
			while(true)
			{
				if(!m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext)
					break;
				int64_t TickStart = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick * time_freq() / GameTickSpeed();
				if(TickStart >= Now)
					break;

				m_aapSnapshots[!g_Config.m_ClDummy][SNAP_PREV] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT];
				m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;

				// set ticks
				m_aCurGameTick[!g_Config.m_ClDummy] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
				m_aPrevGameTick[!g_Config.m_ClDummy] = m_aapSnapshots[!g_Config.m_ClDummy][SNAP_PREV]->m_Tick;
			}
		}

		if(m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT])
		{
			// switch snapshot
			bool Repredict = false;
			int64_t Now = m_aGameTime[g_Config.m_ClDummy].Get(time_get());
			int64_t PredNow = m_PredictedTime.Get(time_get());

			if(m_LastDummy != (bool)g_Config.m_ClDummy && m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV])
			{
				// Load snapshot for m_ClDummy
				GameClient()->OnNewSnapshot();
				Repredict = true;
			}

			while(true)
			{
				if(!m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext)
					break;
				int64_t TickStart = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick * time_freq() / GameTickSpeed();
				if(TickStart >= Now)
					break;

				m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT];
				m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_pNext;

				// set ticks
				m_aCurGameTick[g_Config.m_ClDummy] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick;
				m_aPrevGameTick[g_Config.m_ClDummy] = m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick;

				GameClient()->OnNewSnapshot();
				Repredict = true;
			}

			if(m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV])
			{
				int64_t CurTickStart = m_aapSnapshots[g_Config.m_ClDummy][SNAP_CURRENT]->m_Tick * time_freq() / GameTickSpeed();
				int64_t PrevTickStart = m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick * time_freq() / GameTickSpeed();
				int PrevPredTick = (int)(PredNow * GameTickSpeed() / time_freq());
				int NewPredTick = PrevPredTick + 1;

				m_aGameIntraTick[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)(CurTickStart - PrevTickStart);
				m_aGameTickTime[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)time_freq();
				m_aGameIntraTickSincePrev[g_Config.m_ClDummy] = (Now - PrevTickStart) / (float)(time_freq() / GameTickSpeed());

				int64_t CurPredTickStart = NewPredTick * time_freq() / GameTickSpeed();
				int64_t PrevPredTickStart = PrevPredTick * time_freq() / GameTickSpeed();
				m_aPredIntraTick[g_Config.m_ClDummy] = (PredNow - PrevPredTickStart) / (float)(CurPredTickStart - PrevPredTickStart);

				if(absolute(NewPredTick - m_aapSnapshots[g_Config.m_ClDummy][SNAP_PREV]->m_Tick) > MaxLatencyTicks())
				{
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "client", "prediction time reset!");
					m_PredictedTime.Init(CurTickStart + 2 * time_freq() / GameTickSpeed());
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
			if(m_CurrentServerInfoRequestTime >= 0 &&
				time_get() > m_CurrentServerInfoRequestTime)
			{
				m_ServerBrowser.RequestCurrentServer(ServerAddress());
				m_CurrentServerInfoRequestTime = time_get() + time_freq() * 2;
			}

			// periodically ping server
			if(m_CurrentServerNextPingTime >= 0 &&
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

		if(m_DummyDeactivateOnReconnect && g_Config.m_ClDummy == 1)
		{
			m_DummyDeactivateOnReconnect = false;
			g_Config.m_ClDummy = 0;
		}
		else if(!m_DummyConnected && m_DummyDeactivateOnReconnect)
		{
			m_DummyDeactivateOnReconnect = false;
		}

		m_LastDummy = (bool)g_Config.m_ClDummy;
	}

	// STRESS TEST: join the server again
#ifdef CONF_DEBUG
	if(g_Config.m_DbgStress)
	{
		static int64_t s_ActionTaken = 0;
		int64_t Now = time_get();
		if(State() == IClient::STATE_OFFLINE)
		{
			if(Now > s_ActionTaken + time_freq() * 2)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "reconnecting!");
				Connect(g_Config.m_DbgStressServer);
				s_ActionTaken = Now;
			}
		}
		else
		{
			if(Now > s_ActionTaken + time_freq() * (10 + g_Config.m_DbgStress))
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_DEBUG, "stress", "disconnecting!");
				Disconnect();
				s_ActionTaken = Now;
			}
		}
	}
#endif

	if(m_pMapdownloadTask)
	{
		if(m_pMapdownloadTask->State() == EHttpState::DONE)
			FinishMapDownload();
		else if(m_pMapdownloadTask->State() == EHttpState::ERROR || m_pMapdownloadTask->State() == EHttpState::ABORTED)
		{
			dbg_msg("webdl", "http failed, falling back to gameserver");
			ResetMapDownload(false);
			SendMapRequest();
		}
	}

	if(m_pDDNetInfoTask)
	{
		if(m_pDDNetInfoTask->State() == EHttpState::DONE)
		{
			FinishDDNetInfo();
			ResetDDNetInfoTask();
		}
		else if(m_pDDNetInfoTask->State() == EHttpState::ERROR || m_pDDNetInfoTask->State() == EHttpState::ABORTED)
		{
			ResetDDNetInfoTask();
		}
	}

	if(State() == IClient::STATE_ONLINE)
	{
		if(!m_EditJobs.empty())
		{
			std::shared_ptr<CDemoEdit> pJob = m_EditJobs.front();
			if(pJob->State() == IJob::STATE_DONE)
			{
				char aBuf[IO_MAX_PATH_LENGTH + 64];
				if(pJob->Success())
				{
					str_format(aBuf, sizeof(aBuf), "Successfully saved the replay to '%s'!", pJob->Destination());
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", aBuf);

					GameClient()->Echo(Localize("Successfully saved the replay!"));
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "Failed saving the replay to '%s'...", pJob->Destination());
					m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", aBuf);

					GameClient()->Echo(Localize("Failed saving the replay!"));
				}
				m_EditJobs.pop_front();
			}
		}
	}

	// update the server browser
	m_ServerBrowser.Update();

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
	Kernel()->RegisterInterface(static_cast<IHttp *>(&m_Http), false);
}

void CClient::InitInterfaces()
{
	// fetch interfaces
	m_pEngine = Kernel()->RequestInterface<IEngine>();
	m_pEditor = Kernel()->RequestInterface<IEditor>();
	m_pFavorites = Kernel()->RequestInterface<IFavorites>();
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
	m_pNotifications = Kernel()->RequestInterface<INotifications>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_DemoEditor.Init(&m_SnapshotDelta, m_pConsole, m_pStorage);

	m_ServerBrowser.SetBaseInfo(&m_aNetClient[CONN_CONTACT], m_pGameClient->NetVersion());

#if defined(CONF_AUTOUPDATE)
	m_Updater.Init(&m_Http);
#endif

	m_pConfigManager->RegisterCallback(IFavorites::ConfigSaveCallback, m_pFavorites);
	m_Friends.Init();
	m_Foes.Init(true);

	m_GhostRecorder.Init();
	m_GhostLoader.Init();
}

void CClient::Run()
{
	m_LocalStartTime = m_GlobalStartTime = time_get();
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

#ifndef CONF_WEBASM
	char aNetworkError[256];
	if(!InitNetworkClient(aNetworkError, sizeof(aNetworkError)))
	{
		log_error("client", "%s", aNetworkError);
		ShowMessageBox("Network Error", aNetworkError);
		return;
	}
#endif

	if(!m_Http.Init(std::chrono::seconds{1}))
	{
		const char *pErrorMessage = "Failed to initialize the HTTP client.";
		log_error("client", "%s", pErrorMessage);
		ShowMessageBox("HTTP Error", pErrorMessage);
		return;
	}

	// init graphics
	m_pGraphics = CreateEngineGraphicsThreaded();
	Kernel()->RegisterInterface(m_pGraphics); // IEngineGraphics
	Kernel()->RegisterInterface(static_cast<IGraphics *>(m_pGraphics), false);
	if(m_pGraphics->Init() != 0)
	{
		log_error("client", "couldn't init graphics");
		ShowMessageBox("Graphics Error", "The graphics could not be initialized.");
		return;
	}

	// make sure the first frame just clears everything to prevent undesired colors when waiting for io
	Graphics()->Clear(0, 0, 0);
	Graphics()->Swap();

	// init sound, allowed to fail
	const bool SoundInitFailed = Sound()->Init() != 0;

#if defined(CONF_VIDEORECORDER)
	// init video recorder aka ffmpeg
	CVideo::Init();
#endif

	// init text render
	m_pTextRender = Kernel()->RequestInterface<IEngineTextRender>();
	m_pTextRender->Init();

	// init the input
	Input()->Init();

	// init the editor
	m_pEditor->Init();

	m_ServerBrowser.OnInit();
	// loads the existing ddnet info file if it exists
	LoadDDNetInfo();

	LoadDebugFont();

	if(Steam()->GetPlayerName())
	{
		str_copy(g_Config.m_SteamName, Steam()->GetPlayerName());
	}

	Graphics()->AddWindowResizeListener([this] { OnWindowResize(); });

	GameClient()->OnInit();

	m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", "version " GAME_RELEASE_VERSION " on " CONF_PLATFORM_STRING " " CONF_ARCH_STRING, ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f));
	if(GIT_SHORTREV_HASH)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "git revision hash: %s", GIT_SHORTREV_HASH);
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "client", aBuf, ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f));
	}

	//
	m_FpsGraph.Init(0.0f, 120.0f);

	// never start with the editor
	g_Config.m_ClEditor = 0;

	// process pending commands
	m_pConsole->StoreCommands(false);

	m_Fifo.Init(m_pConsole, g_Config.m_ClInputFifo, CFGFLAG_CLIENT);

	InitChecksum();
	m_pConsole->InitChecksum(ChecksumData());

	// request the new ddnet info from server if already past the welcome dialog
	if(g_Config.m_ClShowWelcome)
		g_Config.m_ClShowWelcome = 0;
	else
		RequestDDNetInfo();

	if(SoundInitFailed)
	{
		SWarning Warning(Localize("Sound error"), Localize("The audio device couldn't be initialised."));
		Warning.m_AutoHide = false;
		m_vWarnings.emplace_back(Warning);
	}

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
			const char *pError = DemoPlayer_Play(m_aCmdPlayDemo, IStorage::TYPE_ALL_OR_ABSOLUTE);
			if(pError)
				log_error("demo_player", "playing passed demo file '%s' failed: %s", m_aCmdPlayDemo, pError);
			m_aCmdPlayDemo[0] = 0;
		}

		// handle pending map edits
		if(m_aCmdEditMap[0])
		{
			int Result = m_pEditor->HandleMapDrop(m_aCmdEditMap, IStorage::TYPE_ALL_OR_ABSOLUTE);
			if(Result)
				g_Config.m_ClEditor = true;
			else
				log_error("editor", "editing passed map file '%s' failed", m_aCmdEditMap);
			m_aCmdEditMap[0] = 0;
		}

		// progress on dummy connect when the connection is online
		if(m_DummySendConnInfo && m_aNetClient[CONN_DUMMY].State() == NETSTATE_ONLINE)
		{
			m_DummySendConnInfo = false;
			SendInfo(CONN_DUMMY);
			m_aNetClient[CONN_DUMMY].Update();
			SendReady(CONN_DUMMY);
			GameClient()->SendDummyInfo(true);
			SendEnterGame(CONN_DUMMY);
		}

		// update input
		if(Input()->Update())
		{
			if(State() == IClient::STATE_QUITTING)
				break;
			else
				SetState(IClient::STATE_QUITTING); // SDL_QUIT
		}

		char aFile[IO_MAX_PATH_LENGTH];
		if(Input()->GetDropFile(aFile, sizeof(aFile)))
		{
			if(str_startswith(aFile, CONNECTLINK_NO_SLASH))
				HandleConnectLink(aFile);
			else if(str_endswith(aFile, ".demo"))
				HandleDemoPath(aFile);
			else if(str_endswith(aFile, ".map"))
				HandleMapPath(aFile);
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
			if(g_Config.m_ClEditor)
				m_pEditor->OnClose();
			g_Config.m_ClEditor = g_Config.m_ClEditor ^ 1;
		}

		// render
		{
			if(g_Config.m_ClEditor)
			{
				if(!m_EditorActive)
				{
					Input()->MouseModeRelative();
					GameClient()->OnActivateEditor();
					m_pEditor->OnActivate();
					m_EditorActive = true;
				}
			}
			else if(m_EditorActive)
			{
				m_EditorActive = false;
			}

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
				// update frametime
				m_RenderFrameTime = (Now - m_LastRenderTime) / (float)time_freq();
				m_FpsGraph.Add(1.0f / m_RenderFrameTime);

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

				if(!m_EditorActive)
					Render();
				else
				{
					m_pEditor->OnRender();
					DebugRender();
				}
				m_pGraphics->Swap();
			}
			else if(!IsRenderActive)
			{
				// if the client does not render, it should reset its render time to a time where it would render the first frame, when it wakes up again
				LastRenderTime = g_Config.m_GfxRefreshRate ? (Now - (time_freq() / (int64_t)g_Config.m_GfxRefreshRate)) : Now;
			}
		}

		AutoScreenshot_Cleanup();
		AutoStatScreenshot_Cleanup();
		AutoCSV_Cleanup();

		m_Fifo.Update();

		if(State() == IClient::STATE_QUITTING || State() == IClient::STATE_RESTARTING)
			break;

		// beNice
		auto Now = time_get_nanoseconds();
		decltype(Now) SleepTimeInNanoSeconds{0};
		bool Slept = false;
		if(g_Config.m_ClRefreshRateInactive && !m_pGraphics->WindowActive())
		{
			SleepTimeInNanoSeconds = (std::chrono::nanoseconds(1s) / (int64_t)g_Config.m_ClRefreshRateInactive) - (Now - LastTime);
			std::this_thread::sleep_for(SleepTimeInNanoSeconds);
			Slept = true;
		}
		else if(g_Config.m_ClRefreshRate)
		{
			SleepTimeInNanoSeconds = (std::chrono::nanoseconds(1s) / (int64_t)g_Config.m_ClRefreshRate) - (Now - LastTime);
			auto SleepTimeInNanoSecondsInner = SleepTimeInNanoSeconds;
			auto NowInner = Now;
			while((SleepTimeInNanoSecondsInner / std::chrono::nanoseconds(1us).count()) > 0ns)
			{
				net_socket_read_wait(m_aNetClient[CONN_MAIN].m_Socket, SleepTimeInNanoSecondsInner);
				auto NowInnerCalc = time_get_nanoseconds();
				SleepTimeInNanoSecondsInner -= (NowInnerCalc - NowInner);
				NowInner = NowInnerCalc;
			}
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

		// update local and global time
		m_LocalTime = (time_get() - m_LocalStartTime) / (float)time_freq();
		m_GlobalTime = (time_get() - m_GlobalStartTime) / (float)time_freq();
	}

	GameClient()->RenderShutdownMessage();
	Disconnect();

	if(!m_pConfigManager->Save())
	{
		char aError[128];
		str_format(aError, sizeof(aError), Localize("Saving settings to '%s' failed"), CONFIG_FILE);
		m_vQuittingWarnings.emplace_back(Localize("Error saving settings"), aError);
	}

	m_Fifo.Shutdown();
	m_Http.Shutdown();
	Engine()->ShutdownJobs();

	GameClient()->RenderShutdownMessage();
	GameClient()->OnShutdown();
	delete m_pEditor;

	// close sockets
	for(unsigned int i = 0; i < std::size(m_aNetClient); i++)
		m_aNetClient[i].Close();

	// shutdown text render while graphics are still available
	m_pTextRender->Shutdown();
}

bool CClient::InitNetworkClient(char *pError, size_t ErrorSize)
{
	NETADDR BindAddr;
	if(g_Config.m_Bindaddr[0] == '\0')
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
	}
	else if(net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) != 0)
	{
		str_format(pError, ErrorSize, "The configured bindaddr '%s' cannot be resolved.", g_Config.m_Bindaddr);
		return false;
	}
	BindAddr.type = NETTYPE_ALL;
	for(unsigned int i = 0; i < std::size(m_aNetClient); i++)
	{
		int &PortRef = i == CONN_MAIN ? g_Config.m_ClPort : i == CONN_DUMMY ? g_Config.m_ClDummyPort : g_Config.m_ClContactPort;
		if(PortRef < 1024) // Reject users setting ports that we don't want to use
		{
			PortRef = 0;
		}
		BindAddr.port = PortRef;
		unsigned RemainingAttempts = 25;
		while(BindAddr.port == 0 || !m_aNetClient[i].Open(BindAddr))
		{
			if(BindAddr.port != 0)
			{
				--RemainingAttempts;
				if(RemainingAttempts == 0)
				{
					if(g_Config.m_Bindaddr[0])
						str_format(pError, ErrorSize, "Could not open the network client, try changing or unsetting the bindaddr '%s'.", g_Config.m_Bindaddr);
					else
						str_copy(pError, "Could not open the network client.", ErrorSize);
					return false;
				}
			}
			BindAddr.port = (secure_rand() % 64511) + 1024;
		}
	}
	return true;
}

bool CClient::CtrlShiftKey(int Key, bool &Last)
{
	if(Input()->ModifierIsPressed() && Input()->ShiftIsPressed() && !Last && Input()->KeyIsPressed(Key))
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
	pSelf->DummyDisconnect(nullptr);
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

void CClient::Con_Restart(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->Restart();
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
	pSelf->SendMsg(CONN_MAIN, &Msg, MSGFLAG_FLUSH);
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

#if defined(CONF_VIDEORECORDER)

void CClient::Con_StartVideo(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = static_cast<CClient *>(pUserData);

	if(pResult->NumArguments())
	{
		pSelf->StartVideo(pResult->GetString(0), false);
	}
	else
	{
		pSelf->StartVideo("video", true);
	}
}

void CClient::StartVideo(const char *pFilename, bool WithTimestamp)
{
	if(State() != IClient::STATE_DEMOPLAYBACK)
	{
		log_error("videorecorder", "Video can only be recorded in demo player.");
		return;
	}

	if(IVideo::Current())
	{
		log_error("videorecorder", "Already recording.");
		return;
	}

	char aFilename[IO_MAX_PATH_LENGTH];
	if(WithTimestamp)
	{
		char aTimestamp[20];
		str_timestamp(aTimestamp, sizeof(aTimestamp));
		str_format(aFilename, sizeof(aFilename), "videos/%s_%s.mp4", pFilename, aTimestamp);
	}
	else
	{
		str_format(aFilename, sizeof(aFilename), "videos/%s.mp4", pFilename);
	}

	// wait for idle, so there is no data race
	Graphics()->WaitForIdle();
	// pause the sound device while creating the video instance
	Sound()->PauseAudioDevice();
	new CVideo(Graphics(), Sound(), Storage(), Graphics()->ScreenWidth(), Graphics()->ScreenHeight(), aFilename);
	Sound()->UnpauseAudioDevice();
	if(!IVideo::Current()->Start())
	{
		log_error("videorecorder", "Failed to start recording to '%s'", aFilename);
		m_DemoPlayer.Stop("Failed to start video recording. See local console for details.");
		return;
	}
	if(m_DemoPlayer.Info()->m_Info.m_Paused)
	{
		IVideo::Current()->Pause(true);
	}
	log_info("videorecorder", "Recording to '%s'", aFilename);
}

void CClient::Con_StopVideo(IConsole::IResult *pResult, void *pUserData)
{
	if(!IVideo::Current())
	{
		log_error("videorecorder", "Not recording.");
		return;
	}

	IVideo::Current()->Stop();
	log_info("videorecorder", "Stopped recording.");
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

	if(net_addr_from_url(&Addr, pResult->GetString(0), nullptr, 0) != 0 && net_addr_from_str(&Addr, pResult->GetString(0)) != 0)
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
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: demorecorder isn't recording. Try to rejoin to fix that.");
	}
	else if(DemoRecorder(RECORDER_REPLAYS)->Length() < 1)
	{
		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: demorecorder isn't recording for at least 1 second.");
	}
	else
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		if(pFilename[0] == '\0')
		{
			char aTimestamp[20];
			str_timestamp(aTimestamp, sizeof(aTimestamp));
			str_format(aFilename, sizeof(aFilename), "demos/replays/%s_%s_(replay).demo", m_aCurrentMap, aTimestamp);
		}
		else
		{
			str_format(aFilename, sizeof(aFilename), "demos/replays/%s.demo", pFilename);
			IOHANDLE Handle = m_pStorage->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!Handle)
			{
				m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "ERROR: invalid filename. Try a different one!");
				return;
			}
			io_close(Handle);
			m_pStorage->RemoveFile(aFilename, IStorage::TYPE_SAVE);
		}

		// Stop the recorder to correctly slice the demo after
		DemoRecorder(RECORDER_REPLAYS)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);

		// Slice the demo to get only the last cl_replay_length seconds
		const char *pSrc = m_aDemoRecorder[RECORDER_REPLAYS].CurrentFilename();
		const int EndTick = GameTick(g_Config.m_ClDummy);
		const int StartTick = EndTick - Length * GameTickSpeed();

		m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "replay", "Saving replay...");

		// Create a job to do this slicing in background because it can be a bit long depending on the file size
		std::shared_ptr<CDemoEdit> pDemoEditTask = std::make_shared<CDemoEdit>(GameClient()->NetVersion(), &m_SnapshotDelta, m_pStorage, pSrc, aFilename, StartTick, EndTick);
		Engine()->AddJob(pDemoEditTask);
		m_EditJobs.push_back(pDemoEditTask);

		// And we restart the recorder
		DemoRecorder_UpdateReplayRecorder();
	}
}

void CClient::DemoSlice(const char *pDstPath, CLIENTFUNC_FILTER pfnFilter, void *pUser)
{
	if(m_DemoPlayer.IsPlaying())
	{
		m_DemoEditor.Slice(m_DemoPlayer.Filename(), pDstPath, g_Config.m_ClDemoSliceBegin, g_Config.m_ClDemoSliceEnd, pfnFilter, pUser);
	}
}

const char *CClient::DemoPlayer_Play(const char *pFilename, int StorageType)
{
	// Don't disconnect unless the file exists (only for play command)
	if(!Storage()->FileExists(pFilename, StorageType))
		return "No demo with this filename exists";

	Disconnect();
	m_aNetClient[CONN_MAIN].ResetErrorString();

	SetState(IClient::STATE_LOADING);
	SetLoadingStateDetail(IClient::LOADING_STATE_DETAIL_LOADING_DEMO);
	if((bool)m_LoadingCallback)
		m_LoadingCallback(IClient::LOADING_CALLBACK_DETAIL_DEMO);

	// try to start playback
	m_DemoPlayer.SetListener(this);
	if(m_DemoPlayer.Load(Storage(), m_pConsole, pFilename, StorageType))
	{
		DisconnectWithReason(m_DemoPlayer.ErrorMessage());
		return m_DemoPlayer.ErrorMessage();
	}

	m_Sixup = m_DemoPlayer.IsSixup();

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

	// setup current server info
	mem_zero(&m_CurrentServerInfo, sizeof(m_CurrentServerInfo));
	str_copy(m_CurrentServerInfo.m_aMap, pMapInfo->m_aName);
	m_CurrentServerInfo.m_MapCrc = pMapInfo->m_Crc;
	m_CurrentServerInfo.m_MapSize = pMapInfo->m_Size;

	GameClient()->OnConnected();

	// setup buffers
	mem_zero(m_aaaDemorecSnapshotData, sizeof(m_aaaDemorecSnapshotData));

	for(int SnapshotType = 0; SnapshotType < NUM_SNAPSHOT_TYPES; SnapshotType++)
	{
		m_aapSnapshots[0][SnapshotType] = &m_aDemorecSnapshotHolders[SnapshotType];
		m_aapSnapshots[0][SnapshotType]->m_pSnap = (CSnapshot *)&m_aaaDemorecSnapshotData[SnapshotType][0];
		m_aapSnapshots[0][SnapshotType]->m_pAltSnap = (CSnapshot *)&m_aaaDemorecSnapshotData[SnapshotType][1];
		m_aapSnapshots[0][SnapshotType]->m_SnapSize = 0;
		m_aapSnapshots[0][SnapshotType]->m_AltSnapSize = 0;
		m_aapSnapshots[0][SnapshotType]->m_Tick = -1;
	}

	// enter demo playback state
	SetState(IClient::STATE_DEMOPLAYBACK);

	m_DemoPlayer.Play();
	GameClient()->OnEnterGame();

	return 0;
}

#if defined(CONF_VIDEORECORDER)
const char *CClient::DemoPlayer_Render(const char *pFilename, int StorageType, const char *pVideoName, int SpeedIndex, bool StartPaused)
{
	const char *pError = DemoPlayer_Play(pFilename, StorageType);
	if(pError)
		return pError;

	StartVideo(pVideoName, false);
	m_DemoPlayer.SetSpeedIndex(SpeedIndex);
	if(StartPaused)
	{
		m_DemoPlayer.Pause();
	}
	return nullptr;
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

void CClient::DemoRecorder_Start(const char *pFilename, bool WithTimestamp, int Recorder, bool Verbose)
{
	if(State() != IClient::STATE_ONLINE)
	{
		if(Verbose)
		{
			m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demorec/record", "client is not online");
		}
	}
	else
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		if(WithTimestamp)
		{
			char aTimestamp[20];
			str_timestamp(aTimestamp, sizeof(aTimestamp));
			str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", pFilename, aTimestamp);
		}
		else
		{
			str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pFilename);
		}

		m_aDemoRecorder[Recorder].Start(
			Storage(),
			m_pConsole,
			aFilename,
			IsSixup() ? GameClient()->NetVersion7() : GameClient()->NetVersion(),
			m_aCurrentMap,
			m_pMap->Sha256(),
			m_pMap->Crc(),
			"client",
			m_pMap->MapSize(),
			0,
			m_pMap->File(),
			nullptr,
			nullptr);
	}
}

void CClient::DemoRecorder_HandleAutoStart()
{
	if(g_Config.m_ClAutoDemoRecord)
	{
		DemoRecorder(RECORDER_AUTO)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);

		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "auto/%s", m_aCurrentMap);
		DemoRecorder_Start(aFilename, true, RECORDER_AUTO);

		if(g_Config.m_ClAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/auto", "" /* empty for wild card */, ".demo", g_Config.m_ClAutoDemoMax);
		}
	}

	DemoRecorder_UpdateReplayRecorder();
}

void CClient::DemoRecorder_UpdateReplayRecorder()
{
	if(!g_Config.m_ClReplays && DemoRecorder(RECORDER_REPLAYS)->IsRecording())
	{
		DemoRecorder(RECORDER_REPLAYS)->Stop(IDemoRecorder::EStopMode::REMOVE_FILE);
	}

	if(g_Config.m_ClReplays && !DemoRecorder(RECORDER_REPLAYS)->IsRecording())
	{
		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "replays/replay_tmp_%s", m_aCurrentMap);
		DemoRecorder_Start(aFilename, true, RECORDER_REPLAYS);
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

	if(pSelf->m_aDemoRecorder[RECORDER_MANUAL].IsRecording())
	{
		pSelf->m_pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "demo_recorder", "Demo recorder already recording");
		return;
	}

	if(pResult->NumArguments())
		pSelf->DemoRecorder_Start(pResult->GetString(0), false, RECORDER_MANUAL, true);
	else
		pSelf->DemoRecorder_Start(pSelf->m_aCurrentMap, true, RECORDER_MANUAL, true);
}

void CClient::Con_StopRecord(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pSelf->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
}

void CClient::Con_AddDemoMarker(IConsole::IResult *pResult, void *pUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	for(int Recorder = 0; Recorder < RECORDER_MAX; Recorder++)
		pSelf->DemoRecorder_AddDemoMarker(Recorder);
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
	m_GlobalTime = (time_get() - m_GlobalStartTime) / (float)time_freq();
}

void CClient::ServerBrowserUpdate()
{
	m_ServerBrowser.RequestResort();
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
	pData->InitFiles();
}

#ifndef DDNET_CHECKSUM_SALT
// salt@checksum.ddnet.tw: db877f2b-2ddb-3ba6-9f67-a6d169ec671d
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
	if(Start < 0 || Length < 0 || Start > std::numeric_limits<int>::max() - Length)
	{
		return 2;
	}
	int End = Start + Length;
	int ChecksumBytesEnd = minimum(End, (int)sizeof(m_Checksum.m_aBytes));
	int FileStart = maximum(Start, (int)sizeof(m_Checksum.m_aBytes));
	unsigned char aStartBytes[sizeof(int32_t)];
	unsigned char aEndBytes[sizeof(int32_t)];
	uint_to_bytes_be(aStartBytes, Start);
	uint_to_bytes_be(aEndBytes, End);

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
	//Tested on windows 11 64 bit (gtx 1660 super, intel UHD 630 opengl 1.2.0, 3.3.0 and vulkan 1.1.0)
	int IsFullscreen = g_Config.m_GfxFullscreen;
	int IsBorderless = g_Config.m_GfxBorderless;

	if(!Graphics()->SetWindowScreen(Index))
	{
		return;
	}

	SetWindowParams(3, false); // prevent DDNet to get stretch on monitors

	CVideoMode CurMode;
	Graphics()->GetCurrentVideoMode(CurMode, Index);

	const int Depth = CurMode.m_Red + CurMode.m_Green + CurMode.m_Blue > 16 ? 24 : 16;
	g_Config.m_GfxColorDepth = Depth;
	g_Config.m_GfxScreenWidth = CurMode.m_WindowWidth;
	g_Config.m_GfxScreenHeight = CurMode.m_WindowHeight;
	g_Config.m_GfxScreenRefreshRate = CurMode.m_RefreshRate;

	Graphics()->ResizeToScreen();

	SetWindowParams(IsFullscreen, IsBorderless);
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

void CClient::SetWindowParams(int FullscreenMode, bool IsBorderless)
{
	g_Config.m_GfxFullscreen = clamp(FullscreenMode, 0, 3);
	g_Config.m_GfxBorderless = (int)IsBorderless;
	Graphics()->SetWindowParams(FullscreenMode, IsBorderless);
}

void CClient::ConchainFullscreen(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		if(g_Config.m_GfxFullscreen != pResult->GetInteger(0))
			pSelf->SetWindowParams(pResult->GetInteger(0), g_Config.m_GfxBorderless);
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
			pSelf->SetWindowParams(g_Config.m_GfxFullscreen, !g_Config.m_GfxBorderless);
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CClient::ToggleWindowVSync()
{
	if(Graphics()->SetVSync(g_Config.m_GfxVsync ^ 1))
		g_Config.m_GfxVsync ^= 1;
}

void CClient::Notify(const char *pTitle, const char *pMessage)
{
	if(m_pGraphics->WindowActive() || !g_Config.m_ClShowNotifications)
		return;

	Notifications()->Notify(pTitle, pMessage);
	Graphics()->NotifyWindow();
}

void CClient::OnWindowResize()
{
	TextRender()->OnPreWindowResize();
	GameClient()->OnWindowResize();
	m_pEditor->OnWindowResize();
	TextRender()->OnWindowResize();
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

void CClient::ConchainWindowResize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pSelf->Graphics() && pResult->NumArguments())
	{
		pSelf->Graphics()->ResizeToScreen();
	}
}

void CClient::ConchainTimeoutSeed(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		pSelf->m_GenerateTimeoutSeed = false;
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
		pSelf->DemoRecorder_UpdateReplayRecorder();
	}
}

void CClient::ConchainLoglevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		pSelf->m_pFileLogger->SetFilter(CLogFilter{IConsole::ToLogLevelFilter(g_Config.m_Loglevel)});
	}
}

void CClient::ConchainStdoutOutputLevel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	CClient *pSelf = (CClient *)pUserData;
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() && pSelf->m_pStdoutLogger)
	{
		pSelf->m_pStdoutLogger->SetFilter(CLogFilter{IConsole::ToLogLevelFilter(g_Config.m_StdoutOutputLevel)});
	}
}

void CClient::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	m_pConsole->Register("dummy_connect", "", CFGFLAG_CLIENT, Con_DummyConnect, this, "Connect dummy");
	m_pConsole->Register("dummy_disconnect", "", CFGFLAG_CLIENT, Con_DummyDisconnect, this, "Disconnect dummy");
	m_pConsole->Register("dummy_reset", "", CFGFLAG_CLIENT, Con_DummyResetInput, this, "Reset dummy");

	m_pConsole->Register("quit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit the client");
	m_pConsole->Register("exit", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Quit, this, "Quit the client");
	m_pConsole->Register("restart", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Restart, this, "Restart the client");
	m_pConsole->Register("minimize", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Minimize, this, "Minimize the client");
	m_pConsole->Register("connect", "r[host|ip]", CFGFLAG_CLIENT, Con_Connect, this, "Connect to the specified host/ip");
	m_pConsole->Register("disconnect", "", CFGFLAG_CLIENT, Con_Disconnect, this, "Disconnect from the server");
	m_pConsole->Register("ping", "", CFGFLAG_CLIENT, Con_Ping, this, "Ping the current server");
	m_pConsole->Register("screenshot", "", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Screenshot, this, "Take a screenshot");

#if defined(CONF_VIDEORECORDER)
	m_pConsole->Register("start_video", "?r[file]", CFGFLAG_CLIENT, Con_StartVideo, this, "Start recording a video");
	m_pConsole->Register("stop_video", "", CFGFLAG_CLIENT, Con_StopVideo, this, "Stop recording a video");
#endif

	m_pConsole->Register("rcon", "r[rcon-command]", CFGFLAG_CLIENT, Con_Rcon, this, "Send specified command to rcon");
	m_pConsole->Register("rcon_auth", "r[password]", CFGFLAG_CLIENT, Con_RconAuth, this, "Authenticate to rcon");
	m_pConsole->Register("rcon_login", "s[username] r[password]", CFGFLAG_CLIENT, Con_RconLogin, this, "Authenticate to rcon with a username");
	m_pConsole->Register("play", "r[file]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_Play, this, "Play back a demo");
	m_pConsole->Register("record", "?r[file]", CFGFLAG_CLIENT, Con_Record, this, "Start recording a demo");
	m_pConsole->Register("stoprecord", "", CFGFLAG_CLIENT, Con_StopRecord, this, "Stop recording a demo");
	m_pConsole->Register("add_demomarker", "", CFGFLAG_CLIENT, Con_AddDemoMarker, this, "Add demo timeline marker");
	m_pConsole->Register("begin_favorite_group", "", CFGFLAG_CLIENT, Con_BeginFavoriteGroup, this, "Use this before `add_favorite` to group favorites. End with `end_favorite_group`");
	m_pConsole->Register("end_favorite_group", "", CFGFLAG_CLIENT, Con_EndFavoriteGroup, this, "Use this after `add_favorite` to group favorites. Start with `begin_favorite_group`");
	m_pConsole->Register("add_favorite", "s[host|ip] ?s['allow_ping']", CFGFLAG_CLIENT, Con_AddFavorite, this, "Add a server as a favorite");
	m_pConsole->Register("remove_favorite", "r[host|ip]", CFGFLAG_CLIENT, Con_RemoveFavorite, this, "Remove a server from favorites");
	m_pConsole->Register("demo_slice_start", "", CFGFLAG_CLIENT, Con_DemoSliceBegin, this, "Mark the beginning of a demo cut");
	m_pConsole->Register("demo_slice_end", "", CFGFLAG_CLIENT, Con_DemoSliceEnd, this, "Mark the end of a demo cut");
	m_pConsole->Register("demo_play", "", CFGFLAG_CLIENT, Con_DemoPlay, this, "Play/pause the current demo");
	m_pConsole->Register("demo_speed", "i[speed]", CFGFLAG_CLIENT, Con_DemoSpeed, this, "Set current demo speed");

	m_pConsole->Register("save_replay", "?i[length] ?r[filename]", CFGFLAG_CLIENT, Con_SaveReplay, this, "Save a replay of the last defined amount of seconds");
	m_pConsole->Register("benchmark_quit", "i[seconds] r[file]", CFGFLAG_CLIENT | CFGFLAG_STORE, Con_BenchmarkQuit, this, "Benchmark frame times for number of seconds to file, then quit");

	RustVersionRegister(*m_pConsole);

	m_pConsole->Chain("cl_timeout_seed", ConchainTimeoutSeed, this);
	m_pConsole->Chain("cl_replays", ConchainReplays, this);

	m_pConsole->Chain("password", ConchainPassword, this);

	// used for server browser update
	m_pConsole->Chain("br_filter_string", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_exclude_string", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_full", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_empty", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_spectators", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_friends", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_country", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_country_index", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_pw", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_gametype", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_gametype_strict", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_connecting_players", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_serveraddress", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_unfinished_map", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("br_filter_login", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("add_favorite", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("remove_favorite", ConchainServerBrowserUpdate, this);
	m_pConsole->Chain("end_favorite_group", ConchainServerBrowserUpdate, this);

	m_pConsole->Chain("gfx_screen", ConchainWindowScreen, this);
	m_pConsole->Chain("gfx_screen_width", ConchainWindowResize, this);
	m_pConsole->Chain("gfx_screen_height", ConchainWindowResize, this);
	m_pConsole->Chain("gfx_screen_refresh_rate", ConchainWindowResize, this);
	m_pConsole->Chain("gfx_fullscreen", ConchainFullscreen, this);
	m_pConsole->Chain("gfx_borderless", ConchainWindowBordered, this);
	m_pConsole->Chain("gfx_vsync", ConchainWindowVSync, this);

	m_pConsole->Chain("loglevel", ConchainLoglevel, this);
	m_pConsole->Chain("stdout_output_level", ConchainStdoutOutputLevel, this);
}

static CClient *CreateClient()
{
	return new CClient;
}

void CClient::HandleConnectAddress(const NETADDR *pAddr)
{
	net_addr_str(pAddr, m_aCmdConnect, sizeof(m_aCmdConnect), true);
}

void CClient::HandleConnectLink(const char *pLink)
{
	// Chrome works fine with ddnet:// but not with ddnet:
	// Check ddnet:// before ddnet: because we don't want the // as part of connect command
	if(str_startswith(pLink, CONNECTLINK_DOUBLE_SLASH))
		str_copy(m_aCmdConnect, pLink + sizeof(CONNECTLINK_DOUBLE_SLASH) - 1);
	else if(str_startswith(pLink, CONNECTLINK_NO_SLASH))
		str_copy(m_aCmdConnect, pLink + sizeof(CONNECTLINK_NO_SLASH) - 1);
	else
		str_copy(m_aCmdConnect, pLink);
	// Edge appends / to the URL
	const int len = str_length(m_aCmdConnect);
	if(m_aCmdConnect[len - 1] == '/')
		m_aCmdConnect[len - 1] = '\0';
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
	if(str_startswith(pCommand, CONNECTLINK_NO_SLASH))
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

static bool SaveUnknownCommandCallback(const char *pCommand, void *pUser)
{
	CClient *pClient = static_cast<CClient *>(pUser);
	pClient->ConfigManager()->StoreUnknownCommand(pCommand);
	return true;
}

static Uint32 GetSdlMessageBoxFlags(IClient::EMessageBoxType Type)
{
	switch(Type)
	{
	case IClient::MESSAGE_BOX_TYPE_ERROR:
		return SDL_MESSAGEBOX_ERROR;
	case IClient::MESSAGE_BOX_TYPE_WARNING:
		return SDL_MESSAGEBOX_WARNING;
	case IClient::MESSAGE_BOX_TYPE_INFO:
		return SDL_MESSAGEBOX_INFORMATION;
	}
	dbg_assert(false, "Type invalid");
	return 0;
}

static void ShowMessageBox(const char *pTitle, const char *pMessage, IClient::EMessageBoxType Type = IClient::MESSAGE_BOX_TYPE_ERROR)
{
	SDL_ShowSimpleMessageBox(GetSdlMessageBoxFlags(Type), pTitle, pMessage, nullptr);
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
static int gs_AndroidStarted = false;
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char *argv[]);
int SDL_main(int argc, char *argv2[])
#else
int main(int argc, const char **argv)
#endif
{
	const int64_t MainStart = time_get();

#if defined(CONF_PLATFORM_ANDROID)
	const char **argv = const_cast<const char **>(argv2);
	// Android might not unload the library from memory, causing globals like gs_AndroidStarted
	// not to be initialized correctly when starting the app again.
	if(gs_AndroidStarted)
	{
		::ShowMessageBox("Android Error", "The app was started, but not closed properly, this causes bugs. Please restart or manually close this task.");
		std::exit(0);
	}
	gs_AndroidStarted = true;
#elif defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(true);
#endif
	CCmdlineFix CmdlineFix(&argc, &argv);

#if defined(CONF_EXCEPTION_HANDLING)
	init_exception_handler();
#endif

	std::vector<std::shared_ptr<ILogger>> vpLoggers;
	std::shared_ptr<ILogger> pStdoutLogger = nullptr;
#if defined(CONF_PLATFORM_ANDROID)
	pStdoutLogger = std::shared_ptr<ILogger>(log_logger_android());
#else
	bool Silent = false;
	for(int i = 1; i < argc; i++)
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0)
		{
			Silent = true;
		}
	}
	if(!Silent)
	{
		pStdoutLogger = std::shared_ptr<ILogger>(log_logger_stdout());
	}
#endif
	if(pStdoutLogger)
	{
		vpLoggers.push_back(pStdoutLogger);
	}
	std::shared_ptr<CFutureLogger> pFutureFileLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureFileLogger);
	std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureConsoleLogger);
	std::shared_ptr<CFutureLogger> pFutureAssertionLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureAssertionLogger);
	log_set_global_logger(log_logger_collection(std::move(vpLoggers)).release());

#if defined(CONF_PLATFORM_ANDROID)
	// Initialize Android after logger is available
	const char *pAndroidInitError = InitAndroid();
	if(pAndroidInitError != nullptr)
	{
		log_error("android", "%s", pAndroidInitError);
		::ShowMessageBox("Android Error", pAndroidInitError);
		std::exit(0);
	}
#endif

	std::stack<std::function<void()>> CleanerFunctions;
	std::function<void()> PerformCleanup = [&CleanerFunctions]() mutable {
		while(!CleanerFunctions.empty())
		{
			CleanerFunctions.top()();
			CleanerFunctions.pop();
		}
	};
	std::function<void()> PerformFinalCleanup = []() {
#ifdef CONF_PLATFORM_ANDROID
		// Forcefully terminate the entire process, to ensure that static variables
		// will be initialized correctly when the app is started again after quitting.
		// Returning from the main function is not enough, as this only results in the
		// native thread terminating, but the Java thread will continue. Java does not
		// support unloading libraries once they have been loaded, so all static
		// variables will not have their expected initial values anymore when the app
		// is started again after quitting. The variable gs_AndroidStarted above is
		// used to check that static variables have been initialized properly.
		// TODO: This is not the correct way to close an activity on Android, as it
		//       ignores the activity lifecycle entirely, which may cause issues if
		//       we ever used any global resources like the camera.
		std::exit(0);
#endif
	};
	std::function<void()> PerformAllCleanup = [PerformCleanup, PerformFinalCleanup]() mutable {
		PerformCleanup();
		PerformFinalCleanup();
	};

	const bool RandInitFailed = secure_random_init() != 0;
	if(!RandInitFailed)
		CleanerFunctions.emplace([]() { secure_random_uninit(); });

	// Register SDL for cleanup before creating the kernel and client,
	// so SDL is shutdown after kernel and client. Otherwise the client
	// may crash when shutting down after SDL is already shutdown.
	CleanerFunctions.emplace([]() { SDL_Quit(); });

	CClient *pClient = CreateClient();
	pClient->SetLoggers(pFutureFileLogger, std::move(pStdoutLogger));

	IKernel *pKernel = IKernel::Create();
	pKernel->RegisterInterface(pClient, false);
	pClient->RegisterInterfaces();
	CleanerFunctions.emplace([pKernel, pClient]() {
		// Ensure that the assert handler doesn't use the client/graphics after they've been destroyed
		dbg_assert_set_handler(nullptr);
		pKernel->Shutdown();
		delete pKernel;
		delete pClient;
	});

	const std::thread::id MainThreadId = std::this_thread::get_id();
	dbg_assert_set_handler([MainThreadId, pClient](const char *pMsg) {
		if(MainThreadId != std::this_thread::get_id())
			return;
		char aVersionStr[128];
		if(!os_version_str(aVersionStr, sizeof(aVersionStr)))
			str_copy(aVersionStr, "unknown");
		char aGpuInfo[256];
		pClient->GetGpuInfoString(aGpuInfo);
		char aMessage[768];
		str_format(aMessage, sizeof(aMessage),
			"An assertion error occurred. Please write down or take a screenshot of the following information and report this error.\n"
			"Please also share the assert log which you should find in the 'dumps' folder in your config directory.\n\n"
			"%s\n\n"
			"Platform: %s\n"
			"Game version: %s %s\n"
			"OS version: %s\n\n"
			"%s", // GPU info
			pMsg, CONF_PLATFORM_STRING, GAME_RELEASE_VERSION, GIT_SHORTREV_HASH != nullptr ? GIT_SHORTREV_HASH : "", aVersionStr,
			aGpuInfo);
		pClient->ShowMessageBox("Assertion Error", aMessage);
		// Client will crash due to assertion, don't call PerformAllCleanup in this inconsistent state
	});

	// create the components
	IEngine *pEngine = CreateEngine(GAME_NAME, pFutureConsoleLogger, 2 * std::thread::hardware_concurrency() + 2);
	pKernel->RegisterInterface(pEngine, false);
	CleanerFunctions.emplace([pEngine]() {
		// Engine has to be destroyed before the graphics so that skin download thread can finish
		delete pEngine;
	});

	IStorage *pStorage = CreateStorage(IStorage::STORAGETYPE_CLIENT, argc, argv);
	pKernel->RegisterInterface(pStorage);

	pFutureAssertionLogger->Set(CreateAssertionLogger(pStorage, GAME_NAME));

#if defined(CONF_EXCEPTION_HANDLING)
	char aBufPath[IO_MAX_PATH_LENGTH];
	char aBufName[IO_MAX_PATH_LENGTH];
	char aDate[64];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aBufName, sizeof(aBufName), "dumps/" GAME_NAME "_%s_crash_log_%s_%d_%s.RTP", CONF_PLATFORM_STRING, aDate, pid(), GIT_SHORTREV_HASH != nullptr ? GIT_SHORTREV_HASH : "");
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, aBufName, aBufPath, sizeof(aBufPath));
	set_exception_handler_log_file(aBufPath);
#endif

	if(RandInitFailed)
	{
		const char *pError = "Failed to initialize the secure RNG.";
		log_error("secure", "%s", pError);
		pClient->ShowMessageBox("Secure RNG Error", pError);
		PerformAllCleanup();
		return -1;
	}

	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT).release();
	pKernel->RegisterInterface(pConsole);

	IConfigManager *pConfigManager = CreateConfigManager();
	pKernel->RegisterInterface(pConfigManager);

	IEngineSound *pEngineSound = CreateEngineSound();
	pKernel->RegisterInterface(pEngineSound); // IEngineSound
	pKernel->RegisterInterface(static_cast<ISound *>(pEngineSound), false);

	IEngineInput *pEngineInput = CreateEngineInput();
	pKernel->RegisterInterface(pEngineInput); // IEngineInput
	pKernel->RegisterInterface(static_cast<IInput *>(pEngineInput), false);

	IEngineTextRender *pEngineTextRender = CreateEngineTextRender();
	pKernel->RegisterInterface(pEngineTextRender); // IEngineTextRender
	pKernel->RegisterInterface(static_cast<ITextRender *>(pEngineTextRender), false);

	IEngineMap *pEngineMap = CreateEngineMap();
	pKernel->RegisterInterface(pEngineMap); // IEngineMap
	pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

	IDiscord *pDiscord = CreateDiscord();
	pKernel->RegisterInterface(pDiscord);

	ISteam *pSteam = CreateSteam();
	pKernel->RegisterInterface(pSteam);

	INotifications *pNotifications = CreateNotifications();
	pKernel->RegisterInterface(pNotifications);

	pKernel->RegisterInterface(CreateEditor(), false);
	pKernel->RegisterInterface(CreateFavorites().release());
	pKernel->RegisterInterface(CreateGameClient());

	pEngine->Init();
	pConsole->Init();
	pConfigManager->Init();
	pNotifications->Init(GAME_NAME " Client");

	// register all console commands
	pClient->RegisterCommands();

	pKernel->RequestInterface<IGameClient>()->OnConsoleInit();

	// init client's interfaces
	pClient->InitInterfaces();

	// execute config file
	if(pStorage->FileExists(CONFIG_FILE, IStorage::TYPE_ALL))
	{
		pConsole->SetUnknownCommandCallback(SaveUnknownCommandCallback, pClient);
		if(!pConsole->ExecuteFile(CONFIG_FILE))
		{
			const char *pError = "Failed to load config from '" CONFIG_FILE "'.";
			log_error("client", "%s", pError);
			pClient->ShowMessageBox("Config File Error", pError);
			PerformAllCleanup();
			return -1;
		}
		pConsole->SetUnknownCommandCallback(IConsole::EmptyUnknownCommandCallback, nullptr);
	}

	// execute autoexec file
	if(pStorage->FileExists(AUTOEXEC_CLIENT_FILE, IStorage::TYPE_ALL))
	{
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
	pConsole->ParseArguments(argc - 1, &argv[1]);
	pConsole->SetUnknownCommandCallback(IConsole::EmptyUnknownCommandCallback, nullptr);

	if(pSteam->GetConnectAddress())
	{
		pClient->HandleConnectAddress(pSteam->GetConnectAddress());
		pSteam->ClearConnectAddress();
	}

	if(g_Config.m_Logfile[0])
	{
		const int Mode = g_Config.m_Logappend ? IOFLAG_APPEND : IOFLAG_WRITE;
		IOHANDLE Logfile = pStorage->OpenFile(g_Config.m_Logfile, Mode, IStorage::TYPE_SAVE_OR_ABSOLUTE);
		if(Logfile)
		{
			pFutureFileLogger->Set(log_logger_file(Logfile));
		}
		else
		{
			log_error("client", "failed to open '%s' for logging", g_Config.m_Logfile);
			pFutureFileLogger->Set(log_logger_noop());
		}
	}
	else
	{
		pFutureFileLogger->Set(log_logger_noop());
	}

	// Register protocol and file extensions
#if defined(CONF_FAMILY_WINDOWS)
	pClient->ShellRegister();
#endif

	// Do not automatically translate touch events to mouse events and vice versa.
	SDL_SetHint("SDL_TOUCH_MOUSE_EVENTS", "0");
	SDL_SetHint("SDL_MOUSE_TOUCH_EVENTS", "0");

	// Support longer IME composition strings (enables SDL_TEXTEDITING_EXT).
#if SDL_VERSION_ATLEAST(2, 0, 22)
	SDL_SetHint(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, "1");
#endif

#if defined(CONF_PLATFORM_MACOS)
	// Hints will not be set if there is an existing override hint or environment variable that takes precedence.
	// So this respects cli environment overrides.
	SDL_SetHint("SDL_MAC_OPENGL_ASYNC_DISPATCH", "1");
#endif

#if defined(CONF_FAMILY_WINDOWS)
	SDL_SetHint("SDL_IME_SHOW_UI", g_Config.m_InpImeNativeUi ? "1" : "0");
#else
	SDL_SetHint("SDL_IME_SHOW_UI", "1");
#endif

#if defined(CONF_PLATFORM_ANDROID)
	// Trap the Android back button so it can be handled in our code reliably
	// instead of letting the system handle it.
	SDL_SetHint("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
	// Force landscape screen orientation.
	SDL_SetHint("SDL_IOS_ORIENTATIONS", "LandscapeLeft LandscapeRight");
#endif

	// init SDL
	if(SDL_Init(0) < 0)
	{
		char aError[256];
		str_format(aError, sizeof(aError), "Unable to initialize SDL base: %s", SDL_GetError());
		log_error("client", "%s", aError);
		pClient->ShowMessageBox("SDL Error", aError);
		PerformAllCleanup();
		return -1;
	}

	// run the client
	log_trace("client", "initialization finished after %.2fms, starting...", (time_get() - MainStart) * 1000.0f / (float)time_freq());
	pClient->Run();

	const bool Restarting = pClient->State() == CClient::STATE_RESTARTING;
#if !defined(CONF_PLATFORM_ANDROID)
	char aRestartBinaryPath[IO_MAX_PATH_LENGTH];
	if(Restarting)
	{
		pStorage->GetBinaryPath(PLAT_CLIENT_EXEC, aRestartBinaryPath, sizeof(aRestartBinaryPath));
	}
#endif

	std::vector<SWarning> vQuittingWarnings = pClient->QuittingWarnings();

	PerformCleanup();

	for(const SWarning &Warning : vQuittingWarnings)
	{
		::ShowMessageBox(Warning.m_aWarningTitle, Warning.m_aWarningMsg);
	}

	if(Restarting)
	{
#if defined(CONF_PLATFORM_ANDROID)
		RestartAndroidApp();
#else
		shell_execute(aRestartBinaryPath, EShellExecuteWindowState::FOREGROUND);
#endif
	}

	PerformFinalCleanup();

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
		m_aDemoRecorder[RECORDER_RACE].Start(
			Storage(),
			m_pConsole,
			pFilename,
			IsSixup() ? GameClient()->NetVersion7() : GameClient()->NetVersion(),
			m_aCurrentMap,
			m_pMap->Sha256(),
			m_pMap->Crc(),
			"client",
			m_pMap->MapSize(),
			0,
			m_pMap->File(),
			nullptr,
			nullptr);
}

void CClient::RaceRecord_Stop()
{
	if(m_aDemoRecorder[RECORDER_RACE].IsRecording())
	{
		m_aDemoRecorder[RECORDER_RACE].Stop(IDemoRecorder::EStopMode::KEEP_FILE);
	}
}

bool CClient::RaceRecord_IsRecording()
{
	return m_aDemoRecorder[RECORDER_RACE].IsRecording();
}

void CClient::RequestDDNetInfo()
{
	if(m_pDDNetInfoTask && !m_pDDNetInfoTask->Done())
		return;

	char aUrl[256];
	str_copy(aUrl, DDNET_INFO_URL);

	if(g_Config.m_BrIndicateFinished)
	{
		char aEscaped[128];
		EscapeUrl(aEscaped, sizeof(aEscaped), PlayerName());
		str_append(aUrl, "?name=");
		str_append(aUrl, aEscaped);
	}

	// Use ipv4 so we can know the ingame ip addresses of players before they join game servers
	m_pDDNetInfoTask = HttpGet(aUrl);
	m_pDDNetInfoTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pDDNetInfoTask->IpResolve(IPRESOLVE::V4);
	Http()->Run(m_pDDNetInfoTask);
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

	*pSmoothTick = (int)(SmoothTime * GameTickSpeed() / time_freq()) + 1;
	*pSmoothIntraTick = (SmoothTime - (*pSmoothTick - 1) * time_freq() / GameTickSpeed()) / (float)(time_freq() / GameTickSpeed());
}

void CClient::AddWarning(const SWarning &Warning)
{
	m_vWarnings.emplace_back(Warning);
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
	return GameTickSpeed() + (PredictionMargin() * GameTickSpeed()) / 1000;
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

bool CClient::ViewLink(const char *pLink)
{
#if defined(CONF_PLATFORM_ANDROID)
	if(SDL_OpenURL(pLink) == 0)
	{
		return true;
	}
	log_error("client", "Failed to open link '%s' (%s)", pLink, SDL_GetError());
	return false;
#else
	if(open_link(pLink))
	{
		return true;
	}
	log_error("client", "Failed to open link '%s'", pLink);
	return false;
#endif
}

bool CClient::ViewFile(const char *pFilename)
{
#if defined(CONF_PLATFORM_MACOS)
	return ViewLink(pFilename);
#else
	// Create a file link so the path can contain forward and
	// backward slashes. But the file link must be absolute.
	char aWorkingDir[IO_MAX_PATH_LENGTH];
	if(fs_is_relative_path(pFilename))
	{
		if(!fs_getcwd(aWorkingDir, sizeof(aWorkingDir)))
		{
			log_error("client", "Failed to open file '%s' (failed to get working directory)", pFilename);
			return false;
		}
		str_append(aWorkingDir, "/");
	}
	else
		aWorkingDir[0] = '\0';

	char aFileLink[IO_MAX_PATH_LENGTH];
	str_format(aFileLink, sizeof(aFileLink), "file://%s%s", aWorkingDir, pFilename);
	return ViewLink(aFileLink);
#endif
}

#if defined(CONF_FAMILY_WINDOWS)
void CClient::ShellRegister()
{
	char aFullPath[IO_MAX_PATH_LENGTH];
	Storage()->GetBinaryPathAbsolute(PLAT_CLIENT_EXEC, aFullPath, sizeof(aFullPath));
	if(!aFullPath[0])
	{
		log_error("client", "Failed to register protocol and file extensions: could not determine absolute path");
		return;
	}

	bool Updated = false;
	if(!shell_register_protocol("ddnet", aFullPath, &Updated))
		log_error("client", "Failed to register ddnet protocol");
	if(!shell_register_extension(".map", "Map File", GAME_NAME, aFullPath, &Updated))
		log_error("client", "Failed to register .map file extension");
	if(!shell_register_extension(".demo", "Demo File", GAME_NAME, aFullPath, &Updated))
		log_error("client", "Failed to register .demo file extension");
	if(!shell_register_application(GAME_NAME, aFullPath, &Updated))
		log_error("client", "Failed to register application");
	if(Updated)
		shell_update();
}

void CClient::ShellUnregister()
{
	char aFullPath[IO_MAX_PATH_LENGTH];
	Storage()->GetBinaryPathAbsolute(PLAT_CLIENT_EXEC, aFullPath, sizeof(aFullPath));
	if(!aFullPath[0])
	{
		log_error("client", "Failed to unregister protocol and file extensions: could not determine absolute path");
		return;
	}

	bool Updated = false;
	if(!shell_unregister_class("ddnet", &Updated))
		log_error("client", "Failed to unregister ddnet protocol");
	if(!shell_unregister_class(GAME_NAME ".map", &Updated))
		log_error("client", "Failed to unregister .map file extension");
	if(!shell_unregister_class(GAME_NAME ".demo", &Updated))
		log_error("client", "Failed to unregister .demo file extension");
	if(!shell_unregister_application(aFullPath, &Updated))
		log_error("client", "Failed to unregister application");
	if(Updated)
		shell_update();
}
#endif

void CClient::ShowMessageBox(const char *pTitle, const char *pMessage, EMessageBoxType Type)
{
	if(m_pGraphics == nullptr || !m_pGraphics->ShowMessageBox(GetSdlMessageBoxFlags(Type), pTitle, pMessage))
		::ShowMessageBox(pTitle, pMessage, Type);
}

void CClient::GetGpuInfoString(char (&aGpuInfo)[256])
{
	if(m_pGraphics != nullptr && m_pGraphics->IsBackendInitialized())
	{
		str_format(aGpuInfo, std::size(aGpuInfo), "GPU: %s - %s - %s", m_pGraphics->GetVendorString(), m_pGraphics->GetRendererString(), m_pGraphics->GetVersionString());
	}
	else
	{
		str_copy(aGpuInfo, "Graphics backend was not yet initialized.");
	}
}

void CClient::SetLoggers(std::shared_ptr<ILogger> &&pFileLogger, std::shared_ptr<ILogger> &&pStdoutLogger)
{
	m_pFileLogger = pFileLogger;
	m_pStdoutLogger = pStdoutLogger;
}
