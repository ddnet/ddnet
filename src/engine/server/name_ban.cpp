#include "name_ban.h"

#include <array> // std::size

CNameBan *IsNameBanned(const char *pName, CNameBan *pNameBans, int NumNameBans)
{
	char aTrimmed[MAX_NAME_LENGTH];
	str_copy(aTrimmed, str_utf8_skip_whitespaces(pName), sizeof(aTrimmed));
	str_utf8_trim_right(aTrimmed);

	int aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int SkeletonLength = str_utf8_to_skeleton(aTrimmed, aSkeleton, std::size(aSkeleton));
	int aBuffer[MAX_NAME_SKELETON_LENGTH * 2 + 2];

	CNameBan *pResult = 0;
	for(int i = 0; i < NumNameBans; i++)
	{
		CNameBan *pBan = &pNameBans[i];
		int Distance = str_utf32_dist_buffer(aSkeleton, SkeletonLength, pBan->m_aSkeleton, pBan->m_SkeletonLength, aBuffer, std::size(aBuffer));
		if(Distance <= pBan->m_Distance || (pBan->m_IsSubstring == 1 && str_utf8_find_nocase(pName, pBan->m_aName)))
		{
			pResult = pBan;
		}
	}
	return pResult;
}
