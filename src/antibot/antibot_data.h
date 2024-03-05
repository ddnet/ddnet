#ifndef ANTIBOT_ANTIBOT_DATA_H
#define ANTIBOT_ANTIBOT_DATA_H

#include <base/vmath.h>

enum
{
	ANTIBOT_ABI_VERSION = 9,

	ANTIBOT_MSGFLAG_NONVITAL = 1,
	ANTIBOT_MSGFLAG_FLUSH = 2,

	ANTIBOT_MAX_CLIENTS = 64,
};

struct CAntibotMapData
{
	int m_Width;
	int m_Height;
	unsigned char *m_pTiles;
};

struct CAntibotPlayerData
{
	char m_aAddress[64];
};

struct CAntibotInputData
{
	int m_TargetX;
	int m_TargetY;
};

// Defined by the network protocol, unlikely to change.
//enum
//{
//	TEAM_SPECTATORS=-1,
//	TEAM_RED=0,
//	TEAM_BLUE=1,
//};

struct CAntibotCharacterData
{
	char m_aName[16];
	CAntibotInputData m_aLatestInputs[3];

	bool m_Alive;
	bool m_Pause;
	int m_Team;

	vec2 m_Pos;
	vec2 m_Vel;
	int m_Angle;
	int m_HookedPlayer;
	int m_SpawnTick;
	int m_WeaponChangeTick;
};

struct CAntibotVersion
{
	int m_AbiVersion;
	int m_Size;

	int m_SizeData;
	int m_SizePlayerData;
	int m_SizeCharacterData;
	int m_SizeInputData;
	int m_SizeMapData;
	int m_SizeRoundData;
};

#define ANTIBOT_VERSION \
	{ \
		ANTIBOT_ABI_VERSION, \
			sizeof(CAntibotVersion), \
			sizeof(CAntibotData), \
			sizeof(CAntibotPlayerData), \
			sizeof(CAntibotCharacterData), \
			sizeof(CAntibotInputData), \
			sizeof(CAntibotMapData), \
			sizeof(CAntibotRoundData), \
	}

struct CAntibotData
{
	CAntibotVersion m_Version;

	int64_t m_Now;
	int64_t m_Freq;
	void (*m_pfnKick)(int ClientId, const char *pMessage, void *pUser);
	void (*m_pfnLog)(const char *pMessage, void *pUser);
	void (*m_pfnReport)(int ClientId, const char *pMessage, void *pUser);
	void (*m_pfnSend)(int ClientId, const void *pData, int DataSize, int Flags, void *pUser);
	void (*m_pfnTeehistorian)(const void *pData, int DataSize, void *pUser);
	void *m_pUser;
};
struct CAntibotRoundData
{
	int m_Tick;
	CAntibotPlayerData m_aPlayers[ANTIBOT_MAX_CLIENTS];
	CAntibotCharacterData m_aCharacters[ANTIBOT_MAX_CLIENTS];
	CAntibotMapData m_Map;
};

#endif // ANTIBOT_ANTIBOT_DATA_H
