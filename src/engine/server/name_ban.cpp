#include "name_ban.h"

CNameBan *IsNameBanned(const char *pName, std::vector<CNameBan> &vNameBans)
{
	char aTrimmed[MAX_NAME_LENGTH];
	str_copy(aTrimmed, str_utf8_skip_whitespaces(pName));
	str_utf8_trim_right(aTrimmed);

	int aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int SkeletonLength = str_utf8_to_skeleton(aTrimmed, aSkeleton, std::size(aSkeleton));
	int aBuffer[MAX_NAME_SKELETON_LENGTH * 2 + 2];

	CNameBan *pResult = nullptr;
	for(CNameBan &Ban : vNameBans)
	{
		int Distance = str_utf32_dist_buffer(aSkeleton, SkeletonLength, Ban.m_aSkeleton, Ban.m_SkeletonLength, aBuffer, std::size(aBuffer));
		if(Distance <= Ban.m_Distance || (Ban.m_IsSubstring == 1 && str_utf8_find_nocase(pName, Ban.m_aName)))
			pResult = &Ban;
	}
	return pResult;
}
