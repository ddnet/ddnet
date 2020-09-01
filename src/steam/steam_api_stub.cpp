#define STEAMAPI DYNAMIC_EXPORT

#include <steam/steam_api_flat.h>

#include <stdlib.h>

extern "C"
{

bool SteamAPI_Init() { return false; }
void SteamAPI_Shutdown() { abort(); }
ISteamApps *SteamAPI_SteamApps_v008() { abort(); }
int SteamAPI_ISteamApps_GetLaunchCommandLine(ISteamApps *pSelf, char *pBuffer, int BufferSize) { abort(); }
const char *SteamAPI_ISteamApps_GetLaunchQueryParam(ISteamApps *pSelf, const char *pKey) { abort(); }
ISteamFriends *SteamAPI_SteamFriends_v017() { abort(); }
const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf) { abort(); }
bool SteamAPI_ISteamFriends_SetRichPresence(ISteamFriends *pSelf, const char *pKey, const char *pValue) { abort(); }
void SteamAPI_ISteamFriends_ClearRichPresence(ISteamFriends *pSelf) { abort(); }

}
