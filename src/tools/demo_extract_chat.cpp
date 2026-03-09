#include <base/dbg.h>
#include <base/logger.h>
#include <base/mem.h>
#include <base/os.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/shared/demo.h>
#include <engine/shared/network.h>
#include <engine/shared/snapshot.h>
#include <engine/storage.h>

#include <game/gamecore.h>

#include <memory>

static const char *TOOL_NAME = "demo_extract_chat";

class CClientSnapshotHandler
{
public:
	struct CClientData
	{
		char m_aName[MAX_NAME_LENGTH];
	};
	CClientData m_aClients[MAX_CLIENTS];

	CSnapshotBuffer m_aDemoSnapshotData[IClient::NUM_SNAPSHOT_TYPES];
	CSnapshot *m_apAltSnapshots[IClient::NUM_SNAPSHOT_TYPES];

	CClientSnapshotHandler() :
		m_aClients()
	{
		mem_zero(m_aDemoSnapshotData, sizeof(m_aDemoSnapshotData));

		for(int SnapshotType = 0; SnapshotType < IClient::NUM_SNAPSHOT_TYPES; SnapshotType++)
		{
			m_apAltSnapshots[SnapshotType] = m_aDemoSnapshotData[SnapshotType].AsSnapshot();
		}
	}

	int UnpackAndValidateSnapshot(CSnapshot *pFrom, CSnapshotBuffer *pTo)
	{
		CUnpacker Unpacker;
		rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder_New();
		pBuilder->Init(false);
		CNetObjHandler NetObjHandler;

		int Num = pFrom->NumItems();
		for(int Index = 0; Index < Num; Index++)
		{
			const CSnapshotItem *pFromItem = pFrom->GetItem(Index);
			const int FromItemSize = pFrom->GetItemSize(Index);
			const int ItemType = pFrom->GetItemType(Index);
			const void *pData = pFromItem->Data();

			if(ItemType <= 0)
			{
				// Don't add extended item type descriptions, they get
				// added implicitly (== 0).
				//
				// Don't add items of unknown item types either (< 0).
				continue;
			}

			Unpacker.Reset(pData, FromItemSize);

			const void *pSecuredData = NetObjHandler.SecureUnpackObj(ItemType, &Unpacker);
			if(!pSecuredData)
			{
				continue;
			}

			const int ItemSize = NetObjHandler.GetUnpackedObjSize(ItemType);
			if(!pBuilder->NewItem(ItemType, pFromItem->Id(), rust::Slice((const int32_t *)pSecuredData, ItemSize / sizeof(int32_t))))
			{
				return -4;
			}
		}

		return pBuilder->Finish(*pTo);
	}

	int SnapNumItems(int SnapId)
	{
		dbg_assert(SnapId >= 0 && SnapId < IClient::NUM_SNAPSHOT_TYPES, "Invalid SnapId: %d", SnapId);
		return m_apAltSnapshots[SnapId]->NumItems();
	}

	IClient::CSnapItem SnapGetItem(int SnapId, int Index)
	{
		dbg_assert(SnapId >= 0 && SnapId < IClient::NUM_SNAPSHOT_TYPES, "Invalid SnapId: %d", SnapId);
		const CSnapshot *pSnapshot = m_apAltSnapshots[SnapId];
		const CSnapshotItem *pSnapshotItem = m_apAltSnapshots[SnapId]->GetItem(Index);
		IClient::CSnapItem Item;
		Item.m_Type = pSnapshot->GetItemType(Index);
		Item.m_Id = pSnapshotItem->Id();
		Item.m_pData = pSnapshotItem->Data();
		Item.m_DataSize = pSnapshot->GetItemSize(Index);
		return Item;
	}

	void OnNewSnapshot()
	{
		int Num = SnapNumItems(IClient::SNAP_CURRENT);
		for(int i = 0; i < Num; i++)
		{
			const IClient::CSnapItem Item = SnapGetItem(IClient::SNAP_CURRENT, i);

			if(Item.m_Type == NETOBJTYPE_CLIENTINFO)
			{
				const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)Item.m_pData;
				int ClientId = Item.m_Id;
				if(ClientId < MAX_CLIENTS)
				{
					CClientData *pClient = &m_aClients[ClientId];
					IntsToStr(pInfo->m_aName, std::size(pInfo->m_aName), pClient->m_aName, sizeof(pClient->m_aName));
				}
			}
		}
	}

	void OnDemoPlayerSnapshot(void *pData, int Size)
	{
		CSnapshotBuffer AltSnapBuffer;
		const int AltSnapSize = UnpackAndValidateSnapshot((CSnapshot *)pData, &AltSnapBuffer);
		if(AltSnapSize < 0)
			return;

		std::swap(m_apAltSnapshots[IClient::SNAP_PREV], m_apAltSnapshots[IClient::SNAP_CURRENT]);
		mem_copy(m_apAltSnapshots[IClient::SNAP_CURRENT], AltSnapBuffer.AsSnapshot(), AltSnapSize);

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

			const IDemoPlayer::CInfo &Info = m_pDemoPlayer->Info()->m_Info;
			char aTime[20];
			str_time((int64_t)(Info.m_CurrentTick - Info.m_FirstTick) / SERVER_TICK_SPEED * 100, ETimeFormat::HOURS, aTime, sizeof(aTime));

			if(Msg == NETMSGTYPE_SV_CHAT)
			{
				CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

				if(pMsg->m_ClientId > -1 && m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName[0] == '\0')
					return;

				const char *Prefix = pMsg->m_Team > 1 ? "whisper" : (pMsg->m_Team ? "teamchat" : "chat");

				if(pMsg->m_ClientId < 0)
				{
					printf("[%s] %s: *** %s\n", aTime, Prefix, pMsg->m_pMessage);
					return;
				}

				if(pMsg->m_Team == TEAM_WHISPER_SEND)
					printf("[%s] %s: -> %s: %s\n", aTime, Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
				else if(pMsg->m_Team == TEAM_WHISPER_RECV)
					printf("[%s] %s: <- %s: %s\n", aTime, Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
				else
					printf("[%s] %s: %s: %s\n", aTime, Prefix, m_pClientSnapshotHandler->m_aClients[pMsg->m_ClientId].m_aName, pMsg->m_pMessage);
			}
			else if(Msg == NETMSGTYPE_SV_BROADCAST)
			{
				CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;
				char aBroadcast[1024];
				while((pMsg->m_pMessage = str_next_token(pMsg->m_pMessage, "\n", aBroadcast, sizeof(aBroadcast))))
				{
					if(aBroadcast[0] != '\0')
					{
						printf("[%s] broadcast: %s\n", aTime, aBroadcast);
					}
				}
			}
		}
	}
};

static int ExtractDemoChat(const char *pDemoFilePath, CSnapshotDelta *pSnapshotDelta, CSnapshotDelta *pSnapshotDeltaSixup, IStorage *pStorage)
{
	CDemoPlayer DemoPlayer(pSnapshotDelta, pSnapshotDeltaSixup, false);

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

static rust::Box<CSnapshotDelta> CreateSnapshotDelta()
{
	rust::Box<CSnapshotDelta> pResult = CSnapshotDelta_New();
	CNetObjHandler NetObjHandler;
	for(int i = 0; i < NUM_NETOBJTYPES; i++)
	{
		pResult->SetStaticsize(i, NetObjHandler.GetObjSize(i));
	}
	return pResult;
}

static rust::Box<CSnapshotDelta> CreateSnapshotDeltaSixup()
{
	rust::Box<CSnapshotDelta> pResult = CSnapshotDelta_New();
	protocol7::CNetObjHandler NetObjHandler7;
	// HACK: only set static size for items, which were available in the first 0.7 release
	// so new items don't break the snapshot delta
	static const int OLD_NUM_NETOBJTYPES = 23;
	for(int i = 0; i < OLD_NUM_NETOBJTYPES; i++)
	{
		pResult->SetStaticsize(i, NetObjHandler7.GetObjSize(i));
	}
	return pResult;
}

int main(int argc, const char *argv[])
{
	// Create storage before setting logger to avoid log messages from storage creation
	std::unique_ptr<IStorage> pStorage = CreateLocalStorage();

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

	rust::Box<CSnapshotDelta> pSnapshotDelta = CreateSnapshotDelta();
	rust::Box<CSnapshotDelta> pSnapshotDeltaSixup = CreateSnapshotDeltaSixup();

	return ExtractDemoChat(argv[1], &*pSnapshotDelta, &*pSnapshotDeltaSixup, pStorage.get());
}
