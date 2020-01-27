/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <engine/shared/packer.h>
#include <engine/shared/uuid_manager.h>

class CMsgPacker : public CPacker
{
public:
	CMsgPacker(int Type, bool System=false)
	{
		Reset();
		if(Type < OFFSET_UUID)
		{
			AddInt((Type<<1)|(System?1:0));
		}
		else
		{
			AddInt((0<<1)|(System?1:0)); // NETMSG_EX, NETMSGTYPE_EX
			g_UuidManager.PackUuid(Type, this);
		}
	}
};

#endif
