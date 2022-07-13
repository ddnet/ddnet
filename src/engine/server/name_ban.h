#ifndef ENGINE_SERVER_NAME_BAN_H
#define ENGINE_SERVER_NAME_BAN_H

#include <base/system.h>
#include <engine/shared/protocol.h>

#include <vector>

enum
{
	MAX_NAME_SKELETON_LENGTH = MAX_NAME_LENGTH * 4,
	MAX_NAMEBAN_REASON_LENGTH = 64
};

class CNameBan
{
public:
	CNameBan() {}
	CNameBan(const char *pName, int Distance, int IsSubstring, const char *pReason = "") :
		m_Distance(Distance), m_IsSubstring(IsSubstring)
	{
		str_copy(m_aName, pName);
		m_SkeletonLength = str_utf8_to_skeleton(m_aName, m_aSkeleton, std::size(m_aSkeleton));
		str_copy(m_aReason, pReason);
	}
	char m_aName[MAX_NAME_LENGTH] = {0};
	char m_aReason[MAX_NAMEBAN_REASON_LENGTH] = {0};
	int m_aSkeleton[MAX_NAME_SKELETON_LENGTH] = {0};
	int m_SkeletonLength = 0;
	int m_Distance = 0;
	int m_IsSubstring = 0;
};

CNameBan *IsNameBanned(const char *pName, std::vector<CNameBan> &vNameBans);

#endif // ENGINE_SERVER_NAME_BAN_H
