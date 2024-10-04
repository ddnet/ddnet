#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <ctime>

enum class TRISTATE
{
	NONE,
	SOME,
	ALL,
};

/**
 * Handle for input/output files/streams.
 *
 * @ingroup File-IO
 */
typedef void *IOHANDLE;

typedef int (*FS_LISTDIR_CALLBACK)(const char *name, int is_dir, int dir_type, void *user);

typedef struct
{
	const char *m_pName;
	time_t m_TimeCreated; // seconds since UNIX Epoch
	time_t m_TimeModified; // seconds since UNIX Epoch
} CFsFileInfo;

typedef int (*FS_LISTDIR_CALLBACK_FILEINFO)(const CFsFileInfo *info, int is_dir, int dir_type, void *user);

/**
 * @ingroup Network-General
 */
typedef struct NETSOCKET_INTERNAL *NETSOCKET;

enum
{
	/**
	 * The maximum bytes necessary to encode one Unicode codepoint with UTF-8.
	 */
	UTF8_BYTE_LENGTH = 4,

	IO_MAX_PATH_LENGTH = 512,

	NETADDR_MAXSTRSIZE = 1 + (8 * 4 + 7) + 1 + 1 + 5 + 1, // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX

	NETTYPE_LINK_BROADCAST = 4,

	NETTYPE_INVALID = 0,
	NETTYPE_IPV4 = 1,
	NETTYPE_IPV6 = 2,
	NETTYPE_WEBSOCKET_IPV4 = 8,
	/**
	 * 0.7 address. This is a flag in NETADDR to avoid introducing a parameter to every networking function
	 * to differenciate between 0.6 and 0.7 connections.
	 */
	NETTYPE_TW7 = 16,

	NETTYPE_ALL = NETTYPE_IPV4 | NETTYPE_IPV6 | NETTYPE_WEBSOCKET_IPV4,
	NETTYPE_MASK = NETTYPE_ALL | NETTYPE_LINK_BROADCAST | NETTYPE_TW7,
};

/**
 * @ingroup Network-General
 */
typedef struct NETADDR
{
	unsigned int type;
	unsigned char ip[16];
	unsigned short port;

	bool operator==(const NETADDR &other) const;
	bool operator!=(const NETADDR &other) const { return !(*this == other); }
} NETADDR;
#endif // BASE_TYPES_H
