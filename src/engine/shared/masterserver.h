/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_MASTERSERVER_H
#define ENGINE_SHARED_MASTERSERVER_H

#define SERVERBROWSE_SIZE 8
extern const unsigned char SERVERBROWSE_GETINFO[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_GETINFO_64_LEGACY[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO_64_LEGACY[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_INFO_EXTENDED[SERVERBROWSE_SIZE];
extern const unsigned char SERVERBROWSE_INFO_EXTENDED_MORE[SERVERBROWSE_SIZE];

extern const unsigned char SERVERBROWSE_CHALLENGE[SERVERBROWSE_SIZE];

enum
{
	SERVERINFO_VANILLA = 0,
	SERVERINFO_64_LEGACY,
	SERVERINFO_EXTENDED,
	SERVERINFO_EXTENDED_MORE,
	SERVERINFO_INGAME,
};
#endif // ENGINE_SHARED_MASTERSERVER_H
