/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_NETWORK_H
#define ENGINE_SHARED_NETWORK_H

#include "ringbuffer.h"
#include "stun.h"

#include <base/math.h>
#include <base/system.h>

class CHuffman;
class CNetBan;
typedef struct DdnetNet CNet;

enum
{
	NETSENDFLAG_VITAL = 1,
	NETSENDFLAG_CONNLESS = 2,
	NETSENDFLAG_FLUSH = 4,
	NETSENDFLAG_EXTENDED = 8,

	NETSTATE_OFFLINE = 0,
	NETSTATE_CONNECTING,
	NETSTATE_ONLINE,

	NETBANTYPE_SOFT = 1,
	NETBANTYPE_DROP = 2,
};

enum
{
	NET_MAX_PACKETSIZE = 1400,
	NET_MAX_PAYLOAD = NET_MAX_PACKETSIZE - 6,
	NET_MAX_CLIENTS = 64,
	NET_MAX_CONSOLE_CLIENTS = 4,

	NET_CONNSTATE_OFFLINE = 0,
	NET_CONNSTATE_CONNECT = 1,
	NET_CONNSTATE_PENDING = 2,
	NET_CONNSTATE_ONLINE = 3,
	NET_CONNSTATE_ERROR = 4,

	NET_CHUNKFLAG_VITAL = 1,
	NET_CHUNKFLAG_RESEND = 2,

	NET_CONNLIMIT_IPS = 16,
};

typedef int SECURITY_TOKEN;

enum
{
	NET_SECURITY_TOKEN_UNKNOWN = -1,
};

typedef int (*NETFUNC_DELCLIENT)(int ClientID, const char *pReason, void *pUser);
typedef int (*NETFUNC_NEWCLIENT_CON)(int ClientID, void *pUser);
typedef int (*NETFUNC_NEWCLIENT)(int ClientID, void *pUser, bool Sixup);
typedef int (*NETFUNC_NEWCLIENT_NOAUTH)(int ClientID, void *pUser);
typedef int (*NETFUNC_CLIENTREJOIN)(int ClientID, void *pUser);

struct CNetChunk
{
	// -1 means that it's a stateless packet
	// 0 on the client means the server
	int m_ClientID;
	NETADDR m_Address; // only used when client_id == -1
	int m_Flags;
	int m_DataSize;
	const void *m_pData;
	// only used if the flags contain NETSENDFLAG_EXTENDED and NETSENDFLAG_CONNLESS
	unsigned char m_aExtraData[4];
};

class CNetChunkHeader
{
public:
	int m_Flags;
	int m_Size;
	int m_Sequence;

	unsigned char *Pack(unsigned char *pData, int Split = 4);
	unsigned char *Unpack(unsigned char *pData, int Split = 4);
};

class CNetChunkResend
{
public:
	int m_Flags;
	int m_DataSize;
	unsigned char *m_pData;

	int m_Sequence;
	int64_t m_LastSendTime;
	int64_t m_FirstSendTime;
};

class CNetPacketConstruct
{
public:
	int m_Flags;
	int m_Ack;
	int m_NumChunks;
	int m_DataSize;
	unsigned char m_aChunkData[NET_MAX_PAYLOAD];
	unsigned char m_aExtraData[4];
};

enum class CONNECTIVITY
{
	UNKNOWN,
	CHECKING,
	UNREACHABLE,
	REACHABLE,
	ADDRESS_KNOWN,
};

class CStun
{
	class CProtocol
	{
		int m_Index;
		NETSOCKET m_Socket;
		CStunData m_Stun;
		bool m_HaveStunServer = false;
		NETADDR m_StunServer;
		bool m_HaveAddr = false;
		NETADDR m_Addr;
		int64_t m_LastResponse = -1;
		int64_t m_NextTry = -1;
		int m_NumUnsuccessfulTries = -1;

	public:
		CProtocol(int Index, NETSOCKET Socket);
		void FeedStunServer(NETADDR StunServer);
		void Refresh();
		void Update();
		bool OnPacket(NETADDR Addr, unsigned char *pData, int DataSize);
		CONNECTIVITY GetConnectivity(NETADDR *pGlobalAddr);
	};
	CProtocol m_aProtocols[2];

public:
	CStun(NETSOCKET Socket);
	void FeedStunServer(NETADDR StunServer);
	void Refresh();
	void Update();
	bool OnPacket(NETADDR Addr, unsigned char *pData, int DataSize);
	CONNECTIVITY GetConnectivity(int NetType, NETADDR *pGlobalAddr);
};

class CConsoleNetConnection
{
private:
	int m_State;

	NETADDR m_PeerAddr;
	NETSOCKET m_Socket;

	char m_aBuffer[NET_MAX_PACKETSIZE];
	int m_BufferOffset;

	char m_aErrorString[256];

	bool m_LineEndingDetected;
	char m_aLineEnding[3];

public:
	void Init(NETSOCKET Socket, const NETADDR *pAddr);
	void Disconnect(const char *pReason);

	int State() const { return m_State; }
	const NETADDR *PeerAddress() const { return &m_PeerAddr; }
	const char *ErrorString() const { return m_aErrorString; }

	void Reset();
	int Update();
	int Send(const char *pLine);
	int Recv(char *pLine, int MaxLength);
};

// server side
class CNetServer
{
	CNet *m_pNet = nullptr;
	unsigned char m_aBuffer[2048];

	NETFUNC_NEWCLIENT m_pfnNewClient = nullptr;
	NETFUNC_DELCLIENT m_pfnDelClient = nullptr;
	void *m_pUser = nullptr;

	// Next client ID to try.
	int m_NextClientID = 0;

	// Stores a mapping from client IDs to peer IDs of the network library.
	// The opposite mapping is stored in the userdata of the library.
	uint64_t m_aPeerMapping[NET_MAX_CLIENTS];

public:
	~CNetServer();

	int SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser);
	int SetCallbacks(NETFUNC_NEWCLIENT pfnNewClient, NETFUNC_NEWCLIENT_NOAUTH pfnNewClientNoAuth, NETFUNC_CLIENTREJOIN pfnClientRejoin, NETFUNC_DELCLIENT pfnDelClient, void *pUser);

	//
	bool Open(NETADDR BindAddr, CNetBan *pNetBan, int MaxClients, int MaxClientsPerIP);
	int Close();

	//
	int Recv(CNetChunk *pChunk, SECURITY_TOKEN *pResponseToken);
	int Send(CNetChunk *pChunk);
	int Update();
	void Wait(uint64_t Microseconds);

	//
	int Drop(int ClientID, const char *pReason);

	// status requests
	const NETADDR *ClientAddr(int ClientID) const;
	bool HasSecurityToken(int ClientID) const;
	NETADDR Address() const;
	NETSOCKET Socket() const;
	CNetBan *NetBan() const;
	int NetType() const;
	int MaxClients() const;

	int SendConnlessSixup(CNetChunk *pChunk, SECURITY_TOKEN ResponseToken);

	//
	void SetMaxClientsPerIP(int Max);
	bool SetTimedOut(int ClientID, int OrigID);
	void SetTimeoutProtected(int ClientID);

	int ResetErrorString(int ClientID);
	const char *ErrorString(int ClientID);

	// anti spoof
	SECURITY_TOKEN GetGlobalToken();
};

class CNetConsole
{
	struct CSlot
	{
		CConsoleNetConnection m_Connection;
	};

	NETSOCKET m_Socket;
	CNetBan *m_pNetBan;
	CSlot m_aSlots[NET_MAX_CONSOLE_CLIENTS];

	NETFUNC_NEWCLIENT_CON m_pfnNewClient;
	NETFUNC_DELCLIENT m_pfnDelClient;
	void *m_pUser;

public:
	void SetCallbacks(NETFUNC_NEWCLIENT_CON pfnNewClient, NETFUNC_DELCLIENT pfnDelClient, void *pUser);

	//
	bool Open(NETADDR BindAddr, CNetBan *pNetBan);
	int Close();

	//
	int Recv(char *pLine, int MaxLength, int *pClientID = nullptr);
	int Send(int ClientID, const char *pLine);
	int Update();

	//
	int AcceptClient(NETSOCKET Socket, const NETADDR *pAddr);
	int Drop(int ClientID, const char *pReason);

	// status requests
	const NETADDR *ClientAddr(int ClientID) const { return m_aSlots[ClientID].m_Connection.PeerAddress(); }
	CNetBan *NetBan() const { return m_pNetBan; }
};

// client side
class CNetClient
{
	CNet *m_pNet = nullptr;
	unsigned char m_aBuffer[2048] = {0};
	int m_State = NETSTATE_OFFLINE;
	int m_PeerID = -1;

	CStun *m_pStun = nullptr;

public:
	~CNetClient();

	// openness
	bool Open(NETADDR BindAddr);
	int Close();

	// connection state
	int Disconnect(const char *pReason);
	int Connect(const NETADDR *pAddr, int NumAddrs);

	// communication
	int Recv(CNetChunk *pChunk);
	int Send(CNetChunk *pChunk);
	void Wait(uint64_t Microseconds);

	// pumping
	int Update();

	int ResetErrorString();

	// error and state
	int NetType() const;
	int State();
	const NETADDR *ServerAddress() const;
	void ConnectAddresses(const NETADDR **ppAddrs, int *pNumAddrs) const;
	int GotProblems(int64_t MaxLatency) const;
	const char *ErrorString() const;

	bool SecurityTokenUnknown() { return false; }

	// stun
	void FeedStunServer(NETADDR StunServer);
	void RefreshStun();
	CONNECTIVITY GetConnectivity(int NetType, NETADDR *pGlobalAddr);
};

class CNetBase
{
	static CHuffman ms_Huffman;

public:
	static void OpenLog(IOHANDLE DataLogSent, IOHANDLE DataLogRecv);
	static void CloseLog();
	static void Init();
	static int Compress(const void *pData, int DataSize, void *pOutput, int OutputSize);
	static int Decompress(const void *pData, int DataSize, void *pOutput, int OutputSize);
};

#endif
