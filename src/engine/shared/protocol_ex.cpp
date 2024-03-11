#include "protocol_ex.h"

#include "config.h"
#include "uuid_manager.h"

#include <base/system.h>
#include <engine/message.h>

#include <new>

void RegisterUuids(CUuidManager *pManager)
{
#define UUID(id, name) pManager->RegisterName(id, name);
#include "protocol_ex_msgs.h"
#undef UUID
}

int UnpackMessageId(int *pId, bool *pSys, CUuid *pUuid, CUnpacker *pUnpacker, CMsgPacker *pPacker)
{
	*pId = 0;
	*pSys = false;
	mem_zero(pUuid, sizeof(*pUuid));

	int MsgId = pUnpacker->GetInt();

	if(pUnpacker->Error())
	{
		return UNPACKMESSAGE_ERROR;
	}

	*pId = MsgId >> 1;
	*pSys = MsgId & 1;

	if(*pId < 0 || *pId >= OFFSET_UUID)
	{
		return UNPACKMESSAGE_ERROR;
	}

	if(*pId != 0) // NETMSG_EX, NETMSGTYPE_EX
	{
		return UNPACKMESSAGE_OK;
	}

	*pId = g_UuidManager.UnpackUuid(pUnpacker, pUuid);

	if(*pId == UUID_INVALID || *pId == UUID_UNKNOWN)
	{
		return UNPACKMESSAGE_ERROR;
	}

	if(*pSys)
	{
		switch(*pId)
		{
		case NETMSG_WHATIS:
		{
			CUuid Uuid2;
			int Id2 = g_UuidManager.UnpackUuid(pUnpacker, &Uuid2);
			if(Id2 == UUID_INVALID)
			{
				break;
			}
			if(Id2 == UUID_UNKNOWN)
			{
				new(pPacker) CMsgPacker(NETMSG_IDONTKNOW, true);
				pPacker->AddRaw(&Uuid2, sizeof(Uuid2));
			}
			else
			{
				new(pPacker) CMsgPacker(NETMSG_ITIS, true);
				pPacker->AddRaw(&Uuid2, sizeof(Uuid2));
				pPacker->AddString(g_UuidManager.GetName(Id2), 0);
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
