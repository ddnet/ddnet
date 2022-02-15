#define STEAMAPI DYNAMIC_EXPORT

#include <steam/steam_api_flat.h>

#include <cstdlib>

extern "C" {

bool SteamAPI_Init() { return false; }
HSteamPipe SteamAPI_GetHSteamPipe() { abort(); }
void SteamAPI_Shutdown() { abort(); }
void SteamAPI_ManualDispatch_Init() { abort(); }
void SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe SteamPipe) { abort(); }
bool SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe SteamPipe, CallbackMsg_t *pCallbackMsg) { abort(); }
void SteamAPI_ManualDispatch_RunFrame(HSteamPipe SteamPipe) { abort(); }
ISteamApps *SteamAPI_SteamApps_v008() { abort(); }
int SteamAPI_ISteamApps_GetLaunchCommandLine(ISteamApps *pSelf, char *pBuffer, int BufferSize) { abort(); }
const char *SteamAPI_ISteamApps_GetLaunchQueryParam(ISteamApps *pSelf, const char *pKey) { abort(); }
ISteamFriends *SteamAPI_SteamFriends_v017() { abort(); }
const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf) { abort(); }
bool SteamAPI_ISteamFriends_SetRichPresence(ISteamFriends *pSelf, const char *pKey, const char *pValue) { abort(); }
void SteamAPI_ISteamFriends_ClearRichPresence(ISteamFriends *pSelf) { abort(); }
}
