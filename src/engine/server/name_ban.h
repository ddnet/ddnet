#ifndef ENGINE_SERVER_NAME_BANS_H
#define ENGINE_SERVER_NAME_BANS_H

#include <base/system.h>
#include <engine/shared/protocol.h>

enum
{
	MAX_NAME_SKELETON_LENGTH=MAX_NAME_LENGTH*4,
	MAX_NAMEBAN_REASON_LENGTH=64
};

class CNameBan
{
public:
	CNameBan() {}
	CNameBan(const char *pName, int Distance, const char *pReason = "") :
		m_Distance(Distance)
	{
		str_copy(m_aName, pName, sizeof(m_aName));
		m_SkeletonLength = str_utf8_to_skeleton(m_aName, m_aSkeleton, sizeof(m_aSkeleton) / sizeof(m_aSkeleton[0]));
		str_copy(m_aReason, pReason, sizeof(m_aReason));
	}
	char m_aName[MAX_NAME_LENGTH];
	char m_aReason[MAX_NAMEBAN_REASON_LENGTH];
	int m_aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int m_SkeletonLength;
	int m_Distance;
};

CNameBan *IsNameBanned(const char *pName, CNameBan *pNameBans, int NumNameBans);

#endif // ENGINE_SERVER_NAME_BANS_H
