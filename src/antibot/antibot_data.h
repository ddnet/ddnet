#ifndef ANTIBOT_ANTIBOT_DATA_H
#define ANTIBOT_ANTIBOT_DATA_H

#include <base/vmath.h>

enum
{
	ANTIBOT_MAX_CLIENTS=64,
};

struct CAntibotMapData
{
	int m_Width;
	int m_Height;
	unsigned char *m_pTiles;
};

struct CAntibotInputData
{
	int m_TargetX;
	int m_TargetY;
};

// Defined by the network protocol, unlikely to change.
//enum
//{
	//TEAM_SPECTATORS=-1,
	//TEAM_RED=0,
	//TEAM_BLUE=1,
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

struct CAntibotData
{
	int m_Tick;
	CAntibotCharacterData m_aCharacters[ANTIBOT_MAX_CLIENTS];
	CAntibotMapData m_Map;
	void (*m_pfnLog)(const char *pMessage, void *pUser);
	void (*m_pfnReport)(int ClientID, const char *pMessage, void *pUser);
	void *m_pUser;
};

#endif // ANTIBOT_ANTIBOT_DATA_H
