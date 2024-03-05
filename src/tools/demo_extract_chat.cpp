#include <base/logger.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/shared/demo.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/storage.h>

#include <game/gamecore.h>

static const char *TOOL_NAME = "demo_extract_chat";

class CClientSnapshotHandler
{
public:
	struct CClientData
	{
		char m_aName[MAX_NAME_LENGTH];
	};
	CClientData m_aClients[MAX_CLIENTS];

	CSnapshotStorage::CHolder m_aDemoSnapshotHolders[IClient::NUM_SNAPSHOT_TYPES];
	char m_aaaDemoSnapshotData[IClient::NUM_SNAPSHOT_TYPES][2][CSnapshot::MAX_SIZE];
	CSnapshotStorage::CHolder *m_apSnapshots[IClient::NUM_SNAPSHOT_TYPES];

	CClientSnapshotHandler() :
		m_aClients(), m_aDemoSnapshotHolders()
	{
		mem_zero(m_aaaDemoSnapshotData, sizeof(m_aaaDemoSnapshotData));

		for(int SnapshotType = 0; SnapshotType < IClient::NUM_SNAPSHOT_TYPES; SnapshotType++)
		{
			m_apSnapshots[SnapshotType] = &m_aDemoSnapshotHolders[SnapshotType];
			m_apSnapshots[SnapshotType]->m_pSnap = (CSnapshot *)&m_aaaDemoSnapshotData[SnapshotType][0];
			m_apSnapshots[SnapshotType]->m_pAltSnap = (CSnapshot *)&m_aaaDemoSnapshotData[SnapshotType][1];
			m_apSnapshots[SnapshotType]->m_SnapSize = 0;
			m_apSnapshots[SnapshotType]->m_AltSnapSize = 0;
			m_apSnapshots[SnapshotType]->m_Tick = -1;
		}
	}

	int UnpackAndValidateSnapshot(CSnapshot *pFrom, CSnapshot *pTo)
	{
		CUnpacker Unpacker;
		CSnapshotBuilder Builder;
		Builder.Init();
		CNetObjHandler NetObjHandler;

		int Num = pFrom->NumItems();
		for(int Index = 0; Index < Num; Index++)
		{
			const CSnapshotItem *pFromItem = pFrom->GetItem(Index);
			const int FromItemSize = pFrom->GetItemSize(Index);
			const int ItemType = pFrom->GetItemType(Index);
			const void *pData = pFromItem->Data();
			Unpacker.Reset(pData, FromItemSize);

			void *pRawObj = NetObjHandler.SecureUnpackObj(ItemType, &Unpacker);
			if(!pRawObj)
				continue;

			const int ItemSize = NetObjHandler.GetUnpackedObjSize(ItemType);
			void *pObj = Builder.NewItem(pFromItem->Type(), pFromItem->Id(), ItemSize);
			if(!pObj)
				return -4;

			mem_copy(pObj, pRawObj, ItemSize);
		}

		return Builder.Finish(pTo);
	}

	int SnapNumItems(int SnapId)
	{
		dbg_assert(SnapId >= 0 && SnapId < IClient::NUM_SNAPSHOT_TYPES, "invalid SnapId");
		if(!m_apSnapshots[SnapId])
			return 0;
		return m_apSnapshots[SnapId]->m_pAltSnap->NumItems();
	}

	void *SnapGetItem(int SnapId, int Index, IClient::CSnapItem *pItem)
	{
		dbg_assert(SnapId >= 0 && SnapId < IClient::NUM_SNAPSHOT_TYPES, "invalid SnapId");
		const CSnapshotItem *pSnapshotItem = m_apSnapshots[SnapId]->m_pAltSnap->GetItem(Index);
		pItem->m_DataSize = m_apSnapshots[SnapId]->m_pAltSnap->GetItemSize(Index);
		pItem->m_Type = m_apSnapshots[SnapId]->m_pAltSnap->GetItemType(Index);
		pItem->m_Id = pSnapshotItem->Id();
		return (void *)pSnapshotItem->Data();
	}

	void OnNewSnapshot()
	{
		int Num = SnapNumItems(IClient::SNAP_CURRENT);
		for(int i = 0; i < Num; i++)
		{
			IClient::CSnapItem Item;
			const void *pData = SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

			if(Item.m_Type == NETOBJTYPE_CLIENTINFO)
			{
				const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)pData;
				int ClientId = Item.m_Id;
				if(ClientId < MAX_CLIENTS)
				{
					CClientData *pClient = &m_aClients[ClientId];
					IntsToStr(&pInfo->m_Name0, 4, pClient->m_aName);
				}
			}
		}
	}

	void OnDemoPlayerSnapshot(void *pData, int Size)
	{
		unsigned char aAltSnapBuffer[CSnapshot::MAX_SIZE];
		CSnapshot *pAltSnapBuffer = (CSnapshot *)aAltSnapBuffer;
		const int AltSnapSize = UnpackAndValidateSnapshot((CSnapshot *)pData, pAltSnapBuffer);
		if(AltSnapSize < 0)
			return;

		std::swap(m_apSnapshots[IClient::SNAP_PREV], m_apSnapshots[IClient::SNAP_CURRENT]);
		mem_copy(m_apSnapshots[IClient::SNAP_CURRENT]->m_pSnap, pData, Size);
		mem_copy(m_apSnapshots[IClient::SNAP_CURRENT]->m_pAltSnap, pAltSnapBuffer, AltSnapSize);

		OnNewSnapshot();
	}
};

class CDemoPlayerMessageListener : public CDemoPlayer::IListener
{
public:
	CDemoPlayer *m_pDemoPlayer;
	CClientSnapshotHandler *m_pClientSnapshotHandler;

	void OnDemoPlayerSnapshot(void *pData, int Size) override
	{
		m_pClientSnapshotHandler->OnDemoPlayerSnapshot(pData, Size);
	}

	void OnDemoPlayerMessage(void *pData, int Size) override
	{
		CUnpacker Unpacker;
		Unpacker.Reset(pData, Size);
		CMsgPacker Packer(NETMSG_EX, true);

		int Msg;
		bool Sys;
		CUuid Uuid;

		int Result = UnpackMessageId(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
		if(Result == UNPACKMESSAGE_ERROR)
			return;

		if(!Sys)
		{
			CNetObjHandler NetObjHandler;
			void *pRawMsg = NetObjHandler.SecureUnpackMsg(Msg, &Unpacker);
			if(!pRawMsg)
				return;

			if(Msg == NETMSGTYPE_SV_CHAT)
			{
				CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

				if(pMsg->m_ClientId > -1 && m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName[0] == '\0')
					return;

				const char *Prefix = pMsg->m_Team > 1 ? "whisper" : (pMsg->m_Team ? "teamchat" : "chat");

				if(pMsg->m_ClientId < 0)
				{
					printf("%s: *** %s\n", Prefix, pMsg->m_pMessage);
					return;
				}

				if(pMsg->m_Team == 2) // WHISPER SEND
					printf("%s: -> %s: %s\n", Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
				else if(pMsg->m_Team == 3) // WHISPER RECEIVE
					printf("%s: <- %s: %s\n", Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
				else
					printf("%s: %s: %s\n", Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
			}
			else if(Msg == NETMSGTYPE_SV_BROADCAST)
			{
				CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
				char aBroadcast[1024];
				while((pMsg->m_pMessage = str_next_token(pMsg->m_pMessage, "\n", aBroadcast, sizeof(aBroadcast))))
				{
					if(aBroadcast[0] != '\0')
					{
						printf("broadcast: %s\n", aBroadcast);
					}
				}
			}
		}
	}
};

static int ExtractDemoChat(const char *pDemoFilePath, IStorage *pStorage)
{
	CSnapshotDelta DemoSnapshotDelta;
	CDemoPlayer DemoPlayer(&DemoSnapshotDelta, false);

	if(DemoPlayer.Load(pStorage, nullptr, pDemoFilePath, IStorage::TYPE_ALL_OR_ABSOLUTE) == -1)
	{
		log_error(TOOL_NAME, "Demo file '%s' failed to load: %s", pDemoFilePath, DemoPlayer.ErrorMessage());
		return -1;
	}

	CClientSnapshotHandler Handler;
	CDemoPlayerMessageListener Listener;
	Listener.m_pDemoPlayer = &DemoPlayer;
	Listener.m_pClientSnapshotHandler = &Handler;
	DemoPlayer.SetListener(&Listener);

	const CDemoPlayer::CPlaybackInfo *pInfo = DemoPlayer.Info();
	CNetBase::Init();
	DemoPlayer.Play();

	while(DemoPlayer.IsPlaying())
	{
		DemoPlayer.Update(false);
		if(pInfo->m_Info.m_Paused)
			break;
	}

	DemoPlayer.Stop();

	return 0;
}

int main(int argc, const char *argv[])
{
	// Create storage before setting logger to avoid log messages from storage creation
	IStorage *pStorage = CreateLocalStorage();

	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();

	if(!pStorage)
	{
		log_error(TOOL_NAME, "Error creating local storage");
		return -1;
	}

	if(argc != 2)
	{
		log_error(TOOL_NAME, "Usage: %s <demo_filename>", TOOL_NAME);
		return -1;
	}

	return ExtractDemoChat(argv[1], pStorage);
}
