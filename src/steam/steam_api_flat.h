#ifndef STEAMAPI_STEAM_STEAM_API_FLAT_H
#define STEAMAPI_STEAM_STEAM_API_FLAT_H

#include <base/dynamic.h>

#ifndef STEAMAPI
#define STEAMAPI DYNAMIC_IMPORT
#endif

extern "C"
{

struct ISteamFriends;

STEAMAPI bool SteamAPI_Init(); // Returns true on success.
STEAMAPI void SteamAPI_Shutdown();
STEAMAPI ISteamFriends *SteamAPI_SteamFriends_v017();
STEAMAPI const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf);

}

#endif // STEAMAPI_STEAM_API_FLAT_H
