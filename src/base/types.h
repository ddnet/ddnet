#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <base/detect.h>

#include <cstdint>
#include <ctime>
#include <functional>

#if defined(CONF_FAMILY_UNIX)
#include <sys/types.h> // pid_t
#endif

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

/**
 * The maximum bytes necessary to encode one Unicode codepoint with UTF-8.
 */
inline constexpr auto UTF8_BYTE_LENGTH = 4;

inline constexpr auto IO_MAX_PATH_LENGTH = 512;

inline constexpr auto NETADDR_MAXSTRSIZE = 1 + (8 * 4 + 7) + 1 + 1 + 5 + 1; // [XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX]:XXXXX

inline constexpr auto NETTYPE_INVALID = 0;
inline constexpr auto NETTYPE_IPV4 = 1 << 0;
inline constexpr auto NETTYPE_IPV6 = 1 << 1;
inline constexpr auto NETTYPE_WEBSOCKET_IPV4 = 1 << 2;
inline constexpr auto NETTYPE_WEBSOCKET_IPV6 = 1 << 3;
inline constexpr auto NETTYPE_LINK_BROADCAST = 1 << 4;
/**
 * 0.7 address. This is a flag in NETADDR to avoid introducing a parameter to every networking function
 * to differenciate between 0.6 and 0.7 connections.
 */
inline constexpr auto NETTYPE_TW7 = 1 << 5;

inline constexpr auto NETTYPE_ALL = NETTYPE_IPV4 | NETTYPE_IPV6 | NETTYPE_WEBSOCKET_IPV4 | NETTYPE_WEBSOCKET_IPV6;
inline constexpr auto NETTYPE_MASK = NETTYPE_ALL | NETTYPE_LINK_BROADCAST | NETTYPE_TW7;

/**
 * @ingroup Network-General
 */
typedef struct NETADDR
{
	unsigned int type;
	unsigned char ip[16];
	unsigned short port;

	bool operator==(const NETADDR &other) const;
	bool operator!=(const NETADDR &other) const;
	bool operator<(const NETADDR &other) const;
} NETADDR;

template<>
struct std::hash<NETADDR>
{
	size_t operator()(const NETADDR &Addr) const noexcept;
};

/**
 * @ingroup Network-General
 */
typedef struct NETSTATS
{
	uint64_t sent_packets;
	uint64_t sent_bytes;
	uint64_t recv_packets;
	uint64_t recv_bytes;
} NETSTATS;

#if defined(CONF_FAMILY_WINDOWS)
/**
 * A handle for a process.
 *
 * @ingroup Shell
 */
typedef void *PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Shell
 */
constexpr PROCESS INVALID_PROCESS = nullptr; // NOLINT(misc-misplaced-const)
#else
/**
 * A handle for a process.
 *
 * @ingroup Shell
 */
typedef pid_t PROCESS;
/**
 * A handle that denotes an invalid process.
 *
 * @ingroup Shell
 */
constexpr PROCESS INVALID_PROCESS = 0;
#endif

#endif // BASE_TYPES_H
