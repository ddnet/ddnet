#include <engine/serverbrowser.h>
#include <base/system.h>

bool IsRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "race")
	    || str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsDDRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddrace")
	    || str_find_nocase(pInfo->m_aGameType, "mkrace");
}

bool IsDDNet(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddracenet")
	    || str_find_nocase(pInfo->m_aGameType, "ddnet");
}

bool Is64Player(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "64")
	    || str_find(pInfo->m_aName, "64")
	    || IsDDNet(pInfo);
}

bool IsPlus(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "+");
}