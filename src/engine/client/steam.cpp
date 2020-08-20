#include <engine/steam.h>

#include <steam/steam_api_flat.h>

class CSteam : public ISteam
{
	char aPlayerName[16];

public:
	CSteam()
	{
		ISteamFriends *pSteamFriends = SteamAPI_SteamFriends_v017();
		str_copy(aPlayerName, SteamAPI_ISteamFriends_GetPersonaName(pSteamFriends), sizeof(aPlayerName));
	}

	~CSteam()
	{
		SteamAPI_Shutdown();
	}


	const char *GetPlayerName()
	{
		return aPlayerName;
	}
};

class CSteamStub : public ISteam
{
	const char *GetPlayerName()
	{
		return 0;
	}
};

ISteam *CreateSteam()
{
	if(!SteamAPI_Init())
	{
		return new CSteamStub();
	}
	return new CSteam();
}
