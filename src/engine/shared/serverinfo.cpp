#include <engine/serverbrowser.h>
#include <base/system.h>

bool CServerInfo::CClientInfo::FromJson(json_value j)
{
	if(j["name"].type != json_string)
		return false;
	str_copy(m_aName, j["name"], sizeof(m_aName));

	if(j["clan"].type != json_string)
		return false;
	str_copy(m_aClan, j["clan"], sizeof(m_aClan));

	if(j["country"].type != json_integer)
		return false;
	m_Country = j["country"];

	if(j["score"].type != json_integer)
		return false;
	m_Score = j["score"];

	if(j["team"].type != json_integer)
		return false;
	m_Player = (int)j["team"] != -1;

	return true;
}

bool CServerInfo::CMapInfo::FromJson(json_value j)
{
	if(j["name"].type != json_string)
		return false;
	str_copy(m_aName, j["name"], sizeof(m_aName));

	if(j["crc"].type != json_string || !str_isallhex(j["crc"]) || str_length(j["crc"]) > 8)
		return false;
	m_Crc = str_toint_base(j["crc"], 16);

	if(j["sha256"].type != json_string || !sha256_from_str(&m_Sha256, j["sha256"]))
	{;} // Don't enforce sha256 yet

	if(j["size"].type != json_integer)
		return false;
	m_Size = j["size"];

	return true;
}

bool CServerInfo::FromJson(NETADDR Addr, json_value j)
{
	m_NetAddr = Addr;

	if(j["name"].type != json_string)
		return false;
	str_copy(m_aName, j["name"], sizeof(m_aName));

	if(j["game_type"].type != json_string)
		return false;
	str_copy(m_aGameType, j["game_type"], sizeof(m_aGameType));

	if(j["version"].type != json_string)
		return false;
	str_copy(m_aVersion, j["version"], sizeof(m_aVersion));

	if(j["passworded"].type != json_boolean)
		return false;
	m_Flags = j["passworded"] ? SERVER_FLAG_PASSWORD : 0;

	if(j["max_players"].type != json_integer || (int)j["max_players"] > MAX_CLIENTS)
		return false;
	m_MaxPlayers = j["max_players"];

	if(j["max_clients"].type != json_integer || (int)j["max_clients"] > MAX_CLIENTS)
		return false;
	m_MaxClients = j["max_clients"];

	if(j["map"].type != json_object || !m_MapInfo.FromJson(j["map"]))
		return false;

	if(j["clients"].type != json_array)
		return false;

	m_NumClients = json_array_length(&j["clients"]);
	m_NumPlayers = 0;

	for(int i = 0; i < m_NumClients; i++){
		if(!m_aClients[i].FromJson(j["clients"][i]))
			return false;

		m_NumPlayers += m_aClients[i].m_Player;
	}

	return true;
}

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
	    || IsFastCap(pInfo)
	    || IsDDRace(pInfo);
}

bool IsFastCap(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "fastcap");
}

bool IsDDRace(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "ddrace")
	    || str_find_nocase(pInfo->m_aGameType, "mkrace")
	    || IsDDNet(pInfo);
}

bool IsBlockInfectionZ(const CServerInfo *pInfo)
{
	return str_find_nocase(pInfo->m_aGameType, "blockZ")
	    || str_find_nocase(pInfo->m_aGameType, "infectionZ");
}

bool IsBlockWorlds(const CServerInfo *pInfo)
{
	return (str_comp_nocase_num(pInfo->m_aGameType, "bw  ", 4) == 0)
	    || (str_comp_nocase(pInfo->m_aGameType, "bw") == 0);
}

bool IsDDNet(const CServerInfo *pInfo)
{
	return (str_find_nocase(pInfo->m_aGameType, "ddracenet")
	    || str_find_nocase(pInfo->m_aGameType, "ddnet"))
	    && !IsBlockInfectionZ(pInfo);
}

// other

bool Is64Player(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "64")
	    || str_find(pInfo->m_aName, "64")
	    || IsDDNet(pInfo)
	    || IsBlockInfectionZ(pInfo)
	    || IsBlockWorlds(pInfo);
}

bool IsPlus(const CServerInfo *pInfo)
{
	return str_find(pInfo->m_aGameType, "+");
}
