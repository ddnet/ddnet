#include <engine/steam.h>

#include <steam/steam_api_flat.h>

class CSteam : public ISteam
{
	ISteamApps *m_pSteamApps;
	ISteamFriends *m_pSteamFriends;
	char m_aPlayerName[16];

public:
	CSteam()
	{
		m_pSteamApps = SteamAPI_SteamApps_v008();
		m_pSteamFriends = SteamAPI_SteamFriends_v017();

		char aCmdLine[128];
		int CmdLineSize = SteamAPI_ISteamApps_GetLaunchCommandLine(m_pSteamApps, aCmdLine, sizeof(aCmdLine));
		if(CmdLineSize >= 128)
		{
			CmdLineSize = 127;
		}
		aCmdLine[CmdLineSize] = 0;
		dbg_msg("steam", "cmdline='%s' param_connect='%s'", aCmdLine, SteamAPI_ISteamApps_GetLaunchQueryParam(m_pSteamApps, "connect"));
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

	void ClearGameInfo()
	{
		SteamAPI_ISteamFriends_ClearRichPresence(m_pSteamFriends);
	}
	void SetGameInfo(NETADDR ServerAddr, const char *pMapName)
	{
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "status", pMapName);
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "steam_display", "#Status");
		SteamAPI_ISteamFriends_SetRichPresence(m_pSteamFriends, "map", pMapName);
	}
};

class CSteamStub : public ISteam
{
	const char *GetPlayerName() { return 0; }
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
