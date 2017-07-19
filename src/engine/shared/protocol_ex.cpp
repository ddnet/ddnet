#include "protocol_ex.h"

#include "config.h"
#include "protocol.h"
#include "uuid_manager.h"

#include <new>

void RegisterUuids(class CUuidManager *pManager)
{
	#define UUID(id, name) pManager->RegisterName(id, name);
	#include "protocol_ex_msgs.h"
	#undef UUID
}

int UnpackMessageID(int *pID, bool *pSys, struct CUuid *pUuid, CUnpacker *pUnpacker, CMsgPacker *pPacker)
{
	*pID = 0;
	*pSys = false;
	mem_zero(pUuid, sizeof(*pUuid));

	int MsgID = pUnpacker->GetInt();

	if(pUnpacker->Error())
	{
		return UNPACKMESSAGE_ERROR;
	}

	*pID = MsgID >> 1;
	*pSys = MsgID & 1;

	if(*pID < 0 || *pID >= OFFSET_UUID)
	{
		return UNPACKMESSAGE_ERROR;
	}

	if(*pID != 0) // NETMSG_EX, NETMSGTYPE_EX
	{
		return UNPACKMESSAGE_OK;
	}

	*pID = g_UuidManager.UnpackUuid(pUnpacker, pUuid);

	if(*pID == UUID_INVALID || *pID == UUID_UNKNOWN)
	{
		return UNPACKMESSAGE_ERROR;
	}

	if(*pSys)
	{
		switch(*pID)
		{
		case NETMSG_WHATIS:
			{
				CUuid Uuid2;
				int ID2 = g_UuidManager.UnpackUuid(pUnpacker, &Uuid2);
				if(ID2 == UUID_INVALID)
				{
					break;
				}
				if(ID2 == UUID_UNKNOWN)
				{
					new (pPacker) CMsgPacker(NETMSG_IDONTKNOW);
					pPacker->AddRaw(&Uuid2, sizeof(Uuid2));
				}
				else
				{
					new (pPacker) CMsgPacker(NETMSG_ITIS);
					pPacker->AddRaw(&Uuid2, sizeof(Uuid2));
					pPacker->AddString(g_UuidManager.GetName(ID2), 0);
				}
				return UNPACKMESSAGE_ANSWER;
			}
		case NETMSG_IDONTKNOW:
			if(g_Config.m_Debug)
			{
				CUuid Uuid2;
				g_UuidManager.UnpackUuid(pUnpacker, &Uuid2);
				if(pUnpacker->Error())
					break;
				char aBuf[UUID_MAXSTRSIZE];
				FormatUuid(Uuid2, aBuf, sizeof(aBuf));
				dbg_msg("uuid", "peer: unknown %s", aBuf);
			}
			break;
		case NETMSG_ITIS:
			if(g_Config.m_Debug)
			{
				CUuid Uuid2;
				g_UuidManager.UnpackUuid(pUnpacker, &Uuid2);
				const char *pName = pUnpacker->GetString(CUnpacker::SANITIZE_CC);
				if(pUnpacker->Error())
					break;
				char aBuf[UUID_MAXSTRSIZE];
				FormatUuid(Uuid2, aBuf, sizeof(aBuf));
				dbg_msg("uuid", "peer: %s %s", aBuf, pName);
			}
			break;
		}
	}
	return UNPACKMESSAGE_OK;
}
