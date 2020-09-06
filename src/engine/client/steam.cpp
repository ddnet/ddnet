#include <engine/steam.h>

#include <steam/steam_api_flat.h>

class CSteam : public ISteam
{
	ISteamApps *m_pSteamApps;
	ISteamFriends *m_pSteamFriends;
	char m_aPlayerName[16];
	bool m_GotConnectAddr;
	NETADDR m_ConnectAddr;

public:
	CSteam()
	{
		m_pSteamApps = SteamAPI_SteamApps_v008();
		m_pSteamFriends = SteamAPI_SteamFriends_v017();

		char aConnect[NETADDR_MAXSTRSIZE];
		int ConnectSize = SteamAPI_ISteamApps_GetLaunchCommandLine(m_pSteamApps, aConnect, sizeof(aConnect));
		if(ConnectSize >= NETADDR_MAXSTRSIZE)
		{
			ConnectSize = NETADDR_MAXSTRSIZE - 1;
		}
		aConnect[ConnectSize] = 0;
		m_GotConnectAddr = false;
		if(aConnect[0])
		{
			if(net_addr_from_str(&m_ConnectAddr, aConnect) == 0)
			{
				m_GotConnectAddr = true;
			}
			else
			{
				dbg_msg("steam", "got unparsable launch connect string: '%s'", aConnect);
			}
		}
		str_copy(m_aPlayerName, SteamAPI_ISteamFriends_GetPersonaName(m_pSteamFriends), sizeof(m_aPlayerName));
	}
	~CSteam()
	{
		SteamAPI_Shutdown();
	}

	const char *GetPlayerName()
	{
		return m_aPlayerName;
	}

	const NETADDR *GetLaunchConnectAddress()
	{
		if(m_GotConnectAddr)
		{
			return &m_ConnectAddr;
		}
		else
		{
			return 0;
		}
	}

	void ClearGameInfo()
	{
		SteamAPI_ISteamFriends_ClearRichPresence(m_pSteamFriends);
	}
	void SetGameInfo(NETADDR ServerAddr, const char *pMapName)
	{
		char aServerAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&ServerAddr, aServerAddr, sizeof(aServerAddr), true);

		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "connect", aServerAddr);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "map", pMapName);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "status", pMapName);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "steam_display", "#Status");
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "steam_player_group", aServerAddr);
	}
};

class CSteamStub : public ISteam
{
	const char *GetPlayerName() { return 0; }
	const NETADDR *GetLaunchConnectAddress() { return 0; }
	void ClearGameInfo() { }
	void SetGameInfo(NETADDR ServerAddr, const char *pMapName) { }
};

ISteam *CreateSteam()
{
	if(!SteamAPI_Init())
	{
		return new CSteamStub();
	}
	return new CSteam();
}
