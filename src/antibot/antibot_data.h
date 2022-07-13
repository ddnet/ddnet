#ifndef ANTIBOT_ANTIBOT_DATA_H
#define ANTIBOT_ANTIBOT_DATA_H

#include <base/system.h>
#include <base/vmath.h>

enum
{
	ANTIBOT_ABI_VERSION = 5,

	ANTIBOT_MSGFLAG_NONVITAL = 1,
	ANTIBOT_MSGFLAG_FLUSH = 2,

	ANTIBOT_MAX_CLIENTS = 64,
};

struct CAntibotMapData
{
	int m_Width = 0;
	int m_Height = 0;
	unsigned char *m_pTiles = nullptr;
};

struct CAntibotInputData
{
	int m_TargetX = 0;
	int m_TargetY = 0;
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
	char m_aName[16] = {0};
	CAntibotInputData m_aLatestInputs[3];

	bool m_Alive = false;
	bool m_Pause = false;
	int m_Team = 0;

	vec2 m_Pos;
	vec2 m_Vel;
	int m_Angle = 0;
	int m_HookedPlayer = 0;
	int m_SpawnTick = 0;
	int m_WeaponChangeTick = 0;
};

struct CAntibotVersion
{
	int m_AbiVersion = 0;
	int m_Size = 0;

	int m_SizeData = 0;
	int m_SizeCharacterData = 0;
	int m_SizeInputData = 0;
	int m_SizeMapData = 0;
	int m_SizeRoundData = 0;
};

#define ANTIBOT_VERSION \
	{ \
		ANTIBOT_ABI_VERSION, \
			sizeof(CAntibotVersion), \
			sizeof(CAntibotData), \
			sizeof(CAntibotCharacterData), \
			sizeof(CAntibotInputData), \
			sizeof(CAntibotMapData), \
			sizeof(CAntibotRoundData), \
	}

struct CAntibotData
{
	CAntibotVersion m_Version;

	int64_t m_Now = 0;
	int64_t m_Freq = 0;
	void (*m_pfnLog)(const char *pMessage, void *pUser);
	void (*m_pfnReport)(int ClientID, const char *pMessage, void *pUser);
	void (*m_pfnSend)(int ClientID, const void *pData, int DataSize, int Flags, void *pUser);
	void *m_pUser = nullptr;
};
struct CAntibotRoundData
{
	int m_Tick = 0;
	CAntibotCharacterData m_aCharacters[ANTIBOT_MAX_CLIENTS];
	CAntibotMapData m_Map;
};

#endif // ANTIBOT_ANTIBOT_DATA_H
