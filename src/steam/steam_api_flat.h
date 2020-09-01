#ifndef STEAMAPI_STEAM_STEAM_API_FLAT_H
#define STEAMAPI_STEAM_STEAM_API_FLAT_H

#include <base/dynamic.h>

#ifndef STEAMAPI
#define STEAMAPI DYNAMIC_IMPORT
#endif

extern "C"
{

struct ISteamApps;
struct ISteamFriends;

STEAMAPI bool SteamAPI_Init(); // Returns true on success.
STEAMAPI void SteamAPI_Shutdown();

STEAMAPI ISteamApps *SteamAPI_SteamApps_v008();
STEAMAPI int SteamAPI_ISteamApps_GetLaunchCommandLine(ISteamApps *pSelf, char *pBuffer, int BufferSize);
STEAMAPI const char *SteamAPI_ISteamApps_GetLaunchQueryParam(ISteamApps *pSelf, const char *pKey);

STEAMAPI ISteamFriends *SteamAPI_SteamFriends_v017();
STEAMAPI void SteamAPI_ISteamFriends_ClearRichPresence(ISteamFriends *pSelf);
STEAMAPI const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf);
STEAMAPI bool SteamAPI_ISteamFriends_SetRichPresence(ISteamFriends *pSelf, const char *pKey, const char *pValue);

}

#endif // STEAMAPI_STEAM_API_FLAT_H
