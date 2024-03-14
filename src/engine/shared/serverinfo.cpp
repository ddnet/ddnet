#include "serverinfo.h"

#include "json.h"
#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>

#include <cstdio>

static bool IsAllowedHex(char c)
{
	static const char ALLOWED[] = "0123456789abcdefABCDEF";
	for(int i = 0; i < (int)sizeof(ALLOWED) - 1; i++)
	{
		if(c == ALLOWED[i])
		{
			return true;
		}
	}
	return false;
}

bool ParseCrc(unsigned int *pResult, const char *pString)
{
	if(str_length(pString) != 8)
	{
		return true;
	}
	for(int i = 0; i < 8; i++)
	{
		if(!IsAllowedHex(pString[i]))
		{
			return true;
		}
	}
	return sscanf(pString, "%08x", pResult) != 1;
}

bool CServerInfo2::FromJson(CServerInfo2 *pOut, const json_value *pJson)
{
	bool Result = FromJsonRaw(pOut, pJson);
	if(Result)
	{
		return Result;
	}
	return pOut->Validate();
}

bool CServerInfo2::Validate() const
{
	bool Error = false;
	Error = Error || m_MaxClients < m_MaxPlayers;
	Error = Error || m_NumClients < m_NumPlayers;
	Error = Error || m_MaxClients < m_NumClients;
	Error = Error || m_MaxPlayers < m_NumPlayers;
	return Error;
}

bool CServerInfo2::FromJsonRaw(CServerInfo2 *pOut, const json_value *pJson)
{
	mem_zero(pOut, sizeof(*pOut));
	bool Error;

	const json_value &ServerInfo = *pJson;
	const json_value &MaxClients = ServerInfo["max_clients"];
	const json_value &MaxPlayers = ServerInfo["max_players"];
	const json_value &ClientScoreKind = ServerInfo["client_score_kind"];
	const json_value &Passworded = ServerInfo["passworded"];
	const json_value &GameType = ServerInfo["game_type"];
	const json_value &Name = ServerInfo["name"];
	const json_value &MapName = ServerInfo["map"]["name"];
	const json_value &Version = ServerInfo["version"];
	const json_value &Clients = ServerInfo["clients"];
	const json_value &RequiresLogin = ServerInfo["requires_login"];

	Error = false;
	Error = Error || MaxClients.type != json_integer;
	Error = Error || MaxPlayers.type != json_integer;
	Error = Error || Passworded.type != json_boolean;
	Error = Error || (ClientScoreKind.type != json_none && ClientScoreKind.type != json_string);
	Error = Error || GameType.type != json_string || str_has_cc(GameType);
	Error = Error || Name.type != json_string || str_has_cc(Name);
	Error = Error || MapName.type != json_string || str_has_cc(MapName);
	Error = Error || Version.type != json_string || str_has_cc(Version);
	Error = Error || Clients.type != json_array;
	if(Error)
	{
		return true;
	}
	pOut->m_MaxClients = json_int_get(&MaxClients);
	pOut->m_MaxPlayers = json_int_get(&MaxPlayers);
	pOut->m_ClientScoreKind = CServerInfo::CLIENT_SCORE_KIND_UNSPECIFIED;
	if(ClientScoreKind.type == json_string && str_startswith(ClientScoreKind, "points"))
	{
		pOut->m_ClientScoreKind = CServerInfo::CLIENT_SCORE_KIND_POINTS;
	}
	else if(ClientScoreKind.type == json_string && str_startswith(ClientScoreKind, "time"))
	{
		pOut->m_ClientScoreKind = CServerInfo::CLIENT_SCORE_KIND_TIME;
	}
	pOut->m_RequiresLogin = false;
	if(RequiresLogin.type == json_boolean)
	{
		pOut->m_RequiresLogin = RequiresLogin;
	}
	pOut->m_Passworded = Passworded;
	str_copy(pOut->m_aGameType, GameType);
	str_copy(pOut->m_aName, Name);
	str_copy(pOut->m_aMapName, MapName);
	str_copy(pOut->m_aVersion, Version);

	pOut->m_NumClients = 0;
	pOut->m_NumPlayers = 0;
	for(unsigned i = 0; i < Clients.u.array.length; i++)
	{
		const json_value &Client = Clients[i];
		const json_value &ClientName = Client["name"];
		const json_value &Clan = Client["clan"];
		const json_value &Country = Client["country"];
		const json_value &Score = Client["score"];
		const json_value &IsPlayer = Client["is_player"];
		const json_value &IsAfk = Client["afk"];
		Error = false;
		Error = Error || ClientName.type != json_string || str_has_cc(ClientName);
		Error = Error || Clan.type != json_string || str_has_cc(ClientName);
		Error = Error || Country.type != json_integer;
		Error = Error || Score.type != json_integer;
		Error = Error || IsPlayer.type != json_boolean;
		if(Error)
		{
			return true;
		}
		if(i < SERVERINFO_MAX_CLIENTS)
		{
			CClient *pClient = &pOut->m_aClients[i];
			str_copy(pClient->m_aName, ClientName);
			str_copy(pClient->m_aClan, Clan);
			pClient->m_Country = json_int_get(&Country);
			pClient->m_Score = json_int_get(&Score);
			pClient->m_IsPlayer = IsPlayer;

			pClient->m_IsAfk = false;
			if(IsAfk.type == json_boolean)
				pClient->m_IsAfk = IsAfk;

			// check if a skin is also available
			bool HasSkin = false;
			const json_value &SkinObj = Client["skin"];
			if(SkinObj.type == json_object)
			{
				const json_value &SkinName = SkinObj["name"];
				const json_value &SkinBodyColor = SkinObj["color_body"];
				const json_value &SkinFeetColor = SkinObj["color_feet"];
				if(SkinName.type == json_string)
				{
					HasSkin = true;
					str_copy(pClient->m_aSkin, SkinName.u.string.ptr);
					// if skin json value existed, then always at least default to "default"
					if(pClient->m_aSkin[0] == '\0')
						str_copy(pClient->m_aSkin, "default");
					// if skin also has custom colors, add them
					if(SkinBodyColor.type == json_integer && SkinFeetColor.type == json_integer)
					{
						pClient->m_CustomSkinColors = true;
						pClient->m_CustomSkinColorBody = SkinBodyColor.u.integer;
						pClient->m_CustomSkinColorFeet = SkinFeetColor.u.integer;
					}
					// else set custom colors off
					else
					{
						pClient->m_CustomSkinColors = false;
					}
				}
			}

			// else make it null terminated
			if(!HasSkin)
			{
				pClient->m_aSkin[0] = '\0';
			}
		}

		pOut->m_NumClients++;
		if((bool)IsPlayer)
		{
			pOut->m_NumPlayers++;
		}
	}
	return false;
}

bool CServerInfo2::operator==(const CServerInfo2 &Other) const
{
	bool Unequal;
	Unequal = false;
	Unequal = Unequal || m_MaxClients != Other.m_MaxClients;
	Unequal = Unequal || m_NumClients != Other.m_NumClients;
	Unequal = Unequal || m_MaxPlayers != Other.m_MaxPlayers;
	Unequal = Unequal || m_NumPlayers != Other.m_NumPlayers;
	Unequal = Unequal || m_ClientScoreKind != Other.m_ClientScoreKind;
	Unequal = Unequal || m_Passworded != Other.m_Passworded;
	Unequal = Unequal || str_comp(m_aGameType, Other.m_aGameType) != 0;
	Unequal = Unequal || str_comp(m_aName, Other.m_aName) != 0;
	Unequal = Unequal || str_comp(m_aMapName, Other.m_aMapName) != 0;
	Unequal = Unequal || str_comp(m_aVersion, Other.m_aVersion) != 0;
	Unequal = Unequal || m_RequiresLogin != Other.m_RequiresLogin;
	if(Unequal)
	{
		return false;
	}
	for(int i = 0; i < m_NumClients; i++)
	{
		Unequal = false;
		Unequal = Unequal || str_comp(m_aClients[i].m_aName, Other.m_aClients[i].m_aName) != 0;
		Unequal = Unequal || str_comp(m_aClients[i].m_aClan, Other.m_aClients[i].m_aClan) != 0;
		Unequal = Unequal || m_aClients[i].m_Country != Other.m_aClients[i].m_Country;
		Unequal = Unequal || m_aClients[i].m_Score != Other.m_aClients[i].m_Score;
		Unequal = Unequal || m_aClients[i].m_IsPlayer != Other.m_aClients[i].m_IsPlayer;
		Unequal = Unequal || m_aClients[i].m_IsAfk != Other.m_aClients[i].m_IsAfk;
		if(Unequal)
		{
			return false;
		}
	}
	return true;
}

CServerInfo2::operator CServerInfo() const
{
	CServerInfo Result = {0};
	Result.m_MaxClients = m_MaxClients;
	Result.m_NumClients = m_NumClients;
	Result.m_MaxPlayers = m_MaxPlayers;
	Result.m_NumPlayers = m_NumPlayers;
	Result.m_ClientScoreKind = m_ClientScoreKind;
	Result.m_RequiresLogin = m_RequiresLogin;
	Result.m_Flags = m_Passworded ? SERVER_FLAG_PASSWORD : 0;
	str_copy(Result.m_aGameType, m_aGameType);
	str_copy(Result.m_aName, m_aName);
	str_copy(Result.m_aMap, m_aMapName);
	str_copy(Result.m_aVersion, m_aVersion);

	for(int i = 0; i < minimum(m_NumClients, (int)SERVERINFO_MAX_CLIENTS); i++)
	{
		str_copy(Result.m_aClients[i].m_aName, m_aClients[i].m_aName);
		str_copy(Result.m_aClients[i].m_aClan, m_aClients[i].m_aClan);
		Result.m_aClients[i].m_Country = m_aClients[i].m_Country;
		Result.m_aClients[i].m_Score = m_aClients[i].m_Score;
		Result.m_aClients[i].m_Player = m_aClients[i].m_IsPlayer;
		Result.m_aClients[i].m_Afk = m_aClients[i].m_IsAfk;

		str_copy(Result.m_aClients[i].m_aSkin, m_aClients[i].m_aSkin);
		Result.m_aClients[i].m_CustomSkinColors = m_aClients[i].m_CustomSkinColors;
		Result.m_aClients[i].m_CustomSkinColorBody = m_aClients[i].m_CustomSkinColorBody;
		Result.m_aClients[i].m_CustomSkinColorFeet = m_aClients[i].m_CustomSkinColorFeet;
	}

	Result.m_NumReceivedClients = minimum(m_NumClients, (int)SERVERINFO_MAX_CLIENTS);
	Result.m_Latency = -1;

	return Result;
}
