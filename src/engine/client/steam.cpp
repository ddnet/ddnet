#include <engine/steam.h>

#include <engine/shared/config.h>

#include <steam/steam_api_flat.h>

class CSteam : public ISteam
{
	HSteamPipe m_SteamPipe;
	ISteamApps *m_pSteamApps;
	ISteamFriends *m_pSteamFriends;
	char m_aPlayerName[16];
	bool m_GotConnectAddr;
	NETADDR m_ConnectAddr;

public:
	CSteam()
	{
		SteamAPI_ManualDispatch_Init();
		m_SteamPipe = SteamAPI_GetHSteamPipe();
		m_pSteamApps = SteamAPI_SteamApps_v008();
		m_pSteamFriends = SteamAPI_SteamFriends_v017();

		ReadLaunchCommandLine();
		str_copy(m_aPlayerName, SteamAPI_ISteamFriends_GetPersonaName(m_pSteamFriends), sizeof(m_aPlayerName));
	}
	~CSteam()
	{
		SteamAPI_Shutdown();
	}

	void ParseConnectString(const char *pConnect)
	{
		if(pConnect[0])
		{
			NETADDR Connect;
			if(net_addr_from_str(&Connect, pConnect) == 0)
			{
				m_ConnectAddr = Connect;
				m_GotConnectAddr = true;
			}
			else
			{
				dbg_msg("steam", "got unparsable connect string: '%s'", pConnect);
			}
		}
	}

	void ReadLaunchCommandLine()
	{
		char aConnect[NETADDR_MAXSTRSIZE];
		int ConnectSize = SteamAPI_ISteamApps_GetLaunchCommandLine(m_pSteamApps, aConnect, sizeof(aConnect));
		if(ConnectSize >= NETADDR_MAXSTRSIZE)
		{
			ConnectSize = NETADDR_MAXSTRSIZE - 1;
		}
		aConnect[ConnectSize] = 0;
		m_GotConnectAddr = false;
		ParseConnectString(aConnect);
	}

	void OnGameRichPresenceJoinRequested(GameRichPresenceJoinRequested_t *pEvent)
	{
		ParseConnectString(pEvent->m_rgchConnect);
	}

	const char *GetPlayerName()
	{
		return m_aPlayerName;
	}

	const NETADDR *GetConnectAddress()
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
	void ClearConnectAddress()
	{
		m_GotConnectAddr = false;
	}

	void Update()
	{
		SteamAPI_ManualDispatch_RunFrame(m_SteamPipe);
		CallbackMsg_t Callback;
		while(SteamAPI_ManualDispatch_GetNextCallback(m_SteamPipe, &Callback))
		{
			switch(Callback.m_iCallback)
			{
			case NewUrlLaunchParameters_t::k_iCallback:
				ReadLaunchCommandLine();
				break;
			case GameRichPresenceJoinRequested_t::k_iCallback:
				OnGameRichPresenceJoinRequested((GameRichPresenceJoinRequested_t *)Callback.m_pubParam);
				break;
			default:
				if(g_Config.m_Debug)
				{
					dbg_msg("steam/dbg", "unhandled callback id=%d", Callback.m_iCallback);
				}
			}
			SteamAPI_ManualDispatch_FreeLastCallback(m_SteamPipe);
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
	const NETADDR *GetConnectAddress() { return 0; }
	void ClearConnectAddress() {}
	void Update() {}
	void ClearGameInfo() {}
	void SetGameInfo(NETADDR ServerAddr, const char *pMapName) {}
};

ISteam *CreateSteam()
{
	if(!SteamAPI_Init())
	{
		return new CSteamStub();
	}
	return new CSteam();
}
