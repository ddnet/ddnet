#ifndef STEAM_STEAM_API_FLAT_H
#define STEAM_STEAM_API_FLAT_H

#include <base/dynamic.h>

#include <cstdint>

#ifndef STEAMAPI
#define STEAMAPI DYNAMIC_IMPORT
#endif

extern "C" {

typedef uint64_t CSteamId;
typedef int32_t HSteamPipe;
typedef int32_t HSteamUser;

struct CallbackMsg_t
{
	HSteamUser m_hSteamUser;
	int m_iCallback;
	unsigned char *m_pubParam;
	int m_cubParam;
};

struct GameRichPresenceJoinRequested_t
{
	enum
	{
		k_iCallback = 337
	};
	CSteamId m_steamIdFriend;
	char m_aRGCHConnect[256];
};

struct NewUrlLaunchParameters_t
{
	enum
	{
		k_iCallback = 1014
	};
	unsigned char m_EmptyStructDontUse;
};

struct ISteamApps;
struct ISteamFriends;

STEAMAPI bool SteamAPI_Init(); // Returns true on success.
STEAMAPI HSteamPipe SteamAPI_GetHSteamPipe();
STEAMAPI void SteamAPI_Shutdown();

STEAMAPI void SteamAPI_ManualDispatch_Init();
STEAMAPI void SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe SteamPipe);
STEAMAPI bool SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe SteamPipe, CallbackMsg_t *pCallbackMsg);
STEAMAPI void SteamAPI_ManualDispatch_RunFrame(HSteamPipe SteamPipe);

STEAMAPI ISteamApps *SteamAPI_SteamApps_v008();
STEAMAPI int SteamAPI_ISteamApps_GetLaunchCommandLine(ISteamApps *pSelf, char *pBuffer, int BufferSize);
STEAMAPI const char *SteamAPI_ISteamApps_GetLaunchQueryParam(ISteamApps *pSelf, const char *pKey);

STEAMAPI ISteamFriends *SteamAPI_SteamFriends_v017();
STEAMAPI void SteamAPI_ISteamFriends_ClearRichPresence(ISteamFriends *pSelf);
STEAMAPI const char *SteamAPI_ISteamFriends_GetPersonaName(ISteamFriends *pSelf);
STEAMAPI bool SteamAPI_ISteamFriends_SetRichPresence(ISteamFriends *pSelf, const char *pKey, const char *pValue);
}

#endif // STEAM_STEAM_API_FLAT_H
