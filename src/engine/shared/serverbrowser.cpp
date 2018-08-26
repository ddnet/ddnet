#include <engine/serverbrowser.h>
#include <base/system.h>

// gametypes

bool IsVanilla(const CServerInfo *pInfo)
{
	return !str_comp(pInfo->m_aGameType, "DM")
	    || !str_comp(pInfo->m_aGameType, "TDM")
	    || !str_comp(pInfo->m_aGameType, "CTF");
}

bool IsCatch(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "catch");
}

bool IsInsta(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "idm")
	    || str_find_nocase(pInfo->m_aGameType, "itdm")
	    || str_find_nocase(pInfo->m_aGameType, "ictf");
}

bool IsFNG(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fng");
}

bool IsRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "race")
	    || str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsFastCap(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fastcap");
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

// other

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
