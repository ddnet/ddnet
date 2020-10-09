#ifndef ENGINE_SHARED_PROTOCOL_EX_H
#define ENGINE_SHARED_PROTOCOL_EX_H

#include <engine/message.h>

enum
{
	NETMSG_EX_INVALID = UUID_INVALID,
	NETMSG_EX_UNKNOWN = UUID_UNKNOWN,

	OFFSET_NETMSG_UUID = OFFSET_UUID,

	__NETMSG_UUID_HELPER = OFFSET_NETMSG_UUID - 1,
#define UUID(id, name) id,
#include "protocol_ex_msgs.h"
#undef UUID
	OFFSET_TEEHISTORIAN_UUID,

	UNPACKMESSAGE_ERROR = 0,
	UNPACKMESSAGE_OK,
	UNPACKMESSAGE_ANSWER,

	SERVERCAP_CURVERSION = 1,
	SERVERCAPFLAG_DDNET = 1 << 0,
	SERVERCAPFLAG_CHATTIMEOUTCODE = 1 << 1,
};

void RegisterUuids(class CUuidManager *pManager);
bool NetworkExDefaultHandler(int *pID, struct CUuid *pUuid, CUnpacker *pUnpacker, CMsgPacker *pPacker, int Type);

int UnpackMessageID(int *pID, bool *pSys, struct CUuid *pUuid, CUnpacker *pUnpacker, CMsgPacker *pPacker);

#endif // ENGINE_SHARED_PROTOCOL_EX_H
