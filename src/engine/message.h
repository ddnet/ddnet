/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <engine/shared/packer.h>
#include <engine/shared/uuid_manager.h>

class CMsgPacker : public CPacker
{
public:
	int m_MsgID;
	bool m_System;
	bool m_NoTranslate;
	CMsgPacker(int Type, bool System = false, bool NoTranslate = false) :
		m_MsgID(Type), m_System(System), m_NoTranslate(NoTranslate)
	{
		Reset();
	}
};

#endif
