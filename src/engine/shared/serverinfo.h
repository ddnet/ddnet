#ifndef ENGINE_SHARED_SERVERINFO_H
#define ENGINE_SHARED_SERVERINFO_H

#include <engine/shared/protocol.h>

#include <engine/external/json/json.h>
#include <base/hash.h>

class CServerInfo
{
public:
    class CClientInfo
	{
	public:
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Country;
		int m_Score;
		bool m_Player;

		int m_FriendState;

        bool FromJson(json_value j);
		json_value *ToJson();
	};
	CClientInfo m_aClients[MAX_CLIENTS];

    class CMapInfo
    {
    public:
        char m_aName[32];
        int m_Crc;
        int m_Size;
        SHA256_DIGEST m_Sha256;

        bool FromJson(json_value j);
		json_value *ToJson();
    };
    CMapInfo m_MapInfo;

	NETADDR m_NetAddr;

	int m_MaxClients;
	int m_NumClients;
	int m_MaxPlayers;
	int m_NumPlayers;
	int m_Flags;
	char m_aGameType[16];
	char m_aName[64];
	char m_aVersion[32];

    bool FromJson(NETADDR Addr, json_value j);
	json_value *ToJson();
};

bool IsVanilla(const CServerInfo *pInfo);
bool IsCatch(const CServerInfo *pInfo);
bool IsInsta(const CServerInfo *pInfo);
bool IsFNG(const CServerInfo *pInfo);
bool IsRace(const CServerInfo *pInfo);
bool IsFastCap(const CServerInfo *pInfo);
bool IsDDRace(const CServerInfo *pInfo);
bool IsDDNet(const CServerInfo *pInfo);
bool IsBlockWorlds(const CServerInfo *pInfo);

bool Is64Player(const CServerInfo *pInfo);
bool IsPlus(const CServerInfo *pInfo);

#endif
