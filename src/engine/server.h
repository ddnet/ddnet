/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H
#include "kernel.h"
#include "message.h"
#include <game/generated/protocol.h>
#include <engine/shared/protocol.h>

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
		int m_ClientVersion;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual int MaxClients() const = 0;
	virtual const char *ClientName(int ClientID) = 0;
	virtual const char *ClientClan(int ClientID) = 0;
	virtual int ClientCountry(int ClientID) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size) = 0;
	virtual void RestrictRconOutput(int ClientID) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		int result = 0;
		T tmp;
		if (ClientID == -1)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(ClientIngame(i))
				{
					mem_copy(&tmp, pMsg, sizeof(T));
					result = SendPackMsgTranslate(&tmp, Flags, i);
				}
		} else {
			mem_copy(&tmp, pMsg, sizeof(T));
			result = SendPackMsgTranslate(&tmp, Flags, ClientID);
		}
		return result;
	}

	template<class T>
	int SendPackMsgTranslate(T *pMsg, int Flags, int ClientID)
	{
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_Emoticon *pMsg, int Flags, int ClientID)
	{
		return Translate(pMsg->m_ClientID, ClientID) && SendPackMsgOne(pMsg, Flags, ClientID);
	}

	char msgbuf[1000];

	int SendPackMsgTranslate(CNetMsg_Sv_Chat *pMsg, int Flags, int ClientID)
	{
		if (pMsg->m_ClientID >= 0 && !Translate(pMsg->m_ClientID, ClientID))
		{
			str_format(msgbuf, sizeof(msgbuf), "%s: %s", ClientName(pMsg->m_ClientID), pMsg->m_pMessage);
			pMsg->m_pMessage = msgbuf;
			pMsg->m_ClientID = VANILLA_MAX_CLIENTS - 1;
		}
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	int SendPackMsgTranslate(CNetMsg_Sv_KillMsg *pMsg, int Flags, int ClientID)
	{
		if (!Translate(pMsg->m_Victim, ClientID)) return 0;
		if (!Translate(pMsg->m_Killer, ClientID)) pMsg->m_Killer = pMsg->m_Victim;
		return SendPackMsgOne(pMsg, Flags, ClientID);
	}

	template<class T>
	int SendPackMsgOne(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID());
		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	bool Translate(int& Target, int Client)
	{
		CClientInfo Info;
		GetClientInfo(Client, &Info);
		if (Info.m_ClientVersion >= VERSION_DDNET_OLD)
			return true;
		int *pMap = GetIdMap(Client);
		bool Found = false;
		for (int i = 0; i < VANILLA_MAX_CLIENTS; i++)
		{
			if (Target == pMap[i])
			{
				Target = i;
				Found = true;
				break;
			}
		}
		return Found;
	}

	bool ReverseTranslate(int& Target, int Client)
	{
		CClientInfo Info;
		GetClientInfo(Client, &Info);
		if (Info.m_ClientVersion >= VERSION_DDNET_OLD)
			return true;
		int *pMap = GetIdMap(Client);
		if (pMap[Target] == -1)
			return false;
		Target = pMap[Target];
		return true;
	}

	virtual void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, int *pMapCrc) = 0;

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		AUTHED_NO=0,
		AUTHED_HELPER,
		AUTHED_MOD,
		AUTHED_ADMIN,

		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual int GetAuthedState(int ClientID) = 0;
	virtual const char *GetAuthName(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;
	virtual void Ban(int ClientID, int Duration, const char *pReason) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;

	// DDRace

	virtual void SaveDemo(int ClientID, float Time) = 0;
	virtual void StartRecord(int ClientID) = 0;
	virtual void StopRecord(int ClientID) = 0;
	virtual bool IsRecording(int ClientID) = 0;

	virtual void GetClientAddr(int ClientID, NETADDR *pAddr) = 0;

	virtual int* GetIdMap(int ClientID) = 0;

	virtual bool DnsblWhite(int ClientID) = 0;
	virtual const char *GetAnnouncementLine(char const *FileName) = 0;
	virtual bool ClientPrevIngame(int ClientID) = 0;
	virtual const char *GetNetErrorString(int ClientID) = 0;
	virtual void ResetNetErrorString(int ClientID) = 0;
	virtual bool SetTimedOut(int ClientID, int OrigID) = 0;
	virtual void SetTimeoutProtected(int ClientID) = 0;

	virtual void SetErrorShutdown(const char *pReason) = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit() = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnMapChange(char *pNewMapName, int MapNameSize) = 0;

	// FullShutdown is true if the program is about to exit (not if the map is changed)
	virtual void OnShutdown(bool FullShutdown = false) = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;

	// DDRace

	virtual void OnSetAuthed(int ClientID, int Level) = 0;
	virtual int GetClientVersion(int ClientID) = 0;
	virtual void SetClientVersion(int ClientID, int Version) = 0;
	virtual bool PlayerExists(int ClientID) = 0;

	virtual void OnClientEngineJoin(int ClientID) = 0;
	virtual void OnClientEngineDrop(int ClientID, const char *pReason) = 0;
};

extern IGameServer *CreateGameServer();
#endif
