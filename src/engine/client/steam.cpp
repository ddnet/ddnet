#include <engine/steam.h>

#include <base/system.h>
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
		str_copy(m_aPlayerName, SteamAPI_ISteamFriends_GetPersonaName(m_pSteamFriends));
	}
	~CSteam() override
	{
		SteamAPI_Shutdown();
	}

	void ParseConnectString(const char *pConnect)
	{
		if(pConnect[0])
		{
			NETADDR Connect;
			if(net_host_lookup(pConnect, &Connect, NETTYPE_ALL) == 0)
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
		ParseConnectString(pEvent->m_aRGCHConnect);
	}

	const char *GetPlayerName() override
	{
		return m_aPlayerName;
	}

	const NETADDR *GetConnectAddress() override
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
	void ClearConnectAddress() override
	{
		m_GotConnectAddr = false;
	}

	void Update() override
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
	void ClearGameInfo() override
	{
		SteamAPI_ISteamFriends_ClearRichPresence(m_pSteamFriends);
	}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) override
	{
		if(AnnounceAddr)
		{
			char aServerAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&ServerAddr, aServerAddr, sizeof(aServerAddr), true);
			SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "connect", aServerAddr);
			SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "steam_player_group", aServerAddr);
		}

		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "map", pMapName);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "status", pMapName);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "steam_display", "#Status");
	}
};

class CSteamStub : public ISteam
{
	const char *GetPlayerName() override { return 0; }
	const NETADDR *GetConnectAddress() override { return 0; }
	void ClearConnectAddress() override {}
	void Update() override {}
	void ClearGameInfo() override {}
	void SetGameInfo(const NETADDR &ServerAddr, const char *pMapName, bool AnnounceAddr) override {}
};

ISteam *CreateSteam()
{
	if(!SteamAPI_Init())
	{
		return new CSteamStub();
	}
	return new CSteam();
}
