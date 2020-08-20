#define STEAMAPI DYNAMIC_EXPORT

#include <steam/steam_api_flat.h>

#include <stdlib.h>

extern "C"
{

bool SteamAPI_Init() { return false; }
void SteamAPI_Shutdown() { abort(); }
ISteamFriends *SteamAPI_SteamFriends_v017() { abort(); }
const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf) { abort(); }

}
