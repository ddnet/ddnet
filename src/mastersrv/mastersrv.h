/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef MASTERSRV_MASTERSRV_H
#define MASTERSRV_MASTERSRV_H
extern const int MASTERSERVER_PORT;

enum ServerType
{
	SERVERTYPE_INVALID = -1,
	SERVERTYPE_NORMAL,
	SERVERTYPE_LEGACY
};

struct CMastersrvAddr
{
	unsigned char m_aIp[16];
	unsigned char m_aPort[2];
};

#define SERVERBROWSE_SIZE 8
extern const unsigned char SERVERBROWSE_HEARTBEAT[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETLIST[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_LIST[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETCOUNT[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_COUNT[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETINFO[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETINFO_64_LEGACY[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO_64_LEGACY[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_INFO_EXTENDED[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO_EXTENDED_MORE[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_FWCHECK[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_FWRESPONSE[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_FWOK[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_FWERROR[SERVERBROWSE_SIZE];

// packet headers for the 0.5 branch

struct CMastersrvAddrLegacy
{
	unsigned char m_aIp[4];
	unsigned char m_aPort[2];
};

extern const unsigned char SERVERBROWSE_HEARTBEAT_LEGACY[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETLIST_LEGACY[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_LIST_LEGACY[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETCOUNT_LEGACY[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_COUNT_LEGACY[SERVERBROWSE_SIZE];

enum
{
	SERVERINFO_VANILLA = 0,
	SERVERINFO_64_LEGACY,
	SERVERINFO_EXTENDED,
	SERVERINFO_EXTENDED_MORE,
	SERVERINFO_INGAME,
};
#endif
