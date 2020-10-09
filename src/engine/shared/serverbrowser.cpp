#include <base/system.h>
#include <engine/serverbrowser.h>

// gametypes

bool IsVanilla(const CServerInfo *pInfo)
{
	return !str_comp(pInfo->m_aGameType, "DM") || !str_comp(pInfo->m_aGameType, "TDM") || !str_comp(pInfo->m_aGameType, "CTF");
}

bool IsCatch(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "catch");
}

bool IsInsta(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "idm") || str_find_nocase(pInfo->m_aGameType, "itdm") || str_find_nocase(pInfo->m_aGameType, "ictf");
}

bool IsFNG(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fng");
}

bool IsRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "race") || IsFastCap(pInfo) || IsDDRace(pInfo);
}

bool IsFastCap(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsDDRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddrace") || str_find_nocase(pInfo->m_aGameType, "mkrace") || IsDDNet(pInfo);
}

bool IsBlockInfectionZ(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "blockz") ||
	       str_find_nocase(pInfo->m_aGameType, "infectionz");
}

bool IsBlockWorlds(const CServerInfo *pInfo)
{
	return (str_comp_nocase_num(pInfo->m_aGameType, "bw  ", 4) == 0) || (str_comp_nocase(pInfo->m_aGameType, "bw") == 0);
}

bool IsCity(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "city");
}

bool IsDDNet(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddracenet") || str_find_nocase(pInfo->m_aGameType, "ddnet") || IsBlockInfectionZ(pInfo);
}

// other

bool Is64Player(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "64") || str_find(pInfo->m_aName, "64") || IsDDNet(pInfo) || IsBlockInfectionZ(pInfo) || IsBlockWorlds(pInfo);
}

bool IsPlus(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "+");
}
