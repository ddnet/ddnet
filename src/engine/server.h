/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H

#include <array>
#include <optional>
#include <type_traits>

#include <base/hash.h>
#include <base/math.h>
#include <base/system.h>

#include "kernel.h"
#include "message.h"
#include <engine/shared/jsonwriter.h>
#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

struct CAntibotRoundData;

// When recording a demo on the server, the ClientId -1 is used
enum
{
	SERVER_DEMO_CLIENT = -1
};

class IServer : public IInterface
{
	MACRO_INTERFACE("server")
protected:
	int m_CurrentGameTick;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
		bool m_GotDDNetVersion;
		int m_DDNetVersion;
		const char *m_pDDNetVersionStr;
		const CUuid *m_pConnectionId;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return SERVER_TICK_SPEED; }

	virtual int Port() const = 0;
	virtual int MaxClients() const = 0;
	virtual int ClientCount() const = 0;
	virtual int DistinctClientCount() const = 0;
	virtual const char *ClientName(int ClientId) const = 0;
	virtual const char *ClientClan(int ClientId) const = 0;
	virtual int ClientCountry(int ClientId) const = 0;
	virtual bool ClientSlotEmpty(int ClientId) const = 0;
	virtual bool ClientIngame(int ClientId) const = 0;
	virtual bool GetClientInfo(int ClientId, CClientInfo *pInfo) const = 0;
	virtual void SetClientDDNetVersion(int ClientId, int DDNetVersion) = 0;
	virtual const NETADDR *ClientAddr(int ClientId) const = 0;
	virtual const std::array<char, NETADDR_MAXSTRSIZE> &ClientAddrStringImpl(int ClientId, bool IncludePort) const = 0;
	inline const char *ClientAddrString(int ClientId, bool IncludePort) const { return ClientAddrStringImpl(ClientId, IncludePort).data(); }

	/**
	 * Returns the version of the client with the given client ID.
	 *
	 * @param ClientId the client Id, which must be between 0 and
	 * MAX_CLIENTS - 1, or equal to SERVER_DEMO_CLIENT for server demos.
	 *
	 * @return The version of the client with the given client ID.
	 * For server demos this is always the latest client version.
	 * On errors, VERSION_NONE is returned.
	 */
	virtual int GetClientVersion(int ClientId) const = 0;
	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientId) = 0;

	template<class T, typename std::enable_if<!protocol7::is_sixup<T>::value, int>::type = 0>
	inline int SendPackMsg(const T *pMsg, int Flags, int ClientId)
	{
		int Result = 0;
		if(ClientId == -1)
		{
			for(int i = 0; i < MaxClients(); i++)
				if(ClientIngame(i))
					Result = SendPackMsgTranslate(pMsg, Flags, i);
		}
		else
		{
			Result = SendPackMsgTranslate(pMsg, Flags, ClientId);
		}
		return Result;
	}

	template<class T, typename std::enable_if<protocol7::is_sixup<T>::value, int>::type = 1>
	inline int SendPackMsg(const T *pMsg, int Flags, int ClientId)
	{
		int Result = 0;
		if(ClientId == -1)
		{
			for(int i = 0; i < MaxClients(); i++)
				if(ClientIngame(i) && IsSixup(i))
					Result = SendPackMsgOne(pMsg, Flags, i);
		}
		else if(IsSixup(ClientId))
			Result = SendPackMsgOne(pMsg, Flags, ClientId);

		return Result;
	}

	template<class T>
	int SendPackMsgTranslate(const T *pMsg, int Flags, int ClientId)
	{
		return SendPackMsgOne(pMsg, Flags, ClientId);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_Emoticon *pMsg, int Flags, int ClientId)
	{
		CNetMsg_Sv_Emoticon MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));
		return Translate(MsgCopy.m_ClientId, ClientId) && SendPackMsgOne(&MsgCopy, Flags, ClientId);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_Chat *pMsg, int Flags, int ClientId)
	{
		CNetMsg_Sv_Chat MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));

		char aBuf[1000];
		if(MsgCopy.m_ClientId >= 0 && !Translate(MsgCopy.m_ClientId, ClientId))
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s", ClientName(MsgCopy.m_ClientId), MsgCopy.m_pMessage);
			MsgCopy.m_pMessage = aBuf;
			MsgCopy.m_ClientId = VANILLA_MAX_CLIENTS - 1;
		}

		if(IsSixup(ClientId))
		{
			protocol7::CNetMsg_Sv_Chat Msg7;
			Msg7.m_ClientId = MsgCopy.m_ClientId;
			Msg7.m_pMessage = MsgCopy.m_pMessage;
			Msg7.m_Mode = MsgCopy.m_Team > 0 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
			Msg7.m_TargetId = -1;
			return SendPackMsgOne(&Msg7, Flags, ClientId);
		}

		return SendPackMsgOne(&MsgCopy, Flags, ClientId);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_KillMsg *pMsg, int Flags, int ClientId)
	{
		CNetMsg_Sv_KillMsg MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));
		if(!Translate(MsgCopy.m_Victim, ClientId))
			return 0;
		if(!Translate(MsgCopy.m_Killer, ClientId))
			MsgCopy.m_Killer = MsgCopy.m_Victim;
		return SendPackMsgOne(&MsgCopy, Flags, ClientId);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_RaceFinish *pMsg, int Flags, int ClientId)
	{
		if(IsSixup(ClientId))
		{
			protocol7::CNetMsg_Sv_RaceFinish Msg7;
			Msg7.m_ClientId = pMsg->m_ClientId;
			Msg7.m_Diff = pMsg->m_Diff;
			Msg7.m_Time = pMsg->m_Time;
			Msg7.m_RecordPersonal = pMsg->m_RecordPersonal;
			Msg7.m_RecordServer = pMsg->m_RecordServer;
			return SendPackMsgOne(&Msg7, Flags, ClientId);
		}
		return SendPackMsgOne(pMsg, Flags, ClientId);
	}

	template<class T>
	int SendPackMsgOne(const T *pMsg, int Flags, int ClientId)
	{
		dbg_assert(ClientId != -1, "SendPackMsgOne called with -1");
		CMsgPacker Packer(T::ms_MsgId, false, protocol7::is_sixup<T>::value);

		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientId);
	}

	bool Translate(int &Target, int Client)
	{
		if(IsSixup(Client))
			return true;
		if(GetClientVersion(Client) >= VERSION_DDNET_OLD)
			return true;
		int *pMap = GetIdMap(Client);
		bool Found = false;
		for(int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		{
			if(Target == pMap[i])
			{
				Target = i;
				Found = true;
				break;
			}
		}
		return Found;
	}

	bool ReverseTranslate(int &Target, int Client)
	{
		if(IsSixup(Client))
			return true;
		if(GetClientVersion(Client) >= VERSION_DDNET_OLD)
			return true;
		Target = clamp(Target, 0, VANILLA_MAX_CLIENTS - 1);
		int *pMap = GetIdMap(Client);
		if(pMap[Target] == -1)
			return false;
		Target = pMap[Target];
		return true;
	}

	virtual void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pSha256, int *pMapCrc) = 0;

	virtual bool WouldClientNameChange(int ClientId, const char *pNameRequest) = 0;
	virtual bool WouldClientClanChange(int ClientId, const char *pClanRequest) = 0;
	virtual void SetClientName(int ClientId, const char *pName) = 0;
	virtual void SetClientClan(int ClientId, const char *pClan) = 0;
	virtual void SetClientCountry(int ClientId, int Country) = 0;
	virtual void SetClientScore(int ClientId, std::optional<int> Score) = 0;
	virtual void SetClientFlags(int ClientId, int Flags) = 0;

	virtual int SnapNewId() = 0;
	virtual void SnapFreeId(int Id) = 0;
	virtual void *SnapNewItem(int Type, int Id, int Size) = 0;

	template<typename T>
	T *SnapNewItem(int Id)
	{
		const int Type = protocol7::is_sixup<T>::value ? -T::ms_MsgId : T::ms_MsgId;
		return static_cast<T *>(SnapNewItem(Type, Id, sizeof(T)));
	}

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV = -1,
		RCON_CID_VOTE = -2,
	};
	virtual void SetRconCid(int ClientId) = 0;
	virtual int GetAuthedState(int ClientId) const = 0;
	virtual const char *GetAuthName(int ClientId) const = 0;
	virtual void Kick(int ClientId, const char *pReason) = 0;
	virtual void Ban(int ClientId, int Seconds, const char *pReason, bool VerbatimReason) = 0;
	virtual void RedirectClient(int ClientId, int Port) = 0;
	virtual void ChangeMap(const char *pMap) = 0;
	virtual void ReloadMap() = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;

	// DDRace

	virtual void SaveDemo(int ClientId, float Time) = 0;
	virtual void StartRecord(int ClientId) = 0;
	virtual void StopRecord(int ClientId) = 0;
	virtual bool IsRecording(int ClientId) = 0;
	virtual void StopDemos() = 0;

	virtual int *GetIdMap(int ClientId) = 0;

	virtual bool DnsblWhite(int ClientId) = 0;
	virtual bool DnsblPending(int ClientId) = 0;
	virtual bool DnsblBlack(int ClientId) = 0;
	virtual const char *GetAnnouncementLine() = 0;
	virtual bool ClientPrevIngame(int ClientId) = 0;
	virtual const char *GetNetErrorString(int ClientId) = 0;
	virtual void ResetNetErrorString(int ClientId) = 0;
	virtual bool SetTimedOut(int ClientId, int OrigId) = 0;
	virtual void SetTimeoutProtected(int ClientId) = 0;

	virtual void SetErrorShutdown(const char *pReason) = 0;
	virtual void ExpireServerInfo() = 0;

	virtual void FillAntibot(CAntibotRoundData *pData) = 0;

	virtual void SendMsgRaw(int ClientId, const void *pData, int Size, int Flags) = 0;

	virtual const char *GetMapName() const = 0;

	virtual bool IsSixup(int ClientId) const = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver")
protected:
public:
	// `pPersistentData` may be null if this is the first time `IGameServer`
	// is instantiated.
	virtual void OnInit(const void *pPersistentData) = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnMapChange(char *pNewMapName, int MapNameSize) = 0;
	// `pPersistentData` may be null if this is the last time `IGameServer`
	// is destroyed.
	virtual void OnShutdown(void *pPersistentData) = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientId) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgId, CUnpacker *pUnpacker, int ClientId) = 0;

	// Called before map reload, for any data that the game wants to
	// persist to the next map.
	//
	// Has the size of the return value of `PersistentClientDataSize()`.
	//
	// Returns whether the game should be supplied with the data when the
	// client connects for the next map.
	virtual bool OnClientDataPersist(int ClientId, void *pData) = 0;

	// Called when a client connects.
	//
	// If it is reconnecting to the game after a map change, the
	// `pPersistentData` point is nonnull and contains the data the game
	// previously stored.
	virtual void OnClientConnected(int ClientId, void *pPersistentData) = 0;

	virtual void OnClientEnter(int ClientId) = 0;
	virtual void OnClientDrop(int ClientId, const char *pReason) = 0;
	virtual void OnClientPrepareInput(int ClientId, void *pInput) = 0;
	virtual void OnClientDirectInput(int ClientId, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientId, void *pInput) = 0;
	virtual void OnClientPredictedEarlyInput(int ClientId, void *pInput) = 0;

	virtual bool IsClientReady(int ClientId) const = 0;
	virtual bool IsClientPlayer(int ClientId) const = 0;

	virtual int PersistentDataSize() const = 0;
	virtual int PersistentClientDataSize() const = 0;

	virtual CUuid GameUuid() const = 0;
	virtual const char *GameType() const = 0;
	virtual const char *Version() const = 0;
	virtual const char *NetVersion() const = 0;

	// DDRace

	virtual void OnPreTickTeehistorian() = 0;

	virtual void OnSetAuthed(int ClientId, int Level) = 0;
	virtual bool PlayerExists(int ClientId) const = 0;

	virtual void TeehistorianRecordAntibot(const void *pData, int DataSize) = 0;
	virtual void TeehistorianRecordPlayerJoin(int ClientId, bool Sixup) = 0;
	virtual void TeehistorianRecordPlayerDrop(int ClientId, const char *pReason) = 0;
	virtual void TeehistorianRecordPlayerRejoin(int ClientId) = 0;
	virtual void TeehistorianRecordPlayerName(int ClientId, const char *pName) = 0;
	virtual void TeehistorianRecordPlayerFinish(int ClientId, int TimeTicks) = 0;
	virtual void TeehistorianRecordTeamFinish(int TeamId, int TimeTicks) = 0;

	virtual void FillAntibot(CAntibotRoundData *pData) = 0;

	/**
	 * Used to report custom player info to master servers.
	 *
	 * @param pJsonWriter A pointer to a CJsonStringWriter which the custom data will be added to.
	 * @param i The client id.
	 */
	virtual void OnUpdatePlayerServerInfo(CJsonStringWriter *pJSonWriter, int Id) = 0;
};

extern IGameServer *CreateGameServer();
#endif
