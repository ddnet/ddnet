#include "name_ban.h"

CNameBan *IsNameBanned(const char *pName, CNameBan *pNameBans, int NumNameBans)
{
	char aTrimmed[MAX_NAME_LENGTH];
	str_copy(aTrimmed, str_utf8_skip_whitespaces(pName), sizeof(aTrimmed));
	str_utf8_trim_right(aTrimmed);

	int aSkeleton[MAX_NAME_SKELETON_LENGTH];
	int SkeletonLength = str_utf8_to_skeleton(aTrimmed, aSkeleton, sizeof(aSkeleton) / sizeof(aSkeleton[0]));
	int aBuffer[MAX_NAME_SKELETON_LENGTH * 2 + 2];

	CNameBan *pResult = 0;
	for(int i = 0; i < NumNameBans; i++)
	{
		CNameBan *pBan = &pNameBans[i];
		int Distance = str_utf32_dist_buffer(aSkeleton, SkeletonLength, pBan->m_aSkeleton, pBan->m_SkeletonLength, aBuffer, sizeof(aBuffer) / sizeof(aBuffer[0]));
		if(Distance <= pBan->m_Distance)
		{
			pResult = pBan;
		}
	}
	return pResult;
}
